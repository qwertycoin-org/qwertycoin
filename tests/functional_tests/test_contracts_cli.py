import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from tools import contracts


class ContractsCliTest(unittest.TestCase):
    def test_deploy_sends_wasm_to_wallet_rpc(self):
        with tempfile.TemporaryDirectory() as directory:
            wasm = Path(directory) / "contract.wasm"
            wasm.write_bytes(b"\x00asm")
            args = SimpleNamespace(
                command="deploy", daemon_rpc="http://127.0.0.1:18197",
                wallet_rpc="http://127.0.0.1:18083", address="devnet-address",
                amount=100000000, gas_price=1, gas_limit=0, args="", wasm=str(wasm),
            )
            with patch.object(contracts, "rpc", side_effect=[
                {"active": True, "root": "root"},
                {"tx_hash": "tx", "contract_id": "contract", "gas_limit": 200, "gas_used": 100},
            ]) as call:
                result = contracts.run(args)
        self.assertEqual("contract", result["contract_id"])
        self.assertEqual("0061736d", call.call_args_list[1].args[2]["code"])
        self.assertEqual("contract_call", call.call_args_list[1].args[1])

    def test_invoke_sends_method_and_contract_id(self):
        args = SimpleNamespace(
            command="invoke", daemon_rpc="http://127.0.0.1:18197",
            wallet_rpc="http://127.0.0.1:18083", address="devnet-address",
            amount=100000000, gas_price=1, gas_limit=0, args="ab",
            contract_id="contract", method=2,
        )
        with patch.object(contracts, "rpc", side_effect=[
            {"active": True, "root": "root"},
            {"tx_hash": "tx", "contract_id": "contract", "gas_limit": 200, "gas_used": 100},
        ]) as call:
            contracts.run(args)
        params = call.call_args_list[1].args[2]
        self.assertEqual(("invoke", "contract", 2, "ab"),
                         (params["kind"], params["contract_id"], params["method"], params["args"]))

    def test_query_rejects_changed_root(self):
        args = SimpleNamespace(command="query", daemon_rpc="http://127.0.0.1:18197", key="ab")
        with patch.object(contracts, "rpc", side_effect=[
            {"active": True, "root": "old"},
            {"height": 60, "root": "new", "found": True, "value": "00", "steps": []},
        ]):
            with self.assertRaisesRegex(contracts.RpcError, "state root changed"):
                contracts.run(args)


if __name__ == "__main__":
    unittest.main()
