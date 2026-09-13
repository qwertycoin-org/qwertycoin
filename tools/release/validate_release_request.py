#!/usr/bin/env python3
"""Validate immutable Core release inputs against the selected source tree."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re


SHA_RE = re.compile(r"^[0-9a-f]{40}$")
TAG_RE = re.compile(r"^v([0-9]+)\.([0-9]+)\.([0-9]+)(?:-rc([1-9][0-9]*))?$")
VERSION_RE = re.compile(r'^#define DEF_QWERTYCOIN_VERSION "([0-9]+)\.([0-9]+)\.([0-9]+)"$', re.MULTILINE)


def fail(message: str) -> None:
    raise SystemExit(message)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--release-tag", required=True)
    parser.add_argument("--expected-revision", required=True)
    parser.add_argument("--target", choices=("all", "linux", "macos", "windows"), required=True)
    parser.add_argument("--version-file", type=Path, default=Path("src/version.cpp.in"))
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    if not SHA_RE.fullmatch(args.expected_revision):
        fail("expected revision must be a lowercase 40-character commit SHA")
    tag_match = TAG_RE.fullmatch(args.release_tag)
    if not tag_match:
        fail("release tag must be vMAJOR.MINOR.PATCH or vMAJOR.MINOR.PATCH-rcN")
    try:
        source = args.version_file.read_text(encoding="utf-8")
    except OSError as exc:
        fail(f"cannot read Core version file: {exc}")
    version_match = VERSION_RE.search(source)
    if not version_match:
        fail("cannot locate DEF_QWERTYCOIN_VERSION in src/version.cpp.in")
    version = ".".join(version_match.groups())
    tag_version = ".".join(tag_match.groups()[:3])
    if tag_version != version:
        fail(f"release tag version {tag_version} does not match Core version {version}")

    result = {
        "expected_revision": args.expected_revision,
        "release_tag": args.release_tag,
        "release_version": version,
        "target": args.target,
        "tag_kind": "release-candidate" if tag_match.group(4) else "stable",
    }
    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")


if __name__ == "__main__":
    main()
