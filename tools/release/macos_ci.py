#!/usr/bin/env python3
"""Protected, identity-neutral macOS signing pipeline. Never print native output."""
import argparse
import base64
import hashlib
import json
import io
import stat
import zipfile
import os
from pathlib import Path
import re
import secrets
import shutil
import subprocess
import tarfile

import sign_macos_package as signer

HERE = Path(__file__).resolve().parent


def require(value, message):
    if not value:
        raise signer.GateError(message)


def run(args, timeout=120, env=None):
    try:
        result = subprocess.run([str(x) for x in args], capture_output=True, timeout=timeout, env=env)
    except subprocess.TimeoutExpired:
        raise signer.GateError('Native step timed out; details withheld') from None
    require(result.returncode == 0, 'Native step failed; details withheld')
    return result.stdout


def api(path):
    return json.loads(run(['gh', 'api', path]))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_request(kind, repo, ref, tag, revision, run_id):
    expected_repo = 'qwertycoin-org/qwertycoin' + ('-gui' if kind == 'gui' else '')
    branch = 'master' if kind == 'gui' else 'main'
    require(repo == expected_repo and ref == 'refs/heads/' + branch, 'Untrusted signing caller')
    require(re.fullmatch(r'v[0-9]+\.[0-9]+\.[0-9]+(?:-rc[1-9][0-9]*)?', tag), 'Invalid version tag')
    require(re.fullmatch(r'[0-9a-f]{40}', revision), 'Invalid source revision')
    require(re.fullmatch(r'[1-9][0-9]*', run_id), 'Invalid source run')
    return branch, ('qwertycoin-gui-' if kind == 'gui' else 'qwertycoin-') + tag + '-macos-arm64'


def validate_run(data, repo, branch, kind):
    require(data.get('repository', {}).get('full_name') == repo and
            data.get('head_repository', {}).get('full_name') == repo and
            data.get('path') == '.github/workflows/release.yml' and
            data.get('name') == 'qwc/' + kind + '-release-candidate' and
            data.get('event') == 'workflow_dispatch' and
            data.get('head_branch') == branch and
            data.get('status') == 'completed' and data.get('conclusion') == 'success' and
            data.get('run_attempt') == 1 and
            re.fullmatch(r'[0-9a-f]{40}', data.get('head_sha', '')), 'Untrusted or incomplete source build')


def preflight(args):
    repo = os.environ['GITHUB_REPOSITORY']
    branch, name = validate_request(args.kind, repo, os.environ['GITHUB_REF'],
                                    args.tag, args.revision, args.run_id)
    data = api(f'repos/{repo}/actions/runs/{args.run_id}')
    validate_run(data, repo, branch, args.kind)
    for ancestor, head in [(args.revision, data['head_sha']), (data['head_sha'], os.environ['GITHUB_SHA'])]:
        run(['git', 'merge-base', '--is-ancestor', ancestor, head])
    # Reject workflows whose native build recipe differs from the reviewed one.
    old = run(['git', 'rev-parse', data['head_sha'] + ':.github/workflows/release.yml'])
    current = run(['git', 'rev-parse', os.environ['GITHUB_SHA'] + ':.github/workflows/release.yml'])
    require(old == current, 'Build workflow differs from trusted revision')
    jobs = api(f'repos/{repo}/actions/runs/{args.run_id}/jobs?per_page=100')['jobs']
    require(any(j['name'] == 'macOS Apple Silicon' and j['conclusion'] == 'success'
                and j['status'] == 'completed' for j in jobs), 'Native macOS build did not pass')
    artifacts = api(f'repos/{repo}/actions/runs/{args.run_id}/artifacts?per_page=100')['artifacts']
    matches = [a for a in artifacts if a['name'] == name and not a['expired']]
    require(len(matches) == 1, 'Candidate artifact is missing or ambiguous')
    artifact = matches[0]
    require(re.fullmatch(r'sha256:[0-9a-f]{64}', artifact.get('digest', '')), 'Artifact has no immutable digest')
    record = dict(kind=args.kind, repository=repo, source_revision=args.revision,
                  tag=args.tag, source_run_id=args.run_id, source_run_attempt=1,
                  build_workflow_revision=data['head_sha'], signing_workflow_revision=os.environ['GITHUB_SHA'],
                  artifact_id=artifact['id'], artifact_digest=artifact['digest'], name=name)
    if args.kind == 'gui':
        record['core_revision'] = run(['git', 'rev-parse', args.revision + ':qwertycoin']).decode().strip()
    args.work.mkdir(parents=True, exist_ok=True)
    (args.work/'request.json').write_text(json.dumps(record, indent=2)+'\n')
    with open(os.environ['GITHUB_OUTPUT'], 'a') as out:
        out.write('artifact_id='+str(artifact['id'])+'\nname='+name+'\n')
        out.write('request='+base64.b64encode(json.dumps(record).encode()).decode()+'\n')
    print('Trusted macOS source build and artifact verified')



