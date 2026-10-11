"""Check parsed daemon paths and regtest restarts using shared module arguments."""

import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request


# Public test address also used by tests/functional_tests/blockchain.py.
ADDRESS = "QWC1GKRV4Wd3DCnubWiv1CBDBJWTCUcvTAZnE9ioE7pUMiqwXpce7GX5kfyaW4X8V523Rkyoa9NQ6LWj2DrEX7cE2PDLobNPxh"
SUFFIX = {"mainnet": "", "testnet": "testnet", "stagenet": "stagenet", "regtest": "fake"}
HTTP = urllib.request.build_opener(urllib.request.ProxyHandler({}))


def port():
    with socket.socket() as listener:
        listener.bind(("127.0.0.1", 0))
        return listener.getsockname()[1]


def rpc(url, method, params=None):
    request = urllib.request.Request(
        url + "/json_rpc",
        json.dumps({"jsonrpc": "2.0", "id": "0", "method": method, "params": params or {}}).encode(),
        {"Content-Type": "application/json"},
    )
    with HTTP.open(request, timeout=30) as response:
        result = json.load(response)
    assert "error" not in result, result
    assert result["result"].get("status") == "OK", result
    return result["result"]


class Daemon:
    def __init__(self, args, log, unconfigured_network=False):
        self.url = f"http://127.0.0.1:{port()}"
        self.args = [arg for arg in args if not arg.startswith(("--rpc-bind-port=", "--p2p-bind-port="))]
        self.args += [f"--rpc-bind-port={self.url.rsplit(':', 1)[1]}", f"--p2p-bind-port={port()}"]
        self.log = log
        self.unconfigured_network = unconfigured_network
        self.rejected = False

    def __enter__(self):
        self.output = self.log.open("wb")
        self.process = subprocess.Popen(
            self.args, stdout=self.output, stderr=subprocess.STDOUT,
            env=os.environ | {"HOME": str(Path.cwd())},
        )
        try:
            deadline = time.monotonic() + 60
            while time.monotonic() < deadline:
                if self.process.poll() is not None:
                    # Testnet/stagenet currently have intentionally unset public
                    # launch parameters. Core opens LMDB before rejecting them,
                    # so their actual parsed database paths are still testable.
                    self.rejected = self.unconfigured_network and self.process.returncode == 1 and (
                        "launch parameters are incomplete; refusing to initialize a public chain" in self.log.read_text()
                    )
                    assert self.rejected, self.log.read_text()
                    return self
                try:
                    rpc(self.url, "get_info")
                    return self
                except (urllib.error.URLError, TimeoutError, ConnectionError):
                    time.sleep(0.1)
            raise AssertionError(f"Daemon startup timed out: {self.log.read_text()}")
        except BaseException:
            self.__exit__(None, None, None)
            raise

    def __exit__(self, *_):
        if self.process.poll() is None:
            self.process.send_signal(signal.SIGINT)
            try:
                self.process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
                raise AssertionError("Daemon did not shut down cleanly")
        self.output.close()
        assert self.process.returncode == 0 or self.rejected, self.log.read_text()


def check(fixture, root):
    name = fixture["network"] + ("-custom" if fixture["custom"] else "-default")
    base = root / name
    expected = base / SUFFIX[fixture["network"]]
    assert fixture["effectiveDataDir"] == str(Path(fixture["dataDir"]) / SUFFIX[fixture["network"]])
    args = [arg.replace(fixture["dataDir"], str(base)) for arg in fixture["daemonArguments"]]
    # Backends create this directory; use the same working directory at runtime.
    expected.mkdir(parents=True, mode=0o700)
    previous_cwd = Path.cwd()
    os.chdir(expected)
    try:
        with Daemon(args, base / "first-start.log", fixture["network"] in ("testnet", "stagenet")) as daemon:
            assert (expected / "lmdb/data.mdb").is_file(), f"Wrong database path: {list(base.rglob('data.mdb'))}"
            assert list(base.rglob("data.mdb")) == [expected / "lmdb/data.mdb"]
            if fixture["network"] == "regtest":
                assert "--keep-fakechain" in args
                rpc(daemon.url, "generateblocks", {"wallet_address": ADDRESS, "amount_of_blocks": 1})
                before = rpc(daemon.url, "get_info")["height"]
                assert before > 1
        if fixture["network"] == "regtest":
            with Daemon(args, base / "restart.log") as daemon:
                assert rpc(daemon.url, "get_info")["height"] == before, "Regtest state was lost on restart"
            with Daemon([arg for arg in args if arg != "--keep-fakechain"], base / "ephemeral.log") as daemon:
                assert rpc(daemon.url, "get_info")["height"] == 1, "Ephemeral regtest did not reset"
    finally:
        os.chdir(previous_cwd)
    print(f"Passed {name}: database path" + (", persistent and ephemeral restarts" if fixture["network"] == "regtest" else ""), flush=True)


with tempfile.TemporaryDirectory(prefix="qwertycoin-runtime-") as directory:
    for fixture in json.loads(Path(sys.argv[1]).read_text()):
        check(fixture, Path(directory))
