#!/usr/bin/env python3

from __future__ import annotations

import sys
from pathlib import Path
from typing import Any

from manifest_v2 import ManifestError, UINT64_MAX, digest, get_path, require_enum, require_int

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "tests/epose/data/PARAMETER_MANIFEST_CONTRACTS.json"

CONSENSUS_PARAMETER_SECTIONS = (
    "activation",
    "chainlock",
    "contracts",
    "epose",
    "mint",
    "schema_version",
    "window",
)

CONFIG_CONSTANTS = {
    "activation.hardfork_version": "HF_VERSION_QWC_CONTRACTS",
    "chainlock.lock_max_age_blocks": "QWC_LOCK_MAX_AGE_BLOCKS",
    "chainlock.quorum_max": "QWC_CHAINLOCK_QUORUM_MAX",
    "chainlock.quorum_min_active_nodes": "QWC_CHAINLOCK_MIN_NODES",
    "chainlock.threshold_percent": "QWC_CHAINLOCK_THRESHOLD_PERCENT",
    "contracts.tx_extra_max_bytes": "QWC_CONTRACT_TX_EXTRA_MAX_SIZE",
    "contracts.tx_gas_cap": "QWC_VM_TX_GAS_CAP",
    "contracts.block_gas_limit": "QWC_VM_BLOCK_GAS_LIMIT",
    "contracts.vm_undo_retention_blocks": "QWC_VM_UNDO_RETENTION_BLOCKS",
    "epose.retained_epochs": "QWC_EPOSE_RETAINED_EPOCHS",
    "epose.min_compatible_committee": "QWC_EPOSE_MIN_COMPATIBLE_COMMITTEE",
    "mint.unlock_blocks": "QWC_MINT_UNLOCK_BLOCKS",
    "policy.wallet_default_window_blocks": "QWC_WALLET_DEFAULT_WINDOW_BLOCKS",
    "policy.wallet_default_window_rounding": "QWC_WALLET_DEFAULT_WINDOW_ROUNDING",
    "policy.window_pool_lead_blocks": "QWC_WINDOW_POOL_LEAD_BLOCKS",
    "window.unlock_delay_blocks": "QWC_WINDOW_UNLOCK_DELAY_BLOCKS",
}


def consensus_parameter_projection(manifest: dict[str, Any]) -> dict[str, Any]:
    if not isinstance(manifest, dict):
        raise ManifestError("manifest must be an object")
    return {section: manifest[section] for section in CONSENSUS_PARAMETER_SECTIONS}


def consensus_parameter_digest(manifest: dict[str, Any]) -> str:
    return digest(consensus_parameter_projection(manifest))


def validate_manifest(manifest: dict[str, Any]) -> None:
    if not isinstance(manifest, dict):
        raise ManifestError("manifest must be an object")
    require_int(get_path(manifest, "schema_version"), "schema_version", 1, 1)
    kind = require_enum(get_path(manifest, "manifest_kind"), "manifest_kind", {"reservation", "activation-candidate"})
    status = require_enum(get_path(manifest, "status"), "status", {"not-activatable", "activatable"})
    require_int(get_path(manifest, "activation.hardfork_version"), "activation.hardfork_version", 18, 18)
    require_int(get_path(manifest, "activation.required_epoch_alignment"), "activation.required_epoch_alignment", 720, 720)
    height = get_path(manifest, "activation.height")
    if height is not None:
        height = require_int(height, "activation.height", 1)
        if height % 720:
            raise ManifestError("activation.height must be an EPoSE epoch start (H-28)")
    if (kind == "reservation") != (status == "not-activatable") or (status == "activatable") != (height is not None):
        raise ManifestError("kind/status/activation.height disagree")

    quorum_max = require_int(get_path(manifest, "chainlock.quorum_max"), "chainlock.quorum_max", 1, 65535)
    require_int(get_path(manifest, "chainlock.quorum_min_active_nodes"), "chainlock.quorum_min_active_nodes", 1, quorum_max)
    require_int(get_path(manifest, "chainlock.threshold_percent"), "chainlock.threshold_percent", 51, 100)
    require_enum(get_path(manifest, "chainlock.threshold_rounding"), "chainlock.threshold_rounding", {"ceil"})
    require_int(get_path(manifest, "chainlock.lock_max_age_blocks"), "chainlock.lock_max_age_blocks", 1)
    require_int(get_path(manifest, "contracts.tx_extra_max_bytes"), "contracts.tx_extra_max_bytes", 1, UINT64_MAX)
    require_int(get_path(manifest, "contracts.vm_undo_retention_blocks"), "contracts.vm_undo_retention_blocks", 1)
    require_int(get_path(manifest, "epose.retained_epochs"), "epose.retained_epochs", 8, 8)
    require_int(get_path(manifest, "epose.min_compatible_committee"), "epose.min_compatible_committee", 5, 5)
    block_gas_limit = require_int(get_path(manifest, "contracts.block_gas_limit"), "contracts.block_gas_limit", 1)
    require_int(get_path(manifest, "contracts.tx_gas_cap"), "contracts.tx_gas_cap", 1, block_gas_limit)
    gas_cost_table = get_path(manifest, "contracts.gas_cost_table")
    if not isinstance(gas_cost_table, dict) or not gas_cost_table:
        raise ManifestError("contracts.gas_cost_table must be a nonempty object")
    for name, cost in gas_cost_table.items():
        require_int(cost, f"contracts.gas_cost_table.{name}", 1)
    require_int(get_path(manifest, "mint.unlock_blocks"), "mint.unlock_blocks", 1)
    require_int(get_path(manifest, "window.unlock_delay_blocks"), "window.unlock_delay_blocks", 0)
    require_int(get_path(manifest, "policy.window_pool_lead_blocks"), "policy.window_pool_lead_blocks", 0)
    require_int(get_path(manifest, "policy.wallet_default_window_blocks"), "policy.wallet_default_window_blocks", 1)
    require_int(get_path(manifest, "policy.wallet_default_window_rounding"), "policy.wallet_default_window_rounding", 1)

    undecided = get_path(manifest, "undecided")
    if not isinstance(undecided, list) or undecided != sorted(set(undecided)):
        raise ManifestError("undecided must be a sorted list of unique names")
    if status == "activatable" and undecided:
        raise ManifestError("activatable manifest still has undecided parameters")

    commitment = get_path(manifest, "commitments.parameter_set_sha256")
    if commitment is not None and commitment != consensus_parameter_digest(manifest):
        raise ManifestError("commitments.parameter_set_sha256 does not match the consensus projection")


if __name__ == "__main__":
    import json

    print(consensus_parameter_digest(json.loads(Path(sys.argv[1] if len(sys.argv) > 1 else MANIFEST).read_text())))
