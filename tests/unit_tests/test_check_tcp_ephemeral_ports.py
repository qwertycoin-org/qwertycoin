import errno
import sys
import unittest
from types import SimpleNamespace
from unittest.mock import patch

import check_tcp_ephemeral_ports


class OccupiedPortTest(unittest.TestCase):
    def test_bound_port_without_listener_runs_unit_tests(self):
        class Socket:
            def bind(self, address):
                if address[1] == 5262:
                    raise OSError(errno.EADDRINUSE, "port occupied")

            def listen(self, backlog):
                pass

            def connect_ex(self, address):
                return errno.ECONNREFUSED

            def close(self):
                pass

            def __enter__(self):
                return self

            def __exit__(self, exc_type, exc_value, traceback):
                self.close()

        completed = SimpleNamespace(returncode=0, stdout="[  PASSED  ] 5 tests.\n")
        with patch.object(sys, "argv", ["check_tcp_ephemeral_ports.py", "/tmp/build/release/tests/unit_tests/unit_tests"]), \
                patch.object(check_tcp_ephemeral_ports.socket, "socket", side_effect=Socket), \
                patch.object(check_tcp_ephemeral_ports.subprocess, "run", return_value=completed) as run:
            self.assertEqual(check_tcp_ephemeral_ports.main(), 0)
            run.assert_called_once()


if __name__ == "__main__":
    unittest.main()
