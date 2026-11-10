"""Fail-closed provenance, archive, credential and workflow boundary tests."""
import base64
import hashlib
import io
import json
import os
from pathlib import Path
import stat
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).parents[1]))
import macos_ci as ci
import verify_macos_ci as verifier


class SigningPipelineGates(unittest.TestCase):
    def test_only_expected_repo_default_branch_and_immutable_inputs(self):
        for kind, repo, branch in [('core', 'qwertycoin', 'main'), ('gui', 'qwertycoin-gui', 'master')]:
            valid = [kind, 'qwertycoin-org/'+repo, 'refs/heads/'+branch, 'v2.0.6', 'a'*40, '123']
            ci.validate_request(*valid)
            for index, value in [(1, 'fork/'+repo), (2, 'refs/pull/1/merge'),
                                 (2, 'refs/heads/feature'), (3, '$(unsafe)'), (3, '../v2.0.6'),
                                 (4, 'main'), (4, 'a'*39), (5, '1/../../secrets')]:
                changed = valid.copy(); changed[index] = value
                with self.subTest(kind=kind, index=index, value=value), self.assertRaises(ci.signer.GateError):
                    ci.validate_request(*changed)

    def test_only_successful_first_attempt_trusted_build(self):
        repo = 'qwertycoin-org/qwertycoin'
        good = dict(repository=dict(full_name=repo), head_repository=dict(full_name=repo),
                    path='.github/workflows/release.yml', name='qwc/core-release-candidate',
                    event='workflow_dispatch', head_branch='main', status='completed',
                    conclusion='success', run_attempt=1, head_sha='a'*40)
        ci.validate_run(good, repo, 'main', 'core')
        bad = dict(head_repository=dict(full_name='fork/qwertycoin'), event='pull_request',
                   head_branch='feature', status='in_progress', conclusion='failure', run_attempt=2,
                   path='.github/workflows/other.yml', name='forged', head_sha='HEAD')
        for key, value in bad.items():
            with self.subTest(key=key), self.assertRaises(ci.signer.GateError):
                ci.validate_run(dict(good, **{key: value}), repo, 'main', 'core')

    @staticmethod
    def archive(entries):
        stream = io.BytesIO()
        with zipfile.ZipFile(stream, 'w') as archive:
            for name, body in entries:archive.writestr(name, body)
        data = stream.getvalue()
        return data, 'sha256:'+hashlib.sha256(data).hexdigest()

    def test_exact_artifact_digest_and_members(self):
        data, digest = self.archive([('candidate.dmg', b'test payload')])
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            ci.extract_artifact(data, digest, {'candidate.dmg'}, root/'good')
            self.assertEqual((root/'good/candidate.dmg').read_bytes(), b'test payload')
            with self.assertRaisesRegex(ci.signer.GateError, 'digest mismatch'):
                ci.extract_artifact(data, 'sha256:'+'0'*64, {'candidate.dmg'}, root/'bad')
            self.assertFalse((root/'bad').exists())

    def test_duplicate_extra_and_traversal_members_rejected(self):
        for entries in [[('../outside', b'x')], [('candidate.dmg', b'x'), ('keychain', b'secret')],
                        [('candidate.dmg', b'a'), ('candidate.dmg', b'b')]]:
            with tempfile.TemporaryDirectory() as temporary:
                data, digest = self.archive(entries)
                with self.assertRaises(ci.signer.GateError):
                    ci.extract_artifact(data, digest, {'candidate.dmg'}, Path(temporary)/'out')
                self.assertFalse((Path(temporary)/'outside').exists())

    def test_symlink_artifact_member_rejected(self):
        link = zipfile.ZipInfo('candidate.dmg'); link.create_system = 3
        link.external_attr = (stat.S_IFLNK | 0o777) << 16
        data, digest = self.archive([(link, b'/private/keychain')])
        with tempfile.TemporaryDirectory() as temporary, self.assertRaises(ci.signer.GateError):
            ci.extract_artifact(data, digest, {'candidate.dmg'}, Path(temporary)/'out')

    def test_self_hosted_signing_rejected_before_reading_payload(self):
        with patch.dict(os.environ, {'RUNNER_ENVIRONMENT':'self-hosted'}), self.assertRaisesRegex(ci.signer.GateError, 'hosted runner'):
            ci.sign_stage(SimpleNamespace())

    def test_private_native_errors_are_not_echoed(self):
        result = SimpleNamespace(returncode=1, stdout=b'private organization', stderr=b'/Users/private-name/key')
        with patch.object(ci.subprocess, 'run', return_value=result), self.assertRaises(ci.signer.GateError) as error:
            ci.run(['native-tool'])
        self.assertNotIn('private-name', str(error.exception))
        self.assertNotIn('organization', str(error.exception))

    def test_verifier_rejects_private_credentials_before_reading_artifacts(self):
        for key in ['APPLE_SIGNING_CERT_P12', 'APPLE_SIGNING_CERT_PASSWORD', 'APPLE_NOTARY_KEY_P8']:
            with patch.dict(os.environ, {key: 'fixture-private-value'}), patch.object(sys, 'argv', [
                'verify', '--incoming', '/absent', '--request', '/absent', '--output', '/absent'
            ]), self.assertRaisesRegex(ci.signer.GateError, 'must not receive signing credentials'):
                verifier.main()

    def test_request_cannot_cross_workflow_revision(self):
        record = dict(kind='core', tag='v2.0.6', source_revision='a'*40, source_run_id='123', signing_workflow_revision='b'*40)
        with tempfile.TemporaryDirectory() as temporary, patch.dict(os.environ, {
            'REQUEST_JSON':base64.b64encode(json.dumps(record).encode()).decode(),
            'GITHUB_REPOSITORY':'qwertycoin-org/qwertycoin', 'GITHUB_REF':'refs/heads/main', 'GITHUB_SHA':'c'*40
        }), self.assertRaisesRegex(ci.signer.GateError, 'workflow revision'):
            ci.restore_request(SimpleNamespace(work=Path(temporary)/'work'))

    def test_cleanup_restores_keychain_search_list_and_removes_credentials(self):
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary); private = work/'private'; private.mkdir()
            (private/'keychain-search-list.json').write_text(json.dumps(['/original/login.keychain-db']))
            (private/'signing.keychain-db').touch(); (private/'notary.p8').write_text('fixture')
            with patch.object(ci, 'run', return_value=b'') as call:
                ci.cleanup(SimpleNamespace(work=work))
            self.assertFalse(private.exists())
            self.assertEqual(call.call_args_list[0].args[0][-1], '/original/login.keychain-db')
            self.assertEqual(call.call_args_list[1].args[0][1], 'delete-keychain')

    def test_workflow_secret_separation_and_no_publication_permissions(self):
        root = Path(__file__).parents[3]
        workflow = (root/'.github/workflows/macos-signing.yml').read_text()
        self.assertNotIn('pull_request_target', workflow)
        self.assertNotIn('contents: write', workflow)
        self.assertNotIn('secrets: inherit', workflow)
        self.assertIn('cancel-in-progress: false', workflow)
        self.assertEqual(workflow.count('persist-credentials: false'), 3)
        verify = workflow.split('\n  verify:\n', 1)[1]
        self.assertIn('environment: macos-verification', verify)
        for private in ['APPLE_SIGNING_CERT_P12', 'APPLE_SIGNING_CERT_PASSWORD', 'APPLE_NOTARY_KEY_P8']:
            self.assertNotIn(private, verify)
        self.assertIn('if: always()', workflow)
        self.assertIn('environment: macos-signing', workflow)
        self.assertNotIn('self-hosted', workflow)


if __name__ == '__main__':unittest.main()
