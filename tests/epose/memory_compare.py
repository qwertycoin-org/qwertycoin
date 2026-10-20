#!/usr/bin/env python3
"""Compare two independently built EPoSE probes; POSIX, no external packages."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time


def measure(binary, epochs):
    # wait4 gives this child's peak RSS, not the cumulative maximum of all
    # children. Use files rather than pipes so verbose failures cannot deadlock.
    with tempfile.TemporaryFile(mode="w+") as stdout, tempfile.TemporaryFile(mode="w+") as stderr:
        started = time.monotonic()
        with subprocess.Popen([str(binary.resolve()), str(epochs)], stdout=stdout, stderr=stderr) as child:
            _, status, usage = os.wait4(child.pid, 0)
            child.returncode = os.waitstatus_to_exitcode(status)
            if child.returncode:
                stderr.seek(0)
                raise RuntimeError(f"{binary}: exit {child.returncode}: {stderr.read()[-4000:]}")
        elapsed = time.monotonic() - started
        stdout.seek(0)
        rows = [json.loads(line) for line in stdout if line.startswith("{")]
    expected_epochs = list(range(1, epochs + 1))
    if [row["epoch"] for row in rows if "epoch" in row] != expected_epochs:
        raise RuntimeError(f"{binary}: incomplete epoch results")
    final = rows[-1]
    if final.get("blocks") != (epochs + 1) * 18 - 3 or final.get("rollback_blocks") != 36:
        raise RuntimeError(f"{binary}: incomplete replay/rollback")
    if len(final.get("state_hashes", [])) != final["blocks"]:
        raise RuntimeError(f"{binary}: incomplete per-block state hashes")
    return {
        "peak_rss_kib": usage.ru_maxrss // 1024 if sys.platform == "darwin" else usage.ru_maxrss,
        "elapsed_seconds": round(elapsed, 3),
        "final": final,
        "epochs": [row for row in rows if "epoch" in row],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--optimized", type=Path, required=True)
    parser.add_argument("--epochs", type=int, default=32, choices=range(2, 1001), metavar="2..1000")
    args = parser.parse_args()
    reference = measure(args.reference, args.epochs)
    optimized = measure(args.optimized, args.epochs)
    if reference["final"] != optimized["final"]:
        raise RuntimeError("canonical state-hash traces or rollback results differ")
    if [(r["epoch"], r["receipts"]) for r in reference["epochs"]] != [
        (r["epoch"], r["receipts"]) for r in optimized["epochs"]
    ]:
        raise RuntimeError("canonical receipt histories differ")
    print(json.dumps({"reference": reference, "optimized": optimized, "equivalent": True}, indent=2))


if __name__ == "__main__":
    main()
