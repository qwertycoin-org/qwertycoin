#!/usr/bin/env python3

from importlib.util import module_from_spec, spec_from_file_location
import argparse
import hashlib
import json
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


def load(name: str):
    spec = spec_from_file_location(name, ROOT / f"{name}.py")
    assert spec and spec.loader
    module = module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


VERIFY = load("verify_candidate_archive")
VALIDATE = load("validate_release_request")


class ArchiveSecurityTests(unittest.TestCase):
    def assert_rejected(self, callback) -> None:
        with self.assertRaises(SystemExit):
            callback()

    def test_rejects_absolute_traversal_and_backslash_paths(self) -> None:
        for value in ("/payload", "payload/../escape", "payload\\escape"):
            with self.subTest(value=value):
                self.assert_rejected(lambda value=value: VERIFY.normalize_member(value))

    def test_rejects_casefold_collision_and_outside_members(self) -> None:
        self.assert_rejected(lambda: VERIFY.validate_names(["release/File", "release/file"], "release"))
        self.assert_rejected(lambda: VERIFY.validate_names(["other/file"], "release"))

    def test_accepts_text_and_binary_sha256_formats(self) -> None:
        digest = "a" * 64
        self.assertIsNotNone(VERIFY.SHA256_LINE.fullmatch(f"{digest}  release/file"))
        self.assertIsNotNone(VERIFY.SHA256_LINE.fullmatch(f"{digest} *release/file"))

    def test_rejects_missing_required_payload(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "release"
            root.mkdir()
            self.assert_rejected(lambda: VERIFY.require_payload(root, "linux"))

    def test_manifest_requires_complete_coverage_and_exact_digests(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary)
            root = destination / "release"
            root.mkdir()
            payload = root / "payload"
            payload.write_bytes(b"candidate")
            manifest = destination / "release.sha256"
            manifest.write_text(
                f"{hashlib.sha256(payload.read_bytes()).hexdigest()}  release/payload\n",
                encoding="utf-8",
            )
            self.assertEqual(VERIFY.verify_manifest(destination, "release"), 1)
            (root / "unhashed").write_bytes(b"unexpected")
            self.assert_rejected(lambda: VERIFY.verify_manifest(destination, "release"))
            (root / "unhashed").unlink()
            payload.write_bytes(b"changed")
            self.assert_rejected(lambda: VERIFY.verify_manifest(destination, "release"))

    def test_metadata_rejects_conflicting_provenance_and_negative_stable_gate(self) -> None:
        expected_revision = "1" * 40
        workflow_revision = "2" * 40
        args = argparse.Namespace(
            expected_tag="v2.0.0-rc1",
            expected_revision=expected_revision,
            expected_workflow_name="qwc/core-release-candidate",
            expected_workflow_revision=workflow_revision,
            expected_run_id="42",
            expected_run_attempt="1",
            expected_os="Linux",
            expected_arch="x86_64",
            require_stable_gate=False,
        )
        manifest = {
            "network": {"type": "mainnet", "network_id": "ab", "genesis_hash": "cd"},
            "commitments": {"parameter_set_sha256": "ef"},
            "epoch": {"length_blocks": 720},
            "committee": {"size": 9, "threshold": 6, "round_offsets": [0, 200, 400], "rounds_required": 2},
            "admission": {"leading_zero_bits": 18, "lease_epochs": 1},
            "reward": {
                "basis_points": 1000,
                "emission_accounting": "actual-issued-subsidy",
                "fee_policy": "subsidy-only",
                "empty_set_policy": "miner-fallback",
            },
        }
        canonical_manifest = (
            json.dumps(manifest, sort_keys=True, separators=(",", ":"), ensure_ascii=False) + "\n"
        ).encode()
        gate = {
            "candidate_source_revision": expected_revision,
            "stable_permitted": False,
            "manifest_sha256": hashlib.sha256(canonical_manifest).hexdigest(),
        }
        evidence = {
            "programs": {
                "qwertycoind": {"help": "pass", "version": "pass"},
                "qwertycoin-wallet-cli": {"help": "pass", "version": "pass"},
                "qwertycoin-wallet-rpc": {"help": "pass", "version": "pass"},
            },
            "isolated_network": {
                "daemon_rpc_ready": "pass",
                "network": "offline-mainnet",
                "genesis_hash_match": "pass",
                "epose_protocol_version": 2,
                "epose_service_reward_bps": 1000,
                "daemon_rpc_stop_ack": "pass",
                "daemon_clean_exit": "pass",
                "daemon_stop_method": "rpc",
                "wallet_rpc_digest_auth": "pass",
                "wallet_seed_restore_address_match": "pass",
                "secrets_logged": False,
            }
        }
        info = {
            "release": {
                "version": "2.0.0",
                "compatible_version": "2.0.0.0",
                "tag": args.expected_tag,
                "source_revision": expected_revision,
            },
            "workflow": {
                "name": args.expected_workflow_name,
                "revision": workflow_revision,
                "run_id": args.expected_run_id,
                "run_attempt": args.expected_run_attempt,
            },
            "platform": {"os": args.expected_os, "architecture": args.expected_arch},
            "build": {
                "targets": ["daemon", "simplewallet", "wallet_rpc_server"],
                "use_device_trezor": False,
                "type": "Release",
            },
            "submodules": {"external/randomx": "3" * 40},
            "network": {
                "type": "mainnet",
                "network_id": "ab",
                "genesis_hash": "cd",
                "parameter_set_sha256": "ef",
                "epoch_length_blocks": 720,
                "committee_size": 9,
                "committee_threshold": 6,
                "committee_round_offsets": [0, 200, 400],
                "committee_rounds_required": 2,
                "admission_leading_zero_bits": 18,
                "admission_lease_epochs": 1,
                "service_reward_basis_points": 1000,
                "service_reward_emission_accounting": "actual-issued-subsidy",
                "service_reward_fee_policy": "subsidy-only",
                "service_reward_empty_set_policy": "miner-fallback",
            },
            "epose_release_gate": gate,
        }
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            evidence_dir = root / "evidence"
            evidence_dir.mkdir()
            (evidence_dir / "EPOSE-RELEASE-GATE.json").write_text(
                json.dumps(gate), encoding="utf-8"
            )
            (evidence_dir / "PARAMETER-MANIFEST-V2.json").write_text(
                json.dumps(manifest), encoding="utf-8"
            )
            (evidence_dir / "RELEASE-EVIDENCE.json").write_text(
                json.dumps(evidence), encoding="utf-8"
            )
            VERIFY.verify_metadata(info, args, root)
            info["workflow"]["run_id"] = "43"
            self.assert_rejected(lambda: VERIFY.verify_metadata(info, args, root))
            info["workflow"]["run_id"] = args.expected_run_id
            args.require_stable_gate = True
            self.assert_rejected(lambda: VERIFY.verify_metadata(info, args, root))


class RequestValidationTests(unittest.TestCase):
    def test_version_parser_matches_current_format(self) -> None:
        text = '#define DEF_QWERTYCOIN_VERSION "2.0.0"\n'
        self.assertEqual(VALIDATE.VERSION_RE.search(text).groups(), ("2", "0", "0"))

    def test_tag_parser_rejects_zero_and_missing_rc_numbers(self) -> None:
        self.assertIsNotNone(VALIDATE.TAG_RE.fullmatch("v2.0.0"))
        self.assertIsNotNone(VALIDATE.TAG_RE.fullmatch("v2.0.0-rc1"))
        self.assertIsNone(VALIDATE.TAG_RE.fullmatch("v2.0.0-rc0"))
        self.assertIsNone(VALIDATE.TAG_RE.fullmatch("v2.0.0-rc"))


if __name__ == "__main__":
    unittest.main()
