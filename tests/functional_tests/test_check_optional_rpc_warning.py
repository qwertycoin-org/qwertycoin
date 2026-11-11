import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


CHECKER = Path(__file__).with_name("check_optional_rpc_warning.py")
WARNING = (
    "CMake Warning at tests/functional_tests/CMakeLists.txt:85 (message):\n"
    "  functional_tests_rpc and check_missing_rpc_methods skipped;\n"
    "  needs optional modules\n\n"
)


class OptionalRpcWarningTest(unittest.TestCase):
    def check_warning(self, warning, expected_status):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "warning.log"
            log.write_text(warning)
            status = subprocess.run(
                [sys.executable, str(CHECKER), str(log)], check=False
            ).returncode
            self.assertEqual(status, expected_status)

    def test_complete_warning(self):
        self.check_warning(WARNING, 0)

    def test_unrelated_warning_before_expected_warning(self):
        unrelated = (
            "CMake Warning at tests/functional_tests/CMakeLists.txt:42 (message):\n"
            "  unrelated warning\n\n"
        )
        self.check_warning(unrelated + WARNING, 0)

    def test_missing_rpc_names(self):
        self.check_warning(
            WARNING.replace("functional_tests_rpc and check_missing_rpc_methods skipped;", "optional tests skipped;"),
            1,
        )


if __name__ == "__main__":
    unittest.main()
