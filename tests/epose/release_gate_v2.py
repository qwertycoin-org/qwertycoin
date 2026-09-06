#!/usr/bin/env python3
"""Fail-closed EPoSE v2 release-gate evaluator.

This tool does not authorize activation.  It checks that the machine-readable
manifest and evidence ledger agree, and reports every missing activation field
and unresolved gate.  A ready result is possible only when both inputs are
explicitly complete.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any


ALLOWED_GATE_STATES = frozenset({"satisfied", "blocked", "not_run"})

REQUIRED_MANIFEST_PATHS = (
    "activation.block_hash",
    "activation.height",
    "network.genesis_hash",
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


class GateError(ValueError):
    pass


def canonical_bytes(value: Any) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=False) + "\n").encode()


def digest(value: Any) -> str:
    return hashlib.sha256(canonical_bytes(value)).hexdigest()


def get_path(value: dict[str, Any], path: str) -> Any:
    current: Any = value
    for component in path.split("."):
        if not isinstance(current, dict) or component not in current:
            raise GateError(f"missing manifest path: {path}")
        current = current[component]
    return current


def evaluate(manifest: dict[str, Any], ledger: dict[str, Any]) -> dict[str, Any]:
    if ledger.get("schema_version") != 1:
        raise GateError("unsupported release-gate ledger schema")
    gates = ledger.get("gates")
    if not isinstance(gates, list) or not gates:
        raise GateError("release-gate ledger must contain gates")

    gate_ids: set[str] = set()
    unresolved: list[dict[str, str]] = []
    for gate in gates:
        if not isinstance(gate, dict):
            raise GateError("gate must be an object")
        gate_id = gate.get("id")
        state = gate.get("state")
        summary = gate.get("summary")
        evidence = gate.get("evidence")
        if not isinstance(gate_id, str) or not gate_id:
            raise GateError("gate id must be a nonempty string")
        if gate_id in gate_ids:
            raise GateError(f"duplicate gate id: {gate_id}")
        gate_ids.add(gate_id)
        if state not in ALLOWED_GATE_STATES:
            raise GateError(f"invalid state for {gate_id}: {state!r}")
        if not isinstance(summary, str) or not summary:
            raise GateError(f"gate {gate_id} requires a summary")
        if not isinstance(evidence, list) or not all(isinstance(item, str) and item for item in evidence):
            raise GateError(f"gate {gate_id} evidence must be a string list")
        if state == "satisfied" and not evidence:
            raise GateError(f"satisfied gate {gate_id} requires evidence")
        if state != "satisfied":
            unresolved.append({"id": gate_id, "state": state, "summary": summary})

    missing_fields = [path for path in REQUIRED_MANIFEST_PATHS if get_path(manifest, path) is None]
    manifest_ready = manifest.get("status") == "activatable"
    declared = ledger.get("overall_status")
    computed = "ready" if manifest_ready and not missing_fields and not unresolved else "no-go"
    if declared != computed:
        raise GateError(f"declared overall_status {declared!r} disagrees with computed {computed!r}")

    return {
        "schema_version": 1,
        "overall_status": computed,
        "manifest_sha256": digest(manifest),
        "gate_ledger_sha256": digest(ledger),
        "manifest_status": manifest.get("status"),
        "missing_manifest_fields": missing_fields,
        "unresolved_gates": unresolved,
        "satisfied_gate_count": len(gates) - len(unresolved),
        "total_gate_count": len(gates),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--gates", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--expect", choices=("ready", "no-go"))
    args = parser.parse_args()

    try:
        manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
        ledger = json.loads(args.gates.read_text(encoding="utf-8"))
        result = evaluate(manifest, ledger)
    except (OSError, json.JSONDecodeError, GateError) as exc:
        print(f"release-gate evaluation failed: {exc}", file=sys.stderr)
        return 2

    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")

    if args.expect is not None:
        if result["overall_status"] != args.expect:
            print(
                f"expected {args.expect!r}, got {result['overall_status']!r}",
                file=sys.stderr,
            )
            return 1
        return 0
    return 0 if result["overall_status"] == "ready" else 1


if __name__ == "__main__":
    raise SystemExit(main())