def restore_request(args):
    record = json.loads(base64.b64decode(os.environ['REQUEST_JSON'], validate=True))
    validate_request(record['kind'], os.environ['GITHUB_REPOSITORY'], os.environ['GITHUB_REF'],
                     record['tag'], record['source_revision'], record['source_run_id'])
    require(record['signing_workflow_revision'] == os.environ['GITHUB_SHA'], 'Wrong signing workflow revision')
    args.work.mkdir(parents=True, exist_ok=True)
    (args.work/'request.json').write_text(json.dumps(record, indent=2)+'\n')


def extract_artifact(payload, expected_digest, expected, destination):
    require('sha256:'+hashlib.sha256(payload).hexdigest() == expected_digest,
            'Downloaded artifact digest mismatch')
    destination.mkdir(parents=True)
    with zipfile.ZipFile(io.BytesIO(payload)) as archive:
        members = archive.infolist()
        require(len(members) == len(expected) and {x.filename for x in members} == expected,
                'Unexpected artifact members')
        require(sum(x.file_size for x in members) < 1024**3 and all(
            not x.is_dir() and not stat.S_ISLNK(x.external_attr >> 16) for x in members),
                'Unsafe artifact ZIP')
        for member in members:
            (destination/member.filename).write_bytes(archive.read(member))


def fetch(args):
    record = json.loads((args.work/'request.json').read_text())
    artifact_id = record['artifact_id']
    expected_digest = record['artifact_digest']
    name = record['name']
    expected = {name+'.tar.gz', name+'.tar.gz.sha256'} if record['kind'] == 'core' else {
        name+'.tar.gz', name+'.sha256', name+'.dmg', name+'.dmg.sha256'}
    if args.stage == 'fetch-signed':
        artifact_id = os.environ['SIGNED_ARTIFACT_ID']
        expected_digest = 'sha256:'+os.environ['SIGNED_ARTIFACT_DIGEST']
        require(re.fullmatch(r'[1-9][0-9]*', artifact_id), 'Invalid signed artifact ID')
        require(re.fullmatch(r'sha256:[0-9a-f]{64}', expected_digest), 'Invalid signed artifact digest')
        expected = {name+'.dmg', 'SHA256SUMS', 'SIGNING-EVIDENCE.json'}
    payload = run(['gh', 'api', f"repos/{record['repository']}/actions/artifacts/{artifact_id}/zip"], timeout=300)
    extract_artifact(payload, expected_digest, expected, args.incoming)
    print('Exact source artifact digest and member set verified')

def unpack_candidate(record, incoming, destination):
    archive = incoming/(record['name']+'.tar.gz')
    require(archive.is_file() and not archive.is_symlink(), 'Missing source archive')
    command = ['python3', HERE/'verify_candidate_archive.py', '--archive', archive,
               '--destination', destination, '--root-name', record['name'], '--platform', 'macos']
    if record['kind'] == 'core':
        manifest = incoming/(record['name']+'.tar.gz.sha256')
        require(manifest.read_text().strip() == digest(archive)+'  '+archive.name, 'Source archive checksum mismatch')
        command += ['--expected-tag', record['tag'], '--expected-revision', record['source_revision'],
                    '--expected-workflow-name', 'qwc/core-release-candidate',
                    '--expected-workflow-revision', record['build_workflow_revision'],
                    '--expected-run-id', record['source_run_id'], '--expected-run-attempt', '1',
                    '--expected-os', 'macOS', '--expected-arch', 'arm64']
    else:
        with tarfile.open(archive) as tar:
            member = tar.getmember(record['name']+'/BUILD-INFO.txt')
            require(member.isfile() and member.size < 65536, 'Invalid GUI metadata')
            lines = tar.extractfile(member).read().decode().splitlines()
        qt = [x.split('=', 1)[1] for x in lines if x.startswith('qt_version=')]
        require(len(qt) == 1 and re.fullmatch(r'5\.15\.[0-9]+', qt[0]), 'Unexpected Qt runtime')
        command += ['--expected-source', record['source_revision'], '--expected-core', record['core_revision'],
                    '--expected-version', record['tag'][1:].split('-rc')[0], '--expected-os', 'macOS',
                    '--expected-arch', 'ARM64', '--expected-qt', qt[0], '--expected-macos-min-version', '15.0']
    run(command)
    return destination/record['name']


