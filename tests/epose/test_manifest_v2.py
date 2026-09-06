#!/usr/bin/env python3

import json
import subprocess
import unittest
from pathlib import Path

from manifest_v2 import ManifestError, validate_manifest


ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "docs/epose/PARAMETER_MANIFEST_V2.json"


class ManifestV2Tests(unittest.TestCase):
    def setUp(self):
        self.manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))

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


if __name__ == "__main__":
    unittest.main()
