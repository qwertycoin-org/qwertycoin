# Consensus Invariants

## Implemented HF17 / EPoSE v1 invariants

1. Two honest nodes with the same chain data compute the same EPoSE state.
2. External APIs never influence block validity.
3. A service reward cannot be paid twice for the same block.
4. Every service reward is reconstructible from blockchain data and QWC P2P messages.
5. Reorgs roll back registrations, attestations, qualification, and rewards.
6. Service-node keys cannot spend wallet funds.
7. Invalid proofs and attestations do not add reward weight.
8. Duplicate verifier votes count once.
9. Parser inputs are bounded before allocation.
10. Every consensus parameter is version-bound.
11. Sentinel, Explorer, DNS, GeoIP, and monitoring are observers only.
12. Coinbase validation after activation must reject wrong service reward outputs.
13. Attestations included in the current block cannot influence that block's service reward.
14. Attestation ordering within an epoch cannot change the next epoch's qualified set.
15. Withholding an otherwise valid current-epoch attestation cannot change any reward before the next epoch.
16. Invalid admission proof implies registration rejection.
17. ZMQ availability must not influence consensus state or block validity.
18. Attestation response hashes must be bound to the challenge, observed tip, subject, verifier, and epoch.
19. Late attestations for a finalized source epoch cannot be accepted in the next epoch to alter that source epoch's reward set.
20. Every EPoSE service reward output must be a unique spendable one-time output.
21. Reorg A -> B -> A must restore the original EPoSE state hash and qualified set.
22. Automatic positive HF17 attestations remain disabled because the legacy path did not contact or authenticate the subject.
23. Reward RPC views must report the finalized reward source epoch, not the mutable current epoch, at and after epoch boundaries.
24. Malformed EPoSE tx-extra payloads must fail closed and roll back any earlier EPoSE state changes from the same transaction batch.
25. Omitting an otherwise valid current-epoch attestation can change future qualification only after epoch finalization; it cannot change the current reward source.
26. Attestations from later epochs cannot mutate an already finalized epoch's qualified set.
27. Service-node qualification must not require local mining.
28. A miner must not need a service-node private key.
29. Normal nodes may relay and include EPoSE registrations and attestations.
30. Off-chain attestation pool state must not directly determine block validity.
31. Same canonical chain implies same EPoSE consensus state.
32. Duplicate relay messages must not add duplicate qualification weight.
33. A service node registration must not require that service node's own block template to be mined.
34. Repeated rewards to the same reward address must use distinct output keys.
35. The disclosed reward view secret key must match the registered public view key.
36. The reward view secret key must not permit spending.
37. `qwertycoin-wallet-cli` must detect EPoSE rewards through normal wallet scanning.
38. Chain reset must not change service-node identity when identity storage is preserved.
39. A service public key must not have conflicting active registrations.
40. Service-node identity must not rely on IP address alone.
41. Qualification must use `ceil(actual_committee_size * 2 / 3)`, not a fixed attestation count.
42. The admission leading-zero target is consensus-relevant and must be versioned or reset from genesis when changed.
43. The service reward amount must be decomposed into the exact standard denomination set expected by validation.
44. A block with a qualified payee must not underpay or overpay the EPoSE service reward denomination set.
45. Explorer/UI grouping of denominated service reward outputs must not change consensus interpretation of raw outputs.

## QWC HF17 / EPoSE v2 launch invariants

These invariants are normative design requirements but are not implemented or
activated by CO-01.