def config(work):
    os.umask(0o077)
    work.mkdir(parents=True, exist_ok=True)
    tokens = json.loads(os.environ['PRIVATE_IDENTITY_TOKENS'])
    require(isinstance(tokens, list) and tokens and all(isinstance(x, str) and len(x) >= 4 for x in tokens),
            'Invalid private identity policy')
    (work/'privacy.json').write_text(json.dumps(tokens))
    # Explicit masks supplement, never replace, captured native diagnostics.
    if os.environ.get('GITHUB_ACTIONS') == 'true':
        for value in tokens + [os.environ['APPLE_SIGNING_ORGANIZATION'], os.environ['APPLE_SIGNING_TEAM_ID']]:
            print('::add-mask::'+value.replace('%', '%25').replace('\r', '%0D').replace('\n', '%0A'))
    cert = work/'certificate.cer'
    cert.write_bytes(base64.b64decode(os.environ['APPLE_SIGNING_CERT_DER'], validate=True))
    require(digest(cert) == os.environ['APPLE_SIGNING_CERT_SHA256'], 'Certificate pin mismatch')
    return cert, tokens


def notarize(path, work):
    # Submission IDs and Apple account metadata remain temporary, not public logs/artifacts.
    cmd = ['xcrun', 'notarytool', 'submit', path, '--keychain-profile', 'qwc-ci-notary',
           '--keychain', work/'signing.keychain-db', '--output-format', 'json']
    receipt = json.loads(run(cmd, timeout=300))
    sid = receipt['id']
    require(re.fullmatch(r'[a-f0-9-]{36}', sid), 'Invalid notarization receipt')
    (work/(path.name+'.notary.json')).write_text(json.dumps(receipt))
    info = json.loads(run(['xcrun', 'notarytool', 'wait', sid, '--keychain-profile', 'qwc-ci-notary',
                          '--keychain', work/'signing.keychain-db', '--timeout', '20m',
                          '--output-format', 'json'], timeout=1230))
    require(info.get('status') == 'Accepted', 'Apple did not accept notarization')
    log = json.loads(run(['xcrun', 'notarytool', 'log', sid, '--keychain-profile', 'qwc-ci-notary',
                         '--keychain', work/'signing.keychain-db']))
    require(log.get('status') == 'Accepted' and not log.get('issues'), 'Notarization reported issues')


def sign_stage(args):
    require(os.environ.get('RUNNER_ENVIRONMENT') == 'github-hosted', 'Signing requires an ephemeral hosted runner')
    record = json.loads((args.work/'request.json').read_text())
    # All archive validation occurs before the private key is imported. No payload is executed here.
    source = unpack_candidate(record, args.incoming, args.work/'unsigned')
    sign_verified_package(args, record, source)


