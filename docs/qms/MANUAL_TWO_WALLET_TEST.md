# Manual two-wallet acceptance test (do not automate)

This test is intentionally not part of the offline developer test run. It must be
performed only after code review, license approval, signed test artifacts, and explicit
operator authorization. Use two disposable, separately backed-up wallet files and
profiles. Never use production seeds or meaningful balances.

The native client must be configured with an operator-supplied SOCKS5 endpoint and a
Tor v3 onion RPC hostname. Keep automatic local-daemon start disabled. Before the test,
prove with packet capture or an equivalent independent check that wallet refresh, scan,
fee lookup, preparation, broadcast, and receive traffic have no direct fallback. Do not
run this procedure through an ordinary browser: the Web Wallet intentionally fails
closed unless its complete route is independently attestable as Tor-only.

1. Open wallet A and wallet B using separate files and messenger stores.
2. In **Messages → My invitation**, export one personal invitation from each wallet.
3. In **Messages → Add contact**, import the other wallet's invitation and confirm the
   displayed fingerprint through the trusted exchange channel.
4. Fund wallet A beforehand with at least as many already mature, independently
   spendable outputs as the plan reports carrier transactions. Wallet B needs no funds
   to receive. Preparing a message never creates funding/splitting transactions.
5. Select B, enter a short message, press **Prepare**, and inspect fragment/transaction
   count and total dynamic fee. Press **Send prepared** only after accepting that plan.
6. After B synchronizes, verify the authenticated plaintext and confirmation-dependent
   status.
7. Repeat with text longer than 255 bytes and exactly 4,096 UTF-8 bytes including umlauts
   and emoji.
8. Send enough alternating messages to cross the 16-send outer-secret rotation boundary.
   Interrupt one receive cycle so an offer or acknowledgement is missed, then verify the
   repeated offer, successful acknowledgement, one grace context, and continued delivery.
9. Close and reopen both wallets. Verify contacts, pending batches, inbox state and
   delivery/confirmation states. Plaintext message history must be absent by default.
   Enable it explicitly, verify persistence, then delete it without deleting contacts or
   ratchet state.
10. Exercise cancellation and a partial-broadcast recovery with disposable funds. A
    retry must reuse the exact stored signed transaction bytes and must not create a new
    fee without renewed review.
11. Test a daemon-provided reorg in the isolated environment. Confirmation state may
    change; cryptographic state must never rewind.
12. Restore an old copy only in the disposable environment. Do not continue sending on
    the restored session. Use the explicit Messenger reset action, exchange fresh
    contact packages, and verify that the old contacts/sessions cannot be used.

Do not use integrated addresses, payment IDs, hardware wallets, multisig or light-wallet
services. Record client revisions, complete routing evidence, carrier hashes, fees,
restart/reorg results, and every manual deviation. This document authorizes no node,
Tor process, transaction, mining, release, deployment, or public test by itself.
