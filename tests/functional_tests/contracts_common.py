import json
import os
import re
import secrets
import struct
import subprocess
import time
from pathlib import Path

from Crypto.Hash import keccak
import util_resources
from framework.daemon import Daemon
from framework.wallet import Wallet

MINER_ADDRESS = 'QWC1GKRV4Wd3DCnubWiv1CBDBJWTCUcvTAZnE9ioE7pUMiqwXpce7GX5kfyaW4X8V523Rkyoa9NQ6LWj2DrEX7cE2PDLobNPxh'
SERVICE_NODE_INDEXES = range(4, 14)


class ContractSigner:
    field = 2**255 - 19
    order = 2**252 + 27742317777372353535851937790883648493
    d = (-121665 * pow(121666, field - 2, field)) % field
    base_x = 15112221349535400772501151409588531511454012693041857206046113283949847762202
    base_y = 46316835694926478169428394003475163141307993866256225615783033603165251855960
    base = (base_x, base_y, 1, base_x * base_y % field)

    @classmethod
    def add(cls, left, right):
        x1, y1, z1, t1 = left
        x2, y2, z2, t2 = right
        a = (y1 - x1) * (y2 - x2) % cls.field
        b = (y1 + x1) * (y2 + x2) % cls.field
        c = 2 * cls.d * t1 * t2 % cls.field
        d = 2 * z1 * z2 % cls.field
        e, f, g, h = b - a, d - c, d + c, b + a
        return e * f % cls.field, g * h % cls.field, f * g % cls.field, e * h % cls.field

    @classmethod
    def multiply(cls, scalar):
        result = (0, 1, 1, 0)
        point = cls.base
        while scalar:
            if scalar & 1:
                result = cls.add(result, point)
            point = cls.add(point, point)
            scalar >>= 1
        return result

    @classmethod
    def encode(cls, point):
        x, y, z, _ = point
        inverse = pow(z, cls.field - 2, cls.field)
        return ((y * inverse % cls.field) | ((x * inverse % cls.field & 1) << 255)).to_bytes(32, 'little')

    @classmethod
    def hash_scalar(cls, value):
        digest = keccak.new(digest_bits=256)
        digest.update(value)
        return int.from_bytes(digest.digest(), 'little') % cls.order

    @classmethod
    def keypair(cls):
        scalar = secrets.randbelow(cls.order - 1) + 1
        return scalar, cls.encode(cls.multiply(scalar)).hex()

    @classmethod
    def sign(cls, digest_hex, scalar, public_hex):
        nonce = secrets.randbelow(cls.order - 1) + 1
        challenge = cls.hash_scalar(bytes.fromhex(digest_hex) + bytes.fromhex(public_hex) + cls.encode(cls.multiply(nonce)))
        response = (nonce - challenge * scalar) % cls.order
        return challenge.to_bytes(32, 'little').hex() + response.to_bytes(32, 'little').hex()


def expect_rpc_error(action, code, message, exact=False):
    try:
        action()
    except AssertionError as error:
        if exact:
            assert error.args[0]['error'] == {'code': code, 'message': message}, error
            return
        assert "'code': {}".format(code) in str(error), error
        assert message in str(error), error
    else:
        raise AssertionError('Expected RPC error {}: {}'.format(code, message))


def assert_mined_replay_rejected(daemon, tx_blob):
    replay = daemon.send_raw_transaction(tx_blob)
    assert replay.status == 'Failed'
    assert replay.reason == 'double spend'
    assert replay.double_spend is True


def mine_and_refresh(wallet, address, blocks=1, daemon_index=4):
    Daemon(idx=daemon_index).generateblocks(address, blocks)
    wallet.refresh()


DAEMON_INDEX = 4
WALLET_INDEX = 6
WALLET_FILENAME = 'contracts_names_wallet'
TERM_PRICE = 100000000
TERM_BLOCKS = 21600
TRANSFER_AMOUNT = 100000000


def sdk(binary, *args, scalar=None):
    result = subprocess.run(
        [str(binary), *map(str, args)], input=None if scalar is None else scalar.to_bytes(32, 'little').hex() + '\n',
        text=True, capture_output=True, check=True)
    assert result.stderr == ''
    return result.stdout.strip()


