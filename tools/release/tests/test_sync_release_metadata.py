#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock


MODULE_PATH = Path(__file__).resolve().parents[1] / "sync_release_metadata.py"
SPEC = importlib.util.spec_from_file_location("sync_release_metadata", MODULE_PATH)
sync = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(sync)


class ReleaseMetadataTests(unittest.TestCase):
    def fixture(self, metadata: dict[str, str]) -> tuple[tempfile.TemporaryDirectory, Path, Path, Path]:
        temporary = tempfile.TemporaryDirectory()
        root = Path(temporary.name)
        metadata_path = root / "current.json"
        readme = root / "README.md"
        version = root / "version.cpp.in"
        metadata_path.write_text(json.dumps(metadata), encoding="utf-8")
        readme.write_text(
            "before\n<!-- release-metadata:start -->\nstale\n"
            "<!-- release-metadata:end -->\nafter\n",
            encoding="utf-8",
        )
        version.write_text(
            f'#define DEF_QWERTYCOIN_VERSION "{metadata["version"]}"\n',
            encoding="utf-8",
        )
        return temporary, metadata_path, readme, version

    def test_generates_readme_from_canonical_metadata(self) -> None:
        metadata = {
            "release_tag": "v2.0.2",
            "source_revision": "5" * 40,
            "version": "2.0.2",
        }
        temporary, metadata_path, readme, version = self.fixture(metadata)
        self.addCleanup(temporary.cleanup)
        with mock.patch.multiple(
            sync,
            METADATA=metadata_path,
            README=readme,
            VERSION_SOURCE=version,
        ):
            data = sync.load()
            sync.verify_version(data)
            generated = sync.updated_readme(data)
        self.assertIn("[`v2.0.2`]", generated)
        self.assertIn("5" * 40, generated)
        self.assertTrue(generated.startswith("before\n"))
        self.assertTrue(generated.endswith("after\n"))

    def test_rejects_mismatched_tag_and_noncanonical_revision(self) -> None:
        for metadata in (
            {"release_tag": "v2.0.1", "source_revision": "5" * 40, "version": "2.0.2"},
            {"release_tag": "v2.0.2", "source_revision": "ABC", "version": "2.0.2"},
        ):
            temporary, metadata_path, readme, version = self.fixture(metadata)
            with temporary, mock.patch.multiple(
                sync,
                METADATA=metadata_path,
                README=readme,
                VERSION_SOURCE=version,
            ):
                with self.assertRaises(SystemExit):
                    sync.load()

    def test_rejects_version_source_mismatch(self) -> None:
        metadata = {
            "release_tag": "v2.0.2",
            "source_revision": "5" * 40,
            "version": "2.0.2",
        }
        temporary, metadata_path, readme, version = self.fixture(metadata)
        self.addCleanup(temporary.cleanup)
        version.write_text('#define DEF_QWERTYCOIN_VERSION "2.0.1"\n', encoding="utf-8")
        with mock.patch.multiple(
            sync,
            METADATA=metadata_path,
            README=readme,
            VERSION_SOURCE=version,
        ):
            with self.assertRaises(SystemExit):
                sync.verify_version(sync.load())


if __name__ == "__main__":
    unittest.main()
