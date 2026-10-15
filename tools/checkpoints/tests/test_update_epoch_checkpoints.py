#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import io
import pathlib
import sys
import tempfile
import unittest
import urllib.error
from unittest import mock


MODULE_PATH = pathlib.Path(__file__).resolve().parents[1] / "update_epoch_checkpoints.py"
SPEC = importlib.util.spec_from_file_location("update_epoch_checkpoints", MODULE_PATH)
snapshots = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
sys.modules[SPEC.name] = snapshots
SPEC.loader.exec_module(snapshots)


HASH_GENESIS = "0" * 64
HASH_29 = "a" * 64
HASH_ACTIVATION = "b" * 64
HASH_30 = "c" * 64


def fixture_tree(root: pathlib.Path) -> None:
    (root / "src/checkpoints").mkdir(parents=True)
    (root / "tests/unit_tests").mkdir(parents=True)
    (root / "docs").mkdir(parents=True)
    (root / "src/checkpoints/checkpoints.cpp").write_text(
        "void checkpoints()\n{\n"
        f'    ADD_CHECKPOINT2(0, "{HASH_GENESIS}", "0x1");\n\n'
        "    // Reset-1 mainnet: completed 720-block EPoSE epoch boundaries.\n"
        f'    ADD_CHECKPOINT2(21599, "{HASH_29}", "0x10");\n\n'
        "    // First block at the EPoSE relay-hardening activation height.\n"
        f'    ADD_CHECKPOINT2(21600, "{HASH_ACTIVATION}", "0x11");\n'
        "    return true;\n}\n"
    )
    (root / "tests/unit_tests/checkpoints.cpp").write_text(
        "constexpr std::array<expected_checkpoint, 3> expected_mainnet_checkpoints{{\n"
        f'    {{0, "{HASH_GENESIS}", "0x1"}},\n'
        f'    {{21599, "{HASH_29}", "0x10"}},\n'
        f'    {{21600, "{HASH_ACTIVATION}", "0x11"}},\n'
        "  }};\n"
    )
    (root / "docs/MAINNET_CHECKPOINTS.md").write_text(
        "| Height | EPoSE boundary | Block hash | Cumulative difficulty |\n"
        "| ---: | ---: | --- | ---: |\n"
        f"| 0 | Genesis | `{HASH_GENESIS}` | `0x1` |\n"
        f"| 21599 | Epoch 29 end | `{HASH_29}` | `0x10` |\n"
        f"| 21600 | Relay-hardening activation | `{HASH_ACTIVATION}` | `0x11` |\n"
    )


class EpochCheckpointTests(unittest.TestCase):
    def test_request_retries_transient_transport_failure(self):
        response = io.BytesIO(b'{"status":"success"}')
        with mock.patch.object(
            snapshots.urllib.request,
            "urlopen",
            side_effect=[urllib.error.URLError("temporary"), response],
        ) as urlopen, mock.patch.object(snapshots.time, "sleep") as sleep:
            self.assertEqual(
                snapshots.request_json("https://example.invalid/status", timeout=1),
                {"status": "success"},
            )
        self.assertEqual(urlopen.call_count, 2)
        sleep.assert_called_once_with(1)

    def test_latest_mature_epoch_boundary(self):
        self.assertEqual(snapshots.latest_mature_epoch_end(22566, 60), 22319)
        self.assertEqual(snapshots.latest_mature_epoch_end(22379, 60), 22319)
        self.assertEqual(snapshots.latest_mature_epoch_end(22378, 60), 21599)
        self.assertIsNone(snapshots.latest_mature_epoch_end(778, 60))

    def test_collect_requires_exact_node_and_explorer_agreement(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            fixture_tree(root)
            endpoints = ("http://one", "http://two", "http://three")
            tips = {
                endpoint: snapshots.TipObservation(endpoint, 22567, 22566, "d" * 64)
                for endpoint in endpoints
            }
            headers = {
                endpoint: snapshots.HeaderObservation(
                    endpoint, 22319, HASH_30, "0x12", 17
                )
                for endpoint in endpoints
            }
            with mock.patch.object(snapshots, "read_tip", side_effect=lambda endpoint, _: tips[endpoint]), \
                 mock.patch.object(snapshots, "read_header", side_effect=lambda endpoint, _height, _timeout: headers[endpoint]), \
                 mock.patch.object(snapshots, "verify_explorer", return_value=22567):
                observed_tips, candidates = snapshots.collect_candidates(
                    root, endpoints, "https://explorer/{height}", 60, 17, 1
                )
            self.assertEqual(observed_tips, tuple(tips.values()))
            self.assertEqual(len(candidates), 1)
            self.assertEqual(candidates[0].epoch, 30)
            self.assertEqual(candidates[0].minimum_depth, 247)

            headers["http://three"] = snapshots.HeaderObservation(
                "http://three", 22319, "e" * 64, "0x12", 17
            )
            with mock.patch.object(snapshots, "read_tip", side_effect=lambda endpoint, _: tips[endpoint]), \
                 mock.patch.object(snapshots, "read_header", side_effect=lambda endpoint, _height, _timeout: headers[endpoint]), \
                 self.assertRaisesRegex(snapshots.SnapshotError, "disagree"):
                snapshots.collect_candidates(
                    root, endpoints, "https://explorer/{height}", 60, 17, 1
                )

    def test_apply_is_consistent_and_idempotent(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            fixture_tree(root)
            candidate = snapshots.CheckpointCandidate(
                epoch=30,
                height=22319,
                block_hash=HASH_30,
                cumulative_difficulty="0x12",
                major_version=17,
                minimum_depth=247,
                headers=(),
                explorer_current_height=22567,
            )
            snapshots.apply_candidates(root, (candidate,))
            expected = {
                0: (HASH_GENESIS, "0x1"),
                21599: (HASH_29, "0x10"),
                21600: (HASH_ACTIVATION, "0x11"),
                22319: (HASH_30, "0x12"),
            }
            self.assertEqual(snapshots.verify_repository_consistency(root), expected)
            before = {
                path: path.read_text()
                for path in (
                    root / "src/checkpoints/checkpoints.cpp",
                    root / "tests/unit_tests/checkpoints.cpp",
                    root / "docs/MAINNET_CHECKPOINTS.md",
                )
            }
            snapshots.apply_candidates(root, ())
            self.assertEqual({path: path.read_text() for path in before}, before)

    def test_repository_drift_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            fixture_tree(root)
            test_path = root / "tests/unit_tests/checkpoints.cpp"
            test_path.write_text(test_path.read_text().replace("0x11", "0x99"))
            with self.assertRaisesRegex(snapshots.SnapshotError, "tables differ"):
                snapshots.verify_repository_consistency(root)

    def test_explorer_must_use_https_and_match(self):
        with self.assertRaisesRegex(snapshots.SnapshotError, "HTTPS"):
            snapshots.verify_explorer(22319, HASH_30, "http://explorer/{height}", 1)
        response = {
            "status": "success",
            "data": {"block_height": 22319, "hash": HASH_30, "current_height": 22567},
        }
        with mock.patch.object(snapshots, "request_json", return_value=response):
            self.assertEqual(
                snapshots.verify_explorer(22319, HASH_30, "https://explorer/{height}", 1),
                22567,
            )
        response["data"]["hash"] = "f" * 64
        with mock.patch.object(snapshots, "request_json", return_value=response), \
             self.assertRaisesRegex(snapshots.SnapshotError, "disagrees"):
            snapshots.verify_explorer(22319, HASH_30, "https://explorer/{height}", 1)


if __name__ == "__main__":
    unittest.main()
