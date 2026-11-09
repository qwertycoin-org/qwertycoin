import subprocess
import sys
import unittest
from pathlib import Path


class RPCCheckerUsage(unittest.TestCase):
    def test_usage_names_the_script(self):
        checker = Path(__file__).with_name('check_missing_rpc_methods.py')
        result = subprocess.run([sys.executable, str(checker)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout.strip(), 'usage: check_missing_rpc_methods.py <rootdir>')


if __name__ == '__main__':
    unittest.main()
