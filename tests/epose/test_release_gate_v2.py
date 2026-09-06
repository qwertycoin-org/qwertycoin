#!/usr/bin/env python3

import copy
import importlib.util
import json
import sys
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

    def test_current_ledger_proves_no_go(self):
        result = MODULE.evaluate(self.manifest, self.ledger)
        self.assertEqual("no-go", result["overall_status"])
        self.assertEqual("not-activatable", result["manifest_status"])
        self.assertIn("activation.height", result["missing_manifest_fields"])
        self.assertGreater(len(result["unresolved_gates"]), 0)

    def test_declared_ready_cannot_hide_null_manifest_fields(self):
        ledger = copy.deepcopy(self.ledger)
        ledger["overall_status"] = "ready"
        with self.assertRaisesRegex(MODULE.GateError, "disagrees"):
            MODULE.evaluate(self.manifest, ledger)

    def test_duplicate_gate_id_fails(self):
        ledger = copy.deepcopy(self.ledger)
        ledger["gates"].append(copy.deepcopy(ledger["gates"][0]))
        with self.assertRaisesRegex(MODULE.GateError, "duplicate gate"):
            MODULE.evaluate(self.manifest, ledger)

    def test_satisfied_gate_requires_evidence(self):
        ledger = copy.deepcopy(self.ledger)
        ledger["gates"][0]["evidence"] = []
        with self.assertRaisesRegex(MODULE.GateError, "requires evidence"):
            MODULE.evaluate(self.manifest, ledger)

    def test_unknown_gate_state_fails(self):
        ledger = copy.deepcopy(self.ledger)
        ledger["gates"][0]["state"] = "waived"
        with self.assertRaisesRegex(MODULE.GateError, "invalid state"):
            MODULE.evaluate(self.manifest, ledger)

    def test_fully_populated_fixture_can_be_ready(self):
        manifest = copy.deepcopy(self.manifest)
        for path in MODULE.REQUIRED_MANIFEST_PATHS:
            current = manifest
            parts = path.split(".")
            for part in parts[:-1]:
                current = current[part]
            if current[parts[-1]] is None:
                current[parts[-1]] = 1
        manifest["status"] = "activatable"
        ledger = copy.deepcopy(self.ledger)
        for gate in ledger["gates"]:
            gate["state"] = "satisfied"
            gate["evidence"] = ["fixture-evidence"]
        ledger["overall_status"] = "ready"
        result = MODULE.evaluate(manifest, ledger)
        self.assertEqual("ready", result["overall_status"])
        self.assertEqual([], result["missing_manifest_fields"])
        self.assertEqual([], result["unresolved_gates"])


if __name__ == "__main__":
    unittest.main()
