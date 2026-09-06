# EPoSE v2 Adversarial Acceptance Matrix

**Date:** 2026-09-06
**Release verdict:** **NO-GO**

`Satisfied` below means the named property has direct executable evidence at the
current layer. `Partial` means a non-activating primitive is covered but the
canonical handler or multi-node result is still absent. `Not run` means no valid
final-protocol execution can yet be performed. Partial rows remain release
blockers.

| Scenario | Status | Current evidence | Required final evidence |
|---|---|---|---|
| Post-seed subject/verifier grinding | Partial | CO-02 snapshot unit tests | HF18 block/reorg test on canonical state |
| Registration changes after prior votes | Partial | CO-02 fixed-snapshot tests | Serialized records through connect/disconnect and replay |
| Malicious threshold approves offline identity | Partial | CO-03 exact probability model | Parameter-bound network simulation and residual-risk statement |
| Missing, forged, or replayed subject signature | Partial | CO-04 dual-signature negative tests | Live transport plus block-carrier rejection |
| One backend serves many identities | Partial | CO-03 concentration model | Multi-identity proxy experiment; no independence claim |
| Honest endpoint eclipsed or stale | Not run | Threat model only | Partition/stale-anchor network fixture without validation split |
| Targeted verifier/payee outage | Partial | CO-03 availability model | Adaptive outage and settled-payout network test |
| Inbound-only operators skip verification | Partial | CO-03 duty model | Measured verifier costs and final incentive analysis |
| Registration flood near evidence expiry | Partial | CO-05 atomic budgets | Integrated deadline-aware template and sustained backlog test |
| Fee-funded versus coinbase carrier | Partial | CO-05 byte-equivalence fixtures | Wallet-funded and miner-template blocks yielding equal state |
| Reorg across enrollment/seed, A-B-A | Partial | CO-02/08 in-memory replay | LMDB, deep reorg, restart, and fresh-replay equality |
| Receipt at deadline -1/0/+1 | Partial | CO-01/02 boundary tests | Serialized canonical-block vectors on independent hosts |
| Reorg beyond retained undo | Partial | CO-08 returns rebuild-required | Full rebuild from canonical/pruned supported history |
| Crash during connect or migration | Not run | Fail-atomic in-memory restore only | Process-kill injection around LMDB transactions |
| Maximum valid block and malicious relay | Partial | CO-05/09 local counters | p95/p99 CPU/RAM/sync measurements under sustained load |
| Unknown version/noncanonical crypto/malformed length | Partial | CO-04/05 parser/signature negatives | Fuzz/sanitizer matrix at integrated handler boundary |
| Conflicting descriptors/duplicate receipts/retries | Partial | CO-02/04/07 deterministic tests | Canonical record processing from real blocks |
| Miner omits or redirects service payout | Partial | CO-06 payment-proof unit tests | Coinbase construction/validation positive and negative blocks |
| Empty qualified set | Not run | CO-06 alternatives fail closed | Owner-approved policy plus emission golden vectors |
| Miner and service use same wallet | Partial | CO-06 allocation model | Matured spend and rescan on final wallet binary |
| Proof transplanted to another coinbase/output | Partial | CO-06 substitution tests | Independent cryptographic review and block-level vectors |
| Ten rewards to the same wallet | Partial | Unique one-time keys unit-tested | Ten matured, independently spent outputs with no collision |
| Fresh sync, restart, and pruned/full convergence | Not run | CO-08 journal primitive | Final LMDB schema across four nodes and fresh replay |
| Old history across activation | Partial | CO-01 version boundary model | Full pre/post-activation chain and deep-reorg validation |

## Interpretation

No row may be marked satisfied merely because a lower-level helper passed. A
release row closes only when the final serialized protocol traverses the same
parsing, state, storage, reward, wallet, and reorg paths used by a production
node. The machine-readable top-level status remains in
`RELEASE_GATES_V2.json`.
