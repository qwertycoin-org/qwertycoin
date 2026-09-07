# EPoSE v2 Threat Model

## Sybil Nodes

Attack: one operator starts 10, 100, or 10,000 service identities on one machine, many containers, many IPv4/IPv6 addresses, cloud servers, proxies, or VPNs.

Precondition: service identities are cheap.

Impact: service rewards concentrate with the attacker.

Current Protection: HF17 uses RandomX-bound, identity-scoped admission work and
counts duplicate attestations once. The non-activating v2 pipeline freezes
future-epoch membership before selection.

Residual Risk: the current 16-leading-zero-bit target was measured only with a
slow light-mode solver and can buy a long eligibility interval. It is not an
approved economic Sybil cost. Optimized steady-state x86-64/ARM64 and shared
dataset measurements remain mandatory.

Test: simulate large identity sets and verify reward rotation, bounded state, and cost assumptions.

## Fake Uptime

Attack: a node claims uptime without serving chain data.

Precondition: self-reported uptime is accepted.

Impact: rewards are paid to non-service nodes.

Current Protection: self-attestation is invalid; qualification requires independent signed attestations. The attestation response hash is derived from the challenge, observed tip, subject, verifier, and epoch, so arbitrary or stale response blobs are rejected by consensus.

Residual Risk: colluding verifiers can still lie, and the current tx-extra format does not carry a subject-signed live service response. Mainnet needs a subject-authenticated proof-of-service path or an explicitly documented replacement design.

Test: invalid responses and self-attestations must not qualify.

## Colluding Committees

Attack: selected verifiers attest each other falsely.

Precondition: committee selection can be predicted or dominated.

Impact: unserved identities earn rewards.

Current Protection: HF17 derives its seed from delayed block data. The
non-activating v2 pipeline freezes membership before its anchor and refuses to
shrink committee or threshold when the eligible set is too small.

Residual Risk: a delayed PoW hash is not final or unbiased, and identities are
not independent operators. The previous committee-9 dynamic-quorum conclusion
is superseded for parameter selection. `SECURITY_PARAMETERS.md` quantifies the
capture/liveness trade-off and leaves all v2 constants unapproved.

Test: collusion simulation with attacker-controlled fractions of the registered set.

Tooling: `qwertycoin-epose-sybil-sim` runs deterministic committee exposure scenarios with 10, 25, 50, and 100 total identities and controlled-identity shares around 10%, 20%, 25%, 33%, 40%, and 50%.

## Replay

Attack: old registrations, challenges, or attestations are replayed.

Precondition: messages are not epoch- or network-bound.

Impact: stale proofs affect current rewards.

Current Protection: signatures include network ID and epoch-bound fields. Admission proof verification is bound to the previous finalized epoch seed. Challenges are epoch-bound, response hashes are challenge- and tip-bound, and block application rejects EPoSE payloads whose embedded epoch does not match the block epoch.

Residual Risk: on-chain storage must reject duplicate or expired entries.

Test: replay registration/proof/attestation across epoch, nettype, and previous finalized epoch seed. Focused unit tests now reject registration proof replay under a different epoch seed, attestation replay under a different epoch seed, tampered registration signatures, tampered attestation signatures, offline attestations, and inactive-subject attestations without retaining partial state.

## Reward Theft

Attack: a miner replaces the service reward address or payee.

Precondition: Coinbase validation does not bind payout to deterministic service state.

Impact: service rewards are stolen or omitted.

Current Protection: deterministic payee selection exists, block template
creation adds service reward outputs when a qualified payee exists, and block
validation rejects missing, wrong-amount, wrong-payee, duplicate-output-key,
overpay, or underpay service rewards. Outputs are normal wallet-compatible
one-time outputs derived from the coinbase transaction key and the registered
reward address.

Residual Risk: validators can verify the one-time outputs because each
registration discloses the reward wallet private view key. That makes incoming
activity of the dedicated reward wallet observable to anyone who reads the
registration data. The spend key remains secret and is not disclosed.

Test: block validation must reject wrong service payee after hardfork activation.

## Reorg

Attack: a competing chain crosses epoch boundaries and changes qualified sets.

Precondition: EPoSE state is stored off-chain or not rollback-safe.

Impact: consensus divergence or duplicate rewards.

Current Protection: EPoSE state is derived deterministically from canonical
on-chain registrations, attestations, and block headers. Block application uses
state snapshots and can rebuild from canonical chain data after rollback.

Residual Risk: dedicated LMDB indexes for derived EPoSE state are still not the
primary storage model. Deep reorg, restart/rejoin, and epoch-boundary tests must
remain part of release validation.

Test: reorg A -> B -> A must restore the exact original EPoSE state.

## Oversized Messages

Attack: malformed P2P/RPC payloads allocate unbounded memory.

Precondition: dynamic vectors or strings are accepted before validation.

Impact: memory exhaustion or crash.

Current Protection: EPoSE registration and attestation payloads are fixed-size, parser/state-apply fuzz coverage exists for service-node identities, registration tx-extra payloads, attestations, attestation tx-extra payloads, EPoSE tx-extra extraction, and fail-closed chain-state application. No EPoSE P2P parser exists yet.

Residual Risk: medium until long-running fuzz campaigns, a clean UBSan run, and any future EPoSE P2P/RPC write parsers are covered.

Test: fuzz registration, proof, attestation, P2P and RPC parsers. The current 11-seed EPoSE parser/state-apply corpus passed on seed host A under normal execution, AddressSanitizer, and UBSan, including truncated tx-extra, wrong-network/epoch replay noise, and oversized/noisy EPoSE inputs. ASan execution on seed host A requires the documented unconfined seccomp plus `setarch -R` wrapper because Ubuntu 20 `libasan.so.5` can otherwise crash intermittently during loader-side ASan allocator initialization with host `vm.mmap_rnd_bits=32`. UBSan still reports an inherited Boost serialization static-initialization issue in the `epose_unit_tests` binary before GTest execution, so no clean unit-test UBSan pass is claimed yet.

## Consensus Oracle

Attack: Explorer, Sentinel, DNS, GeoIP, or a foundation service controls reward eligibility.

Precondition: external API results are accepted in block validation.

Impact: centralization and consensus failure.

Current Protection: design forbids external consensus oracles.

Residual Risk: implementation review must keep Sentinel v2 read-only.

Test: block validation must not call external services.
