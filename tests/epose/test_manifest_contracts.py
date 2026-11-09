#!/usr/bin/env python3

import copy
import json
import re
import unittest

from manifest_contracts import (
    CONFIG_CONSTANTS,
    MANIFEST,
    ROOT,
    consensus_parameter_digest,
    validate_manifest,
)
from manifest_v2 import ManifestError


def config_text():
    return (ROOT / "src/cryptonote_config.h").read_text(encoding="utf-8")


class ManifestContractsTests(unittest.TestCase):
    def setUp(self):
        self.manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))

    def test_checked_in_manifest_is_valid_and_not_activatable(self):
        validate_manifest(self.manifest)
        self.assertEqual("reservation", self.manifest["manifest_kind"])
        self.assertIsNone(self.manifest["activation"]["height"])

    def test_hash_matches_manifest_and_compiled_constant(self):
        expected = consensus_parameter_digest(self.manifest)
        self.assertEqual(expected, self.manifest["commitments"]["parameter_set_sha256"])
        compiled = re.search(
            r'QWC_CONTRACTS_PARAMETER_SET_HASH\[\]\s*=\s*"([0-9a-f]{64})"', config_text()
        )
        self.assertIsNotNone(compiled)
        self.assertEqual(expected, compiled.group(1))

    def test_every_manifest_constant_matches_cryptonote_config(self):
        text = config_text()
        define = {
            name: int(value)
            for name, value in re.findall(r"^#define\s+(HF_VERSION_QWC_CONTRACTS)\s+(\d+)", text, re.M)
        }
        for path, name in CONFIG_CONSTANTS.items():
            with self.subTest(path=path):
                target = self.manifest
                for component in path.split("."):
                    target = target[component]
                if name in define:
                    self.assertEqual(target, define[name])
                    continue
                match = re.search(rf"\b{name}\s*=\s*([^;]+);", text)
                self.assertIsNotNone(match, name)
                expr = match.group(1).strip()
                if expr == "CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW":
                    expr = re.search(
                        r"^#define\s+CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW\s+(\d+)", text, re.M
                    ).group(1)
                self.assertEqual(target, eval(expr, {"__builtins__": {}}, {}))

    def test_policy_section_is_outside_the_commitment(self):
        changed = copy.deepcopy(self.manifest)
        changed["policy"]["window_pool_lead_blocks"] += 1
        self.assertEqual(consensus_parameter_digest(self.manifest), consensus_parameter_digest(changed))

    def test_consensus_values_are_bound_by_the_commitment(self):
        for path in (
            ("chainlock", "quorum_max"),
            ("chainlock", "threshold_percent"),
            ("chainlock", "lock_max_age_blocks"),
            ("contracts", "tx_extra_max_bytes"),
            ("contracts", "vm_undo_retention_blocks"),
            ("epose", "retained_epochs"),
            ("mint", "unlock_blocks"),
            ("window", "unlock_delay_blocks"),
        ):
            with self.subTest(path=path):
                changed = copy.deepcopy(self.manifest)
                changed[path[0]][path[1]] += 1
                self.assertNotEqual(
                    consensus_parameter_digest(self.manifest), consensus_parameter_digest(changed)
                )
                with self.assertRaises(ManifestError):
                    validate_manifest(changed)

    def test_activation_height_must_be_an_epoch_start(self):
        broken = copy.deepcopy(self.manifest)
        broken["manifest_kind"] = "activation-candidate"
        broken["status"] = "activatable"
        broken["undecided"] = []
        broken["activation"]["height"] = 721
        with self.assertRaises(ManifestError):
            validate_manifest(broken)

    def test_v2_manifest_and_hash_are_untouched(self):
        v2 = json.loads((ROOT / "docs/epose/PARAMETER_MANIFEST_V2.json").read_text(encoding="utf-8"))
        self.assertEqual(
            "2c26755094535871dd3ede7bd1b50aba82a9fb6831f0a17f32968eb0385145c6",
            v2["commitments"]["parameter_set_sha256"],
        )
        self.assertEqual(17, v2["encoding"]["hardfork_version"])


if __name__ == "__main__":
    unittest.main()
