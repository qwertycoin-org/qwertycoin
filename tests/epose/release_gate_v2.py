#!/usr/bin/env python3
"""Fail-closed, candidate-bound EPoSE v2 release-gate evaluator."""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any

from manifest_v2 import ManifestError, digest, validate_manifest


ALLOWED_GATE_STATES = frozenset({"satisfied", "blocked", "not_run"})


class GateError(ValueError):
    pass


def file_sha256(path: Path) -> str:
    hasher = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def validate_policy(policy: dict[str, Any]) -> tuple[str, ...]:
    if policy.get("schema_version") != 1:
        raise GateError("unsupported release-gate policy schema")
    identifiers = policy.get("required_gate_ids")
    if not isinstance(identifiers, list) or not identifiers:
        raise GateError("release-gate policy must contain required_gate_ids")
    if not all(isinstance(identifier, str) and identifier for identifier in identifiers):
        raise GateError("required gate ids must be nonempty strings")
    if len(set(identifiers)) != len(identifiers):
        raise GateError("release-gate policy contains duplicate gate ids")
    return tuple(identifiers)


def validate_evidence(
    evidence: Any,
    *,
    gate_id: str,
    source_revision: str,
    manifest_sha256: str,
    evidence_root: Path,
) -> None:
    if not isinstance(evidence, list) or not evidence:
        raise GateError(f"satisfied gate {gate_id} requires structured evidence")
    for index, item in enumerate(evidence):
        label = f"gate {gate_id} evidence[{index}]"
        if not isinstance(item, dict):
            raise GateError(f"{label} must be an object")
        required = {"path", "sha256", "source_revision", "manifest_sha256", "result", "artifact_ref"}
        if set(item) != required:
            raise GateError(f"{label} has an invalid field set")
        relative_path = item["path"]
        if not isinstance(relative_path, str) or not relative_path or Path(relative_path).is_absolute() or ".." in Path(relative_path).parts:
            raise GateError(f"{label} path must be a safe relative path")
        if item["source_revision"] != source_revision:
            raise GateError(f"{label} targets the wrong source revision")
        if item["manifest_sha256"] != manifest_sha256:
            raise GateError(f"{label} targets the wrong manifest")
        if item["result"] not in {"passed", "approved"}:
            raise GateError(f"{label} has a non-satisfying result")
        artifact = evidence_root / relative_path
        if not artifact.is_file():
            raise GateError(f"{label} artifact does not exist")
        actual_sha256 = file_sha256(artifact)
        if item["sha256"] != actual_sha256:
            raise GateError(f"{label} artifact digest mismatch")
        if item["artifact_ref"] != f"sha256:{actual_sha256}":
            raise GateError(f"{label} artifact_ref is not immutable")


def evaluate(
    manifest: dict[str, Any],
    ledger: dict[str, Any],
    policy: dict[str, Any],
    *,
    evidence_root: Path,
    allow_test_fixture: bool = False,
) -> dict[str, Any]:
    try:
        missing_fields = validate_manifest(manifest, allow_test_fixture=allow_test_fixture)
    except ManifestError as exc:
        raise GateError(str(exc)) from exc
    required_ids = validate_policy(policy)
    if ledger.get("schema_version") != 2:
        raise GateError("unsupported release-gate ledger schema")
    gates = ledger.get("gates")
    if not isinstance(gates, list) or not gates:
        raise GateError("release-gate ledger must contain gates")
    by_id: dict[str, dict[str, Any]] = {}
    for gate in gates:
        if not isinstance(gate, dict):
            raise GateError("gate must be an object")
        gate_id = gate.get("id")
        if not isinstance(gate_id, str) or not gate_id:
            raise GateError("gate id must be a nonempty string")
        if gate_id in by_id:
            raise GateError(f"duplicate gate id: {gate_id}")
        by_id[gate_id] = gate
    missing_gate_ids = sorted(set(required_ids) - set(by_id))
    unknown_gate_ids = sorted(set(by_id) - set(required_ids))
    if missing_gate_ids:
        raise GateError(f"missing mandatory gate ids: {', '.join(missing_gate_ids)}")
    if unknown_gate_ids:
        raise GateError(f"unknown gate ids: {', '.join(unknown_gate_ids)}")

    manifest_sha256 = digest(manifest)
    source_revision = manifest.get("release", {}).get("source_revision")
    unresolved: list[dict[str, str]] = []
    for gate_id in required_ids:
        gate = by_id[gate_id]
        state = gate.get("state")
        summary = gate.get("summary")
        evidence = gate.get("evidence")
        if state not in ALLOWED_GATE_STATES:
            raise GateError(f"invalid state for {gate_id}: {state!r}")
        if not isinstance(summary, str) or not summary:
            raise GateError(f"gate {gate_id} requires a summary")
        if state == "satisfied":
            if not isinstance(source_revision, str):
                raise GateError(f"satisfied gate {gate_id} requires a candidate source revision")
            validate_evidence(
                evidence,
                gate_id=gate_id,
                source_revision=source_revision,
                manifest_sha256=manifest_sha256,
                evidence_root=evidence_root,
            )
        elif evidence not in ([], None):
            raise GateError(f"unresolved gate {gate_id} must not claim satisfying evidence")
        if state != "satisfied":
            unresolved.append({"id": gate_id, "state": state, "summary": summary})

    computed = "ready" if manifest.get("status") == "activatable" and not missing_fields and not unresolved else "no-go"
    if ledger.get("overall_status") != computed:
        raise GateError(f"declared overall_status {ledger.get('overall_status')!r} disagrees with computed {computed!r}")
    return {
        "schema_version": 2,
        "overall_status": computed,
        "manifest_sha256": manifest_sha256,
        "gate_policy_sha256": digest(policy),
        "gate_ledger_sha256": digest(ledger),
        "manifest_status": manifest.get("status"),
        "missing_manifest_fields": missing_fields,
        "unresolved_gates": unresolved,
        "satisfied_gate_count": len(required_ids) - len(unresolved),
        "total_gate_count": len(required_ids),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--gates", type=Path, required=True)
    parser.add_argument("--policy", type=Path, required=True)
    parser.add_argument("--evidence-root", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--expect", choices=("ready", "no-go"))
    args = parser.parse_args()
    try:
        manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
        ledger = json.loads(args.gates.read_text(encoding="utf-8"))
        policy = json.loads(args.policy.read_text(encoding="utf-8"))
        result = evaluate(manifest, ledger, policy, evidence_root=args.evidence_root)
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
            print(f"expected {args.expect!r}, got {result['overall_status']!r}", file=sys.stderr)
            return 1
        return 0
    return 0 if result["overall_status"] == "ready" else 1


if __name__ == "__main__":
    raise SystemExit(main())
