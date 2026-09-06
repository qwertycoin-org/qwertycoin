#!/usr/bin/env python3

import importlib.util
import json
import sys
import unittest
from pathlib import Path


MODEL_PATH = Path(__file__).with_name("reference_model_v2.py")
SPEC = importlib.util.spec_from_file_location("epose_reference_model_v2", MODEL_PATH)
MODEL = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
sys.modules[SPEC.name] = MODEL
SPEC.loader.exec_module(MODEL)


class TimingTests(unittest.TestCase):
    def setUp(self):
        self.timing = MODEL.Timing(activation_height=1440)

    def test_activation_must_be_epoch_aligned(self):
        with self.assertRaisesRegex(MODEL.ModelError, "not epoch aligned"):
            MODEL.Timing(activation_height=1441)

    def test_two_epoch_warmup(self):
        self.assertEqual(3, self.timing.first_service_epoch)
        self.assertEqual(2880, self.timing.first_payout_height)

    def test_cutoff_and_anchor_are_adjacent(self):
        self.assertEqual(2099, self.timing.enrollment_cutoff(3))
        self.assertEqual(2100, self.timing.committee_anchor(3))

    def test_deadline_and_payout_seed_are_adjacent(self):
        self.assertEqual(2819, self.timing.evidence_deadline(3))
        self.assertEqual(2820, self.timing.payout_seed_height(3))

    def test_activation_boundary_rejects_cross_version_blocks(self):
        self.assertTrue(self.timing.block_version_allowed(1439, 17))
        self.assertFalse(self.timing.block_version_allowed(1439, 18))
        for version in (0, 1, 16, 19, 255):
            self.assertFalse(self.timing.block_version_allowed(1439, version))
        self.assertFalse(self.timing.block_version_allowed(1440, 17))
        self.assertTrue(self.timing.block_version_allowed(1440, 18))
        for version in (0, 1, 16, 19, 255):
            self.assertFalse(self.timing.block_version_allowed(1440, version))

    def test_payout_source_is_exactly_previous_epoch(self):
        self.assertTrue(self.timing.payout_allowed(2880, 3))
        self.assertTrue(self.timing.payout_allowed(3599, 3))
        self.assertFalse(self.timing.payout_allowed(3600, 3))

    def test_maximum_representable_epoch_boundary(self):
        maximum = (MODEL.UINT64_MAX - 719) // 720
        self.timing.end(maximum)
        with self.assertRaisesRegex(MODEL.ModelError, "overflows"):
            self.timing.end(maximum + 1)


class EnvelopeTests(unittest.TestCase):
    def test_canonical_encoding(self):
        envelope = MODEL.encode_envelope([(1, 1, b"\x01\x02\x03")])
        field = MODEL.encode_tx_extra_field(envelope)
        self.assertEqual(
            "051751455032010001000b0000000101000003000000010203",
            field.hex(),
        )
        self.assertEqual([(1, 1, b"\x01\x02\x03")], MODEL.decode_tx_extra_field(field, 64, 4))

    def test_unknown_version_rejected(self):
        envelope = bytearray.fromhex("514550320200000000000000")
        with self.assertRaisesRegex(MODEL.ModelError, "unsupported envelope"):
            MODEL.decode_envelope(bytes(envelope), 64, 4)

    def test_nonzero_flags_rejected(self):
        envelope = bytearray.fromhex("514550320101000000000000")
        with self.assertRaisesRegex(MODEL.ModelError, "unsupported envelope"):
            MODEL.decode_envelope(bytes(envelope), 64, 4)

    def test_empty_envelope_is_rejected_by_encoder_and_decoder(self):
        with self.assertRaisesRegex(MODEL.ModelError, "empty envelope"):
            MODEL.encode_envelope([])
        with self.assertRaisesRegex(MODEL.ModelError, "empty envelope"):
            MODEL.decode_envelope(bytes.fromhex("514550320100000000000000"), 64, 4)

    def test_record_budget_charged_before_semantics(self):
        envelope = MODEL.encode_envelope([(1, 1, b"same"), (1, 1, b"same")])
        with self.assertRaisesRegex(MODEL.ModelError, "record budget exceeded"):
            MODEL.decode_envelope(envelope, 64, 1)


class ManifestTests(unittest.TestCase):
    def setUp(self):
        path = Path(__file__).parents[2] / "docs" / "epose" / "PARAMETER_MANIFEST_V2.json"
        self.manifest = json.loads(path.read_text(encoding="utf-8"))

    def test_reservation_manifest_is_not_activatable(self):
        self.assertEqual("not-activatable", self.manifest["status"])
        MODEL.validate_manifest(self.manifest)
        missing = MODEL.missing_activation_fields(self.manifest)
        self.assertIn("activation.height", missing)
        self.assertNotIn("activation.block_hash", missing)
        self.assertIn("network.genesis_hash", missing)
        self.assertIn("reward.fee_policy", missing)
        self.assertIn("resource_limits.max_epose_bytes_per_block", missing)

    def test_manifest_canonicalization_is_stable(self):
        canonical = MODEL.canonical_manifest_bytes(self.manifest)
        self.assertTrue(canonical.endswith(b"\n"))
        self.assertNotIn(b" ", canonical)
        self.assertEqual(64, len(MODEL.manifest_hash(self.manifest)))


if __name__ == "__main__":
    unittest.main()
