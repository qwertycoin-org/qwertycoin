#!/usr/bin/env python3
"""Create a minimal Docker build context from a verified Linux release package."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat


PROGRAMS = (
    "qwertycoind",
    "qwertycoin-wallet-cli",
    "qwertycoin-wallet-rpc",
)
DOCUMENTS = ("LICENSE", "THIRD-PARTY-NOTICES.md")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")


def fail(message: str) -> None:
    raise SystemExit(message)


def regular_file(path: Path, *, executable: bool = False) -> os.stat_result:
    try:
        metadata = path.lstat()
    except OSError as exc:
        fail(f"cannot inspect required file {path}: {exc}")
    if not stat.S_ISREG(metadata.st_mode) or path.is_symlink():
        fail(f"required path is not a regular non-symlink file: {path}")
    if executable and metadata.st_mode & 0o111 == 0:
        fail(f"required program is not executable: {path}")
    return metadata


def validated_library_files(root: Path) -> list[Path]:
    if not root.is_dir() or root.is_symlink():
        fail("release package has no regular lib directory")
    files: list[Path] = []
    casefolded: set[str] = set()
    for path in sorted(root.rglob("*")):
        relative = path.relative_to(root)
        normalized = PurePosixPath(relative.as_posix())
        if normalized.is_absolute() or ".." in normalized.parts:
            fail(f"unsafe library path: {relative}")
        folded = normalized.as_posix().casefold()
        if folded in casefolded:
            fail(f"case-insensitive library path collision: {relative}")
        casefolded.add(folded)
        metadata = path.lstat()
        if stat.S_ISDIR(metadata.st_mode):
            if path.is_symlink():
                fail(f"library directory is a symlink: {relative}")
            continue
        if not stat.S_ISREG(metadata.st_mode) or path.is_symlink():
            fail(f"library payload is not a regular non-symlink file: {relative}")
        files.append(path)
    if not files:
        fail("release package lib directory is empty")
    return files


def ensure_empty_directory(path: Path) -> None:
    if path.exists():
        if not path.is_dir() or path.is_symlink():
            fail("output path is not a regular directory")
        if any(path.iterdir()):
            fail("output directory must be empty")
    else:
        path.mkdir(parents=True, mode=0o755)


def copy_regular(source: Path, destination: Path, *, executable: bool = False) -> None:
    regular_file(source, executable=executable)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination, follow_symlinks=False)
    destination.chmod(0o755 if executable else 0o644)


def digest(path: Path) -> str:
    hasher = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def prepare(package_root: Path, dockerfile: Path, entrypoint: Path, output: Path) -> dict[str, object]:
    if package_root.is_symlink():
        fail("package root must not be a symlink")
    package_root = package_root.resolve(strict=True)
    if not package_root.is_dir():
        fail("package root is not a regular directory")
    ensure_empty_directory(output)
    release_output = output / "release"
    release_output.mkdir(mode=0o755)

    for name in PROGRAMS:
        copy_regular(package_root / name, release_output / name, executable=True)
    for name in DOCUMENTS:
        copy_regular(package_root / name, release_output / name)
    for source in validated_library_files(package_root / "lib"):
        relative = source.relative_to(package_root / "lib")
        copy_regular(source, release_output / "lib" / relative)
    copy_regular(dockerfile, output / "Dockerfile")
    copy_regular(entrypoint, output / "entrypoint.sh", executable=True)

    payload = []
    for path in sorted(value for value in output.rglob("*") if value.is_file()):
        relative = path.relative_to(output).as_posix()
        payload.append({"path": relative, "sha256": digest(path), "size": path.stat().st_size})
    manifest = {
        "schema_version": 1,
        "programs": list(PROGRAMS),
        "files": payload,
    }
    manifest_path = output / "CONTEXT-MANIFEST.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    manifest_path.chmod(0o644)
    return manifest


def verify_context(root: Path) -> int:
    if root.is_symlink():
        fail("Docker context root must not be a symlink")
    root = root.resolve(strict=True)
    manifest_path = root / "CONTEXT-MANIFEST.json"
    regular_file(manifest_path)
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"invalid Docker context manifest: {exc}")
    if not isinstance(manifest, dict) or manifest.get("schema_version") != 1:
        fail("unsupported Docker context manifest")
    entries = manifest.get("files")
    if not isinstance(entries, list):
        fail("Docker context manifest has no file list")
    if manifest.get("programs") != list(PROGRAMS):
        fail("Docker context manifest has an unexpected program set")
    expected: dict[str, tuple[str, int]] = {}
    for entry in entries:
        if not isinstance(entry, dict):
            fail("invalid Docker context manifest entry")
        relative = entry.get("path")
        file_digest = entry.get("sha256")
        size = entry.get("size")
        if (
            not isinstance(relative, str)
            or PurePosixPath(relative).is_absolute()
            or ".." in PurePosixPath(relative).parts
            or not isinstance(file_digest, str)
            or not SHA256_RE.fullmatch(file_digest)
            or not isinstance(size, int)
            or size < 0
            or relative in expected
        ):
            fail("unsafe Docker context manifest entry")
        expected[relative] = (file_digest, size)
    allowed_directories = {"."}
    for relative in expected:
        parent = PurePosixPath(relative).parent
        while parent.as_posix() != ".":
            allowed_directories.add(parent.as_posix())
            parent = parent.parent
    actual: set[str] = set()
    for path in root.rglob("*"):
        relative = path.relative_to(root).as_posix()
        metadata = path.lstat()
        if stat.S_ISLNK(metadata.st_mode):
            fail(f"Docker context contains a symlink: {relative}")
        if stat.S_ISDIR(metadata.st_mode):
            if relative not in allowed_directories:
                fail(f"Docker context contains an unexpected directory: {relative}")
            continue
        if not stat.S_ISREG(metadata.st_mode):
            fail(f"Docker context contains a special file: {relative}")
        if path != manifest_path:
            actual.add(relative)
    if actual != set(expected):
        fail("Docker context manifest coverage mismatch")
    for relative, (file_digest, size) in expected.items():
        path = root / relative
        regular_file(path)
        if path.stat().st_size != size or digest(path) != file_digest:
            fail(f"Docker context file mismatch: {relative}")
    return len(expected)


def main() -> None:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    prepare_parser = subparsers.add_parser("prepare")
    prepare_parser.add_argument("--package-root", type=Path, required=True)
    prepare_parser.add_argument("--dockerfile", type=Path, required=True)
    prepare_parser.add_argument("--entrypoint", type=Path, required=True)
    prepare_parser.add_argument("--output", type=Path, required=True)
    verify_parser = subparsers.add_parser("verify")
    verify_parser.add_argument("--context", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "prepare":
        manifest = prepare(args.package_root, args.dockerfile, args.entrypoint, args.output)
        print(f"Prepared Docker context with {len(manifest['files'])} verified files")
    else:
        count = verify_context(args.context)
        print(f"Verified Docker context with {count} exact files")


if __name__ == "__main__":
    main()
