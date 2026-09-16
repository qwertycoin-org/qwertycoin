# EPoSE v2 protocol

This document describes the EPoSE v2 path accepted by the current Qwertycoin
Core. It deliberately excludes the retired EPoSE-v1 prototype.

## Scope and authority

QWC mainnet starts at hardfork version `17` at height `0`. EPoSE protocol
version `2` is active for that public-chain profile. The profile is accepted
only when the selected network is mainnet and the genesis hash matches the
compiled value.

The normative implementation is:

- `src/epose/coordinator_v2.cpp` for the compiled profile and reward planning;
- `src/epose/block_transition_v2.cpp` for atomic block application;
- `src/epose/semantic_batch_v2.cpp` for record semantics;
- `src/epose/*_v2.{h,cpp}` for codecs and state machines;
- `src/cryptonote_core/blockchain.cpp` and
  `src/cryptonote_core/cryptonote_tx_utils.cpp` for block validation and
  Coinbase construction.

Runtime configuration cannot change consensus parameters.

## Consensus profile

The exact values are listed in [`SECURITY_PARAMETERS.md`](SECURITY_PARAMETERS.md).
The identifiers that bind this profile are:

| Item | Value |
| --- | --- |
| QWC hardfork version | `17` |
| EPoSE protocol version | `2` |
| Activation height | `0` |
| Mainnet genesis hash | `4f95857586e2c66063c277370eda99cd75897d773af09f0c3cd1e22f7e87db39` |
| Parameter-set hash | `2c26755094535871dd3ede7bd1b50aba82a9fb6831f0a17f32968eb0385145c6` |

`compiled_consensus_parameters_v2()` rejects another network, genesis, an
incomplete profile, or an invalid parameter set. There is no legacy-v1
fallback on the public QWC-HF17 chain.

## Identity and lifecycle

An identity is derived from the offline operator-authorization public key plus
the network, genesis hash, and parameter-set hash. It is not an IP address, DNS
name, reward address, or online service key.

An identity descriptor binds:

- stable identity ID;
- rotating online service public key;
- operator-authorization public key;
- primary public QWC reward address;
- signed endpoint-descriptor hash;
- monotonically increasing sequence;
- effective and expiry epochs.

The lifecycle supports registration, lease renewal, descriptor update,
deregistration, and service-key recovery. A transition is future-effective and
must reference the previous descriptor. Authorization rules require the
operator key and, where applicable, the current service key. Active identities
cannot share a service key.

The operator/service keystore is bound to the network, genesis, and parameter
set. It does not contain wallet spend or view keys.

## Endpoint descriptors

Endpoint descriptors are signed discovery objects. They contain the online
service public key, canonical transport/host/port, service kind and version,
sequence, expiry epoch, and signature.

Accepted hosts are canonical public IPv4, IPv6, or lowercase DNS names.
Loopback, private, link-local, multicast, unspecified, mapped, malformed, and
other prohibited targets fail closed. Endpoint discovery is bounded and
non-consensus; a descriptor is useful only when its hash is authorized by
canonical lifecycle or frozen membership state.

## Admission

Admission is an epoch-scoped RandomX proof. A lease binds the frozen member,
target epoch, canonical context block, nonce, work hash, and lease hash.

For target epoch `E`, the work context is the block at the start of epoch
`E - 1`. The lease must be included no later than the enrollment cutoff for
`E`. The current target is 18 leading zero bits. Validation charges the
admission-work budget before expensive RandomX verification.

The maximum frozen population is 100 identities. A lease may replace an older
lease for the same identity and target epoch only with a higher descriptor
sequence. Byte-identical valid duplicates are idempotent; conflicting semantic
keys are invalid.

## Epoch timing

With activation height `A = 0`, epoch length `L = 720`, and anchor depth
`D = 60`:

```text
epoch_start(E)       = E * L
epoch_end(E)         = epoch_start(E) + L - 1
enrollment_cutoff(E) = epoch_start(E) - D - 1
committee_anchor(E)  = epoch_start(E) - D
evidence_deadline(E) = epoch_end(E) - D
```

Epoch `0` is the activation epoch. Epoch `1` is the first service epoch. The
first possible service payout is at the start of epoch `2` (height `1440`).

Membership is frozen before records in the committee-anchor block are applied.
Therefore records through the cutoff are eligible, while records in the anchor
block are not. Qualification closes after records in the inclusive deadline
block are applied.

## Frozen membership and committees

At the committee anchor, accepted leases and their authorized lifecycle
descriptors are sorted canonically and committed to a membership snapshot.
Later descriptor, admission, or arrival-order changes cannot alter that
snapshot.

