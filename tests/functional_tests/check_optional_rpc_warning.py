import re
import sys
from pathlib import Path


lines = Path(sys.argv[1]).read_text().splitlines()
warning_start = re.compile(r"CMake Warning at tests/functional_tests/CMakeLists\.txt:\d+ \(message\):")
required = (
    "functional_tests_rpc and check_missing_rpc_methods skipped;",
    "chainlock_daemon_methods also skipped",
)
for index, line in enumerate(lines):
    if warning_start.fullmatch(line):
        warning_lines = []
        for warning_line in lines[index + 1:]:
            if not warning_line:
                break
            warning_lines.append(warning_line.strip())
        warning = " ".join(warning_lines)
        if all(phrase in warning for phrase in required):
            sys.exit(0)
sys.exit(1)
