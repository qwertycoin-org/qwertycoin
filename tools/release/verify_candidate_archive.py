#!/usr/bin/env python3
"""Safely extract and verify one Qwertycoin Core release candidate."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import tarfile
import zipfile


SHA256_LINE = re.compile(r"^([0-9a-f]{64}) ([ *])(.+)$")
MAX_ARCHIVE_MEMBERS = 10_000
MAX_UNCOMPRESSED_BYTES = 2 * 1024 * 1024 * 1024


def fail(message: str) -> None:
    raise SystemExit(message)


def normalize_member(name: str) -> str:
    if not name or "\0" in name or "\\" in name:
        fail(f"unsafe archive member name: {name!r}")
    stripped = name.rstrip("/")
    path = PurePosixPath(stripped)
    if not stripped or path.is_absolute() or any(part in ("", ".", "..") for part in path.parts):
        fail(f"unsafe archive member path: {name!r}")
    return str(path)


def require_expected_member(name: str, root_name: str) -> None:
    if name == f"{root_name}.sha256":
        return
    if name != root_name and not name.startswith(f"{root_name}/"):
        fail(f"archive member is outside expected root: {name}")


def validate_names(names: list[str], root_name: str) -> None:
    seen: set[str] = set()
    casefolded: dict[str, str] = {}
    for raw_name in names:
        name = normalize_member(raw_name)
        require_expected_member(name, root_name)
        if name in seen:
            fail(f"duplicate archive member: {name}")
        seen.add(name)
        folded = name.casefold()
        if folded in casefolded and casefolded[folded] != name:
            fail(f"case-folding collision: {casefolded[folded]} and {name}")
        casefolded[folded] = name


def extract_tar(archive: Path, destination: Path, root_name: str) -> None:
    with tarfile.open(archive, "r:gz") as package:
        members = package.getmembers()
        if (
            len(members) > MAX_ARCHIVE_MEMBERS
            or sum(member.size for member in members) > MAX_UNCOMPRESSED_BYTES
        ):
            fail("Core release archive exceeds extraction safety limits")
        validate_names([member.name for member in members], root_name)
        for member in members:
            name = normalize_member(member.name)
            if member.issym() or member.islnk():
                fail(f"links are not permitted in Core release archives: {name}")
            if not (member.isfile() or member.isdir()):
                fail(f"special archive member is not permitted: {name}")
        package.extractall(destination)


def extract_zip(archive: Path, destination: Path, root_name: str) -> None:
    with zipfile.ZipFile(archive) as package:
        members = package.infolist()
        if (
            len(members) > MAX_ARCHIVE_MEMBERS
            or sum(member.file_size for member in members) > MAX_UNCOMPRESSED_BYTES
        ):
            fail("Core release archive exceeds extraction safety limits")
        validate_names([member.filename for member in members], root_name)
        for member in members:
            if stat.S_ISLNK(member.external_attr >> 16):
                fail(f"links are not permitted in Windows archives: {member.filename}")
        package.extractall(destination)


def regular_files(root: Path) -> set[str]:
    return {
        path.relative_to(root.parent).as_posix()
        for path in root.rglob("*")
        if path.is_file() and not path.is_symlink()
    }


def verify_manifest(destination: Path, root_name: str) -> int:
    manifest = destination / f"{root_name}.sha256"
    if not manifest.is_file() or manifest.is_symlink():
        fail("inner SHA-256 manifest is missing")
    expected: dict[str, str] = {}
    for line_number, line in enumerate(manifest.read_text(encoding="utf-8").splitlines(), 1):
        match = SHA256_LINE.fullmatch(line)
        if not match:
            fail(f"invalid SHA-256 manifest line {line_number}")
        digest, _mode, raw_name = match.groups()
        name = normalize_member(raw_name)
        require_expected_member(name, root_name)
        if name == f"{root_name}.sha256" or name in expected:
            fail(f"invalid or duplicate SHA-256 path: {name}")
        expected[name] = digest
    root = destination / root_name
    if not root.is_dir() or root.is_symlink():
        fail("candidate root directory is missing")
    actual = regular_files(root)
    if actual != set(expected):
        fail(f"SHA-256 coverage mismatch; unhashed={sorted(actual-set(expected))[:3]}, absent={sorted(set(expected)-actual)[:3]}")
    for name, digest in expected.items():
        actual_digest = hashlib.sha256((destination / name).read_bytes()).hexdigest()
        if actual_digest != digest:
            fail(f"SHA-256 mismatch: {name}")
    return len(expected)


def read_json(path: Path, description: str) -> dict:
    if not path.is_file() or path.is_symlink():
        fail(f"{description} is missing or not a regular file")
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"invalid {description}: {exc}")
    if not isinstance(value, dict):
        fail(f"{description} must be a JSON object")
    return value


def require_payload(root: Path, platform: str) -> None:
    suffix = ".exe" if platform == "windows" else ""
    required = [
        f"qwertycoind{suffix}",
        f"qwertycoin-wallet-cli{suffix}",
        f"qwertycoin-wallet-rpc{suffix}",
        "BUILD-INFO.json",
        "README-RELEASE.md",
        "THIRD-PARTY-NOTICES.md",
        "LICENSE",
        "evidence/RELEASE-EVIDENCE.json",
        "evidence/EPOSE-RELEASE-GATE.json",
        "evidence/PARAMETER-MANIFEST-V2.json",
    ]
    for relative in required:
        candidate = root / relative
        if not candidate.is_file() or candidate.is_symlink():
            fail(f"required payload is missing: {relative}")
    if platform in ("linux", "macos") and not any((root / "lib").glob("*")):
        fail("portable Unix package has no bundled runtime libraries")
    if platform == "windows" and not any(root.glob("*.dll")):
        fail("portable Windows package has no bundled runtime DLLs")


def verify_metadata(info: dict, args: argparse.Namespace, root: Path) -> None:
    release_version = args.expected_tag.removeprefix("v").split("-rc", 1)[0]
    expected = {
        ("release", "version"): release_version,
        ("release", "tag"): args.expected_tag,
        ("release", "source_revision"): args.expected_revision,
        ("workflow", "name"): args.expected_workflow_name,
        ("workflow", "revision"): args.expected_workflow_revision,
        ("workflow", "run_id"): args.expected_run_id,
        ("workflow", "run_attempt"): args.expected_run_attempt,
        ("platform", "os"): args.expected_os,
        ("platform", "architecture"): args.expected_arch,
    }
    for path, value in expected.items():
        current: object = info
        for part in path:
            current = current.get(part) if isinstance(current, dict) else None
        if current != value:
            fail(f"BUILD-INFO mismatch for {'.'.join(path)}: expected {value!r}, got {current!r}")
    release = info.get("release")
    compatible_version = release.get("compatible_version") if isinstance(release, dict) else None
    if not isinstance(compatible_version, str) or not re.fullmatch(
        rf"{re.escape(release_version)}\.[0-9]+", compatible_version
    ):
        fail("BUILD-INFO has an invalid compatible Core version")
    build = info.get("build")
    if not isinstance(build, dict) or build.get("targets") != ["daemon", "simplewallet", "wallet_rpc_server"]:
        fail("BUILD-INFO does not declare the exact supported target list")
    if build.get("use_device_trezor") is not False or build.get("type") != "Release":
        fail("BUILD-INFO build contract is not a Trezor-disabled Release build")
    submodules = info.get("submodules")
    if not isinstance(submodules, dict) or not submodules:
        fail("BUILD-INFO has no recursive submodule provenance")
    gate = info.get("epose_release_gate")
    if not isinstance(gate, dict) or gate.get("candidate_source_revision") != args.expected_revision:
        fail("EPoSE gate is not bound to the candidate source revision")
    gate_file = read_json(root / "evidence/EPOSE-RELEASE-GATE.json", "EPoSE release gate")
    if gate_file != gate:
        fail("BUILD-INFO and packaged EPoSE release gate disagree")
    manifest = read_json(root / "evidence/PARAMETER-MANIFEST-V2.json", "EPoSE parameter manifest")
    canonical_manifest = (json.dumps(
        manifest, sort_keys=True, separators=(",", ":"), ensure_ascii=False
    ) + "\n").encode()
    if hashlib.sha256(canonical_manifest).hexdigest() != gate.get("manifest_sha256"):
        fail("packaged EPoSE manifest does not match the evaluated release gate")
    committee = manifest.get("committee", {})
    admission = manifest.get("admission", {})
    reward = manifest.get("reward", {})
    expected_network = {
        "type": manifest.get("network", {}).get("type"),
        "network_id": manifest.get("network", {}).get("network_id"),
        "genesis_hash": manifest.get("network", {}).get("genesis_hash"),
        "parameter_set_sha256": manifest.get("commitments", {}).get("parameter_set_sha256"),
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
    }
    if info.get("network") != expected_network:
        fail("BUILD-INFO network identity does not match the packaged EPoSE manifest")
    if args.require_stable_gate and gate.get("stable_permitted") is not True:
        fail("stable publication is blocked by the packaged EPoSE release gate")
    evidence = read_json(root / "evidence/RELEASE-EVIDENCE.json", "release evidence")
    isolated = evidence.get("isolated_network")
    programs = evidence.get("programs")
    network = info.get("network")
    if (
        not isinstance(isolated, dict)
        or not isinstance(programs, dict)
        or any(
            programs.get(name) != {"help": "pass", "version": "pass"}
            for name in (
                "qwertycoind",
                "qwertycoin-wallet-cli",
                "qwertycoin-wallet-rpc",
            )
        )
        or isolated.get("daemon_rpc_ready") != "pass"
        or not isinstance(network, dict)
        or isolated.get("network") != "offline-mainnet"
        or isolated.get("genesis_hash_match") != "pass"
        or isolated.get("epose_protocol_version") != 2
        or isolated.get("epose_service_reward_bps") != network.get("service_reward_basis_points")
        or isolated.get("daemon_rpc_stop_ack") != "pass"
        or isolated.get("daemon_clean_exit") != "pass"
        or isolated.get("daemon_stop_method") not in {"rpc", "rpc-plus-platform-signal"}
        or isolated.get("wallet_rpc_digest_auth") != "pass"
        or isolated.get("wallet_seed_restore_address_match") != "pass"
        or isolated.get("secrets_logged") is not False
    ):
        fail("candidate lacks the complete isolated network and wallet smoke evidence")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--destination", type=Path, required=True)
    parser.add_argument("--root-name", required=True)
    parser.add_argument("--platform", choices=("linux", "macos", "windows"), required=True)
    parser.add_argument("--expected-tag", required=True)
    parser.add_argument("--expected-revision", required=True)
    parser.add_argument("--expected-workflow-name", required=True)
    parser.add_argument("--expected-workflow-revision", required=True)
    parser.add_argument("--expected-run-id", required=True)
    parser.add_argument("--expected-run-attempt", default="1")
    parser.add_argument("--expected-os", required=True)
    parser.add_argument("--expected-arch", required=True)
    parser.add_argument("--require-stable-gate", action="store_true")
    args = parser.parse_args()
    if args.destination.exists():
        fail(f"refusing to reuse extraction destination: {args.destination}")
    args.destination.mkdir(parents=True)
    try:
        if args.archive.name.endswith(".tar.gz"):
            extract_tar(args.archive, args.destination, args.root_name)
        elif args.archive.suffix == ".zip":
            extract_zip(args.archive, args.destination, args.root_name)
        else:
            fail("unsupported archive type")
        count = verify_manifest(args.destination, args.root_name)
        root = args.destination / args.root_name
        require_payload(root, args.platform)
        info = read_json(root / "BUILD-INFO.json", "BUILD-INFO.json")
        verify_metadata(info, args, root)
        print(f"Verified {args.platform} Core candidate: {count} files and exact source/workflow provenance")
    except BaseException:
        shutil.rmtree(args.destination, ignore_errors=True)
        raise


if __name__ == "__main__":
    main()
