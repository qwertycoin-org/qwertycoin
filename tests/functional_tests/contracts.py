# Copyright (c) 2026, Qwertycoin
#
# SPDX-License-Identifier: BSD-3-Clause

"""Test contract RPC calls

Test the following RPCs:
    - get_vm_state_root
    - get_vm_state_proof
    - vm_call_dry_run
    - contract_call
    - get_vm_state_value
    - invoke_app_shader

"""

from __future__ import print_function
import re
import util_resources
import zmq
from framework.daemon import Daemon
from framework.wallet import Wallet
from contracts_common import ContractSigner, MINER_ADDRESS, assert_mined_replay_rejected, expect_rpc_error, mine_and_refresh

INACTIVE_DAEMON_IDX = 0
ACTIVE_DAEMON_IDX = 4
ACTIVE_WALLET_IDX = 6
WALLET_FILENAME = 'contracts_wallet'
SEED = 'velvet lymph giddy number token physics poetry unquoted nibs useful sabotage limits benches lifestyle eden nitrogen anvil fewest avoid batch vials washing fences goat unquoted'
ADDRESS = MINER_ADDRESS
TRANSFER_AMOUNT = 100000000
MATURITY_BLOCKS = 100
UNKNOWN_KEY = '00' * 32
FIRST_PUBLIC_METHOD = 2


def uleb(value):
    out = bytearray()
    while True:
        byte = value & 0x7f
        value >>= 7
        out.append(byte | 0x80 if value else byte)
        if not value:
            return bytes(out)


def section(section_id, payload):
    return bytes([section_id]) + uleb(len(payload)) + payload


def name(text):
    return uleb(len(text)) + text.encode()


def empty_shader_hex(trap_method=False, add_sig_count=0):
    exported = ['Ctor', 'Dtor', 'Method_2']
    types = section(1, uleb(1) + bytes([0x60, 1, 0x7f, 0]))
    imports = section(2, uleb(1) + name('env') + name('AddSig') + bytes([0, 0])) if add_sig_count else b''
    functions = section(3, uleb(len(exported)) + b'\x00' * len(exported))
    exports = section(7, uleb(len(exported)) + b''.join(name(n) + bytes([0]) + uleb(i + bool(add_sig_count)) for i, n in enumerate(exported)))
    body = uleb(2) + bytes([0, 0x0b])
    add_sig_body = (bytes([0, 0x20, 0, 0x10, 0, 0x0b]) if add_sig_count == 1 else
                    bytes([0, 0x20, 0, 0x10, 0, 0x20, 0, 0x41, 0x20, 0x6a, 0x10, 0, 0x0b]))
    method_body = (uleb(len(add_sig_body)) + add_sig_body if add_sig_count else
                   uleb(3) + bytes([0, 0x00, 0x0b]) if trap_method else body)
    code = section(10, uleb(len(exported)) + body * 2 + method_body)
    return (b'\x00asm\x01\x00\x00\x00' + types + imports + functions + exports + code).hex()


def hdr_info_shader_hex():
    method_type = section(1, uleb(3) + bytes([0x60, 1, 0x7f, 1, 0x7f]) +
                          bytes([0x60, 5, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 1, 0x7f]) +
                          bytes([0x60, 1, 0x7f, 0]))
    imports = section(2, uleb(2) + name('env') + name('get_HdrInfo') + bytes([0, 0]) +
                      name('env') + name('SaveVar') + bytes([0, 1]))
    functions = section(3, uleb(3) + bytes([2, 2, 2]))
    exports = section(7, uleb(3) + b''.join(name(n) + bytes([0]) + uleb(i + 2)
                                         for i, n in enumerate(('Ctor', 'Dtor', 'Method_2'))))
    empty = bytes([2, 0, 0x0b])
    method = bytes([0, 0x02, 0x40, 0x20, 0, 0x10, 0, 0x0d, 0, 0x00, 0x0b,
                    0x20, 0, 0x41, 0xd0, 0, 0x6a, 0x41, 1,
                    0x20, 0, 0x41, 0xd0, 0, 0x41, 0,
                    0x10, 1, 0x1a, 0x0b])
    code = section(10, uleb(3) + empty + empty + uleb(len(method)) + method)
    return (b'\x00asm\x01\x00\x00\x00' + method_type + imports + functions + exports + code).hex()