46. The intended public chain has no pre-activation blocks: QWC block version 17 is the only valid genesis version and selects EPoSE protocol version 2.
47. No v1 registration, attestation, qualification, or descriptor is implicitly promoted into v2.
48. The fresh-genesis v2 activation height is exactly 0; epoch 0 remains reward-ineligible.
49. A reservation manifest containing any required `null` value cannot activate.
50. Epoch 0 and both activation warm-up epochs produce no v2 service payout.
51. Membership and every selection-relevant descriptor field are frozen before the committee anchor is known.
52. Receipt membership, threshold, and qualification read the same immutable snapshot.
53. Qualification closes before the payout seed is known.
54. A receipt at the evidence deadline is eligible; one block later is invalid.
55. A lease at the enrollment cutoff is eligible; one block later is invalid for that target epoch.
56. V2 records use the dedicated envelope and never extend the legacy 255-byte nonce limit.
57. Unknown envelope/record versions, nonzero reserved flags, malformed lengths, trailing bytes, and noncanonical outer varints fail closed.
58. Structural and cryptographic budgets are charged before duplicate elimination.
59. A byte-identical valid duplicate is idempotent but still consumes resource budget.
60. A conflicting record with the same semantic key is invalid unless an activated lifecycle rule defines a unique ordering.
61. An invalid duplicate cannot be ignored because a valid record with the same key appeared first.
62. Every v2 transcript binds network ID, genesis hash, protocol version, parameter-set hash, and epoch.
63. Reward validation reads only a qualification set closed before the candidate block.
64. Miner transaction, transactions, tx-extra fields, envelopes, and records are processed in canonical serialized order.
65. One invalid EPoSE record leaves no partial transition from its transaction batch.
66. Coinbase and fee-funded carriers use the same parser, budgets, and state transition.
67. Local relay/template policy never changes validity of a complete block.
68. Disconnect restores descriptors, leases, snapshots, receipts, qualification, payouts, and accounting to the exact parent state.
69. Reorg across any cutoff or anchor rebuilds all dependent objects from replacement canonical blocks.
70. Index/cache loss never turns a required service allocation into an empty-set fallback.
71. All epoch arithmetic is checked unsigned 64-bit arithmetic; overflow fails closed.
72. A reserved but not fully specified record type is invalid in consensus.
73. RandomX remains the sole chain-selection and block-production mechanism after v2 activation.
74. Block validation performs no DNS lookup, live probe, remote RPC, or external API request.
75. Final activation requires approved resource, committee, admission, reward, emission, payment-proof, state-schema, and pruning parameters.
76. CO-02 membership primitives do not become production consensus merely by
    sharing QWC block version 17; only the canonical hardened-v2 dispatcher may
    invoke them, and release remains blocked until every production entry point
    is integrated and tested.
