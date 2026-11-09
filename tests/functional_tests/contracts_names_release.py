import os
import signal
import struct
import time

from contracts_common import DAEMON_INDEX, MINER_ADDRESS, TERM_BLOCKS, TERM_PRICE, NamesTest, assert_mined_replay_rejected, sdk


class NamesReleaseTest(NamesTest):
    def run_test(self):
        beneficiary_scalar, beneficiary, release_height = self.setup()
        assert release_height > self.daemon.get_height().height
        pids = [int(pid) for pid in os.environ['QWC_NAMES_SERVICE_PIDS'].split(',')]
        for pid in pids:
            os.kill(pid, signal.SIGSTOP)
        try:
            remaining = release_height - self.daemon.get_height().height
            sample = min(100, remaining)
            start = time.monotonic()
            self.daemon.generateblocks(MINER_ADDRESS, sample)
            elapsed = time.monotonic() - start
            print('names mining paused services: {} blocks in {:.2f}s ({:.2f} blocks/s)'.format(
                sample, elapsed, sample / elapsed), flush=True)
            remaining -= sample
            bulk_total = remaining
            start = time.monotonic()
            mined = 0
            while remaining:
                batch = min(1000, remaining)
                self.daemon.generateblocks(MINER_ADDRESS, batch)
                remaining -= batch
                mined += batch
                print('names release mining: {}/{} blocks'.format(mined, bulk_total), flush=True)
            elapsed = time.monotonic() - start
            if mined:
                print('names mining paused services bulk: {} blocks in {:.2f}s ({:.2f} blocks/s)'.format(
                    mined, elapsed, mined / elapsed), flush=True)
        finally:
            for pid in pids:
                os.kill(pid, signal.SIGCONT)
        assert self.daemon.get_height().height == release_height
        self.wallet.set_daemon('http://127.0.0.1:{}'.format(18180 + DAEMON_INDEX),
                               trusted=True, ssl_support='disabled')
        self.wallet.refresh()
        label = 'alice'
        settle_args = (bytes([len(label)]) + label.encode().ljust(63, b'\0')).hex()
        settled = self.call(kind='invoke', contract_id=self.contract_id, method=6, args=settle_args)
        self.mine()
        assert self.daemon.get_transactions([settled.tx_hash]).txs[0].in_pool is False
        state = self.wallet.get_vm_state_value(sdk(self.binary, 'state-key', self.contract_id, label))
        assert state.found
        assert struct.unpack_from('<3Q', bytes.fromhex(state.value), 72) == (1, 1, release_height + TERM_BLOCKS)
        config_key = self.contract_id + '0063'
        config = self.wallet.get_vm_state_value(config_key)
        assert config.found
        assert struct.unpack_from('<4Q', bytes.fromhex(config.value), 32) == (TERM_PRICE, TERM_PRICE, 0, 2)
        self.wallet.refresh()
        before = self.wallet.get_balance().balance
        amount = TERM_PRICE // 2
        args = sdk(self.binary, 'withdraw', amount, 0)
        unsigned = self.call(kind='invoke', contract_id=self.contract_id, method=7,
                             args=args, mint_address=self.address, external_signers=[beneficiary],
                             do_not_relay=True, get_tx_metadata=True)
        signature = sdk(self.binary, 'sign-call', unsigned.signature_hash, '--scalar', scalar=beneficiary_scalar)
        signed = self.wallet.attach_contract_signatures(unsigned.tx_metadata,
                    [{'public_key': beneficiary, 'signature': signature}], get_tx_hex=True)
        assert signed.signature_hash == unsigned.signature_hash
        assert signed.tx_blob and signed.tx_metadata
        stale_unsigned = self.call(kind='invoke', contract_id=self.contract_id, method=7,
                                   args=args, mint_address=self.address, external_signers=[beneficiary],
                                   do_not_relay=True, get_tx_metadata=True)
        stale_signature = sdk(self.binary, 'sign-call', stale_unsigned.signature_hash,
                              '--scalar', scalar=beneficiary_scalar)
        stale_signed = self.wallet.attach_contract_signatures(stale_unsigned.tx_metadata,
                       [{'public_key': beneficiary, 'signature': stale_signature}],
                       do_not_relay=True, get_tx_hex=True)
        assert stale_signed.signature_hash == stale_unsigned.signature_hash
        assert stale_signed.tx_hash != signed.tx_hash and stale_signed.tx_blob
        self.mine()
        mined = self.daemon.get_transactions([signed.tx_hash]).txs[0]
        assert mined.tx_hash == signed.tx_hash and mined.as_hex == signed.tx_blob and mined.in_pool is False
        after = before + amount - unsigned.fee
        def assert_unchanged():
            assert self.daemon.get_vm_state_root().withdrawn == amount
            config = self.wallet.get_vm_state_value(config_key)
            assert struct.unpack_from('<4Q', bytes.fromhex(config.value), 32) == (TERM_PRICE, TERM_PRICE - amount, 1, 2)
            assert self.wallet.get_balance().balance == after
        assert_unchanged()
        assert_mined_replay_rejected(self.daemon, signed.tx_blob)
        assert_unchanged()
        stale = self.daemon.send_raw_transaction(stale_signed.tx_blob)
        assert stale.status == 'Failed' and stale.reason == 'tx contract call rejected'
        assert stale.contract_call_rejected is True and stale.double_spend is False
        assert_unchanged()
        self.wait_for_peers(attempts=120000)


if __name__ == '__main__':
    NamesReleaseTest().run_test()