def sign_verified_package(args, record, source):
    """Native signing stage, also exercised by private macOS integration tests."""
    private = args.work/'private'; cert, tokens = config(private)
    keychain = private/'signing.keychain-db'; password = secrets.token_urlsafe(32)
    (private/'keychain-password').write_text(password)
    (private/'certificate.p12').write_bytes(base64.b64decode(os.environ['APPLE_SIGNING_CERT_P12'], validate=True))
    (private/'notary.p8').write_bytes(base64.b64decode(os.environ['APPLE_NOTARY_KEY_P8'], validate=True))
    original = run(['security', 'list-keychains', '-d', 'user']).decode()
    (private/'keychain-search-list.json').write_text(json.dumps(re.findall(r'"([^"]+)"', original)))
    try:
        run(['security', 'create-keychain', '-p', password, keychain])
        run(['security', 'set-keychain-settings', '-lut', '3600', keychain])
        run(['security', 'unlock-keychain', '-p', password, keychain])
        run(['security', 'import', private/'certificate.p12', '-k', keychain,
             '-P', os.environ['APPLE_SIGNING_CERT_PASSWORD'], '-T', '/usr/bin/codesign'])
        run(['security', 'set-key-partition-list', '-S', 'apple-tool:,apple:', '-k', password, keychain])
        run(['security', 'list-keychains', '-d', 'user', '-s', keychain, *json.loads((private/'keychain-search-list.json').read_text())])
        run(['xcrun', 'notarytool', 'store-credentials', 'qwc-ci-notary', '--keychain', keychain,
             '--key', private/'notary.p8', '--key-id', os.environ['APPLE_NOTARY_KEY_ID'],
             '--issuer', os.environ['APPLE_NOTARY_ISSUER_ID']])
        signed = args.work/'signed'/record['name']
        run(['python3', HERE/'sign_macos_package.py', '--input', source, '--output', signed,
             '--certificate', cert, '--organization', os.environ['APPLE_SIGNING_ORGANIZATION'],
             '--team-id', os.environ['APPLE_SIGNING_TEAM_ID'], '--privacy-token-file', private/'privacy.json'], timeout=1200)
        args.output.mkdir(parents=True, exist_ok=True)
        dmg = args.output/(record['name']+'.dmg')
        require(not dmg.exists(), 'Refusing to overwrite signed output')
        if record['kind'] == 'gui':
            app = signed/'qwertycoin-gui.app'; archive = args.work/'app-notary.zip'
            run(['ditto', '-c', '-k', '--keepParent', app, archive])
            notarize(archive, private)
            run(['xcrun', 'stapler', 'staple', app]); run(['xcrun', 'stapler', 'validate', app])
            run(['bash', HERE/'create_macos_dmg.sh', signed, record['name'], args.output], timeout=300)
        else:
            run(['hdiutil', 'create', '-srcfolder', signed, '-volname', 'Qwertycoin Core', '-format', 'UDZO', dmg], timeout=300)
        identity = hashlib.sha1(cert.read_bytes()).hexdigest().upper()
        run(['codesign', '--force', '--timestamp', '--sign', identity, dmg])
        notarize(dmg, private)
        run(['xcrun', 'stapler', 'staple', dmg]); run(['xcrun', 'stapler', 'validate', dmg])
        # Record no organization, team, certificate fingerprint or Apple account IDs.
        evidence = dict(record, notarization='Accepted', hardened_runtime=True, secure_timestamp=True,
                        certificate_policy='matched', files={dmg.name: digest(dmg)})
        (args.output/'SIGNING-EVIDENCE.json').write_text(json.dumps(evidence, indent=2)+'\n')
        (args.output/'SHA256SUMS').write_text(digest(dmg)+'  '+dmg.name+'\n')
        for p in args.output.glob('*.sha256'):p.unlink()  # Pre-sign DMG checksum from existing packager is stale.
        print('Signed and notarized candidate prepared for independent verification')
    finally:
        cleanup(args)


def cleanup(args):
    private = args.work/'private'
    if not private.exists():return
    failed = False
    saved = private/'keychain-search-list.json'
    if saved.exists():
        try:run(['security', 'list-keychains', '-d', 'user', '-s', *json.loads(saved.read_text())])
        except signer.GateError:failed = True
    keychain = private/'signing.keychain-db'
    if keychain.exists():
        try:run(['security', 'delete-keychain', keychain])
        except signer.GateError:failed = True
    shutil.rmtree(private)
    require(not failed, 'Temporary credential cleanup failed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stage', choices=['preflight', 'request', 'fetch', 'fetch-signed', 'sign', 'cleanup'])
    parser.add_argument('--kind', choices=['core', 'gui'])
    parser.add_argument('--tag'); parser.add_argument('--revision'); parser.add_argument('--run-id')
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--incoming', type=Path); parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.stage == 'preflight':preflight(args)
    elif args.stage == 'request':restore_request(args)
    elif args.stage in ('fetch', 'fetch-signed'):fetch(args)
    elif args.stage == 'sign':sign_stage(args)
    else:cleanup(args)


if __name__ == '__main__':
    try:main()
    except (Exception, SystemExit) as error:
        if isinstance(error, SystemExit) and error.code == 0:raise
        print(str(error) if isinstance(error, signer.GateError) else 'Signing pipeline failed; private diagnostics withheld')
        raise SystemExit(1)
