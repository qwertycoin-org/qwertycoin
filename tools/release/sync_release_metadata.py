#!/usr/bin/env python3
"""Generate or verify human-readable release metadata from one canonical file."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
METADATA = ROOT / "docs" / "releases" / "current.json"
README = ROOT / "README.md"
VERSION_SOURCE = ROOT / "src" / "version.cpp.in"
START = "<!-- release-metadata:start -->"
END = "<!-- release-metadata:end -->"


def load() -> dict[str, str]:
    data = json.loads(METADATA.read_text())
    required = {"release_tag", "source_revision", "version"}
    if set(data) != required:
        raise SystemExit(f"{METADATA}: expected exactly {sorted(required)}")
    if data["release_tag"] != f"v{data['version']}":
        raise SystemExit("release_tag must equal v + version")
    if not re.fullmatch(r"[0-9a-f]{40}", data["source_revision"]):
        raise SystemExit("source_revision must be a full lowercase Git commit")
    return data


def block(data: dict[str, str]) -> str:
    tag = data["release_tag"]
    revision = data["source_revision"]
    return f"""<!-- release-metadata:start -->
The current published Core release is
[`{tag}`](https://github.com/qwertycoin-org/qwertycoin/releases/tag/{tag}),
built from source revision
[`{revision}`](https://github.com/qwertycoin-org/qwertycoin/commit/{revision}).
Release artifacts are available for Linux x86_64, macOS Apple Silicon, and
Windows x86_64.
<!-- release-metadata:end -->"""


def updated_readme(data: dict[str, str]) -> str:
    text = README.read_text()
    pattern = re.compile(
        rf"{re.escape(START)}.*?{re.escape(END)}",
        re.DOTALL,
    )
    if len(pattern.findall(text)) != 1:
        raise SystemExit("README release metadata markers must occur exactly once")
    return pattern.sub(block(data), text)


def verify_version(data: dict[str, str]) -> None:
    text = VERSION_SOURCE.read_text()
    matches = re.findall(
        r'^#define DEF_QWERTYCOIN_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"$',
        text,
        re.MULTILINE,
    )
    if len(matches) != 1:
        raise SystemExit(
            f"{VERSION_SOURCE}: expected exactly one canonical product version"
        )
    source_version = tuple(int(part) for part in matches[0].split("."))
    published_version = tuple(int(part) for part in data["version"].split("."))
    if source_version < published_version:
        raise SystemExit(
            f"{VERSION_SOURCE}: source version {matches[0]} is older than "
            f"published version {data['version']}"
        )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()
    data = load()
    verify_version(data)
    expected = updated_readme(data)
    if args.write:
        README.write_text(expected)
    elif README.read_text() != expected:
        raise SystemExit("README release metadata is stale; run this tool with --write")
    print(f"release metadata verified: {data['release_tag']} {data['source_revision']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
