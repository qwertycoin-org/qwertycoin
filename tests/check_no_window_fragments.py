#!/usr/bin/env python3
import re
import subprocess
import sys
from pathlib import Path

SCANNED_DIRECTORIES = ("src", "tests", "contrib", "cmake", "utils")
FRAGMENT_SUFFIX = "_window" + "_v2.h"
FRAGMENT_INCLUDE = re.compile(
    rb'^[ \t]*#[ \t]*include[ \t]*[<"][^>"\r\n]*' + re.escape(FRAGMENT_SUFFIX.encode()) + rb'[>"]')
LINE_DIRECTIVE = re.compile(rb"^[ \t]*#[ \t]*line\b")
ABSOLUTE_PATH = re.compile(rb"(?<![\w.:/~$-])/(?:srv/build/|Users/)")


def tracked_files(root):
    listing = subprocess.check_output(
        ["git", "ls-files", "-z", "--", *SCANNED_DIRECTORIES], cwd=root)
    return [Path(name.decode()) for name in listing.split(b"\0") if name]


def find_violations(root):
    violations = []
    for name in tracked_files(root):
        if name.name.endswith(FRAGMENT_SUFFIX):
            violations.append(f"{name}:1: window fragment file")
        path = root / name
        if not path.is_file():
            continue
        for number, line in enumerate(path.read_bytes().split(b"\n"), 1):
            if FRAGMENT_INCLUDE.match(line):
                violations.append(f"{name}:{number}: window fragment include")
            if LINE_DIRECTIVE.match(line):
                violations.append(f"{name}:{number}: #line directive")
            if ABSOLUTE_PATH.search(line):
                violations.append(f"{name}:{number}: absolute build path")
    return violations


if __name__ == "__main__":
    found = find_violations(Path(sys.argv[1]) if len(sys.argv) > 1 else Path.cwd())
    for violation in found:
        print(violation)
    sys.exit(1 if found else 0)
