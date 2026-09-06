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

## Reserved HF18 / EPoSE v2 invariants

These invariants are normative design requirements but are not implemented or
activated by CO-01.

46. Every pre-activation block is interpreted exactly by its historical HF17/v1 rules.
47. No v1 registration, attestation, qualification, or descriptor is implicitly promoted into v2.
48. A v2 activation height is strictly above the release-freeze tip and aligned to 720 blocks.
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
76. CO-02 code is unreachable from HF17 parsing, validation, rewards, RPC, relay, and block templates.
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
94. Pre-HF18 generic transaction-extra parsing does not recognize the reserved EPoSE tag `0x05`.
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
110. HF17 reward and payment validation are unchanged by the non-activating CO-06 primitive.
