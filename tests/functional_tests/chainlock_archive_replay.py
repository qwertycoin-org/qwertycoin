#!/usr/bin/env python3

import os
import pathlib
import shutil
import subprocess
import sys
import tarfile
import tempfile


REPO = pathlib.Path(__file__).resolve().parents[2]
TEST = pathlib.Path(os.environ.get(
    "QWC_CHAINLOCK_REPLAY_TEST_BINARY",
    REPO / "build/release/tests/unit_tests/epose_unit_tests"))


def stage_archive(directory):
    archive = os.environ.get("QWC_CHAINLOCK_REPLAY_ARCHIVE")
    if not archive or not pathlib.Path(archive).is_file():
        raise SystemExit("QWC_CHAINLOCK_REPLAY_ARCHIVE must name an existing archive file")
    lmdb = directory / "devnet" / "lmdb"
    lmdb.mkdir(parents=True)
    with tarfile.open(archive, "r:gz") as source:
        for name in ("lmdb/data.mdb", "lmdb/lock.mdb"):
            with source.extractfile(name) as input_file:
                with (lmdb / pathlib.Path(name).name).open("wb") as output_file:
                    shutil.copyfileobj(input_file, output_file)


def main():
    with tempfile.TemporaryDirectory(prefix="chainlock-archive-replay-") as temporary:
        root = pathlib.Path(temporary)
        negative = root / "negative"
        stage_archive(negative)
        environment = os.environ.copy()
        environment["QWC_CHAINLOCK_REPLAY_NEGATIVE_DIR"] = str(negative)
        result = subprocess.run(
            [str(TEST), "--gtest_filter=epose_blockchain_recovery.archive_replay_blocks_only"],
            env=environment,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=1200,
            check=False,
        )
        print(result.stdout, end="")
        print(f"archive_replay_exit={result.returncode}")
        return result.returncode


if __name__ == "__main__":
    sys.exit(main())
