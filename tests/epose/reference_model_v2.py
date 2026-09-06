#!/usr/bin/env python3
"""Independent CO-01 timing and envelope reference model.

This model deliberately implements only rules finalized by CO-01. Reserved
record payloads remain semantically invalid until their owning change order
defines fields, transcripts, limits, and vectors.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Tuple


UINT64_MAX = (1 << 64) - 1
MAGIC = b"QEP2"
TX_EXTRA_TAG_EPOSE = 0x05
ENVELOPE_VERSION = 1
KNOWN_RECORD_TYPES = frozenset(range(1, 6))


class ModelError(ValueError):
    pass


def checked_u64(value: int, name: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool):
        raise ModelError(f"{name} must be an integer")
    if value < 0 or value > UINT64_MAX:
        raise ModelError(f"{name} is outside uint64")
    return value


def checked_add(left: int, right: int, name: str) -> int:
    checked_u64(left, name)
    checked_u64(right, name)
    if left > UINT64_MAX - right:
        raise ModelError(f"{name} overflows uint64")
    return left + right


def checked_mul(left: int, right: int, name: str) -> int:
    checked_u64(left, name)
    checked_u64(right, name)
    if left and right > UINT64_MAX // left:
        raise ModelError(f"{name} overflows uint64")
    return left * right


def encode_uvarint(value: int) -> bytes:
    checked_u64(value, "varint")
    output = bytearray()
    while value >= 0x80:
        output.append((value & 0x7F) | 0x80)
        value >>= 7
    output.append(value)
    return bytes(output)


def decode_uvarint(blob: bytes, offset: int = 0) -> Tuple[int, int]:
    value = 0
    shift = 0
    start = offset
    while offset < len(blob) and shift <= 63:
        byte = blob[offset]
        offset += 1
        value |= (byte & 0x7F) << shift
        if not byte & 0x80:
            if encode_uvarint(value) != blob[start:offset]:
                raise ModelError("noncanonical varint")
            return value, offset
        shift += 7
    raise ModelError("truncated or overflowing varint")


@dataclass(frozen=True)
class Timing:
    activation_height: int
    epoch_length: int = 720
    anchor_depth: int = 60

    def __post_init__(self) -> None:
        checked_u64(self.activation_height, "activation_height")
        checked_u64(self.epoch_length, "epoch_length")
        checked_u64(self.anchor_depth, "anchor_depth")
        if self.epoch_length == 0:
            raise ModelError("epoch_length must be positive")
        if self.anchor_depth == 0 or self.anchor_depth >= self.epoch_length:
            raise ModelError("anchor_depth must be in 1..epoch_length-1")
        if self.activation_height % self.epoch_length:
            raise ModelError("activation height is not epoch aligned")

    @property
    def activation_epoch(self) -> int:
        return self.activation_height // self.epoch_length

    @property
    def first_service_epoch(self) -> int:
        return checked_add(self.activation_epoch, 1, "first_service_epoch")

    @property
    def first_payout_height(self) -> int:
        return self.start(checked_add(self.activation_epoch, 2, "first_payout_epoch"))

    def start(self, epoch: int) -> int:
        return checked_mul(checked_u64(epoch, "epoch"), self.epoch_length, "epoch_start")

    def end(self, epoch: int) -> int:
        return checked_add(self.start(epoch), self.epoch_length - 1, "epoch_end")

    def enrollment_cutoff(self, epoch: int) -> int:
        start = self.start(epoch)
        if start <= self.anchor_depth:
            raise ModelError("epoch has no representable enrollment cutoff")
        return start - self.anchor_depth - 1

    def committee_anchor(self, epoch: int) -> int:
        start = self.start(epoch)
        if start < self.anchor_depth:
            raise ModelError("epoch has no representable committee anchor")
        return start - self.anchor_depth

    def evidence_deadline(self, epoch: int) -> int:
        return self.end(epoch) - self.anchor_depth

    def payout_seed_height(self, service_epoch: int) -> int:
        next_epoch = checked_add(service_epoch, 1, "payout_epoch")
        return self.start(next_epoch) - self.anchor_depth

    def phase(self, height: int) -> str:
        checked_u64(height, "height")
        if height < self.activation_height:
            return "legacy-v1"
        if height < self.start(self.first_service_epoch):
            return "v2-enrollment-warmup"
        if height < self.first_payout_height:
            return "v2-service-warmup"
        return "v2-service-and-payout"

    def lease_allowed(self, inclusion_height: int, target_epoch: int) -> bool:
        checked_u64(inclusion_height, "inclusion_height")
        if target_epoch < self.first_service_epoch:
            return False
        return self.activation_height <= inclusion_height <= self.enrollment_cutoff(target_epoch)

    def receipt_allowed(self, inclusion_height: int, service_epoch: int) -> bool:
        checked_u64(inclusion_height, "inclusion_height")
        if service_epoch < self.first_service_epoch:
            return False
        return self.start(service_epoch) <= inclusion_height <= self.evidence_deadline(service_epoch)

    def payout_allowed(self, height: int, source_epoch: int) -> bool:
        checked_u64(height, "height")
        if source_epoch < self.first_service_epoch:
            return False
        payout_epoch = checked_add(source_epoch, 1, "payout_epoch")
        return self.start(payout_epoch) <= height <= self.end(payout_epoch)

    def block_version_allowed(self, height: int, major_version: int) -> bool:
        checked_u64(height, "height")
        if not isinstance(major_version, int) or isinstance(major_version, bool) or not 0 <= major_version <= 0xFF:
            raise ModelError("major_version is outside uint8")
        return major_version < 18 if height < self.activation_height else major_version >= 18


def encode_envelope(records: Iterable[Tuple[int, int, bytes]]) -> bytes:
    encoded_records = bytearray()
    count = 0
    for record_type, version, payload in records:
        if record_type not in KNOWN_RECORD_TYPES:
            raise ModelError("unknown record type")
        if version != 1:
            raise ModelError("unsupported reserved record version")
        payload = bytes(payload)
        encoded_records += struct.pack("<BBHI", record_type, version, 0, len(payload))
        encoded_records += payload
        count += 1
    if count > 0xFFFF or len(encoded_records) > 0xFFFFFFFF:
        raise ModelError("envelope exceeds structural encoding")
    return MAGIC + struct.pack("<BBHI", ENVELOPE_VERSION, 0, count, len(encoded_records)) + encoded_records


def decode_envelope(blob: bytes, max_bytes: int, max_records: int) -> list[Tuple[int, int, bytes]]:
    if len(blob) > max_bytes:
        raise ModelError("envelope byte budget exceeded")
    if len(blob) < 12:
        raise ModelError("truncated envelope")
    if blob[:4] != MAGIC:
        raise ModelError("wrong envelope magic")
    version, flags, count, records_size = struct.unpack_from("<BBHI", blob, 4)
    if version != ENVELOPE_VERSION or flags != 0:
        raise ModelError("unsupported envelope version or flags")
    if count > max_records:
        raise ModelError("record budget exceeded")
    if records_size != len(blob) - 12:
        raise ModelError("records_size mismatch")
    records = []
    offset = 12
    for _ in range(count):
        if len(blob) - offset < 8:
            raise ModelError("truncated record header")
        record_type, record_version, record_flags, payload_size = struct.unpack_from("<BBHI", blob, offset)
        offset += 8
        if record_type not in KNOWN_RECORD_TYPES:
            raise ModelError("unknown record type")
        if record_version != 1 or record_flags != 0:
            raise ModelError("unsupported record version or flags")
        if payload_size > len(blob) - offset:
            raise ModelError("truncated record payload")
        payload = blob[offset:offset + payload_size]
        offset += payload_size
        records.append((record_type, record_version, payload))
    if offset != len(blob):
        raise ModelError("record count leaves trailing bytes")
    return records


def encode_tx_extra_field(envelope: bytes) -> bytes:
    return encode_uvarint(TX_EXTRA_TAG_EPOSE) + encode_uvarint(len(envelope)) + envelope


def decode_tx_extra_field(blob: bytes, max_bytes: int, max_records: int) -> list[Tuple[int, int, bytes]]:
    tag, offset = decode_uvarint(blob)
    if tag != TX_EXTRA_TAG_EPOSE:
        raise ModelError("wrong tx_extra tag")
    size, offset = decode_uvarint(blob, offset)
    if size != len(blob) - offset:
        raise ModelError("outer envelope size mismatch")
    return decode_envelope(blob[offset:], max_bytes, max_records)


def canonical_manifest_bytes(manifest: dict) -> bytes:
    return (json.dumps(manifest, sort_keys=True, separators=(",", ":"), ensure_ascii=False) + "\n").encode("utf-8")


def manifest_hash(manifest: dict) -> str:
    return hashlib.sha256(canonical_manifest_bytes(manifest)).hexdigest()


def missing_activation_fields(manifest: dict) -> list[str]:
    required = {
        "activation.block_hash": manifest["activation"]["block_hash"],
        "activation.height": manifest["activation"]["height"],
        "network.genesis_hash": manifest["network"]["genesis_hash"],
        "admission.lease_epochs": manifest["admission"]["lease_epochs"],
        "admission.leading_zero_bits": manifest["admission"]["leading_zero_bits"],
        "committee.round_offsets": manifest["committee"]["round_offsets"],
        "committee.rounds_required": manifest["committee"]["rounds_required"],
        "committee.size": manifest["committee"]["size"],
        "committee.threshold": manifest["committee"]["threshold"],
        "reward.empty_set_policy": manifest["reward"]["empty_set_policy"],
        "reward.emission_accounting": manifest["reward"]["emission_accounting"],
        "reward.fee_policy": manifest["reward"]["fee_policy"],
        "reward.payment_proof_scheme": manifest["reward"]["payment_proof_scheme"],
        "state.index_schema": manifest["state"]["index_schema"],
        "state.pruned_validation_mode": manifest["state"]["pruned_validation_mode"],
    }
    required.update({f"resource_limits.{key}": value for key, value in manifest["resource_limits"].items()})
    return sorted(key for key, value in required.items() if value is None)


def validate_vectors(path: Path) -> None:
    vectors = json.loads(path.read_text(encoding="utf-8"))
    timing = Timing(**vectors["timing"])
    for case in vectors["height_cases"]:
        operation = case["operation"]
        if operation == "phase":
            actual = timing.phase(case["height"])
        elif operation == "lease_allowed":
            actual = timing.lease_allowed(case["height"], case["epoch"])
        elif operation == "receipt_allowed":
            actual = timing.receipt_allowed(case["height"], case["epoch"])
        elif operation == "payout_allowed":
            actual = timing.payout_allowed(case["height"], case["epoch"])
        elif operation == "block_version_allowed":
            actual = timing.block_version_allowed(case["height"], case["major_version"])
        else:
            raise ModelError(f"unknown vector operation: {operation}")
        if actual != case["expected"]:
            raise AssertionError(f"{case['name']}: expected {case['expected']!r}, got {actual!r}")

    limits = vectors["vector_only_limits"]
    for case in vectors["envelope_cases"]:
        blob = bytes.fromhex(case["hex"])
        try:
            records = decode_tx_extra_field(blob, limits["max_envelope_bytes"], limits["max_records"])
            actual = {"valid": True, "record_count": len(records)}
        except ModelError as exc:
            actual = {"valid": False, "error": str(exc)}
        expected = case["expected"]
        if actual != expected:
            raise AssertionError(f"{case['name']}: expected {expected!r}, got {actual!r}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("vectors", type=Path)
    args = parser.parse_args()
    validate_vectors(args.vectors)
    print(f"validated CO-01 vectors: {args.vectors}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ModelError, AssertionError, json.JSONDecodeError) as exc:
        print(f"vector validation failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
