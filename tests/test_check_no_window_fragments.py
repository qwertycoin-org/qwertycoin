import subprocess
import sys
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

CHECKER = Path(__file__).resolve().parent / "check_no_window_fragments.py"
FRAGMENT_NAME = "orphan_window" + "_v2.h"
BAD_FILES = {
    "fragment_file": (f"src/{FRAGMENT_NAME}", ""),
    "fragment_include": ("src/source.cpp", f'#include "{FRAGMENT_NAME}"\n'),
    "line_directive": ("tests/source.cpp", '#line 12 "source.cpp"\n'),
    "srv_build_path": ("cmake/module.cmake", "set(ROOT /" + "srv/build/hf18)\n"),
    "users_path": ("contrib/note.txt", "/" + "Users/builder/work\n"),
}


def git(root, *arguments):
    subprocess.run(["git", *arguments], cwd=root, check=True, capture_output=True)


def run_checker(root):
    return subprocess.run([sys.executable, str(CHECKER), str(root)],
                          capture_output=True, text=True)


def seeded_repository(directory, name, content):
    root = Path(directory)
    git(root, "init", "-q")
    path = root / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)
    git(root, "add", name)
    return root


class CheckNoWindowFragmentsTest(unittest.TestCase):
    def test_clean_repository_passes(self):
        with TemporaryDirectory() as directory:
            root = seeded_repository(directory, "src/source.cpp", "int main() { return 0; }\n")
            self.assertEqual(0, run_checker(root).returncode)

    def test_bad_files_fail_with_location(self):
        for case, (name, content) in BAD_FILES.items():
            with self.subTest(case=case), TemporaryDirectory() as directory:
                root = seeded_repository(directory, name, content)
                result = run_checker(root)
                self.assertNotEqual(0, result.returncode)
                self.assertIn(f"{name}:", result.stdout)

    def test_untracked_and_unscanned_files_are_ignored(self):
        with TemporaryDirectory() as directory:
            root = seeded_repository(directory, "docs/note.txt", "/srv/build\n")
            (root / "src").mkdir()
            (root / "src" / "untracked.cpp").write_text('#line 1 "x"\n')
            self.assertEqual(0, run_checker(root).returncode)


if __name__ == "__main__":
    unittest.main()
