#!/usr/bin/env python3
"""Authoritative typed validator for EPoSE v2 parameter manifests."""

from __future__ import annotations

import hashlib
import json
from typing import Any


UINT64_MAX = (1 << 64) - 1
HEX_DIGITS = frozenset("0123456789abcdef")


class ManifestError(ValueError):
    pass


def canonical_bytes(value: Any) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False) + "\n").encode()


def digest(value: Any) -> str:
    return hashlib.sha256(canonical_bytes(value)).hexdigest()


def get_path(value: dict[str, Any], path: str) -> Any:
    current: Any = value
    for component in path.split("."):
        if not isinstance(current, dict) or component not in current:
            raise ManifestError(f"missing manifest path: {path}")
        current = current[component]
    return current


def require_int(value: Any, path: str, minimum: int = 0, maximum: int = UINT64_MAX) -> int:
    if not isinstance(value, int) or isinstance(value, bool):
        raise ManifestError(f"{path} must be an integer")
    if not minimum <= value <= maximum:
        raise ManifestError(f"{path} is outside {minimum}..{maximum}")
    return value


def require_bool(value: Any, path: str) -> bool:
    if not isinstance(value, bool):
        raise ManifestError(f"{path} must be a boolean")
    return value


def require_enum(value: Any, path: str, allowed: set[str]) -> str:
    if not isinstance(value, str) or value not in allowed:
        raise ManifestError(f"{path} must be one of {sorted(allowed)}")
    return value


def require_hex(value: Any, path: str, digits: int) -> str:
    if not isinstance(value, str) or len(value) != digits or any(character not in HEX_DIGITS for character in value):
        raise ManifestError(f"{path} must be {digits} lowercase hexadecimal digits")
    return value


REQUIRED_ACTIVATION_PATHS = (
    "activation.height",
    "network.genesis_hash",
    "release.source_revision",
    "admission.lease_epochs",
    "admission.leading_zero_bits",
    "committee.round_offsets",
    "committee.rounds_required",
    "committee.size",
    "committee.threshold",
    "resource_limits.max_active_population",
    "resource_limits.max_admission_verifications_per_block",
    "resource_limits.max_envelope_bytes_per_transaction",
    "resource_limits.max_envelopes_per_transaction",
    "resource_limits.max_epose_bytes_per_block",
    "resource_limits.max_records_per_block",
    "resource_limits.max_records_per_envelope",
    "resource_limits.max_signature_verifications_per_block",
    "resource_limits.minimum_undo_blocks",
    "reward.empty_set_policy",
    "reward.emission_accounting",
    "reward.fee_policy",
    "reward.payment_proof_scheme",
    "state.index_schema",
    "state.pruned_validation_mode",
)


def missing_activation_fields(manifest: dict[str, Any]) -> list[str]:
    return sorted(path for path in REQUIRED_ACTIVATION_PATHS if get_path(manifest, path) is None)


