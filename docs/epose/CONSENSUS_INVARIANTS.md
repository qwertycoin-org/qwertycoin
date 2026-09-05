# Consensus Invariants

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
22. Automatically generated miner attestations must use the same response-hash construction that block validation enforces.
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
