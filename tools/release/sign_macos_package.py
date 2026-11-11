#!/usr/bin/env python3
"""Sign a verified, extracted GUI or Core package in a NEW private directory.

No key export, credential installation, notarization, or publication is performed.
All native diagnostics are captured: they can contain personal keychain paths.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import plistlib
import re
import shutil
import subprocess
import tempfile


class GateError(Exception):
    pass


def run(args):
    try:
        result = subprocess.run(args, capture_output=True, timeout=120)
    except subprocess.TimeoutExpired:
        raise GateError(f'{Path(args[0]).name} timed out; check native keychain approval or connectivity') from None
    if result.returncode:
        raise GateError(f"{Path(args[0]).name} failed (exit {result.returncode}); diagnostics withheld")
    return result.stdout


def macho(path):
    if path.is_symlink() or not path.is_file():
        return False
    with path.open('rb') as handle:
        return handle.read(4) in (
            b'\xcf\xfa\xed\xfe', b'\xce\xfa\xed\xfe', b'\xfe\xed\xfa\xcf',
            b'\xfe\xed\xfa\xce', b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca',
        )


def validate_tree(root):
    for path in root.rglob('*'):
        if path.is_symlink():
            resolved = path.resolve(strict=True)
            if root not in resolved.parents:
                raise GateError('Package contains an escaping symbolic link')
        elif not (path.is_file() or path.is_dir()):
            raise GateError('Package contains a special file')


def privacy_scan(root, tokens):
    # Deliberately report counts, not matching personal data or file contents.
    matches = 0
    # Unicode-aware component boundaries avoid treating ICU language/country
    # names with accented suffixes as personal names. Scan both UTF-16 byte
    # alignments because strings can be embedded at arbitrary binary offsets.
    needles = [(t.casefold(), re.compile(r'(?<![^\W_])' + re.escape(t.casefold()) +
                                        r'(?![^\W_])')) for t in tokens]
    def contains(text):
        folded = text.casefold()
        return any(token in folded and needle.search(folded) for token, needle in needles)

    def encoded_texts(data):
        yield data.decode('utf-8', errors='replace')
        for encoding in ('utf-16-le', 'utf-16-be'):
            for offset in (0, 1):
                yield data[offset:].decode(encoding, errors='replace')

    for path in root.rglob('*'):
        if contains(str(path.relative_to(root))):
            matches += 1
        if path.is_file() and not path.is_symlink():
            if any(contains(text) for text in encoded_texts(path.read_bytes())):
                matches += 1
    if matches:
        raise GateError(f'Privacy scan rejected {matches} file/name matches; values withheld')


def approve_certificate(cert, org, team):
    subject = run(['openssl', 'x509', '-inform', 'DER', '-in', str(cert),
                   '-noout', '-subject', '-nameopt', 'RFC2253']).decode().strip()
    fields = subject.removeprefix('subject=').strip().split(',')
    required = {f'CN=Developer ID Application: {org} ({team})',
                f'OU={team}', f'O={org}', f'UID={team}'}
    countries = [f for f in fields if re.fullmatch(r'C=[A-Z]{2}', f)]
    if len(countries) != 1 or len(fields) != 5 or set(fields) != required | set(countries):
        raise GateError('Certificate subject does not exactly match the approved organization/team')
    run(['openssl', 'x509', '-inform', 'DER', '-in', str(cert), '-noout', '-checkend', '86400'])
    identities = run(['security', 'find-identity', '-v', '-p', 'codesigning']).decode()
    wanted = f'Developer ID Application: {org} ({team})'
    cert_sha1 = hashlib.sha1(cert.read_bytes()).hexdigest().upper()
    matches = re.findall(r'\b([A-Fa-f0-9]{40})\s+"([^"]+)"', identities)
    if (cert_sha1, wanted) not in [(h.upper(), n) for h, n in matches]:
        raise GateError('Approved certificate has no usable signing identity in accessible keychains')
    return cert_sha1


def verify(path, cert, requirement, scratch):
    run(['codesign', '--verify', '--strict', '-R', '=' + requirement, str(path)])
    prefix = str(scratch / 'leaf-')
    run(['codesign', '-d', '--extract-certificates=' + prefix, str(path)])
    if (scratch / 'leaf-0').read_bytes() != cert.read_bytes():
        raise GateError('Signed code does not carry the exact approved certificate')
    try:
        display = subprocess.run(['codesign', '-d', '--verbose=4', str(path)],
                                 capture_output=True, timeout=120)
    except subprocess.TimeoutExpired:
        raise GateError('Signature inspection timed out; diagnostics withheld') from None
    if display.returncode or b'runtime' not in display.stderr or b'Timestamp=' not in display.stderr:
        raise GateError('Hardened runtime or secure timestamp missing')
    data = run(['codesign', '-d', '--entitlements', ':-', str(path)])
    actual = plistlib.loads(data) if data.strip() else {}
    executable = path.name in {'qwertycoind', 'qwertycoin-wallet-cli', 'qwertycoin-wallet-rpc', 'qwertycoin-gui'}
    expected = {'com.apple.security.cs.allow-jit': True} if executable else {}
    if actual != expected:
        raise GateError('Unexpected executable or library entitlements')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--certificate', type=Path, required=True)
    parser.add_argument('--organization', required=True)
    parser.add_argument('--team-id', required=True)
    parser.add_argument('--privacy-token-file', type=Path, required=True,
                        help='Private JSON array of personal names/emails/build paths; never committed')
    args = parser.parse_args()
    if platform.system() != 'Darwin':
        raise GateError('Signing must run on macOS')
    if not re.fullmatch(r'[A-Z0-9]{10}', args.team_id) or any(c in args.organization for c in ',\n\r'):
        raise GateError('Invalid organization/team input')
    source = args.input.resolve(strict=True)
    output = args.output.absolute()
    if not source.is_dir() or output.exists() or output.is_symlink():
        raise GateError('Input must be a directory and output must not exist')
    if source == output.resolve() or source in output.resolve().parents:
        raise GateError('Output cannot be inside input')
    tokens = json.loads(args.privacy_token_file.read_text())
    if not isinstance(tokens, list) or not tokens or not all(isinstance(t, str) and len(t) >= 4 for t in tokens):
        raise GateError('Privacy token file must contain a nonempty list of strings of length >= 4')
    validate_tree(source)
    privacy_scan(source, tokens)
    identity = approve_certificate(args.certificate, args.organization, args.team_id)
    binaries = [p for p in source.rglob('*') if macho(p)]
    if not binaries:
        raise GateError('No Mach-O binaries in package')
    names = {p.name for p in binaries}
    if not {'qwertycoind', 'qwertycoin-wallet-cli', 'qwertycoin-wallet-rpc'} <= names:
        raise GateError('Package must include the three Qwertycoin Core executables')
    # Reject preexisting broken signatures before copying or signing anything.
    for path in binaries:
        run(['codesign', '--verify', '--strict', str(path)])
    os.umask(0o077)
    output.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(source, output, symlinks=True)
    requirement = ('anchor apple generic and certificate leaf[field.1.2.840.113635.100.6.1.13] exists '
                   f'and certificate leaf[subject.OU] = "{args.team_id}"')
    with tempfile.TemporaryDirectory(prefix='qwc-sign-') as temporary:
        scratch = Path(temporary)
        entitlements = scratch / 'jit.plist'
        # RandomX and Qt QML use JIT. Do NOT disable library validation, enable
        # debugging, or grant unrestricted executable memory as a workaround.
        entitlements.write_bytes(plistlib.dumps({'com.apple.security.cs.allow-jit': True}))
        def sign(path, executable=False):
            cmd = ['codesign', '--force', '--timestamp', '--options', 'runtime', '--sign', identity]
            if executable:
                cmd += ['--entitlements', str(entitlements)]
            run(cmd + [str(path)])

        signed_binaries = [output / p.relative_to(source) for p in binaries]
        for path in sorted(signed_binaries, key=lambda p: len(p.parts), reverse=True):
            sign(path, path.name in {'qwertycoind', 'qwertycoin-wallet-cli', 'qwertycoin-wallet-rpc', 'qwertycoin-gui'})
        bundles = [p for p in output.rglob('*') if p.is_dir() and not p.is_symlink()
                   and p.suffix in {'.framework', '.app', '.xpc', '.appex'}]
        for path in sorted(bundles, key=lambda p: len(p.parts), reverse=True):
            sign(path, path.suffix == '.app')
        for path in signed_binaries:
            verify(path, args.certificate, requirement, scratch)
        for path in bundles:
            run(['codesign', '--verify', '--deep', '--strict', '-R', '=' + requirement, str(path)])
        privacy_scan(output, tokens)
    evidence = {'schema': 1, 'certificate_policy': 'matched',
                'macho_files_verified': len(signed_binaries), 'bundles_verified': len(bundles),
                'hardened_runtime': True, 'secure_timestamp': True,
                'personal_token_scan': 'passed', 'notarization': 'not-submitted'}
    (output / 'SIGNING-INFO.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(json.dumps(evidence))


if __name__ == '__main__':
    try:
        main()
    except (GateError, OSError, ValueError) as error:
        # OSError may include an absolute personal path. Never echo it.
        print(str(error) if isinstance(error, GateError) else 'Input or filesystem validation failed; details withheld')
        raise SystemExit(1)
