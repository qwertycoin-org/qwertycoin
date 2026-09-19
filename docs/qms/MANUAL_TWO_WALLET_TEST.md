# Manual two-wallet acceptance test (do not automate)

This test is intentionally not part of the offline developer test run. Use two separate
wallet files and profiles with the extended Qt wallet. Keep automatic local daemon start
disabled and enter an existing, unchanged node endpoint supplied by the operator.

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
8. Close and reopen both wallets. Verify contacts, outgoing plaintext, incoming text and
   delivery/confirmation states. Optionally fund B and repeat B→A.

Do not use production seeds for experimental acceptance. Do not use integrated
addresses, payment IDs, hardware wallets, multisig or light-wallet services.

