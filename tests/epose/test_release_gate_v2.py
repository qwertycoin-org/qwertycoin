#!/usr/bin/env python3

import copy
import hashlib
import importlib.util
import json
import sys
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("release_gate_v2.py")
SPEC = importlib.util.spec_from_file_location("epose_release_gate_v2", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)

ROOT = Path(__file__).parents[2]


class ReleaseGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest = json.loads((ROOT / "docs/epose/PARAMETER_MANIFEST_V2.json").read_text())
        cls.ledger = json.loads((ROOT / "docs/epose/review/RELEASE_GATES_V2.json").read_text())
        cls.policy = json.loads((ROOT / "docs/epose/review/RELEASE_GATE_POLICY_V2.json").read_text())

    def evaluate(self, manifest, ledger, *, root=ROOT, allow_test_fixture=False):
        return MODULE.evaluate(
            manifest,
            ledger,
            self.policy,
            evidence_root=root,
            allow_test_fixture=allow_test_fixture,
        )

    def valid_test_manifest(self):
        manifest = copy.deepcopy(self.manifest)
        manifest["manifest_kind"] = "test-fixture"
        manifest["status"] = "activatable"
        manifest["activation"]["height"] = 0
        manifest["network"]["genesis_hash"] = "11" * 32
        manifest["release"]["source_revision"] = "22" * 20
        manifest["admission"].update({"lease_epochs": 1, "leading_zero_bits": 20})
        manifest["committee"].update({"round_offsets": [0, 200, 400], "rounds_required": 2, "size": 15, "threshold": 11})
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
                "max_template_epose_bytes": 32768,
                "max_template_records": 512,
                "minimum_undo_blocks": 2160,
                "reserved_enrollment_queue_bytes": 262144,
                "reserved_enrollment_queue_items": 512,
                "reserved_enrollment_template_bytes": 8192,
                "reserved_enrollment_template_records": 128,
                "reserved_evidence_queue_bytes": 262144,
                "reserved_evidence_queue_items": 512,
                "reserved_evidence_template_bytes": 8192,
                "reserved_evidence_template_records": 128,
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
        manifest["state"].update({"index_schema": 1, "pruned_validation_mode": "unsupported-fail-closed"})
        return manifest

    def test_current_ledger_proves_no_go(self):
        result = self.evaluate(self.manifest, self.ledger)
        self.assertEqual("no-go", result["overall_status"])
        self.assertEqual(13, result["total_gate_count"])
        self.assertEqual(13, len(result["unresolved_gates"]))
        self.assertNotIn("activation.block_hash", result["missing_manifest_fields"])

    def test_missing_unknown_and_duplicate_gate_ids_fail(self):
        missing = copy.deepcopy(self.ledger)
        missing["gates"].pop()
        with self.assertRaisesRegex(MODULE.GateError, "missing mandatory"):
            self.evaluate(self.manifest, missing)
        unknown = copy.deepcopy(self.ledger)
        unknown["gates"][0]["id"] = "invented"
        with self.assertRaisesRegex(MODULE.GateError, "missing mandatory"):
            self.evaluate(self.manifest, unknown)
        duplicate = copy.deepcopy(self.ledger)
        duplicate["gates"].append(copy.deepcopy(duplicate["gates"][0]))
        with self.assertRaisesRegex(MODULE.GateError, "duplicate gate"):
            self.evaluate(self.manifest, duplicate)

    def test_all_ones_and_boolean_integer_fixtures_fail(self):
        invalid = self.valid_test_manifest()
        invalid["committee"].update({"round_offsets": [1], "rounds_required": 1, "size": 1, "threshold": 1})
        invalid["resource_limits"] = {name: 1 for name in invalid["resource_limits"]}
        with self.assertRaises(MODULE.GateError):
            self.evaluate(invalid, self.ledger, allow_test_fixture=True)
        invalid = self.valid_test_manifest()
        invalid["committee"]["size"] = True
        with self.assertRaisesRegex(MODULE.GateError, "must be an integer"):
            self.evaluate(invalid, self.ledger, allow_test_fixture=True)

    def test_test_fixture_cannot_authorize_release_cli_path(self):
        with self.assertRaisesRegex(MODULE.GateError, "test fixture"):
            self.evaluate(self.valid_test_manifest(), self.ledger)

    def test_candidate_bound_evidence_is_required_and_verified(self):
        manifest = self.valid_test_manifest()
        ledger = copy.deepcopy(self.ledger)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            artifact = root / "result.json"
            artifact.write_text('{"result":"passed"}\n', encoding="utf-8")
            artifact_hash = hashlib.sha256(artifact.read_bytes()).hexdigest()
            manifest_hash = MODULE.digest(manifest)
            evidence = {
                "path": "result.json",
                "sha256": artifact_hash,
                "source_revision": manifest["release"]["source_revision"],
                "manifest_sha256": manifest_hash,
                "result": "passed",
                "artifact_ref": f"sha256:{artifact_hash}",
            }
            for gate in ledger["gates"]:
                gate["state"] = "satisfied"
                gate["evidence"] = [copy.deepcopy(evidence)]
            ledger["overall_status"] = "ready"
            result = self.evaluate(manifest, ledger, root=root, allow_test_fixture=True)
            self.assertEqual("ready", result["overall_status"])
            ledger["gates"][0]["evidence"][0]["source_revision"] = "33" * 20
            with self.assertRaisesRegex(MODULE.GateError, "wrong source revision"):
                self.evaluate(manifest, ledger, root=root, allow_test_fixture=True)

    def test_missing_or_altered_artifact_fails(self):
        manifest = self.valid_test_manifest()
        ledger = copy.deepcopy(self.ledger)
        gate = ledger["gates"][0]
        gate["state"] = "satisfied"
        gate["evidence"] = [
            {
                "path": "does-not-exist.txt",
                "sha256": "44" * 32,
                "source_revision": manifest["release"]["source_revision"],
                "manifest_sha256": MODULE.digest(manifest),
                "result": "passed",
                "artifact_ref": "sha256:" + "44" * 32,
            }
        ]
        with self.assertRaisesRegex(MODULE.GateError, "does not exist"):
            self.evaluate(manifest, ledger, allow_test_fixture=True)


if __name__ == "__main__":
    unittest.main()
