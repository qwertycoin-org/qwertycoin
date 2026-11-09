#!/usr/bin/env python3

import argparse
import json
import sys
from pathlib import Path

import requests


class RpcError(Exception):
    pass


def rpc(url, method, params):
    try:
        response = requests.post(
            url.rstrip("/") + "/json_rpc",
            json={"jsonrpc": "2.0", "id": "contracts", "method": method, "params": params},
            timeout=30,
        )
        response.raise_for_status()
        data = response.json()
    except (requests.RequestException, ValueError) as error:
        raise RpcError(f"{method}: {error}") from error
    if "error" in data:
        error = data["error"]
        raise RpcError(f"{method}: {error.get('code')}: {error.get('message')}")
    result = data.get("result")
    if not isinstance(result, dict):
        raise RpcError(f"{method}: missing result")
    if result.get("status", "OK") != "OK":
        raise RpcError(f"{method}: {result['status']}")
    return result


def run(args):
    if args.command == "derive":
        return rpc(args.wallet_rpc, "derive_contract_key", {
            "contract_id": args.contract_id, "key_id": args.key_id,
        })

    root = rpc(args.daemon_rpc, "get_vm_state_root", {})
    if not root.get("active"):
        raise RpcError("contract VM is inactive at the daemon tip")

    if args.command == "query":
        proof = rpc(args.daemon_rpc, "get_vm_state_proof", {"key": args.key})
        if proof.get("root") != root.get("root"):
            raise RpcError("state root changed during query; retry")
        return {"height": proof["height"], "root": proof["root"],
                "found": proof["found"], "value": proof["value"], "steps": proof["steps"]}

    params = {
        "kind": args.command,
        "destinations": [{"address": args.address, "amount": args.amount}],
        "gas_price": args.gas_price,
        "gas_limit": args.gas_limit,
        "args": args.args,
    }
    if args.command == "deploy":
        params["code"] = Path(args.wasm).read_bytes().hex()
    else:
        params["contract_id"] = args.contract_id
        params["method"] = args.method
    if args.signer:
        params["signers"] = args.signer
    if args.key_id:
        params["signer_key_ids"] = args.key_id
    result = rpc(args.wallet_rpc, "contract_call", params)
    return {"tx_hash": result["tx_hash"], "contract_id": result["contract_id"],
            "gas_limit": result["gas_limit"], "gas_used": result["gas_used"]}


def main():
    parser = argparse.ArgumentParser(description="Deploy, invoke, and query contracts on a devnet")
    parser.add_argument("--daemon-rpc", default="http://127.0.0.1:18197")
    parser.add_argument("--wallet-rpc", default="http://127.0.0.1:18083")
    commands = parser.add_subparsers(dest="command", required=True)
    for command in ("deploy", "invoke"):
        sub = commands.add_parser(command)
        sub.add_argument("--address", required=True)
        sub.add_argument("--amount", type=int, default=100000000)
        sub.add_argument("--gas-price", type=int, default=1)
        sub.add_argument("--gas-limit", type=int, default=0)
        sub.add_argument("--args", default="", help="Hex encoded contract arguments")
        sub.add_argument("--signer", action="append", default=[], help="Public key requested by the app shader, in order")
        sub.add_argument("--key-id", action="append", default=[], help="32-byte key id for each requested signer, in order")
        if command == "deploy":
            sub.add_argument("wasm")
        else:
            sub.add_argument("contract_id")
            sub.add_argument("--method", type=int, default=2)
    commands.add_parser("query").add_argument("key", help="Hex encoded state key")
    derive = commands.add_parser("derive")
    derive.add_argument("contract_id")
    derive.add_argument("key_id")
    args = parser.parse_args()
    try:
        print(json.dumps(run(args), sort_keys=True))
    except (RpcError, OSError, KeyError) as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
