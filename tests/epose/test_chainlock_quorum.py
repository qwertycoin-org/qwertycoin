import copy
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from chainlock_quorum import check_lock_epoch, check_locked_header, derive_quorum, keccak256, threshold, verify_lock_signatures


class ChainlockQuorumTests(unittest.TestCase):
    def test_keccak_padding(self):
        self.assertEqual(keccak256(b"").hex(), "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470")
        self.assertEqual(keccak256(b"abc").hex(), "4e03657aea45a94fc7d47ba826c8d667c0d1e6e33a64a036ec44f58fa12d6c45")

    def test_vector_and_tampering(self):
        with (ROOT / "tests/data/chainlock_vm_state_proof.json").open(encoding="utf-8") as source:
            vector = json.load(source)
        inputs = vector["get_chainlock_quorum_inputs"]
        pinned = {key: inputs[key] for key in
                  ("activation_height", "epoch_length", "anchor_depth", "network_id", "genesis_hash", "parameter_set_hash")}
        expected = vector["get_chainlock"]["quorum"]
        self.assertEqual(check_lock_epoch(vector["get_chainlock"], inputs, pinned), inputs["epoch"])
        check_locked_header(vector["get_chainlock"])
        self.assertEqual(derive_quorum(inputs, pinned), expected)
        def verify(blob):
            verify_lock_signatures(blob.hex(), expected, pinned["network_id"], pinned["genesis_hash"],
                                   pinned["parameter_set_hash"])

        original_blob = bytearray.fromhex(vector["get_chainlock"]["lock_blob"])
        verify(original_blob)
        changed_blob = original_blob.copy()
        changed_blob[-1] ^= 1
        with self.assertRaises(ValueError):
            verify(changed_blob)
        changed_blob = original_blob.copy()
        first_signer = next(index for index in range(len(expected)) if changed_blob[48 + index // 8] & (1 << (index % 8)))
        changed_blob[48 + first_signer // 8] &= ~(1 << (first_signer % 8))
        with self.assertRaises(ValueError):
            verify(changed_blob)
        changed_blob = original_blob.copy()
        bitmap = int.from_bytes(changed_blob[48:55], "little")
        retained = [index for index in range(len(expected)) if bitmap & (1 << index)][:threshold(len(expected)) - 1]
        changed_blob[48:55] = sum(1 << index for index in retained).to_bytes(7, "little")
        changed_blob = changed_blob[:55 + len(retained) * 64]
        with self.assertRaises(ValueError):
            verify(changed_blob)
        changed_blob = original_blob.copy()
        changed_blob[48 + len(expected) // 8] |= 1 << (len(expected) % 8)
        with self.assertRaises(ValueError):
            verify(changed_blob)
        changed = copy.deepcopy(inputs)
        changed["qualified_members"][0] = "00" * 32
        with self.assertRaises(ValueError):
            derive_quorum(changed, pinned)
        changed = copy.deepcopy(inputs)
        changed["snapshot_block_id"] = "00" * 32
        self.assertNotEqual(derive_quorum(changed, pinned), expected)
        for field, value in (("epoch_length", 21), ("anchor_depth", 6), ("parameter_set_hash", "01" * 32)):
            changed = copy.deepcopy(inputs)
            changed[field] = value
            with self.assertRaises(ValueError):
                derive_quorum(changed, pinned)
        changed = copy.deepcopy(inputs)
        changed["snapshot_members"] = changed["snapshot_members"][:9]
        changed["snapshot_total"] = 9
        with self.assertRaises(ValueError):
            derive_quorum(changed, pinned)
        changed = copy.deepcopy(inputs)
        changed["qualified_members"] = changed["qualified_members"][:9]
        changed["qualification_total"] = 9
        with self.assertRaises(ValueError):
            derive_quorum(changed, pinned)
        changed = copy.deepcopy(vector["get_chainlock"])
        changed["quorum_epoch"] -= 1
        with self.assertRaises(ValueError):
            check_lock_epoch(changed, inputs, pinned)
        changed = copy.deepcopy(vector["get_chainlock"])
        header = bytearray.fromhex(changed["block_hashing_blob"])
        header[0] ^= 1
        changed["block_hashing_blob"] = header.hex()
        with self.assertRaises(ValueError):
            check_locked_header(changed)
        changed = copy.deepcopy(vector["get_chainlock"])
        changed["block_id"] = (bytes([int(changed["block_id"][:2], 16) ^ 1])
                               + bytes.fromhex(changed["block_id"][2:])).hex()
        with self.assertRaises(ValueError):
            check_lock_epoch(changed, inputs, pinned)
        changed = copy.deepcopy(vector["get_chainlock"])
        blob = bytearray.fromhex(changed["lock_blob"])
        blob[40:48] = (inputs["epoch"] - 1).to_bytes(8, "little")
        changed["lock_blob"] = blob.hex()
        changed["quorum_epoch"] -= 1
        with self.assertRaises(ValueError):
            check_lock_epoch(changed, inputs, pinned)

    def test_threshold(self):
        self.assertEqual(threshold(50), 34)
        self.assertEqual(threshold(10), 7)


if __name__ == "__main__":
    unittest.main()
