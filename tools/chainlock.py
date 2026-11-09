#!/usr/bin/env python3

import argparse
import json
import sys

from chainlock_quorum import check_lock_epoch, check_locked_header, derive_quorum, fetch_quorum_inputs, lock_epoch, threshold, verify_lock_signatures


def main():
    parser = argparse.ArgumentParser(description="Fetch a ChainLock and a VM state proof")
    parser.add_argument("--daemon-rpc", default="http://127.0.0.1:18197")
    parser.add_argument("--height", type=int, default=0)
    parser.add_argument("--key", help="Hex-encoded VM state key")
    parser.add_argument("--vector", help="Check a saved JSON test vector")
    parser.add_argument("--parameters", help="Local JSON file of pinned network parameters")
    args = parser.parse_args()
    if args.height < 0 or args.height > (1 << 64) - 1:
        parser.error("height must be a uint64")
    if not args.parameters and not args.vector:
        parser.error("--parameters is required for live verification")
    if args.vector:
        try:
            with open(args.vector, encoding="utf-8") as source:
                vector = json.load(source)
            if args.parameters:
                with open(args.parameters, encoding="utf-8") as source:
                    pinned = json.load(source)
            else:
                pinned = {key: vector["get_chainlock_quorum_inputs"][key] for key in
                          ("activation_height", "epoch_length", "anchor_depth", "network_id", "genesis_hash", "parameter_set_hash")}
            check_lock_epoch(vector["get_chainlock"], vector["get_chainlock_quorum_inputs"], pinned)
            check_locked_header(vector["get_chainlock"])
            quorum = derive_quorum(vector["get_chainlock_quorum_inputs"], pinned)
            if quorum != vector["get_chainlock"]["quorum"]:
                raise ValueError("derived quorum differs from vector consensus quorum")
            verify_lock_signatures(vector["get_chainlock"]["lock_blob"], quorum, pinned["network_id"],
                                   pinned["genesis_hash"], pinned["parameter_set_hash"])
            print(json.dumps({"quorum": quorum, "threshold": threshold(len(quorum))}, sort_keys=True))
            return 0
        except (KeyError, TypeError, ValueError, OSError) as error:
            print(str(error), file=sys.stderr)
            return 1
    try:
        from contracts import RpcError, rpc
    except ImportError as error:
        print(str(error), file=sys.stderr)
        return 1
    try:
        with open(args.parameters, encoding="utf-8") as source:
            pinned = json.load(source)
        lock = rpc(args.daemon_rpc, "get_chainlock", {"height": args.height})
        expected_epoch = lock_epoch(lock, pinned["epoch_length"])
        inputs = fetch_quorum_inputs(rpc, args.daemon_rpc, expected_epoch, pinned)
        check_lock_epoch(lock, inputs, pinned)
        check_locked_header(lock)
        if inputs["test_override"]:
            raise ValueError("test quorum override is not valid for a live client")
        quorum = derive_quorum(inputs, pinned)
        if quorum != lock["quorum"] or threshold(len(quorum)) != lock["threshold"]:
            raise ValueError("derived quorum differs from node consensus quorum")
        verify_lock_signatures(lock["lock_blob"], quorum, pinned["network_id"],
                               pinned["genesis_hash"], pinned["parameter_set_hash"])
        result = {"get_chainlock": lock, "get_chainlock_quorum_inputs": inputs}
        if args.key is not None:
            proof = rpc(args.daemon_rpc, "get_vm_state_proof", {"height": lock["height"], "key": args.key})
            if proof["root"] != lock["vm_state_root"]:
                raise RpcError("VM proof root differs from the locked block root")
            result["get_vm_state_proof"] = proof
        print(json.dumps(result, sort_keys=True))
    except (RpcError, KeyError, TypeError, ValueError, OSError) as error:
        print(str(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