class ContractsTest():
    def run_test(self):
        self.check_inactive_chain()
        self.reset()
        self.create()
        self.mine()
        self.check_active_chain()
        self.deploy()
        self.invoke()
        self.check_state_values()
        self.check_hdr_info()
        self.check_dry_run_rejects_empty_call()
        self.check_app_shader()
        self.check_external_signatures()
        self.check_external_attach_and_relay()

    def reset(self):
        print('Resetting blockchain')
        daemon = Daemon(idx = ACTIVE_DAEMON_IDX)
        res = daemon.get_height()
        daemon.pop_blocks(res.height - 1)
        daemon.flush_txpool()

    def check_inactive_chain(self):
        print('Test contract RPCs before the contracts hard fork')
        daemon = Daemon(idx = INACTIVE_DAEMON_IDX)
        res = daemon.get_vm_state_root()
        assert res.active == False
        res = daemon.vm_call_dry_run('')
        assert res.status != 'OK'
        assert res.accepted == False

    def create(self):
        print('Creating wallet')
        self.wallet = Wallet(idx = ACTIVE_WALLET_IDX)
        try: self.wallet.close_wallet()
        except: pass
        util_resources.remove_wallet_files(WALLET_FILENAME)
        self.wallet.restore_deterministic_wallet(seed = SEED, filename = WALLET_FILENAME)
        self.wallet.auto_refresh(enable = False)

    def mine(self):
        print('Mining past coinbase maturity')
        daemon = Daemon(idx = ACTIVE_DAEMON_IDX)
        for _ in range(MATURITY_BLOCKS):
            daemon.generateblocks(ADDRESS)
        self.wallet.refresh()

    def mine_and_refresh(self):
        mine_and_refresh(self.wallet, ADDRESS, daemon_index=ACTIVE_DAEMON_IDX)

    def check_active_chain(self):
        print('Test get_vm_state_root')
        daemon = Daemon(idx = ACTIVE_DAEMON_IDX)
        res = daemon.get_vm_state_root()
        assert res.active == True
        assert res.height == daemon.get_height().height - 1
        assert len(res.root) == 64
        self.initial_root = res.root

    def deploy(self):
        print('Test contract_call deploy')
        res = self.wallet.contract_call(
            kind = 'deploy',
            code = empty_shader_hex(),
            destinations = [{'address': ADDRESS, 'amount': TRANSFER_AMOUNT}])
        assert len(res.tx_hash) == 64
        assert len(res.contract_id) == 64
        assert res.gas_used > 0
        assert res.gas_limit >= res.gas_used
        self.contract_id = res.contract_id
        assert len(self.wallet.derive_contract_key(self.contract_id, UNKNOWN_KEY).public_key) == 64
        self.mine_and_refresh()

    def invoke(self):
        print('Test contract_call invoke')
        res = self.wallet.contract_call(
            kind = 'invoke',
            contract_id = self.contract_id,
            method = FIRST_PUBLIC_METHOD,
            destinations = [{'address': ADDRESS, 'amount': TRANSFER_AMOUNT}])
        assert len(res.tx_hash) == 64
        assert res.contract_id == self.contract_id
        assert res.gas_limit > 0
        self.mine_and_refresh()

    def check_state_values(self):
        print('Test get_vm_state_value and get_vm_state_proof')
        daemon = Daemon(idx = ACTIVE_DAEMON_IDX)

        res = self.wallet.get_vm_state_value(self.contract_id)
        assert res.found == True
        assert len(res.value) > 0
        assert res.root == daemon.get_vm_state_root().root
        deployed_value = res.value

        proof = daemon.get_vm_state_proof(self.contract_id)
        assert proof.height == daemon.get_height().height - 1
        assert proof.found == True
        assert proof.value == deployed_value
        assert proof.root == res.root
        assert daemon.get_vm_state_root(height=proof.height).root == proof.root
        genesis = daemon.get_vm_state_proof(self.contract_id, height=0)
        assert genesis.height == 0 and genesis.found == False
        genesis_root = daemon.get_vm_state_root(height=0)
        assert genesis_root.height == 0 and genesis_root.root == genesis.root
        assert re.fullmatch('[0-9a-f]{64}', proof.leaf_path)
        assert re.fullmatch('[0-9a-f]{64}', proof.leaf_value)

        res = self.wallet.get_vm_state_value(UNKNOWN_KEY)
        assert res.found == False
        assert res.value == ''
        assert res.root == daemon.get_vm_state_root().root

        proof = daemon.get_vm_state_proof(UNKNOWN_KEY)
        assert proof.found == False

        try:
            self.wallet.get_vm_state_value('not hex')
            assert False
        except AssertionError as error:
            assert 'error' in str(error)

    def check_hdr_info(self):
        print('Test get_HdrInfo against block header RPC')
        daemon = Daemon(idx = ACTIVE_DAEMON_IDX)
        height = daemon.get_height().height - 4
        expected = daemon.getblockheaderbyheight(height).block_header
        previous = daemon.getblockheaderbyheight(height - 1).block_header.hash
        deployed = self.wallet.contract_call(
            kind = 'deploy', code = hdr_info_shader_hex(),
            destinations = [{'address': ADDRESS, 'amount': TRANSFER_AMOUNT}])
        self.mine_and_refresh()
        args = (height.to_bytes(8, 'little') + bytes(72) + b'h').hex()
        self.wallet.contract_call(
            kind = 'invoke', contract_id = deployed.contract_id,
            method = FIRST_PUBLIC_METHOD, args = args,
            destinations = [{'address': ADDRESS, 'amount': TRANSFER_AMOUNT}])
        self.mine_and_refresh()
        stored = self.wallet.get_vm_state_value(deployed.contract_id + '00' + '68')
        assert stored.found == True
        raw = bytes.fromhex(stored.value)
        assert len(raw) == 80
        assert int.from_bytes(raw[:8], 'little') == height
        assert raw[8:40].hex() == expected.hash
        assert raw[40:72].hex() == previous
        assert int.from_bytes(raw[72:], 'little') == expected.timestamp

    def check_dry_run_rejects_empty_call(self):
        print('Test vm_call_dry_run')
        daemon = Daemon(idx = ACTIVE_DAEMON_IDX)
        res = daemon.vm_call_dry_run('')
        assert res.accepted == False
        res = daemon.vm_call_dry_run('zz')
        assert res.status != 'OK'
        call = bytes([1, 1]) + bytes(32) + bytes([0, 0, 0]) + uleb(100001) + bytes([1, 0]) + bytes(7)
        extra = (bytes([8]) + call).hex()
        assert daemon.vm_call_dry_run(extra).status == 'OK'
        restricted = Daemon(port = 18584)
        result = restricted.vm_call_dry_run(extra)
        assert result.status == 'Gas exceeds restricted VM dry run cap', result.status

    def check_app_shader(self):
        print('Test invoke_app_shader')
        res = self.wallet.invoke_app_shader(empty_shader_hex(), FIRST_PUBLIC_METHOD, 'key=value')
        assert res.steps_used > 0
        assert len(res.get('calls', [])) == 0
        assert 'document' in res

        for bad_params in [{'code': 'zz'}, {'code': ''}, {'code': empty_shader_hex(), 'args': 'no_separator'}]:
            try:
                self.wallet.invoke_app_shader(**bad_params)
                assert False
            except AssertionError as error:
                assert 'error' in str(error)

        request = {
            'method': 'invoke_app_shader',
            'params': {
                'code': empty_shader_hex(trap_method=True),
                'method': FIRST_PUBLIC_METHOD,
                'approved_keys': [{'contract_id': UNKNOWN_KEY, 'key_id': UNKNOWN_KEY, 'public_key': UNKNOWN_KEY}],
            },
            'jsonrpc': '2.0',
            'id': '0',
        }
        try:
            self.wallet.rpc.send_json_rpc_request(request)
            assert False
        except AssertionError as error:
            assert "'code': -54" in str(error)

    expect_rpc_error = staticmethod(expect_rpc_error)

    def assert_not_in_pool(self, tx_hash):
        pool = Daemon(idx = ACTIVE_DAEMON_IDX).get_transaction_pool_hashes()
        assert tx_hash not in pool.get('tx_hashes', [])

    def check_external_signatures(self):
        print('Test external AddSig RPC flow')
        daemon = Daemon(idx = ACTIVE_DAEMON_IDX)
        deployed = self.wallet.contract_call(
            kind = 'deploy', code = empty_shader_hex(add_sig_count=1),
            destinations = [{'address': ADDRESS, 'amount': TRANSFER_AMOUNT}])
        self.mine_and_refresh()
        contract_id = deployed.contract_id
        key = self.wallet.derive_contract_key(contract_id, '01' * 32).public_key
        other = self.wallet.derive_contract_key(contract_id, '02' * 32).public_key
        call = dict(kind = 'invoke', contract_id = contract_id, method = FIRST_PUBLIC_METHOD,
                    args = key, external_signers = [key],
                    destinations = [{'address': ADDRESS, 'amount': TRANSFER_AMOUNT}])
        self.expect_rpc_error(lambda: self.wallet.contract_call(**call), -53,
                              'External signers need do_not_relay and get_tx_metadata')
        unsigned_call = dict(call, do_not_relay = True, get_tx_metadata = True)
        self.expect_rpc_error(lambda: self.wallet.contract_call(**dict(unsigned_call, external_signers = ['zz' * 32])),
                              -53, 'Invalid external signer')
        self.expect_rpc_error(lambda: self.wallet.contract_call(**dict(unsigned_call, external_signers = [other])),
                              -53, 'External signer is not requested by the contract')
        unsigned = self.wallet.contract_call(**unsigned_call)
        assert re.fullmatch('[0-9a-f]{64}', unsigned.signature_hash)
        assert unsigned.tx_metadata
        self.assert_not_in_pool(unsigned.tx_hash)
        attach = lambda metadata, signatures: self.wallet.attach_contract_signatures(
            metadata, signatures, do_not_relay = True)
        self.expect_rpc_error(lambda: attach('xyz', []), -26, 'Failed to parse hex')
        self.expect_rpc_error(lambda: attach('deadbeef', []), -27, 'Failed to parse tx metadata')
        self.expect_rpc_error(lambda: attach(unsigned.tx_metadata, []), -53, 'Invalid contract signature count')
        self.expect_rpc_error(lambda: attach(unsigned.tx_metadata, [{'public_key': key, 'signature': '0' * 127}]),
                              -26, 'Invalid contract signature encoding')
        self.expect_rpc_error(lambda: attach(unsigned.tx_metadata, [{'public_key': key, 'signature': ('01' + '00' * 31) * 2}]),
                              -53, 'Contract signature does not verify')
        self.assert_not_in_pool(unsigned.tx_hash)
        self.expect_rpc_error(lambda: self.wallet.relay_tx(unsigned.tx_metadata), -4,
                              'Contract call still lacks signatures')
        self.assert_not_in_pool(unsigned.tx_hash)
        view_key = self.wallet.query_key('view_key').key
        self.wallet.close_wallet()
        try:
            util_resources.remove_wallet_files(WALLET_FILENAME + '_view')
            self.wallet.generate_from_keys(filename = WALLET_FILENAME + '_view',
                                           address = ADDRESS, viewkey = view_key)
            self.expect_rpc_error(lambda: self.wallet.contract_call(**unsigned_call), -53,
                                  'External signers require a spendable local wallet')
        finally:
            self.wallet.close_wallet()
            self.wallet.open_wallet(WALLET_FILENAME)

    def check_external_attach_and_relay(self):
        print('Test partial external AddSig attach and relay')
        daemon = Daemon(idx = ACTIVE_DAEMON_IDX)
        deployed = self.wallet.contract_call(
            kind = 'deploy', code = empty_shader_hex(add_sig_count=2),
            destinations = [{'address': ADDRESS, 'amount': TRANSFER_AMOUNT}])
        self.mine_and_refresh()
        contract_id = deployed.contract_id
        scalar1, key1 = ContractSigner.keypair()
        scalar2, key2 = ContractSigner.keypair()
        unsigned = self.wallet.contract_call(
            kind = 'invoke', contract_id = contract_id, method = FIRST_PUBLIC_METHOD,
            args = key1 + key2, external_signers = [key1, key2],
            do_not_relay = True, get_tx_metadata = True,
            destinations = [{'address': ADDRESS, 'amount': TRANSFER_AMOUNT}])
        digest = unsigned.signature_hash
        first = {'public_key': key1, 'signature': ContractSigner.sign(digest, scalar1, key1)}
        second = {'public_key': key2, 'signature': ContractSigner.sign(digest, scalar2, key2)}
        self.expect_rpc_error(lambda: self.wallet.attach_contract_signatures(unsigned.tx_metadata, [first], do_not_relay=False),
                              -53, 'Contract call still lacks signatures')
        self.assert_not_in_pool(unsigned.tx_hash)
        partial = self.wallet.attach_contract_signatures(unsigned.tx_metadata, [first], do_not_relay=True, get_tx_hex=True)
        assert partial.signature_hash == digest
        assert partial.tx_metadata != unsigned.tx_metadata
        partial_rejection = daemon.send_raw_transaction(partial.tx_blob)
        assert partial_rejection.status == 'Failed'
        assert partial_rejection.contract_call_rejected is True
        assert partial_rejection.reason == 'tx contract call rejected'
        self.assert_not_in_pool(partial.tx_hash)
        self.expect_rpc_error(lambda: self.wallet.relay_tx(partial.tx_metadata), -4,
                              'Contract call still lacks signatures')
        self.assert_not_in_pool(partial.tx_hash)
        self.expect_rpc_error(lambda: self.wallet.attach_contract_signatures(partial.tx_metadata, [first], do_not_relay=True),
                              -53, 'Contract signer is not pending')
        complete = self.wallet.attach_contract_signatures(partial.tx_metadata, [second], do_not_relay=False, get_tx_hex=True)
        assert complete.signature_hash == digest
        assert complete.tx_hash in daemon.get_transaction_pool_hashes().get('tx_hashes', [])
        self.mine_and_refresh()
        mined = daemon.get_transactions([complete.tx_hash])
        assert len(mined.txs) == 1
        assert mined.txs[0].tx_hash == complete.tx_hash
        assert mined.txs[0].in_pool == False
        assert_mined_replay_rejected(daemon, complete.tx_blob)
        with zmq.Context.instance().socket(zmq.REQ) as rpc:
            rpc.setsockopt(zmq.RCVTIMEO, 30000)
            rpc.connect('tcp://127.0.0.1:{}'.format(18380 + ACTIVE_DAEMON_IDX))
            rpc.send_json({'jsonrpc': '2.0', 'id': 1, 'method': 'send_raw_tx_hex',
                           'params': {'tx_as_hex': complete.tx_blob, 'relay': False}})
            replay = rpc.recv_json()
        assert replay['error']['message'] == 'double spend'
        state = self.wallet.get_vm_state_value(contract_id)
        assert state.found == True
        assert state.root == daemon.get_vm_state_root().root


if __name__ == '__main__':
    ContractsTest().run_test()
