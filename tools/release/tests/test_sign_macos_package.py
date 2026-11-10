import importlib.util
import hashlib
import plistlib
import subprocess
from types import SimpleNamespace
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('sign_macos_package', Path(__file__).parents[1] / 'sign_macos_package.py')
sign = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sign)


class PrivacyAndIdentityGates(unittest.TestCase):
    def test_personal_name_email_and_build_path_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for data in (b'Copyright Casey Example', b'casey@example.invalid', b'/Users/casey/build/source.cpp'):
                (root / 'binary').write_bytes(data)
                with self.assertRaisesRegex(sign.GateError, 'Privacy scan rejected'):
                    sign.privacy_scan(root, ['Casey'])

    def test_unrelated_word_prefix_does_not_match_person(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'locales').write_bytes(b'markup\0MARKETING\0')
            sign.privacy_scan(root, ['Mark'])

    def test_utf16_personal_metadata_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for encoding in ('utf-16-le', 'utf-16-be'):
                (root / 'metadata').write_bytes('Owner Casey Example'.encode(encoding))
                with self.assertRaises(sign.GateError):
                    sign.privacy_scan(root, ['Casey'])
                (root / 'metadata').write_bytes('markup MARKETING Markà\0'.encode(encoding))
                sign.privacy_scan(root, ['Mark'])

    def test_personal_filename_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'casey-build.txt').write_bytes(b'nonpersonal content')
            with self.assertRaises(sign.GateError):
                sign.privacy_scan(root, ['Casey'])

    def test_escaping_symlink_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / 'package'
            root.mkdir()
            (root / 'outside').symlink_to(Path(temporary))
            with self.assertRaises(sign.GateError):
                sign.validate_tree(root)

    def test_internal_framework_symlink_allowed(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'Versions' / 'A').mkdir(parents=True)
            (root / 'Versions' / 'Current').symlink_to('A')
            sign.validate_tree(root)

    def test_personal_or_additional_subject_fields_rejected(self):
        good = 'subject=CN=Developer ID Application: Example LLC (ABCDEFGHIJ),OU=ABCDEFGHIJ,O=Example LLC,C=US,UID=ABCDEFGHIJ'
        for subject in (good.replace('Example LLC', 'Casey Example'), good + ',emailAddress=casey@example.invalid'):
            with patch.object(sign, 'run', return_value=subject.encode()):
                with self.assertRaisesRegex(sign.GateError, 'approved organization'):
                    sign.approve_certificate(Path('unused.cer'), 'Example LLC', 'ABCDEFGHIJ')

    def test_libressl_subject_spacing_and_exact_identity_pin(self):
        with tempfile.TemporaryDirectory() as temporary:
            cert = Path(temporary) / 'leaf.cer'
            cert.write_bytes(b'test certificate')
            fingerprint = hashlib.sha1(cert.read_bytes()).hexdigest().upper()
            subject = b'subject= C=US,O=Example LLC,OU=ABCDEFGHIJ,CN=Developer ID Application: Example LLC (ABCDEFGHIJ),UID=ABCDEFGHIJ\n'
            identities = f'1) {fingerprint} "Developer ID Application: Example LLC (ABCDEFGHIJ)"'.encode()
            with patch.object(sign, 'run', side_effect=[subject, b'', identities]):
                self.assertEqual(sign.approve_certificate(cert, 'Example LLC', 'ABCDEFGHIJ'), fingerprint)
            with patch.object(sign, 'run', side_effect=[subject, b'', identities.replace(fingerprint.encode(), b'0' * 40)]):
                with self.assertRaisesRegex(sign.GateError, 'no usable signing identity'):
                    sign.approve_certificate(cert, 'Example LLC', 'ABCDEFGHIJ')

    def test_native_timeout_is_bounded_and_diagnostics_suppressed(self):
        with patch.object(sign.subprocess, 'run', side_effect=subprocess.TimeoutExpired(['codesign', '/private/personal/path'], 120)):
            with self.assertRaises(sign.GateError) as failure:
                sign.run(['codesign', '/private/personal/path'])
            self.assertNotIn('/private/personal/path', str(failure.exception))

    def test_debugging_entitlement_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cert = root / 'approved.cer'
            cert.write_bytes(b'approved certificate')
            (root / 'leaf-0').write_bytes(cert.read_bytes())
            bad = plistlib.dumps({'com.apple.security.cs.allow-jit': True, 'com.apple.security.get-task-allow': True})
            with patch.object(sign, 'run', side_effect=[b'', b'', bad]) as native_run, patch.object(sign.subprocess, 'run', return_value=SimpleNamespace(returncode=0, stderr=b'flags=runtime\nTimestamp=valid')):
                with self.assertRaisesRegex(sign.GateError, 'Unexpected.*entitlements'):
                    sign.verify(root / 'qwertycoind', cert, 'requirement', root)
                self.assertEqual(native_run.call_args_list[0].args[0][4], '=requirement')
                self.assertEqual(native_run.call_args_list[1].args[0][2], '--extract-certificates=' + str(root / 'leaf-'))


if __name__ == '__main__':
    unittest.main()
