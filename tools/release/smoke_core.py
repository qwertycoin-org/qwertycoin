#!/usr/bin/env python3
"""Run isolated release smokes against the three packaged Core programs."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import secrets
import shutil
import signal
import socket
import subprocess
import tempfile
import time
import urllib.error
import urllib.request


def fail(message: str) -> None:
    raise SystemExit(message)


def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def runtime_environment(root: Path) -> dict[str, str]:
    environment: dict[str, str] = {"LANG": "C", "LC_ALL": "C"}
    if os.name == "nt":
        for name in ("SYSTEMROOT", "WINDIR", "COMSPEC", "PATHEXT", "TEMP", "TMP"):
            if os.environ.get(name):
                environment[name] = os.environ[name]
        system_root = environment.get("SYSTEMROOT", r"C:\Windows")
        environment["PATH"] = os.pathsep.join((str(root), str(Path(system_root) / "System32"), system_root))
    else:
        environment["PATH"] = "/usr/bin:/bin:/usr/sbin:/sbin"
    return environment


def binary(root: Path, name: str) -> Path:
    suffix = ".exe" if os.name == "nt" else ""
    candidate = root / f"{name}{suffix}"
    if not candidate.is_file():
        fail(f"required packaged executable is missing: {candidate.name}")
    return candidate


def evidence_program_name(path: Path) -> str:
    """Return the platform-neutral program key used by release evidence."""
    return path.stem if path.suffix.lower() == ".exe" else path.name


def run_cli(path: Path, argument: str, environment: dict[str, str]) -> str:
    process = subprocess.run(
        [str(path), argument],
        env=environment,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=60,
    )
    if process.returncode != 0:
        fail(f"{path.name} {argument} failed with exit code {process.returncode}")
    if "Qwertycoin" not in process.stdout:
        fail(f"{path.name} {argument} did not identify Qwertycoin")
    return process.stdout


class DigestRpcClient:
    """Use the platform curl client without exposing credentials in argv or logs."""

    def __init__(
        self,
        username: str,
        password: str,
        url: str,
        config_path: Path,
        environment: dict[str, str],
    ) -> None:
        curl = shutil.which("curl", path=environment.get("PATH"))
        if not curl:
            raise RuntimeError("system curl is required for the Digest-authenticated wallet RPC smoke")
        self.curl = curl
        self.config_path = config_path
        self.environment = environment
        config_path.write_text(
            "\n".join(
                (
                    "silent",
                    "show-error",
                    "fail",
                    "digest",
                    f'user = "{username}:{password}"',
                    'request = "POST"',
                    'header = "Content-Type: application/json"',
                    f'url = "{url}"',
                    "data-binary = @-",
                )
            )
            + "\n",
            encoding="utf-8",
        )
        if os.name != "nt":
            config_path.chmod(0o600)

    def request(self, payload: bytes, timeout: int) -> bytes:
        process = subprocess.run(
            [self.curl, "--config", str(self.config_path)],
            input=payload,
            env=self.environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            timeout=timeout,
        )
        if process.returncode != 0:
            raise RuntimeError("Digest-authenticated wallet RPC request failed")
        return process.stdout


def json_request(url: str, method: str, parameters: dict | None = None, opener=None) -> dict:
    payload = json.dumps({"jsonrpc": "2.0", "id": "release-smoke", "method": method, "params": parameters or {}}).encode()
    try:
        if opener:
            response_body = opener.request(payload, timeout=10)
        else:
            request = urllib.request.Request(url, data=payload, headers={"Content-Type": "application/json"})
            with urllib.request.urlopen(request, timeout=10) as response:
                response_body = response.read()
        result = json.loads(response_body.decode("utf-8"))
    except (OSError, subprocess.SubprocessError, urllib.error.URLError, json.JSONDecodeError) as exc:
        raise RuntimeError(f"JSON-RPC {method} failed") from exc
    if "error" in result:
        error = result["error"]
        raise RuntimeError(f"JSON-RPC {method} returned error {error.get('code')}: {error.get('message')}")
    return result.get("result", {})


def wait_for_rpc(url: str, method: str, timeout: int, opener=None) -> dict:
    deadline = time.monotonic() + timeout
    last_error: Exception | None = None
    while time.monotonic() < deadline:
        try:
            return json_request(url, method, opener=opener)
        except Exception as exc:  # readiness loop intentionally retains only the final error type
            last_error = exc
            time.sleep(0.5)
    raise RuntimeError(f"RPC readiness timed out for {method}: {type(last_error).__name__}")


def stop_process(process: subprocess.Popen, timeout: int = 20) -> None:
    if process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=10)


def daemon_stop_request(json_rpc_url: str) -> None:
    endpoint = json_rpc_url.removesuffix("/json_rpc") + "/stop_daemon"
    request = urllib.request.Request(
        endpoint,
        data=b"{}",
        headers={"Content-Type": "application/json"},
    )
    try:
        with urllib.request.urlopen(request, timeout=10) as response:
            result = json.loads(response.read().decode("utf-8"))
    except (OSError, urllib.error.URLError, json.JSONDecodeError) as exc:
        raise RuntimeError("daemon /stop_daemon request failed") from exc
    if result.get("status") != "OK":
        raise RuntimeError("daemon /stop_daemon did not return status OK")


def cleanly_stop_daemon(process: subprocess.Popen, url: str) -> str:
    """Request RPC shutdown, then use the platform console signal if RPC stalls."""
    daemon_stop_request(url)
    try:
        process.wait(timeout=10)
        stop_method = "rpc"
    except subprocess.TimeoutExpired:
        if os.name == "nt":
            process.send_signal(signal.CTRL_BREAK_EVENT)
        else:
            process.terminate()
        try:
            process.wait(timeout=30)
        except subprocess.TimeoutExpired as exc:
            raise RuntimeError("daemon did not stop after RPC and platform shutdown signals") from exc
        stop_method = "rpc-plus-platform-signal"
    if process.returncode != 0:
        raise RuntimeError(f"daemon shutdown returned exit code {process.returncode}")
    return stop_method


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--package-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--expected-version", required=True)
    args = parser.parse_args()

    root = args.package_root.resolve()
    environment = runtime_environment(root)
    daemon = binary(root, "qwertycoind")
    wallet_cli = binary(root, "qwertycoin-wallet-cli")
    wallet_rpc = binary(root, "qwertycoin-wallet-rpc")
    try:
        build_info = json.loads((root / "BUILD-INFO.json").read_text(encoding="utf-8"))
        expected_genesis = build_info["network"]["genesis_hash"]
        expected_reward_bps = int(build_info["network"]["service_reward_basis_points"])
    except (OSError, KeyError, TypeError, ValueError, json.JSONDecodeError) as exc:
        fail(f"cannot read expected network identity from BUILD-INFO.json: {exc}")
    if not isinstance(expected_genesis, str) or len(expected_genesis) != 64:
        fail("BUILD-INFO.json does not contain a valid expected genesis hash")
    results: dict[str, object] = {"schema_version": 1, "programs": {}, "isolated_network": {}}

    for program in (daemon, wallet_cli, wallet_rpc):
        version_output = run_cli(program, "--version", environment)
        run_cli(program, "--help", environment)
        expected_release_marker = f"v{args.expected_version}-release"
        if expected_release_marker not in version_output:
            fail(f"{program.name} does not report the expected tagged release marker {expected_release_marker}")
        results["programs"][evidence_program_name(program)] = {
            "version": "pass",
            "help": "pass",
        }

    with tempfile.TemporaryDirectory(prefix="qwc-core-release-smoke-") as temp_name:
        temp = Path(temp_name)
        daemon_rpc_port = free_port()
        p2p_port = free_port()
        daemon_log = (temp / "daemon.log").open("w", encoding="utf-8")
        daemon_process = subprocess.Popen(
            [
                str(daemon), "--offline",
                "--data-dir", str(temp / "daemon-data"), "--rpc-bind-ip", "127.0.0.1",
                "--rpc-bind-port", str(daemon_rpc_port), "--p2p-bind-ip", "127.0.0.1",
                "--p2p-bind-port", str(p2p_port), "--no-zmq", "--non-interactive",
                "--max-concurrency", "1", "--log-level", "0",
            ],
            env=environment,
            stdin=subprocess.DEVNULL,
            stdout=daemon_log,
            stderr=subprocess.STDOUT,
            creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0,
        )
        daemon_url = f"http://127.0.0.1:{daemon_rpc_port}/json_rpc"
        wallet_process: subprocess.Popen | None = None
        try:
            info = wait_for_rpc(daemon_url, "get_info", 120)
            if not info.get("offline") or info.get("nettype") != "mainnet":
                fail("isolated daemon did not report offline mainnet identity")
            genesis = json_request(daemon_url, "get_block_header_by_height", {"height": 0})
            actual_genesis = genesis.get("block_header", {}).get("hash")
            if actual_genesis != expected_genesis:
                fail("isolated daemon genesis does not match BUILD-INFO.json")
            epose = json_request(daemon_url, "get_epose_info")
            if (
                not epose.get("enabled")
                or int(epose.get("protocol_version", 0)) != 2
                or int(epose.get("service_reward_bps", -1)) != expected_reward_bps
            ):
                fail("isolated daemon did not expose the expected EPoSE v2 RPC baseline")

            wallet_rpc_port = free_port()
            username = "release-smoke"
            rpc_password = secrets.token_urlsafe(32)
            wallet_password = secrets.token_urlsafe(32)
            config = temp / "wallet-rpc.conf"
            config.write_text(
                "\n".join(
                    (
                        f"wallet-dir={temp / 'wallets'}",
                        f"shared-ringdb-dir={temp / 'shared-ringdb'}",
                        "rpc-bind-ip=127.0.0.1",
                        f"rpc-bind-port={wallet_rpc_port}",
                        f"rpc-login={username}:{rpc_password}",
                        f"daemon-address=http://127.0.0.1:{daemon_rpc_port}",
                        "trusted-daemon=1",
                        "no-initial-sync=1",
                        "rpc-ssl=disabled",
                        "daemon-ssl=disabled",
                        f"log-file={temp / 'wallet-rpc.log'}",
                        "log-level=0",
                    )
                ) + "\n",
                encoding="utf-8",
            )
            if os.name != "nt":
                config.chmod(0o600)
            (temp / "wallets").mkdir()
            wallet_process = subprocess.Popen(
                [str(wallet_rpc), "--config-file", str(config)],
                env=environment,
                stdin=subprocess.DEVNULL,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            wallet_url = f"http://127.0.0.1:{wallet_rpc_port}/json_rpc"
            opener = DigestRpcClient(
                username,
                rpc_password,
                wallet_url,
                temp / "wallet-rpc-curl.conf",
                environment,
            )
            wait_for_rpc(wallet_url, "get_version", 90, opener)
            json_request(wallet_url, "create_wallet", {"filename": "created", "password": wallet_password, "language": "English"}, opener)
            created_address = json_request(wallet_url, "get_address", {"account_index": 0, "address_index": [0]}, opener).get("address")
            seed = json_request(wallet_url, "query_key", {"key_type": "mnemonic"}, opener).get("key")
            if not created_address or not seed:
                fail("wallet RPC did not return an address and mnemonic for the disposable wallet")
            json_request(wallet_url, "close_wallet", {"autosave_current": True}, opener)
            restored = json_request(
                wallet_url,
                "restore_deterministic_wallet",
                {
                    "restore_height": 0,
                    "filename": "restored",
                    "seed": seed,
                    "seed_offset": "",
                    "password": wallet_password,
                    "language": "English",
                    "autosave_current": True,
                },
                opener,
            )
            if restored.get("address") != created_address:
                fail("restored disposable wallet address does not match the created wallet")
            json_request(wallet_url, "stop_wallet", opener=opener)
            wallet_process.wait(timeout=30)
            if wallet_process.returncode != 0:
                fail("wallet RPC did not stop cleanly")
            wallet_process = None

            daemon_stop_method = cleanly_stop_daemon(daemon_process, daemon_url)
            results["isolated_network"] = {
                "daemon_rpc_ready": "pass",
                "network": "offline-mainnet",
                "genesis_hash_match": "pass",
                "epose_protocol_version": 2,
                "epose_service_reward_bps": expected_reward_bps,
                "daemon_rpc_stop_ack": "pass",
                "daemon_clean_exit": "pass",
                "daemon_stop_method": daemon_stop_method,
                "wallet_rpc_digest_auth": "pass",
                "wallet_seed_restore_address_match": "pass",
                "secrets_logged": False,
            }
        finally:
            if wallet_process is not None:
                stop_process(wallet_process)
            stop_process(daemon_process)
            daemon_log.close()

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(results, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("Packaged Core smoke passed: CLI version/help, offline daemon EPoSE RPC, authenticated wallet restore")


if __name__ == "__main__":
    main()
