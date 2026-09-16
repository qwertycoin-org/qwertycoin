# EPoSE v2 consensus invariants

These invariants summarize properties enforced by the current v2 code and its
unit/integration tests. They are review aids; the implementation remains the
source of truth.

## Chain and activation

1. RandomX proof of work is the sole block-production and chain-selection
   mechanism.
2. Public QWC-HF17 starts at height `0` and uses only EPoSE protocol v2.
3. Mainnet consensus parameters are compiled and bound to the exact genesis
   and parameter-set hashes.
4. Runtime files, CLI options, DNS, RPC, explorer, and monitoring cannot change
   block validity.
5. Legacy-v1 records and configuration never populate v2 state.

## Determinism and ordering

6. The same canonical blocks produce the same lifecycle, membership,
   qualification, reward, and state-commitment results.
7. Transactions, tx-extra fields, envelopes, and records are processed in
   canonical serialized order.
8. Membership is frozen before processing the committee-anchor block.
9. Qualification closes after processing the inclusive evidence-deadline block.
10. Committee selection and payee selection depend only on committed canonical
    data, never arrival order or local cache order.

## Identity and admission

11. Identity is derived from the operator authority and chain/profile context,
    not from endpoint or reward address.
12. Operator and online service authorities are distinct valid keys.
13. One active service key cannot belong to multiple identities.
14. Lifecycle changes are sequence-checked, future-effective, and authorized.
15. Admission work is RandomX-bound to the declared target epoch and canonical
    context block.
16. Late, wrong-context, wrong-target, duplicate-conflicting, or invalid-work
    admissions are rejected.
17. Frozen population cannot exceed the compiled ceiling.

## Service evidence

18. Subject and selected verifier must both belong to the same frozen snapshot.
19. A receipt is bound to network, genesis, parameter set, snapshot, round
    anchor, roles, endpoint, nonce, and requested canonical object.
20. Positive evidence requires both subject and verifier signatures.
21. The response object must equal the requested canonical object in the
    verifier's local chain view.
22. Consensus receipt validation performs no network I/O, DNS lookup, wall-clock
    comparison, or external API call.
23. One semantic receipt slot counts at most once; an authenticated exact
    duplicate is idempotent and a conflict is invalid.
24. Quorum is `ceil(2*n/3)` of the committee that can actually be formed, and
    qualification requires two passing rounds out of three.
25. Evidence received after the round window or qualification deadline cannot
    alter the closed set.

## Carrier and resources

26. V2 records use the dedicated `0x05` transaction-extra field and canonical
    `QEP2` envelope.
27. Unknown versions/types, nonzero flags, malformed lengths, noncanonical
    varints, trailing bytes, and wrong record contexts fail closed.
28. Coinbase and fee-funded carriers use the same structural parser and
    cumulative budgets.
29. Structural and cryptographic work is charged before duplicate elimination.
30. Relay queues and templates are bounded local policy; they never override
    full-block validity.

## Rewards and funds

31. A payout block uses only the immediately preceding closed qualification set.
32. Service reward is exactly 10% of scheduled subsidy; fees remain with the
    miner.
33. An empty source set gives the full scheduled subsidy to the miner.
34. A required service payment has exactly one valid scoped payment proof and
    the exact expected one-time outputs.
35. Missing, duplicate, wrong-recipient, wrong-amount, underpaid, or overpaid
    service allocations invalidate the block.
36. Repeated rewards to one address use distinct one-time output keys and remain
    normal wallet-spendable Coinbase outputs after maturity.
37. EPoSE operator/service keys cannot spend wallet funds; wallet private keys
    are neither serialized nor configured.
38. Public reward RPC mappings are accepted only for an exact canonical block
    whose Coinbase passes the production verifier.

## Atomicity, persistence, and reorgs

39. An invalid record leaves no partial transition from its transaction or block.
40. The LMDB state commitment is written atomically with the canonical block.
41. Disconnect restores the exact parent EPoSE state within the undo window.
42. Deeper reorg/startup recovery replays canonical blocks and verifies stored
    commitments.
43. Missing or corrupt derived state fails closed; it cannot silently convert a
    required service payment into miner fallback.
44. A reorg from branch A to B and back to A restores the original EPoSE state
    hash, qualification set, and reward plan.

Tests covering these boundaries are concentrated in
`tests/unit_tests/epose_*`, `tests/unit_tests/blockchain_db.cpp`,
`tests/unit_tests/test_tx_utils.cpp`, and `tests/epose/`.
