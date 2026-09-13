#!/usr/bin/env python3

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tests/epose/integration/remote_mainnet_rehearsal.sh"


class RemoteMainnetRehearsalConfigTests(unittest.TestCase):
    def make_environment(self, temp: Path, nodes=None, inventory_mode=0o600):
        key = temp / "ssh-key"
        known_hosts = temp / "known-hosts"
        inventory = temp / "inventory.json"
        key.write_text("test-only-placeholder\n", encoding="utf-8")
        known_hosts.write_text("test-only-placeholder\n", encoding="utf-8")
        key.chmod(0o600)
        known_hosts.chmod(0o600)
        if nodes is None:
            nodes = [
                {
                    "name": f"node-{index}",
                    "ssh_target": f"operator{index}@node-{index}.example.invalid",
                    "public_endpoint": f"node-{index}.example.invalid",
                }
                for index in range(4)
            ]
        inventory.write_text(
            json.dumps({"schema_version": 1, "nodes": nodes}), encoding="utf-8"
        )
        inventory.chmod(inventory_mode)
        environment = os.environ.copy()
        environment.update(
            {
                "QWC_REHEARSAL_SSH_KEY": str(key),
                "QWC_REHEARSAL_KNOWN_HOSTS": str(known_hosts),
                "QWC_REHEARSAL_INVENTORY_FILE": str(inventory),
            }
        )
        return environment

    def invoke(self, environment):
        return subprocess.run(
            [str(SCRIPT), "validate-config"],
            env=environment,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=10,
        )

    def test_valid_external_inventory_is_accepted_without_disclosure(self):
        with tempfile.TemporaryDirectory(prefix="qwc-rehearsal-config-") as name:
            environment = self.make_environment(Path(name))
            result = self.invoke(environment)
            self.assertEqual(0, result.returncode, result.stderr)
            self.assertEqual(
                "rehearsal access configuration valid for 4 nodes\n", result.stdout
            )
            self.assertNotIn("operator", result.stdout + result.stderr)
            self.assertNotIn(str(Path(name)), result.stdout + result.stderr)

    def test_missing_configuration_fails_closed(self):
        environment = os.environ.copy()
        for name in (
            "QWC_REHEARSAL_SSH_KEY",
            "QWC_REHEARSAL_KNOWN_HOSTS",
            "QWC_REHEARSAL_INVENTORY_FILE",
        ):
            environment.pop(name, None)
        result = self.invoke(environment)
        self.assertEqual(78, result.returncode)
        self.assertEqual("", result.stdout)

    def test_world_readable_inventory_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix="qwc-rehearsal-config-") as name:
            environment = self.make_environment(Path(name), inventory_mode=0o644)
            result = self.invoke(environment)
            self.assertEqual(78, result.returncode)
            self.assertIn("must not be readable", result.stderr)

    def test_invalid_target_is_rejected_without_echoing_it(self):
        with tempfile.TemporaryDirectory(prefix="qwc-rehearsal-config-") as name:
            nodes = [
                {
                    "name": f"node-{index}",
                    "ssh_target": f"operator{index}@node-{index}.example.invalid",
                    "public_endpoint": f"node-{index}.example.invalid",
                }
                for index in range(4)
            ]
            marker = "unsafe-target-marker"
            nodes[2]["ssh_target"] = f"operator@host.invalid;{marker}"
            environment = self.make_environment(Path(name), nodes=nodes)
            result = self.invoke(environment)
            self.assertEqual(78, result.returncode)
            self.assertNotIn(marker, result.stdout + result.stderr)

    def test_wrong_node_count_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix="qwc-rehearsal-config-") as name:
            nodes = [
                {
                    "name": f"node-{index}",
                    "ssh_target": f"operator{index}@node-{index}.example.invalid",
                    "public_endpoint": f"node-{index}.example.invalid",
                }
                for index in range(3)
            ]
            result = self.invoke(self.make_environment(Path(name), nodes=nodes))
            self.assertEqual(78, result.returncode)

    def test_symlinked_inventory_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix="qwc-rehearsal-config-") as name:
            temp = Path(name)
            environment = self.make_environment(temp)
            inventory = Path(environment["QWC_REHEARSAL_INVENTORY_FILE"])
            link = temp / "inventory-link.json"
            link.symlink_to(inventory)
            environment["QWC_REHEARSAL_INVENTORY_FILE"] = str(link)
            result = self.invoke(environment)
            self.assertEqual(78, result.returncode)

    def test_script_contains_no_embedded_access_inventory(self):
        source = SCRIPT.read_text(encoding="utf-8")
        self.assertNotRegex(source, r"(?m)^SSH_TARGETS=\([^)]")
        self.assertNotRegex(source, r"(?m)^PUBLIC_ENDPOINTS=\([^)]")
        self.assertNotIn("id_ed25519_", source)


if __name__ == "__main__":
    unittest.main()