def required_artifacts():
    directory = os.environ.get('QWC_NAMES_WASM_DIR')
    binary = os.environ.get('QWC_NAMES_BIN')
    assert directory, 'QWC_NAMES_WASM_DIR is required for contracts_names'
    assert binary, 'QWC_NAMES_BIN is required for contracts_names'
    path = Path(directory)
    for name in ('names-contract.wasm', 'names-app.wasm', 'SHA256SUMS'):
        assert (path / name).is_file(), 'Missing names SDK artifact: ' + name
    assert Path(binary).is_file() and os.access(binary, os.X_OK), 'QWC_NAMES_BIN is not executable'
    subprocess.run(['sha256sum', '-c', 'SHA256SUMS'], cwd=path, stdout=subprocess.DEVNULL, check=True)
    return path, binary


class NamesTest:
    def __init__(self):
        self.path, self.binary = required_artifacts()
        self.daemon = Daemon(idx=DAEMON_INDEX)
        self.wallet = Wallet(idx=WALLET_INDEX)

    def mine(self, blocks=1, address=MINER_ADDRESS):
        mine_and_refresh(self.wallet, address, blocks, DAEMON_INDEX)

    def wait_for_peers(self, attempts=300):
        tip = self.daemon.get_info().top_block_hash
        for _ in range(attempts):
            if all(Daemon(idx=index).get_info().top_block_hash == tip for index in SERVICE_NODE_INDEXES):
                return
            time.sleep(0.2)
        raise AssertionError('EPoSE service nodes did not synchronize')

    def wait_for_quorum(self):
        self.wait_for_peers()
        for _ in range(1200):
            info = self.daemon.get_epose_info()
            assert info.enabled and info.local_service_node_key_loaded
            if (info.current_epoch >= 2 and info.qualification_available
                    and info.qualification_epoch == info.current_epoch - 1
                    and info.qualified_count >= 10 and info.service_node_count >= 10):
                if all(Daemon(idx=index).get_epose_info().local_service_node_active
                       for index in SERVICE_NODE_INDEXES):
                    return
            self.mine()
            self.wait_for_peers()
            time.sleep(1)
        raise AssertionError('EPoSE quorum did not qualify')

    def finalized_lock(self, height):
        self.wait_for_peers()
        for _ in range(20):
            time.sleep(1)
            self.mine()
            lock = self.daemon.get_chainlock(height)
            if lock.status == 'OK':
                assert lock.height == height and lock.lock_carrier_height > height
                assert lock.block_id == self.daemon.getblockheaderbyheight(height).block_header.hash
                assert len(lock.quorum) >= 10 and lock.threshold >= 7
                return lock
        raise AssertionError('Registration block has no finalized ChainLock: ' + lock.status)

    def call(self, **params):
        return self.wallet.contract_call(destinations=[{'address': self.address, 'amount': TRANSFER_AMOUNT}], **params)

    def app(self, method, args):
        request = {'jsonrpc': '2.0', 'id': 'names', 'method': 'invoke_app_shader',
                   'params': {'code': self.app_code, 'method': method, 'args': args,
                              'contract_ids': [self.contract_id]}}
        return self.wallet.rpc.send_json_rpc_request(request)

    def record(self, label, owner, pointer, height, slot):
        key = sdk(self.binary, 'state-key', self.contract_id, label)
        proof = self.daemon.get_vm_state_proof(key)
        state = self.wallet.get_vm_state_value(key)
        assert proof.status == 'OK' and proof.found and state.found
        assert proof.root == state.root == self.daemon.get_vm_state_root().root
        assert proof.value == state.value
        entry = bytes.fromhex(state.value)
        expires = height + TERM_BLOCKS
        expected = bytes.fromhex(owner) + bytes.fromhex(pointer) + struct.pack('<6Q', expires, 1, 0, expires, 1, slot)
        assert len(expected) == 112
        assert entry == expected
        return struct.unpack_from('<Q', entry, 88)[0]

    def setup(self):
        height = self.daemon.get_height().height
        if height > 1:
            self.daemon.pop_blocks(height - 1)
            self.daemon.flush_txpool()
        try:
            self.wallet.close_wallet()
        except AssertionError:
            pass
        util_resources.remove_wallet_files(WALLET_FILENAME)
        self.wallet.create_wallet(filename=WALLET_FILENAME)
        self.wallet.auto_refresh(enable=False)
        self.address = self.wallet.get_address().address
        time.sleep(10)
        start = time.monotonic()
        for _ in range(10):
            self.daemon.generateblocks(self.address, 10)
            self.wait_for_peers()
            time.sleep(3)
        elapsed = time.monotonic() - start
        print('names mining with services: 100 blocks in {:.2f}s ({:.2f} blocks/s)'.format(elapsed, 100 / elapsed), flush=True)
        self.wallet.refresh()
        self.wait_for_quorum()
        beneficiary_scalar, beneficiary = ContractSigner.keypair()
        before_root = self.daemon.get_vm_state_root().root
        deploy = self.call(kind='deploy', code=(self.path / 'names-contract.wasm').read_bytes().hex(),
                           args=sdk(self.binary, 'deploy-args', beneficiary, TERM_PRICE))
        assert re.fullmatch('[0-9a-f]{64}', deploy.contract_id)
        assert deploy.gas_used > 0 and deploy.gas_limit >= deploy.gas_used
        self.contract_id = deploy.contract_id
        self.app_code = (self.path / 'names-app.wasm').read_bytes().hex()
        self.mine()
        deployed = self.wallet.get_vm_state_value(self.contract_id)
        assert deployed.found and deployed.root == self.daemon.get_vm_state_root().root
        assert deployed.root != before_root
        assert self.daemon.get_vm_state_proof(self.contract_id).value == deployed.value
        assert self.daemon.get_vm_state_root().gas_used == deploy.gas_used

        total_deposited = 0
        release_height = None
        for slot, label in enumerate(('alice', 'four')):
            scalar, owner = ContractSigner.keypair()
            _, pointer = ContractSigner.keypair()
            args = sdk(self.binary, 'register', self.contract_id, label, pointer, 1, 0, scalar=scalar)
            price = int(sdk(self.binary, 'price', label, 1, TERM_PRICE))
            assert price == TERM_PRICE * (4 if len(label) == 4 else 1)
            generated = self.app(5, 'contract=' + self.contract_id + ',args=' + args)
            assert generated.steps_used > 0 and len(generated.calls) == 1
            call = generated.calls[0]
            assert call.kind == 'invoke' and call.contract_id == self.contract_id
            assert call.method == 2 and call.args == args and call.deposit == price
            params = {'kind': 'invoke', 'contract_id': self.contract_id, 'method': 2, 'args': args}
            expect_rpc_error(lambda: self.call(deposit=price - 1, **params), -4, 'Contract call dry-run is failed', exact=True)
            registered = self.call(deposit=price, **params)
            assert registered.contract_id == self.contract_id
            assert registered.gas_used > 0 and registered.gas_limit >= registered.gas_used
            self.mine()
            assert self.daemon.get_transactions([registered.tx_hash]).txs[0].in_pool is False
            commitment = self.daemon.get_vm_state_root()
            total_deposited += price
            assert commitment.gas_used == registered.gas_used and commitment.deposited == total_deposited
            expires = self.record(label, owner, pointer, self.daemon.get_height().height - 1, slot)
            if label == 'alice':
                release_height = expires
            read = self.app(4, 'contract=' + self.contract_id + ',label=' + label + ',terms=1')
            assert read.steps_used > 0 and len(read.calls) == 1
            assert read.calls[0].method == 3 and read.calls[0].deposit == price
            assert json.loads(read.document) == {'expires': expires, 'method': 3, 'deposit': price}
            expect_rpc_error(lambda: self.call(deposit=price, **params), -4, 'Contract call dry-run is failed', exact=True)
        registration_height = self.daemon.get_height().height - 1
        lock = self.finalized_lock(registration_height)
        assert re.fullmatch('[0-9a-f]{64}', lock.vm_state_root) and lock.vm_state_root == commitment.root
        withdrawal_args = sdk(self.binary, 'withdraw', TERM_PRICE, 0)
        expect_rpc_error(lambda: self.call(kind='invoke', contract_id=self.contract_id, method=7,
                                           args=withdrawal_args, mint_address=self.address),
                         -4, 'Contract call dry-run is failed', exact=True)
        assert self.daemon.get_vm_state_root().withdrawn == 0
        return beneficiary_scalar, beneficiary, release_height
