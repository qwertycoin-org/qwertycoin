#!/usr/bin/env python3
"""Inspect public Linux release bytes before Docker packaging."""

from __future__ import annotations

import argparse
import hashlib
from importlib.util import module_from_spec, spec_from_file_location
import json
from pathlib import Path
import re
import tarfile


def load_verifier():
    path = Path(__file__).resolve().with_name("verify_candidate_archive.py")
    spec = spec_from_file_location("qwc_verify_candidate_archive", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load candidate archive verifier")
    module = module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


verify = load_verifier()


TAG_RE = re.compile(r"^v([0-9]+\.[0-9]+\.[0-9]+)(-rc[1-9][0-9]*)?$")
REVISION_RE = re.compile(r"^[0-9a-f]{40}$")
RUN_RE = re.compile(r"^[1-9][0-9]*$")
MAX_BUILD_INFO_BYTES = 2 * 1024 * 1024


def fail(message: str) -> None:
    raise SystemExit(message)


def sha256(path: Path) -> str:
    hasher = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def expected_asset_names(tag: str) -> set[str]:
    return {
        f"qwertycoin-{tag}-linux-x86_64.tar.gz",
        f"qwertycoin-{tag}-macos-arm64.tar.gz",
        f"qwertycoin-{tag}-windows-x86_64.zip",
    }


def read_checksums(path: Path, tag: str) -> dict[str, str]:
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as exc:
        fail(f"cannot read public SHA256SUMS: {exc}")
    result: dict[str, str] = {}
    for line_number, line in enumerate(lines, 1):
        match = verify.SHA256_LINE.fullmatch(line)
        if not match:
            fail(f"invalid public SHA256SUMS line {line_number}")
        digest, _mode, name = match.groups()
        if Path(name).name != name or name in result:
            fail(f"unsafe or duplicate public checksum name: {name}")
        result[name] = digest
    if set(result) != expected_asset_names(tag):
        fail("public SHA256SUMS does not contain the exact native release asset set")
    return result


def read_build_info(archive: Path, root_name: str) -> dict:
    expected_name = f"{root_name}/BUILD-INFO.json"
    try:
        with tarfile.open(archive, "r:gz") as package:
            members = package.getmembers()
            if (
                len(members) > verify.MAX_ARCHIVE_MEMBERS
                or sum(member.size for member in members) > verify.MAX_UNCOMPRESSED_BYTES
            ):
                fail("Core release archive exceeds inspection safety limits")
            verify.validate_names([member.name for member in members], root_name)
            matches = [member for member in members if member.name.rstrip("/") == expected_name]
            if len(matches) != 1:
                fail("release archive does not contain exactly one BUILD-INFO.json")
            member = matches[0]
            if not member.isfile() or member.issym() or member.islnk():
                fail("BUILD-INFO.json is not a regular archive member")
            if member.size <= 0 or member.size > MAX_BUILD_INFO_BYTES:
                fail("BUILD-INFO.json has an unsafe size")
            stream = package.extractfile(member)
            if stream is None:
                fail("cannot read BUILD-INFO.json from release archive")
            value = json.loads(stream.read().decode("utf-8"))
    except (OSError, tarfile.TarError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        fail(f"cannot inspect release archive metadata: {exc}")
    if not isinstance(value, dict):
        fail("BUILD-INFO.json is not an object")
    return value


def inspect(archive: Path, checksums: Path, expected_tag: str, expected_revision: str) -> dict[str, object]:
    tag_match = TAG_RE.fullmatch(expected_tag)
    if not tag_match:
        fail("release tag must be vMAJOR.MINOR.PATCH or vMAJOR.MINOR.PATCH-rcN")
    if not REVISION_RE.fullmatch(expected_revision):
        fail("expected revision must be a lowercase 40-character SHA")
    root_name = f"qwertycoin-{expected_tag}-linux-x86_64"
    expected_archive_name = f"{root_name}.tar.gz"
    if archive.name != expected_archive_name:
        fail(f"unexpected Linux archive name: {archive.name}")
    digest = sha256(archive)
    expected_digest = read_checksums(checksums, expected_tag)[expected_archive_name]
    if digest != expected_digest:
        fail("Linux release archive does not match public SHA256SUMS")

    info = read_build_info(archive, root_name)
    release = info.get("release")
    workflow = info.get("workflow")
    platform = info.get("platform")
    if not isinstance(release, dict) or not isinstance(workflow, dict) or not isinstance(platform, dict):
        fail("BUILD-INFO.json lacks release, workflow or platform provenance")
    full_version = expected_tag.removeprefix("v")
    core_version = tag_match.group(1)
    expected_values = {
        "release.tag": (release.get("tag"), expected_tag),
        "release.version": (release.get("version"), core_version),
        "release.source_revision": (release.get("source_revision"), expected_revision),
        "workflow.name": (workflow.get("name"), "qwc/core-release-candidate"),
        "platform.os": (platform.get("os"), "Linux"),
        "platform.architecture": (platform.get("architecture"), "x86_64"),
    }
    for field, (actual, expected) in expected_values.items():
        if actual != expected:
            fail(f"BUILD-INFO mismatch for {field}: expected {expected!r}, got {actual!r}")
    workflow_revision = workflow.get("revision")
    run_id = str(workflow.get("run_id", ""))
    run_attempt = str(workflow.get("run_attempt", ""))
    if not isinstance(workflow_revision, str) or not REVISION_RE.fullmatch(workflow_revision):
        fail("BUILD-INFO has an invalid workflow revision")
    if not RUN_RE.fullmatch(run_id) or not RUN_RE.fullmatch(run_attempt):
        fail("BUILD-INFO has an invalid workflow run ID or attempt")
    return {
        "archive_sha256": digest,
        "archive_size": archive.stat().st_size,
        "core_version": core_version,
        "full_version": full_version,
        "prerelease": tag_match.group(2) is not None,
        "root_name": root_name,
        "workflow_name": "qwc/core-release-candidate",
        "workflow_revision": workflow_revision,
        "workflow_run_id": run_id,
        "workflow_run_attempt": run_attempt,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--checksums", type=Path, required=True)
    parser.add_argument("--expected-tag", required=True)
    parser.add_argument("--expected-revision", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = inspect(args.archive, args.checksums, args.expected_tag, args.expected_revision)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("Public Linux release archive matches checksums and exact release provenance")


if __name__ == "__main__":
    main()
