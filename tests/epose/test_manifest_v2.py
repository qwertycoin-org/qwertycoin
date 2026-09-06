#!/usr/bin/env python3

import json
import copy
import subprocess
import unittest
from pathlib import Path

from manifest_v2 import ManifestError, validate_manifest


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "docs/epose/PARAMETER_MANIFEST_V2.json"


class ManifestV2Tests(unittest.TestCase):
    def setUp(self):
        self.manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))

    def complete_test_candidate(self):
        manifest = copy.deepcopy(self.manifest)
        manifest["manifest_kind"] = "test-fixture"
        manifest["status"] = "activatable"
        manifest["activation"]["height"] = 1440
        manifest["network"]["genesis_hash"] = "11" * 32
        manifest["release"]["source_revision"] = "22" * 20
        manifest["admission"].update({"lease_epochs": 1, "leading_zero_bits": 20})
        manifest["committee"].update(
            {"round_offsets": [0, 200, 400], "rounds_required": 2, "size": 15, "threshold": 11}
        )
        manifest["resource_limits"].update(
            {
                "max_active_population": 1000,
                "max_admission_verifications_per_block": 8,
                "max_envelope_bytes_per_transaction": 65536,
                "max_envelopes_per_transaction": 4,
                "max_epose_bytes_per_block": 262144,
                "max_records_per_block": 1024,
                "max_records_per_envelope": 256,
                "max_relay_queue_bytes": 1048576,
                "max_relay_queue_items": 2048,
                "max_signature_verifications_per_block": 2048,
                "minimum_undo_blocks": 2160,
                "reserved_enrollment_queue_bytes": 262144,
                "reserved_enrollment_queue_items": 512,
                "reserved_evidence_queue_bytes": 262144,
                "reserved_evidence_queue_items": 512,
            }
        )
        manifest["reward"].update(
            {
                "empty_set_policy": "miner-fallback",
                "emission_accounting": "actual-issued-subsidy",
                "fee_policy": "subsidy-only",
                "payment_proof_scheme": "scoped-tx-proof-v1",
            }
        )
        manifest["state"].update(
            {"index_schema": 1, "pruned_validation_mode": "unsupported-fail-closed"}
        )
        return manifest

    def test_reservation_manifest_is_typed_and_nonactivatable(self):
        missing = validate_manifest(self.manifest)
        self.assertEqual(self.manifest["status"], "not-activatable")
        self.assertGreater(len(missing), 0)

    def test_dependency_manifest_matches_declared_baseline_gitlinks(self):
        revision = self.manifest["dependencies"]["core_source_commit"]
        output = subprocess.check_output(
            ["git", "ls-tree", "-r", revision], cwd=ROOT, text=True
        )
        actual = {
            path.removeprefix("external/"): object_id
            for mode, _kind, object_id, path in
            (line.split(maxsplit=3) for line in output.splitlines())
            if mode == "160000" and path.startswith("external/")
        }
        declared = self.manifest["dependencies"]["submodules"]
        self.assertEqual(declared, {f"external/{name}": value for name, value in actual.items()})

    def test_known_bad_dependency_inventory_fails(self):
        broken = json.loads(json.dumps(self.manifest))
        broken["dependencies"]["submodules"]["external/randomx"] = "1" * 40
        broken["dependencies"]["submodules"]["external/miniupnp"] = "2" * 40
        declared = broken["dependencies"]["submodules"]
        revision = broken["dependencies"]["core_source_commit"]
        output = subprocess.check_output(["git", "ls-tree", "-r", revision], cwd=ROOT, text=True)
        actual = {
            path: object_id
            for mode, _kind, object_id, path in
            (line.split(maxsplit=3) for line in output.splitlines())
            if mode == "160000" and path.startswith("external/")
        }
        self.assertNotEqual(declared, actual)

    def test_booleans_are_not_integers(self):
        broken = json.loads(json.dumps(self.manifest))
        broken["encoding"]["hardfork_version"] = True
        with self.assertRaises(ManifestError):
            validate_manifest(broken)

    def test_admission_context_is_the_immediately_preceding_epoch(self):
        for invalid in (True, 0, 2):
            with self.subTest(value=invalid):
                broken = json.loads(json.dumps(self.manifest))
                broken["admission"]["context_epoch_offset"] = invalid
                with self.assertRaises(ManifestError):
                    validate_manifest(broken)

    def test_relay_queue_reservations_cannot_exceed_total_capacity(self):
        broken = json.loads(json.dumps(self.manifest))
        broken["resource_limits"].update(
            {
                "max_relay_queue_items": 10,
                "reserved_enrollment_queue_items": 6,
                "reserved_evidence_queue_items": 5,
            }
        )
        with self.assertRaises(ManifestError):
            validate_manifest(broken)

    def test_complete_supported_candidate_passes_typed_validation(self):
        self.assertEqual(
            [],
            validate_manifest(self.complete_test_candidate(), allow_test_fixture=True),
        )

    def test_every_consensus_field_rejects_unsupported_values(self):
        mutations = (
            ("admission.algorithm", "anything"),
            ("admission.leading_zero_bits", 0),
            ("admission.leading_zero_bits", True),
            ("admission.leading_zero_bits", "twenty"),
            ("admission.lease_epochs", -1),
            ("admission.lease_epochs", "forever"),
            ("committee.round_offsets", [1, 200, 400]),
            ("encoding.envelope_version", 999),
            ("encoding.envelope_magic_ascii", "QEP3"),
            ("encoding.integer_byte_order", "big-endian"),
            ("encoding.record_versions.service_receipt", 2),
            ("carrier.coinbase_envelope_allowed", False),
            ("carrier.fee_funded_envelope_allowed", False),
            ("carrier.legacy_nonce_allowed_after_activation", True),
            ("admission.target_epoch_required", False),
            ("epoch.epoch_zero_has_v2_rewards", True),
            ("reward.schedule", "height-modulo"),
        )
        for path, invalid in mutations:
            with self.subTest(path=path, value=invalid):
                broken = self.complete_test_candidate()
                target = broken
                components = path.split(".")
                for component in components[:-1]:
                    target = target[component]
                target[components[-1]] = invalid
                with self.assertRaises(ManifestError):
                    validate_manifest(broken, allow_test_fixture=True)


if __name__ == "__main__":
    unittest.main()