77. A v2 admission lease included at the enrollment cutoff may enter the target snapshot; one included at the committee anchor or later may not.
78. Snapshot membership is canonically ordered and cannot change after the committee anchor.
79. Committee selection reads only the named immutable snapshot and binds its hash, anchor, round, network, genesis, and parameter set.
80. If fewer than the configured number of non-subject verifiers exist, the v2 economic committee is empty; its size and threshold never shrink locally.
81. A prevalidated receipt can contribute only when both identities are in the same frozen snapshot and the verifier is selected for that slot.
82. A receipt at the evidence deadline may contribute; a receipt one block later may not.
83. Qualification closes exactly once and only at the evidence deadline.
84. Arrival order of otherwise independent admission leases cannot change the snapshot hash, committee, or qualification result.
85. Replacing the committee anchor changes the dependent snapshot context; replaying the original anchor restores the original state hash.
86. A v2 positive receipt requires both a valid subject signature and a valid selected-verifier signature.
87. The challenge signature binds network, genesis, parameter set, snapshot, anchor, epoch, round, service kind, both identities, frozen endpoint, nonce, and requested object.
88. The verifier signature binds the subject signature and response digest without a circular transaction or receipt hash.
89. A canonical-object receipt is invalid unless the response-object hash equals the requested-object hash.
90. Receipt cryptographic validation performs no live network operation or wall-clock comparison.
91. A valid receipt for one network, genesis, parameter set, snapshot, anchor, round, endpoint, role assignment, or nonce is invalid after transplant to another.
92. Only a fully authenticated v2 receipt may become a CO-02 prevalidated receipt slot.
93. Subject participation does not prove an independent operator, dedicated machine, continuous uptime, or resistance to a colluding committee.
94. Generic transaction-extra parsing recognizes tag `0x05` only in explicit strict EPoSE-v2 mode; inherited HF16 and legacy-v1 paths cannot authorize it.
95. V2 outer tag and size varints are canonical; overlong or overflowing encodings fail closed.
96. Envelope magic, version, flags, record count, record lengths, and remaining bytes match exactly.
97. Empty envelopes, unknown record types, disabled record versions, and nonzero envelope or record flags are invalid.
98. Record byte and cryptographic costs are charged before semantic validation or duplicate elimination.
99. A failed block-budget charge leaves the previously accumulated budget unchanged.
100. Coinbase and ordinary fee-funded envelope fields use the same structural parser and operation accounting.
101. No default limit makes the envelope activatable; all transaction and block limits come from an approved parameter manifest.
102. V2 service allocation is calculated from scheduled subsidy only; transaction fees are never shared with service nodes.
103. The v2 service share remains exactly 1,000 BPS unless a separately approved tokenomics change says otherwise.
104. An empty v2 qualification set has no implicit fallback; an unset policy fails closed.
105. Permanent non-issuance, if approved, advances scheduled emission without adding the withheld allocation to issued supply.
106. V2 payout rotation uses `(height - payout_epoch_start) mod qualified_count` and only the immediately preceding closed qualification set.
107. A scoped payment proof binds the complete payment context and a proof-excluded coinbase commitment; it cannot be transplanted across height, parent, payee, output, or coinbase.
108. New v2 payment verification does not require or serialize a private reward view key.
109. A zero service allocation due to integer rounding requires no service output or payment proof.
110. Hardened QWC-HF17 reward and payment validation must exclusively use the v2 reward rules; legacy-v1 state cannot supply a payee.
111. A v2 identity ID is stable across online service-key recovery and derives from network/genesis, parameter set, and offline operator authority.
112. Wallet spend secrets and operator-authorization secrets never enter a lifecycle descriptor or daemon signing path.
113. Every lifecycle transition is predecessor- and sequence-bound and takes effect only in a permitted future epoch.
114. Reward-address, endpoint, renewal, deregistration, and service-key changes require offline operator authorization.
115. Service-key recovery requires proof of the replacement key but not the compromised or unavailable old online key.
116. A later lifecycle record cannot change a descriptor already selected for an earlier epoch or settled reward.
117. Duplicate lifecycle records are signature-checked before idempotent handling; an invalid record cannot become acceptable because its message matches stored state.
118. Operator-authority rotation is unsupported until a separate recovery design is approved.
119. An active service public key belongs to at most one stable identity in any target epoch; admission and freeze enforce the same uniqueness independently.
120. Exact receipt duplicates are subject- and verifier-signature checked before idempotent handling; the verifier signing-message hash alone is not a complete-record authentication cache key.
121. Canonical blocks, not the EPoSE index, are the replay oracle and consensus source of truth.
122. Every persisted checkpoint binds height, block hash, parent hash, state root, canonical payload hash, schema, and parameter set.
123. A failed checkpoint connect, image restore, checksum, schema, or context validation leaves committed index state unchanged.
124. Undo is bounded; a reorg beyond retained undo explicitly rebuilds and never silently starts from empty EPoSE state.
125. A → B → A reorg recovery produces the same index root as fresh canonical replay at A.
126. Activation requires EPoSE index updates in the same LMDB write transaction as block connect/disconnect.
127. A missing/corrupt index is recoverable derived data, not grounds for a different block-validity result.
128. Pruned validation remains unsupported until required EPoSE history retention/reconstruction is proved.
129. Endpoint descriptors bind network/genesis, parameter set, service identity, typed transport, canonical host/port, service version, sequence, and expiry.
130. DNS answers, live probes, redirects, elapsed wall time, and external APIs never participate in block validation.
131. Public-internet probes reject local/private/link-local/multicast/unspecified/metadata and IPv4-mapped private destinations and recheck DNS answers at connection time.
132. Probe work is bounded by request/response bytes, timeout, global concurrency, peer count, and per-peer concurrency.
133. Unknown admission contexts are rejected before allocating a RandomX VM, dataset, cache, or pending verification entry.
134. EPoSE RPC list/scan operations require pagination and checked page/scan ceilings.
135. Local overload may reject relay/probe work but cannot change deterministic validation of the same complete block.
136. The proof-excluded Coinbase commitment removes only typed v2 payment-proof records; every unrelated wallet and EPoSE field, output, amount, unlock height, and transaction byte remains committed.
137. A required v2 service payout contains exactly one scoped payment-proof record, and its output set is reconstructed from the actual Coinbase outputs before proof verification.
138. Generic outer `tx_extra` encoding and every embedded v2 envelope are canonical before any record can authorize a state transition or payment.
