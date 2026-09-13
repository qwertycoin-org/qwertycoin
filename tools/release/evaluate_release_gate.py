#!/usr/bin/env python3
"""Evaluate and bind the repository EPoSE release gate to one candidate."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys
import tempfile


def fail(message: str) -> None:
    raise SystemExit(message)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--expected-revision", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--require-ready", action="store_true")
    parser.add_argument("--repository", type=Path, default=Path("."))
    args = parser.parse_args()

    repository = args.repository.resolve()
    manifest_path = repository / "docs/epose/PARAMETER_MANIFEST_V2.json"
    policy_path = repository / "docs/epose/review/RELEASE_GATE_POLICY_V2.json"
    gates_path = repository / "docs/epose/review/RELEASE_GATES_V2.json"
    evaluator_dir = repository / "tests/epose"
    evaluator = evaluator_dir / "release_gate_v2.py"

    with tempfile.TemporaryDirectory(prefix="qwc-release-gate-") as temp_dir:
        evaluated_path = Path(temp_dir) / "evaluated.json"
        process = subprocess.run(
            [
                sys.executable,
                str(evaluator),
                "--manifest",
                str(manifest_path),
                "--policy",
                str(policy_path),
                "--gates",
                str(gates_path),
                "--evidence-root",
                str(repository),
                "--output",
                str(evaluated_path),
            ],
            cwd=evaluator_dir,
            text=True,
            capture_output=True,
        )
        if process.returncode not in (0, 1):
            detail = process.stderr.strip() or process.stdout.strip()
            fail(f"authoritative EPoSE release-gate evaluator failed: {detail}")
        try:
            evaluated = json.loads(evaluated_path.read_text(encoding="utf-8"))
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as exc:
            fail(f"cannot read evaluated release-gate data: {exc}")

    manifest_revision = manifest.get("release", {}).get("source_revision")
    candidate_bound = manifest_revision == args.expected_revision
    ready = evaluated.get("overall_status") == "ready"
    evaluated["candidate_source_revision"] = args.expected_revision
    evaluated["manifest_source_revision"] = manifest_revision
    evaluated["candidate_bound"] = candidate_bound
    evaluated["stable_permitted"] = ready and candidate_bound
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(evaluated, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print(
        f"EPoSE release gate: {evaluated.get('overall_status')} "
        f"({evaluated.get('satisfied_gate_count')}/{evaluated.get('total_gate_count')} satisfied), "
        f"candidate_bound={str(candidate_bound).lower()}"
    )
    if args.require_ready and not evaluated["stable_permitted"]:
        fail("stable release blocked: EPoSE gate is not ready and candidate-bound")


if __name__ == "__main__":
    main()