def validate_manifest(manifest: dict[str, Any], *, allow_test_fixture: bool = False) -> list[str]:
    if not isinstance(manifest, dict):
        raise ManifestError("manifest must be an object")
    if require_int(get_path(manifest, "schema_version"), "schema_version", 2, 2) != 2:
        raise ManifestError("unsupported schema_version")

    kind = require_enum(
        get_path(manifest, "manifest_kind"),
        "manifest_kind",
        {"reservation", "activation-candidate", "test-fixture"},
    )
    if kind == "test-fixture" and not allow_test_fixture:
        raise ManifestError("test fixture cannot authorize a release")
    status = require_enum(get_path(manifest, "status"), "status", {"not-activatable", "activatable"})
    if kind == "reservation" and status != "not-activatable":
        raise ManifestError("reservation manifest must not be activatable")

    require_hex(get_path(manifest, "network.network_id"), "network.network_id", 32)
    require_enum(get_path(manifest, "network.type"), "network.type", {"mainnet", "testnet", "stagenet"})
    genesis_hash = get_path(manifest, "network.genesis_hash")
    if genesis_hash is not None:
        require_hex(genesis_hash, "network.genesis_hash", 64)

    activation_height = get_path(manifest, "activation.height")
    epoch_length = require_int(get_path(manifest, "epoch.length_blocks"), "epoch.length_blocks", 2)
    alignment = require_int(get_path(manifest, "activation.required_epoch_alignment"), "activation.required_epoch_alignment", 2)
    anchor_depth = require_int(get_path(manifest, "epoch.anchor_depth_blocks"), "epoch.anchor_depth_blocks", 1, epoch_length - 1)
    require_int(get_path(manifest, "epoch.warmup_epochs"), "epoch.warmup_epochs", 2, 2)
    require_bool(get_path(manifest, "epoch.epoch_zero_has_v2_rewards"), "epoch.epoch_zero_has_v2_rewards")
    if activation_height is not None:
        activation_height = require_int(activation_height, "activation.height", alignment)
        if activation_height % alignment or activation_height % epoch_length:
            raise ManifestError("activation.height must satisfy epoch alignment")

    source_revision = get_path(manifest, "release.source_revision")
    if source_revision is not None:
        require_hex(source_revision, "release.source_revision", 40)

    if require_int(get_path(manifest, "encoding.hardfork_version"), "encoding.hardfork_version", 18, 18) != 18:
        raise ManifestError("hardfork version reservation changed")
    require_int(get_path(manifest, "encoding.epose_protocol_version"), "encoding.epose_protocol_version", 2, 2)
    require_int(get_path(manifest, "carrier.tx_extra_tag"), "carrier.tx_extra_tag", 5, 5)
    require_bool(get_path(manifest, "carrier.coinbase_envelope_allowed"), "carrier.coinbase_envelope_allowed")
    require_bool(get_path(manifest, "carrier.fee_funded_envelope_allowed"), "carrier.fee_funded_envelope_allowed")
    require_bool(get_path(manifest, "carrier.legacy_nonce_allowed_after_activation"), "carrier.legacy_nonce_allowed_after_activation")
    require_bool(get_path(manifest, "admission.target_epoch_required"), "admission.target_epoch_required")
    require_int(get_path(manifest, "admission.context_epoch_offset"), "admission.context_epoch_offset", 1, 1)
    require_int(get_path(manifest, "reward.basis_points"), "reward.basis_points", 1000, 1000)

    committee_size = get_path(manifest, "committee.size")
    threshold = get_path(manifest, "committee.threshold")
    rounds_required = get_path(manifest, "committee.rounds_required")
    round_offsets = get_path(manifest, "committee.round_offsets")
    if committee_size is not None:
        committee_size = require_int(committee_size, "committee.size", 1, 65535)
    if threshold is not None:
        threshold = require_int(threshold, "committee.threshold", 1, 65535)
    if committee_size is not None and threshold is not None and threshold > committee_size:
        raise ManifestError("committee.threshold exceeds committee.size")
    if round_offsets is not None:
        if not isinstance(round_offsets, list) or not round_offsets:
            raise ManifestError("committee.round_offsets must be a nonempty list")
        checked_offsets = [require_int(value, f"committee.round_offsets[{index}]", 0, epoch_length - anchor_depth - 1) for index, value in enumerate(round_offsets)]
        if checked_offsets != sorted(set(checked_offsets)):
            raise ManifestError("committee.round_offsets must be strictly increasing")
    if rounds_required is not None:
        rounds_required = require_int(rounds_required, "committee.rounds_required", 1, 65535)
        if isinstance(round_offsets, list) and rounds_required > len(round_offsets):
            raise ManifestError("committee.rounds_required exceeds configured rounds")

    limits = get_path(manifest, "resource_limits")
    if not isinstance(limits, dict):
        raise ManifestError("resource_limits must be an object")
    checked_limits: dict[str, int] = {}
    for name, value in limits.items():
        if value is not None:
            checked_limits[name] = require_int(value, f"resource_limits.{name}", 1)
    if {"max_records_per_envelope", "max_records_per_block"} <= checked_limits.keys() and checked_limits["max_records_per_envelope"] > checked_limits["max_records_per_block"]:
        raise ManifestError("per-envelope record limit exceeds block record limit")
    if {"max_envelope_bytes_per_transaction", "max_epose_bytes_per_block"} <= checked_limits.keys() and checked_limits["max_envelope_bytes_per_transaction"] > checked_limits["max_epose_bytes_per_block"]:
        raise ManifestError("per-transaction envelope bytes exceed block EPoSE bytes")
    if "minimum_undo_blocks" in checked_limits and checked_limits["minimum_undo_blocks"] < epoch_length * 2:
        raise ManifestError("minimum_undo_blocks must cover at least two epochs")

    for path, allowed in (
        ("reward.empty_set_policy", {"miner-fallback", "permanent-nonissuance"}),
        ("reward.emission_accounting", {"scheduled-subsidy-advance", "actual-issued-subsidy"}),
        ("reward.fee_policy", {"subsidy-only"}),
        ("reward.payment_proof_scheme", {"scoped-tx-proof-v1"}),
        ("state.pruned_validation_mode", {"supported", "unsupported-fail-closed"}),
    ):
        value = get_path(manifest, path)
        if value is not None:
            require_enum(value, path, allowed)
    index_schema = get_path(manifest, "state.index_schema")
    if index_schema is not None:
        require_int(index_schema, "state.index_schema", 1, 0xFFFFFFFF)

    dependencies = get_path(manifest, "dependencies")
    if not isinstance(dependencies, dict) or not isinstance(dependencies.get("submodules"), dict):
        raise ManifestError("dependencies and dependencies.submodules must be objects")
    require_hex(dependencies.get("core_source_commit"), "dependencies.core_source_commit", 40)
    require_hex(dependencies.get("monero_upstream_commit"), "dependencies.monero_upstream_commit", 40)
    for path, revision in dependencies["submodules"].items():
        if not isinstance(path, str) or not path:
            raise ManifestError("submodule path must be nonempty")
        require_hex(revision, f"dependencies.submodules.{path}", 40)

    missing = missing_activation_fields(manifest)
    if status == "activatable" and (kind != "activation-candidate" and not allow_test_fixture):
        raise ManifestError("only an activation candidate may be activatable")
    if status == "activatable" and missing:
        raise ManifestError(f"activatable manifest has unset fields: {', '.join(missing)}")
    if status == "not-activatable" and not missing and kind != "reservation":
        raise ManifestError("complete candidate must declare activatable or remain an explicit reservation")
    return missing