For each subject and round, candidate verifiers are every other frozen member.
Selection hashes the network, genesis, parameter set, snapshot, epoch, round,
round anchor, subject, and candidate. The lowest scores form a committee of at
most nine members.

The quorum is computed from the committee that can actually be formed:

```text
actual_committee = min(9, frozen_population - 1)
required_receipts = ceil(2 * actual_committee / 3)
```

The configured 6-of-9 threshold is validated as the full-size case. A subject
must pass at least two of three rounds. Round offsets are `0`, `200`, and `400`
blocks from the service-epoch start.

## Canonical service receipts

The implemented service kind is `canonical-object`. A challenge binds:

- protocol version and service kind;
- epoch, round, snapshot, and canonical round anchor;
- subject and selected verifier service keys;
- subject endpoint-descriptor hash;
- nonce and requested canonical-object hash.

The subject returns the canonical block bytes and signs the response. The
selected verifier validates those bytes against its own canonical chain view,
then signs a receipt that also commits to the subject signature. Both
signatures are required. A response whose object hash differs from the
requested object hash is invalid.

Consensus validates the transcript only. It performs no DNS lookup, wall-clock
comparison, live network request, or remote RPC call while validating a block.

Round zero uses the pre-epoch committee anchor. Later rounds use an anchor
inside the epoch, so their receipts cannot be included in the same block that
creates the anchor. Each `(epoch, round, subject, verifier, service-kind)` slot
accepts one authenticated receipt; an exact authenticated duplicate is
idempotent and a conflict is invalid.

## Qualification

At the evidence deadline, Core counts distinct verifier keys per subject and
round. A round passes at the dynamically computed quorum. A subject qualifies
when at least two rounds pass. The canonically ordered qualified key set and
its hash are then immutable for that epoch.

An empty or one-member snapshot produces no qualified identities because an
independent verifier committee cannot be formed.

## Envelope carrier

EPoSE v2 uses transaction-extra tag `0x05`. The envelope magic is `QEP2` and
the envelope version is `1`. Supported version-1 record types are:

1. identity descriptor;
2. admission lease;
3. service receipt;
4. lifecycle authorization;
5. service payment proof.

The same canonical parser handles Coinbase and ordinary fee-funded carriers.
Payment proofs are Coinbase-only. Legacy nonce records are not accepted as v2
records after activation.

The parser rejects unknown types or versions, nonzero flags, empty envelopes,
noncanonical or overflowing varints, malformed lengths, trailing bytes, wrong
record context, and all configured resource-limit violations. Core completes
the cheap structural and cumulative-budget pass for the entire block before it
runs signature or RandomX verification.

## Relay and mining templates

Signed envelopes are relayed over typed P2P messages and stored in a bounded,
non-consensus queue. Queue admission is context-bound, idempotent, conflict
safe, and deadline aware. Capacity is reserved separately for enrollment and
evidence traffic. Template selection is deterministic and bounded.

Relay contents never determine block validity. A node receiving a complete
block reparses and validates its records from canonical bytes. A normal miner
does not need a service-node keystore and a service node does not need to mine.

## Rewards

Payouts begin in epoch `2`. A block in payout epoch `E` uses the closed
qualification set from source epoch `E - 1`. Selection is deterministic and
uses the epoch payout anchor plus the block position. Details are in
[`REWARDS.md`](REWARDS.md).

The service allocation is exactly 10% of scheduled subsidy. Transaction fees
remain entirely with the miner. If the source qualification set is empty, the
miner receives the full scheduled subsidy.

## Atomic state and reorgs

Block processing is fail-atomic:

1. parse every transaction and charge structural budgets;
2. freeze membership when the block is an anchor boundary;
3. apply records in canonical transaction and wire order to a copied state;
4. verify resource-accounting consistency;
5. close qualification when the block is the inclusive deadline;
6. commit the new state and its LMDB commitment only if every step succeeds.

Disconnect restores the exact parent state from bounded undo data. A reorg
deeper than the in-memory undo window triggers canonical replay from persisted
state. Missing, corrupt, wrong-genesis, wrong-parameter, or inconsistent state
commitments fail closed; they do not turn a required payout into miner fallback.

## Retired v1 surfaces

The daemon still recognizes old `--service-node*` option names so it can return
a deterministic error. It does not load a v1 key, construct v1 state, request a
wallet private view key, or create a funded registration transaction.

`get_service_node_registration_payload` is retained as a compatibility RPC but
returns an error explaining that v2 enrollment is automatic. No v1 record is
promoted into v2 state.
