#!/usr/bin/env python3
"""Write candidate metadata from measured build and repository values."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess


VERSION_RE = re.compile(r'^#define DEF_QWERTYCOIN_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"$', re.MULTILINE)
COMPATIBLE_VERSION_RE = re.compile(r'^#define DEF_MONERO_VERSION "([0-9]+(?:\.[0-9]+){3})"$', re.MULTILINE)
RELEASE_NAME_RE = re.compile(r'^#define DEF_QWERTYCOIN_RELEASE_NAME "([^"]+)"$', re.MULTILINE)


def git(*arguments: str) -> str:
    return subprocess.check_output(["git", *arguments], text=True).rstrip()


def submodules() -> dict[str, str]:
    result: dict[str, str] = {}
    output = git("submodule", "status", "--recursive")
    for line in output.splitlines():
        match = re.match(r"^[ +]([0-9a-f]{40}) ([^ ]+)", line)
        if not match:
            raise SystemExit(f"unexpected submodule status line: {line!r}")
        revision, path = match.groups()
        result[path] = revision
    return dict(sorted(result.items()))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--release-tag", required=True)
    parser.add_argument("--expected-revision", required=True)
    parser.add_argument("--workflow-revision", required=True)
    parser.add_argument("--workflow-name", required=True)
    parser.add_argument("--run-id", required=True)
    parser.add_argument("--run-attempt", required=True)
    parser.add_argument("--runner-os", required=True)
    parser.add_argument("--runner-arch", required=True)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--sdk", required=True)
    parser.add_argument("--toolchain", required=True)
    parser.add_argument("--build-options", type=Path, required=True)
    parser.add_argument("--gate", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    parser.add_argument("--compatibility", required=True)
    parser.add_argument("--signing-status", required=True)
    args = parser.parse_args()

    source_revision = git("rev-parse", "HEAD")
    if source_revision != args.expected_revision:
        raise SystemExit("checked-out source revision does not match expected revision")
    source = Path("src/version.cpp.in").read_text(encoding="utf-8")
    version_match = VERSION_RE.search(source)
    compatible_version_match = COMPATIBLE_VERSION_RE.search(source)
    release_name_match = RELEASE_NAME_RE.search(source)
    if not version_match or not compatible_version_match or not release_name_match:
        raise SystemExit("cannot parse Core release version/name")
    version = version_match.group(1)
    if not (args.release_tag == f"v{version}" or re.fullmatch(rf"v{re.escape(version)}-rc[1-9][0-9]*", args.release_tag)):
        raise SystemExit("release tag does not match the Core public version")

    gate = json.loads(args.gate.read_text(encoding="utf-8"))
    manifest = json.loads(Path("docs/epose/PARAMETER_MANIFEST_V2.json").read_text(encoding="utf-8"))
    build_options = json.loads(args.build_options.read_text(encoding="utf-8"))
    evidence = json.loads(args.evidence.read_text(encoding="utf-8"))
    network = manifest.get("network", {})
    commitments = manifest.get("commitments", {})
    committee = manifest.get("committee", {})
    admission = manifest.get("admission", {})
    reward = manifest.get("reward", {})

    result = {
        "schema_version": 1,
        "release": {
            "version": version,
            "compatible_version": compatible_version_match.group(1),
            "tag": args.release_tag,
            "source_revision": source_revision,
            "release_name": release_name_match.group(1),
            "tag_state": "exact-tag-at-head",
        },
        "workflow": {
            "name": args.workflow_name,
            "revision": args.workflow_revision,
            "run_id": str(args.run_id),
            "run_attempt": str(args.run_attempt),
        },
        "platform": {
            "os": args.runner_os,
            "architecture": args.runner_arch,
            "compiler": args.compiler,
            "cmake": args.cmake,
            "sdk": args.sdk,
            "toolchain": args.toolchain,
            "compatibility": args.compatibility,
            "signing_status": args.signing_status,
        },
        "build": build_options,
        "submodules": submodules(),
        "network": {
            "type": network.get("type"),
            "network_id": network.get("network_id"),
            "genesis_hash": network.get("genesis_hash"),
            "parameter_set_sha256": commitments.get("parameter_set_sha256"),
            "epoch_length_blocks": manifest.get("epoch", {}).get("length_blocks"),
            "committee_size": committee.get("size"),
            "committee_threshold": committee.get("threshold"),
            "committee_round_offsets": committee.get("round_offsets"),
            "committee_rounds_required": committee.get("rounds_required"),
            "admission_leading_zero_bits": admission.get("leading_zero_bits"),
            "admission_lease_epochs": admission.get("lease_epochs"),
            "service_reward_basis_points": reward.get("basis_points"),
            "service_reward_emission_accounting": reward.get("emission_accounting"),
            "service_reward_fee_policy": reward.get("fee_policy"),
            "service_reward_empty_set_policy": reward.get("empty_set_policy"),
        },
        "epose_release_gate": gate,
        "verification": evidence,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
