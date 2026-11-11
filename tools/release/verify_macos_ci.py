#!/usr/bin/env python3
"""Verify a notarized DMG on a fresh runner without signing/notary credentials."""
import argparse
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import tempfile
import time
import urllib.request

import macos_ci as ci
import sign_macos_package as signer


def runtime(package, kind, version, work):
    bin_dir = package/'Contents/MacOS' if kind == 'gui' else package
    runtime_env = {k: v for k, v in os.environ.items()
                   if not k.startswith(('APPLE_', 'GH_', 'GITHUB_')) and k != 'PRIVATE_IDENTITY_TOKENS'}
    for name in ('qwertycoind', 'qwertycoin-wallet-cli', 'qwertycoin-wallet-rpc'):
        ci.require(version.encode() in ci.run([bin_dir/name, '--version'], env=runtime_env), 'Wrong executable version')
    if kind == 'gui':
        home = work/'preferences'; home.mkdir()
        env = dict(runtime_env, CFFIXED_USER_HOME=str(home), XDG_CONFIG_HOME=str(home/'config'),
                   XDG_CACHE_HOME=str(home/'cache'))
        profile = '(version 1)(allow default)(deny network*)(deny file-read* file-write* (subpath '+json.dumps(str(Path.home()))+'))'
        profile += '(allow file-read* file-write* (subpath '+json.dumps(str(work))+'))'
        result = subprocess.run(['/usr/bin/sandbox-exec', '-p', profile, str(bin_dir/'qwertycoin-gui'),
                                 '--test-qml', '--disable-check-updates', '--log-file', str(work/'gui.log')],
                                capture_output=True, env=env, timeout=90)
        ci.require(result.returncode == 0 and not re.search(
            rb'TypeError|ReferenceError|module .* is not installed|QQmlApplicationEngine failed|'
            rb'failed to load component|no root objects|cannot load library|could not load the Qt platform plugin',
            result.stdout+result.stderr, re.I), 'GUI runtime smoke failed')
    ports = []
    for _ in range(2):
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0)); ports.append(sock.getsockname()[1])
    command = [str(bin_dir/'qwertycoind'), '--regtest', '--offline', '--fixed-difficulty', '1',
               '--data-dir', str(work/'chain'), '--log-file', str(work/'daemon.log'),
               '--rpc-bind-ip', '127.0.0.1', '--rpc-bind-port', str(ports[0]),
               '--p2p-bind-ip', '127.0.0.1', '--p2p-bind-port', str(ports[1]),
               '--no-zmq', '--no-igd', '--hide-my-port', '--non-interactive', '--max-concurrency', '1']
    def rpc(method, params=None):
        req = urllib.request.Request('http://127.0.0.1:%d/json_rpc' % ports[0],
            data=json.dumps(dict(jsonrpc='2.0', id='0', method=method, params=params or {})).encode(),
            headers={'Content-Type': 'application/json'})
        with urllib.request.urlopen(req, timeout=120) as response:
            data = json.load(response)
        ci.require('error' not in data, 'Disposable runtime RPC failed')
        return data['result']
    with (work/'daemon-output.log').open('wb') as log:
        process = subprocess.Popen(command, stdout=log, stderr=log, cwd=work, env=runtime_env)
        try:
            for _ in range(60):
                ci.require(process.poll() is None, 'Disposable daemon failed to start')
                try:
                    before = rpc('get_info')['height']; break
                except Exception:time.sleep(.5)
            else:raise signer.GateError('Disposable daemon readiness timed out')
            rpc('generateblocks', dict(wallet_address='QWC1GKRV4Wd3DCnubWiv1CBDBJWTCUcvTAZnE9ioE7pUMiqwXpce7GX5kfyaW4X8V523Rkyoa9NQ6LWj2DrEX7cE2PDLobNPxh', amount_of_blocks=2))
            ci.require(rpc('get_info')['height'] == before+2 and
                       rpc('get_last_block_header')['block_header']['major_version'] == 17,
                       'RandomX runtime check failed')
        finally:
            process.terminate()
            try:process.wait(timeout=20)
            except subprocess.TimeoutExpired:process.kill(); process.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--incoming', type=Path, required=True)
    parser.add_argument('--request', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    ci.require(not any(os.environ.get(k) for k in ('APPLE_SIGNING_CERT_P12', 'APPLE_SIGNING_CERT_PASSWORD',
                                                  'APPLE_NOTARY_KEY_P8')), 'Verifier must not receive signing credentials')
    request = json.loads(args.request.read_text())
    evidence = json.loads((args.incoming/'SIGNING-EVIDENCE.json').read_text())
    ci.require(all(evidence.get(k) == v for k, v in request.items()), 'Candidate provenance mismatch')
    name = request['name']+'.dmg'; dmg = args.incoming/name
    ci.require(evidence.get('files') == {name: ci.digest(dmg)} and evidence.get('notarization') == 'Accepted',
               'Signed DMG hash or notarization evidence mismatch')
    ci.require({p.name for p in args.incoming.iterdir()} == {name, 'SHA256SUMS', 'SIGNING-EVIDENCE.json'},
               'Unexpected signed artifact files')
    ci.require((args.incoming/'SHA256SUMS').read_text() == ci.digest(dmg)+'  '+name+'\n', 'Checksum manifest mismatch')
    with tempfile.TemporaryDirectory(prefix='qwc-ci-verify-') as temporary:
        work = Path(temporary); cert, tokens = ci.config(work/'policy')
        ci.run(['xcrun', 'stapler', 'validate', dmg])
        ci.run(['spctl', '--assess', '--type', 'open', '--context', 'context:primary-signature', dmg])
        requirement = ('anchor apple generic and certificate leaf[field.1.2.840.113635.100.6.1.13] exists '
                       'and certificate leaf[subject.OU] = "'+os.environ['APPLE_SIGNING_TEAM_ID']+'"')
        ci.run(['codesign', '--verify', '--strict', '-R', '='+requirement, dmg])
        ci.run(['codesign', '-d', '--extract-certificates='+str(work/'container-'), dmg])
        ci.require((work/'container-0').read_bytes() == cert.read_bytes(), 'Container signer pin mismatch')
        mount = work/'mount'; mount.mkdir()
        ci.run(['hdiutil', 'attach', dmg, '-readonly', '-nobrowse', '-mountpoint', mount])
        try:
            signer.privacy_scan(mount, tokens)
            metadata = work/'metadata'; metadata.mkdir(); attribute_count = 0
            for path in mount.rglob('*'):
                if path.is_symlink():continue
                for attribute in ci.run(['xattr', path]).decode().splitlines():
                    value = bytes.fromhex(ci.run(['xattr', '-p', '-x', attribute, path]).decode())
                    (metadata/str(attribute_count)).write_bytes(attribute.encode()+b'\0'+value)
                    attribute_count += 1
            signer.privacy_scan(metadata, tokens)
            package = mount/'Qwertycoin.app' if request['kind'] == 'gui' else mount
            count = 0
            for path in package.rglob('*'):
                if signer.macho(path):
                    signer.verify(path, cert, requirement, work)
                    ci.run(['codesign', '--verify', '--strict', '-R', '=notarized', path]); count += 1
            ci.require(count >= 3, 'Package has no complete native payload')
            if request['kind'] == 'gui':
                ci.run(['codesign', '--verify', '--deep', '--strict', package])
                ci.run(['xcrun', 'stapler', 'validate', package])
                ci.run(['spctl', '--assess', '--type', 'execute', package])
                ci.require((mount/'Applications').is_symlink() and os.readlink(mount/'Applications') == '/Applications',
                           'Invalid application installation link')
            copied = work/('Qwertycoin.app' if request['kind'] == 'gui' else 'core')
            ci.run(['ditto', '--rsrc', '--extattr', package, copied])
            if request['kind'] == 'gui':
                ci.run(['python3', ci.HERE/'compare_bundle_trees.py', package, copied])
        finally:ci.run(['hdiutil', 'detach', mount])
        if request['kind'] == 'gui':
            ci.run(['xcrun', 'stapler', 'validate', copied]); ci.run(['spctl', '--assess', '--type', 'execute', copied])
        runtime(copied, request['kind'], request['tag'][1:].split('-rc')[0], work)
    args.output.mkdir(parents=True, exist_ok=True)
    # Only allowlisted deliverables, never a runner workspace, keychain or logs.
    import shutil
    for filename in (name, 'SHA256SUMS', 'SIGNING-EVIDENCE.json'):
        shutil.copyfile(args.incoming/filename, args.output/filename)
    report = dict(source_revision=request['source_revision'], source_run_id=request['source_run_id'],
                  files=evidence['files'], verified_macho_count=count, notarization='Accepted',
                  gatekeeper='passed', runtime='passed', randomx_blocks=2,
                  privacy='passed', xattrs_scanned=attribute_count)
    (args.output/'VERIFICATION.json').write_text(json.dumps(report, indent=2)+'\n')
    print('Independent signature, notarization, runtime and privacy verification passed')


if __name__ == '__main__':
    try:main()
    except Exception as error:
        print(str(error) if isinstance(error, signer.GateError) else 'Independent verification failed; details withheld')
        raise SystemExit(1)
