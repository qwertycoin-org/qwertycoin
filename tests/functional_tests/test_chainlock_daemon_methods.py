import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'utils' / 'python-rpc'))

from framework.daemon import Daemon


class RecordingRPC:
    def __init__(self):
        self.requests = []
        self.response = object()

    def send_request(self, path, arguments):
        self.requests.append((path, arguments))
        return self.response


class ChainlockDaemonMethods(unittest.TestCase):
    def setUp(self):
        self.daemon = Daemon()
        self.daemon.rpc = RecordingRPC()

    def test_get_chainlock(self):
        self.assertIs(self.daemon.get_chainlock(), self.daemon.rpc.response)
        self.assertIs(self.daemon.get_chainlock(42, client='review'), self.daemon.rpc.response)
        self.assertEqual(self.daemon.rpc.requests, [
            ('/get_chainlock', {'height': 0, 'client': ''}),
            ('/get_chainlock', {'height': 42, 'client': 'review'}),
        ])

    def test_get_chainlock_quorum_inputs(self):
        self.assertIs(self.daemon.get_chainlock_quorum_inputs(7), self.daemon.rpc.response)
        self.assertIs(self.daemon.get_chainlock_quorum_inputs(7, 3, 4, 'review'), self.daemon.rpc.response)
        self.assertIs(self.daemon.get_chainlock_quorum_inputs(), self.daemon.rpc.response)
        self.assertEqual(self.daemon.rpc.requests, [
            ('/get_chainlock_quorum_inputs', {'epoch': 7, 'offset': 0, 'limit': 128, 'client': ''}),
            ('/get_chainlock_quorum_inputs', {'epoch': 7, 'offset': 3, 'limit': 4, 'client': 'review'}),
            ('/get_chainlock_quorum_inputs', {'epoch': (1 << 64) - 1, 'offset': 0, 'limit': 128, 'client': ''}),
        ])


if __name__ == '__main__':
    unittest.main()
