import json
import re
import unittest

from manifest_contracts import MANIFEST, ROOT


class GasManifestTests(unittest.TestCase):
    def test_cost_table_matches_compiled_vm_constants(self):
        manifest = json.loads(MANIFEST.read_text())
        source = (ROOT / "src/vm/gas.h").read_text()
        costs = {
            name: int(value)
            for name, value in re.findall(r"constexpr uint64_t (\w+) = (\d+);", source)
        }
        self.assertEqual(costs, manifest["contracts"]["gas_cost_table"])

    def test_block_and_transaction_caps_match_config(self):
        manifest = json.loads(MANIFEST.read_text())
        source = (ROOT / "src/cryptonote_config.h").read_text()
        for field, constant in (
            ("block_gas_limit", "QWC_VM_BLOCK_GAS_LIMIT"),
            ("tx_gas_cap", "QWC_VM_TX_GAS_CAP"),
        ):
            with self.subTest(field=field):
                match = re.search(rf"\b{constant}\s*=\s*(\d+);", source)
                self.assertIsNotNone(match)
                self.assertEqual(int(match.group(1)), manifest["contracts"][field])


if __name__ == "__main__":
    unittest.main()
