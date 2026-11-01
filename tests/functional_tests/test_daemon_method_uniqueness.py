import ast
import collections
import unittest
from pathlib import Path


class DaemonMethodUniqueness(unittest.TestCase):
    def test_daemon_methods_are_unique(self):
        source = Path(__file__).resolve().parents[2] / "utils/python-rpc/framework/daemon.py"
        module = ast.parse(source.read_text())
        daemon = next(node for node in module.body if isinstance(node, ast.ClassDef) and node.name == "Daemon")
        names = collections.Counter(
            node.name for node in daemon.body if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef))
        )
        self.assertEqual(sorted(name for name, count in names.items() if count > 1), [])


if __name__ == "__main__":
    unittest.main()
