#!/usr/bin/env python3

import os
import pathlib
import subprocess
import sys
import tempfile

from chainlock_archive_replay import TEST, stage_archive


def check_template_selection_scan():
    source = (
        pathlib.Path(__file__).resolve().parents[2]
        / "src/cryptonote_core/blockchain.cpp"
    ).read_text()
    start = source.index("bool Blockchain::select_template_chainlock(")
    end = source.index("void Blockchain::tombstone_main_chainlocks_from", start)
    if "for_chainlocks_descending" in source[start:end]:
        raise AssertionError("template selection scans historical chainlocks")


def main():
    check_template_selection_scan()
    with tempfile.TemporaryDirectory(prefix="chainlock-archive-perf-") as temporary:
        directory = pathlib.Path(temporary)
        stage_archive(directory)
        environment = os.environ.copy()
        environment["QWC_CHAINLOCK_REPLAY_PERF_DIR"] = str(directory)
        result = subprocess.run(
            [str(TEST), "--gtest_filter=epose_blockchain_recovery.archive_replay_chainlock_timing"],
            env=environment,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=1200,
            check=False,
        )
        print(result.stdout, end="")
        return result.returncode


if __name__ == "__main__":
    sys.exit(main())
