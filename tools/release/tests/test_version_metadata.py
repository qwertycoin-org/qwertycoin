#!/usr/bin/env python3
"""Version provenance must match the compiled canonical/compatible formats."""

from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = spec_from_file_location("write_build_info", ROOT / "write_build_info.py")
BUILD_INFO = module_from_spec(SPEC)
SPEC.loader.exec_module(BUILD_INFO)


class VersionMetadataTests(unittest.TestCase):
    def source(self, compatible: str) -> str:
        return (
            '#define DEF_QWERTYCOIN_VERSION "2.0.5"\n'
            '#define DEF_QWERTYCOIN_RELEASE_NAME "QWC v2 Mainnet"\n'
            + compatible + "\n"
        )

    def test_derives_four_component_version_from_canonical_version(self):
        source = self.source(
            '#define DEF_QWERTYCOIN_COMPATIBLE_VERSION DEF_QWERTYCOIN_VERSION ".0"'
        )
        self.assertEqual(
            BUILD_INFO.parse_version_source(source),
            ("2.0.5", "2.0.5.0", "QWC v2 Mainnet"),
        )

    def test_supports_immutable_legacy_release_sources(self):
        source = self.source('#define DEF_MONERO_VERSION "2.0.5.0"')
        self.assertEqual(BUILD_INFO.parse_version_source(source)[1], "2.0.5.0")

    def test_rejects_disagreeing_legacy_version(self):
        source = self.source('#define DEF_MONERO_VERSION "2.0.4.0"')
        with self.assertRaises(SystemExit):
            BUILD_INFO.parse_version_source(source)

    def test_rejects_mixed_version_sources(self):
        source = self.source(
            '#define DEF_QWERTYCOIN_COMPATIBLE_VERSION DEF_QWERTYCOIN_VERSION ".0"\n'
            '#define DEF_MONERO_VERSION "2.0.5.0"'
        )
        with self.assertRaises(SystemExit):
            BUILD_INFO.parse_version_source(source)

    def test_rejects_duplicate_canonical_version(self):
        source = self.source('#define DEF_MONERO_VERSION "2.0.5.0"')
        source += '#define DEF_QWERTYCOIN_VERSION "2.0.6"\n'
        with self.assertRaises(SystemExit):
            BUILD_INFO.parse_version_source(source)

    def test_rejects_missing_or_unsupported_compatible_version(self):
        for definition in (
            "",
            '#define DEF_QWERTYCOIN_COMPATIBLE_VERSION "2.0.5.0"',
            '#define DEF_QWERTYCOIN_COMPATIBLE_VERSION DEF_QWERTYCOIN_VERSION ".0.1"',
        ):
            with self.subTest(definition=definition), self.assertRaises(SystemExit):
                BUILD_INFO.parse_version_source(self.source(definition))


if __name__ == "__main__":
    unittest.main()
