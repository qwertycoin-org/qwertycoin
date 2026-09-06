# EPoSE Protocol

> **Status:** EPoSE protocol version 1 is the only implemented consensus
> protocol. The hardened protocol described in "Reserved hardened protocol"
> below is a normative design reservation for CO-01. It is not activated and
> must not be accepted, relayed as consensus evidence, or paid by current
> binaries.

## Constants

Defined in `src/epose/service_node.h`:

- `EPOSE_PROTOCOL_VERSION = 1`
- `EPOSE_EPOCH_LENGTH = 720`
- `EPOSE_FINALITY_DEPTH = 60`
- `EPOSE_REGISTRATION_TTL_EPOCHS = 30`
- `EPOSE_VERIFIER_COMMITTEE_SIZE = 9`
- `required_attestations = ceil(actual_committee_size * 2 / 3)`
- `EPOSE_SERVICE_REWARD_BPS = 1000`
- `EPOSE_ADMISSION_LEADING_ZERO_BITS = 16`
- `EPOSE_IDENTITY_BLOB_SIZE = 249`
- `EPOSE_ATTESTATION_BLOB_SIZE = 234`
- `EPOSE_TX_EXTRA_NONCE_REGISTRATION = 0x70`
- `EPOSE_TX_EXTRA_NONCE_ATTESTATION = 0x71`
- `EPOSE_ATTESTATION_POOL_MAX_ENTRIES = 4096`
- `EPOSE_ATTESTATION_RELAY_MAX_BATCH = 32`

EPoSE consensus is active from genesis for `HF_VERSION_QWC_EPOSE_V1 = 17`.

Mainnet, testnet, and stagenet start directly at QWC protocol v17 from height 0. QWC v17 inherits Monero's current v16 consensus rules and adds EPoSE; historical Monero hardforks are not scheduled as later QWC activations. Low-height activation for deterministic regression tests uses local `HardFork` test fixtures.

The current v1 parameters are committee target `9`, dynamic
`ceil(actual_committee_size * 2 / 3)` quorum, and `16` admission leading-zero
bits. Older documents that state `5`, `2`, or `8` describe superseded PoC
revisions and are not authoritative.

## Service Node Identity

```text
ServiceNodeIdentity {
  version
  service_public_key
  reward_address
  reward_view_secret_key
  endpoint_commitment
  registration_epoch
  expiry_epoch
  admission_nonce
  admission_hash
  signature
}
```

The registration signature signs all fields except `signature` and includes the QWC network ID. A registration is invalid if:

- version is unsupported,
- public key is invalid,
- expiry is not after registration,
- TTL exceeds `EPOSE_REGISTRATION_TTL_EPOCHS`,
- reward view secret key does not derive to the reward address public view key,
- signature verification fails.

### Identity Binary Encoding

All integer fields are little-endian. Fixed-size crypto values use their canonical byte representation.

```text
uint8   version
bytes32 service_public_key
bytes32 reward_spend_public_key
bytes32 reward_view_secret_key
bytes32 endpoint_commitment
uint64  registration_epoch
uint64  expiry_epoch
uint64  admission_nonce
bytes32 admission_hash
bytes64 signature
```

Total size: 249 bytes.

The reward view public key is derived from `reward_view_secret_key` while parsing
the identity. This keeps the registration payload at 249 bytes while disclosing
the information validators need for deterministic service-reward validation.

When transported through Monero's existing `tx_extra_nonce` field, the identity blob is prefixed with `EPOSE_TX_EXTRA_NONCE_REGISTRATION`. The resulting payload is 250 bytes and remains within `TX_EXTRA_NONCE_MAX_COUNT = 255`.

EPoSE transaction-extra extraction uses Monero's existing `parse_tx_extra` parser first, then inspects `tx_extra_nonce` entries. Non-EPoSE nonce subtypes are ignored. A malformed EPoSE registration nonce is rejected instead of being partially accepted.

## Admission Proof

The admission hash is network-, identity-, reward-, endpoint-, epoch-, previous-epoch-hash-, and nonce-bound. It is calculated with RandomX using the previous finalized epoch hash as the RandomX seed and the following domain-separated payload:

```text
RandomX(
  seed = previous_finalized_epoch_hash,
  data = "QWC_EPOSE_ADMISSION_V1"
       || network_id
       || service_public_key
       || reward_spend_public_key
       || reward_view_public_key
       || reward_view_secret_key
       || endpoint_commitment
       || registration_epoch
       || previous_finalized_epoch_hash
       || admission_nonce
)
```

A proof is valid if the calculated RandomX hash equals `admission_hash` and meets the leading-zero target. Verification cost is one RandomX hash plus fixed-size parsing/signature checks.

The current hardened target is `EPOSE_ADMISSION_LEADING_ZERO_BITS = 16`. This is intentionally lower than the initial 18/20/22/24-bit candidates because the current RandomX light-mode admission search measured on seed host A is too slow for those values. A 16-bit target is still 256x more expensive than the previous 8-bit target and remains inside the intended registration-cost corridor for the measured implementation.

## Attestation

```text
ServiceAttestation {
  version
  verifier_public_key
  subject_public_key
  epoch
  challenge_hash
  response_hash
  observed_tip_hash
  service_ok
  signature
}
```

The signature includes the network ID and all fields except `signature`.

The response hash is consensus-derived:

```text
response_hash =
  H("QWC_EPOSE_RESP_V1"
    || epoch
    || challenge_hash
    || observed_tip_hash
    || subject_public_key
    || verifier_public_key)
```

This keeps the current tx-extra payload bounded while binding an attestation
response to the exact challenge, observed chain tip, subject, verifier, and
epoch. It is still not a complete mainnet proof-of-service handshake because
the on-chain attestation is signed by the verifier, not by the subject service
node. Mainnet hardening must add or specify a subject-authenticated response
path without exceeding transport limits.

An attestation is invalid if:

- version is unsupported,
- `service_ok` is false,
- verifier and subject are equal,
- either public key is invalid,
- `response_hash` does not match the deterministic response binding,
- signature verification fails.

### Attestation Binary Encoding

```text
uint8   version
bytes32 verifier_public_key
bytes32 subject_public_key
uint64  epoch
bytes32 challenge_hash
bytes32 response_hash
bytes32 observed_tip_hash
uint8   service_ok
bytes64 signature
```

`service_ok` must be exactly `0` or `1`. Total size: 234 bytes.

When transported through Monero's existing `tx_extra_nonce` field, the attestation blob is prefixed with `EPOSE_TX_EXTRA_NONCE_ATTESTATION`. The resulting payload is 235 bytes and remains within `TX_EXTRA_NONCE_MAX_COUNT = 255`.

EPoSE attestation extraction follows the same rule: unrelated nonce payloads are ignored, while malformed EPoSE attestation payloads make extraction fail closed.

## Attestation Relay

Service nodes publish signed EPoSE payloads through `NOTIFY_NEW_EPOSE_PAYLOADS`.
The message carries serialized `service_node_identity` registration blobs and
serialized `service_attestation` blobs, not wallet transactions. Normal nodes
validate, deduplicate, keep valid candidates in an in-memory attestation pool,
and relay only newly accepted payloads.

The pool is not consensus state. It is a bounded relay/template policy cache.
Canonical EPoSE state is derived only from registrations and attestations that
are included on-chain and accepted by `chain_state::apply_tx_extra`.

Normal non-service-node miner daemons may include valid relayed EPoSE payloads
from their pool in `miner_tx.extra`. A miner does not need a service-node key,
and a service node does not need to mine locally in order for its registrations
or attestations to reach the chain.

## Qualification

For epoch `E`, a registered node qualifies if:

```text
registration_epoch <= E < expiry_epoch
and
unique_valid_attestations(subject, E) >= required_attestations
```

`required_attestations` is a dynamic quorum, not a fixed constant:

```text
actual_committee_size = size(select_verifiers(active_nodes, subject, target = 9))
required_attestations = ceil(actual_committee_size * 2 / 3)
```

Expected thresholds:

```text
N=1 -> 1
N=2 -> 2
N=3 -> 2
N=4 -> 3
N=5 -> 4
N=6 -> 4
N=7 -> 5
N=8 -> 6
N=9 -> 6
```

The resulting qualified set is sorted by service public key and deduplicated.
Duplicate votes by the same verifier for the same subject/epoch do not add
additional weight. Self-attestations are invalid, and only deterministic
committee members can attest for a subject.

Qualification in epoch `E` is not paid in epoch `E`. Blocks in epoch `E + 1` select rewards only from the finalized qualified set of epoch `E`. This prevents miners from changing a block's current service reward by selecting, ordering, or withholding attestation transactions in the block or elsewhere in the same reward epoch.

## Deterministic Chain State

The current implementation contains an in-memory deterministic `chain_state` helper that applies EPoSE payloads extracted from transaction extra fields:

```text
apply_tx_extra(tx_extra, previous_epoch_hash):
  registrations = extract_registrations_from_tx_extra(tx_extra)
  attestations = extract_attestations_from_tx_extra(tx_extra)
  snapshot current_state
  apply every registration through service_registry_state
  verify every attestation signature
  require the verifier to be in the deterministic subject committee
  require the deterministic challenge hash for round 0
  require the deterministic response hash for the challenge and observed tip
  prune expired registrations and old attestations after successful epoch application
  if any EPoSE payload is invalid:
    restore snapshot
    reject
```

This is intentionally not LMDB persistence yet. It is the deterministic connect/disconnect primitive that block processing calls while coupling the EPoSE view to the existing blockchain database transaction. Expired registrations and old attestations are retained for a bounded epoch window and then pruned deterministically, so equivalent canonical chains produce equivalent state hashes without unbounded in-memory growth.

## Payout Selection

For every qualified service key:

```text
rank = H("QWC_EPOSE_PAYOUT_V1" || epoch_seed || service_public_key)
```

The ranked set is sorted by `rank`, then by public key as tie-breaker.

At block height `H` in reward epoch `R`, the source epoch is:

```text
source_epoch = max(0, R - 1)
```

The payee is selected from `qualified_service_nodes(source_epoch)`:

```text
selected = ranked[H % ranked.size]
```

If no node qualifies, the service reward remains with the miner for that block until a final consensus rule says otherwise.

## Service Reward Output

When a qualified payee exists, the block reward is split by:

```text
total_reward = base_reward + transaction_fees
service_reward = floor(total_reward * EPOSE_SERVICE_REWARD_BPS / 10000)
miner_reward = total_reward - service_reward
```

`EPOSE_SERVICE_REWARD_BPS = 1000`, so the current implementation pays 10% to
one selected service node and 90% to the miner. Whether final public mainnet
keeps fee sharing or switches to subsidy-only service rewards is still an
explicit tokenomics decision.

The service reward is paid as normal CryptoNote one-time outputs. The miner
transaction uses its normal transaction public key `R = rG`; the selected
service node registration provides a standard reward address `(A, B)` and the
matching private view key `a`.

Block construction derives each service output key with the standard derivation:

```text
derivation = rA
P = derive_public_key(derivation, output_index, B)
```

The 10% service amount is decomposed into normal amount denominations before
outputs are added. This keeps wallet scanning normal and avoids practically
unspendable unique amount classes for large per-block service rewards.

Validation reads `R` from the coinbase transaction extra and recomputes:

```text
derivation = aR
P_expected = derive_public_key(derivation, output_index, B)
```

All matching EPoSE output amounts, sorted ascending, must equal the denomination
decomposition of the expected service reward. Coinbase validation also rejects
duplicate output public keys anywhere in the miner transaction.

---

## Reserved hardened protocol (CO-01)

### Status and compatibility boundary

The hardened protocol reserves these identifiers:

| Identifier | Reserved value | Activation status |
|---|---:|---|
| QWC hardfork version | `18` | Reserved; no activation height assigned |
| EPoSE protocol version | `2` | Reserved; parser/transition not implemented |
| `tx_extra` envelope tag | `0x05` | Reserved in the pinned QWC/Monero tag space |
| Envelope format version | `1` | Reserved for the first v2 envelope |

Reservation prevents two QWC features from choosing the same identifiers. It
does not make any v2 byte sequence valid. Before the activation manifest is
complete, every block is validated exactly as HF17/v1 validates it today.

An activation proposal MUST set an activation height `A` such that:

```text
A > observed_mainnet_tip_at_release_freeze
A mod 720 = 0
block.major_version < 18 for heights < A
block.major_version >= 18 for heights >= A
```

Historical HF17 blocks and v1 payloads retain their original meaning. At and
after `A`, new v1 registration and attestation payloads are rejected; historical
v1 state is read-only and cannot supply a v2 committee or payee. There is no
implicit migration of an identity, descriptor, admission lease, receipt, or
reward destination from v1 to v2.

The activation height, activation block hash, parameter-set hash, reward-policy
identifier, and final resource limits remain deliberately unassigned. A binary
MUST refuse to advertise v2 activation while any required manifest field is
unassigned.

### Consensus inputs and processing order

The v2 transition is a pure function of:

```text
transition(parent_state, canonical_block_bytes, active_parameter_manifest)
    -> new_state | invalid_block
```

No DNS result, wall clock, live probe, RPC response, relay pool, explorer,
database iteration order, or local cache is an input. For a candidate block at
height `H`, validation proceeds in this exact order:

1. Determine the active hardfork and parameter-set hash from the parent state.
2. Derive all reward expectations from qualification sets closed before `H`.
3. Validate the miner transaction, including any required service allocation.
4. Parse every transaction in canonical block order: miner transaction first,
   then ordinary transactions in the serialized order in the block.
5. Within one transaction, parse `tx_extra` fields in serialized order.
6. Within one EPoSE envelope, parse records in serialized order while charging
   resource budgets before semantic duplicate elimination.
7. Validate every record against the immutable snapshot and height window that
   its context names.
8. Apply the transaction batch atomically. One malformed or invalid EPoSE record
   invalidates the transaction's EPoSE transition; no prefix is retained.
9. Close any cutoff reached at `H` only after all valid records in block `H`
   have been applied.
10. Commit EPoSE state in the same chain database transaction as the block.

Reward validation therefore never reads registrations or receipts introduced
by the candidate block. Implementations may cache derived data, but cache loss
or iteration order cannot change the result.

### Epoch pipeline

Let:

```text
B = 720 blocks
D = 60 blocks
start(E) = E * B
end(E) = start(E) + B - 1
enrollment_cutoff(E) = start(E) - D - 1
committee_anchor(E) = start(E) - D
evidence_deadline(E) = end(E) - D
payout_seed_height(E) = start(E + 1) - D
first_payout_height(E) = start(E + 1)
```

All arithmetic is unsigned 64-bit checked arithmetic. A value that overflows or
an epoch whose `start(E)` cannot be represented is invalid. `E = 0` has no v2
service, qualification, or payout semantics.

If activation occurs at `A = start(U)`:

- epoch `U` is enrollment-only;
- epoch `U + 1` is the first measurable service epoch;
- epoch `U + 2` is the first possible payout epoch;
- the first possible v2 payout height is `A + 2 * B`.

```text
epoch U                  epoch U+1                    epoch U+2
activation/enrollment -> measured service/settle -> first v2 payout
          | cutoff ----> anchor ---- receipts ----> deadline | payout seed
          |                immutable snapshot(E)              |
          +---------------------------------------------------> Q(E) only
```

For service epoch `E >= U + 1`:

1. leases and descriptor versions targeting `E` are accepted no later than
   `enrollment_cutoff(E)`;
2. after that block, the admitted identity set and descriptor versions are
   frozen into `snapshot(E)`;
3. `committee_anchor(E)` supplies the committee seed only after membership is
   frozen;
4. receipts are accepted only in their specified round window and no later
   than `evidence_deadline(E)`;
5. after the deadline block, `qualification(E)` is immutable;
6. the payout seed is read from `payout_seed_height(E)` after qualification is
   closed;
7. payout epoch `E + 1` uses only qualification set `E`; the entitlement is
   valid from `start(E + 1)` through `end(E + 1)` and is stale afterward.

The terms **anchor** and **settled** describe depth and closed state only. They
do not assert finality. A higher-work reorg replaces every dependent snapshot,
committee, receipt set, qualification set, payout schedule, and state root.

### Dedicated envelope

V2 MUST NOT use `tx_extra_nonce` for protocol records. The reserved outer field
uses normal `tx_extra` framing:

```text
varint tag = 0x05
varint envelope_size
bytes[envelope_size] envelope
```

The envelope byte order is little-endian and has this canonical form:

```text
bytes4 magic = "QEP2"
uint8  envelope_version = 1
uint8  flags = 0
uint16 record_count
uint32 records_size
record records[record_count]
```

`records_size` MUST equal the exact remaining byte count. A record is:

```text
uint8  record_type
uint8  record_version
uint16 flags = 0
uint32 payload_size
bytes[payload_size] payload
```

Integers are fixed-width little-endian; varints are used only by the inherited
outer `tx_extra` framing and MUST use its canonical encoding. Unknown envelope
versions, unknown record types, unsupported active record versions, nonzero
flags, truncated fields, trailing bytes, count mismatches, noncanonical outer
varints, or any manifest limit violation invalidate the transaction.

The reserved record-type registry is:

| Type | Name | First record version |
|---:|---|---:|
| `0x01` | identity descriptor | `1` |
| `0x02` | admission lease | `1` |
| `0x03` | service receipt | `1` |
| `0x04` | descriptor lifecycle authorization | `1` |
| `0x05` | scoped service-payment proof | `1` |

CO-02, CO-04, CO-06, and CO-07 finalize the payload fields and cryptographic
transcripts for these records. Until the relevant specification and executable
vectors are merged, their versions remain reserved and invalid in consensus.

### CO-02 frozen membership implementation

`src/epose/membership_v2.*` implements the deterministic, non-activating v2
membership pipeline. It does not parse a v2 record and is not called by HF17
block validation. Its purpose is to make the cutoff, snapshot, committee, and
qualification transition executable before later change orders finalize the
wire records and cryptographic proofs.

The module accepts only explicitly **prevalidated** admission leases and
receipt slots. This boundary is intentional: CO-03 supplies calibrated
admission parameters and proof validation; CO-04 supplies authenticated receipt
parsing and signatures; CO-05 supplies the bounded transaction-extra envelope
and carriers; CO-06 supplies payout and payment-proof semantics; and CO-07
supplies descriptor lifecycle authorization. Until those layers exist, no
network input can reach this module and no v2 state can affect a block reward.

An admitted member freezes these selection-relevant commitments:

```text
service_public_key
descriptor_hash
reward_binding_hash
endpoint_descriptor_hash
sequence
target_epoch
lease_hash
inclusion_height
```

At `committee_anchor(E)`, the module builds `snapshot(E)` only from leases for
`E` included at or before `enrollment_cutoff(E)`. Members are sorted by service
public key. The snapshot hash binds the `QWC_EPOSE_SNAPSHOT_V2` domain, network
ID, genesis hash, parameter-set hash, epoch, cutoff and anchor heights, anchor
hash, and every ordered frozen member.

Committee scoring uses `QWC_EPOSE_SELECT_V2` and binds the same global context
plus snapshot hash, epoch, round, anchor hash, subject, and candidate. It never
reads the live v1 registry. If fewer than the configured number of non-subject
candidates exist, it returns no committee instead of shrinking the economic
threshold.

CO-02 models a future CO-04 receipt only as a cryptographically-prevalidated
slot `(epoch, round, service_kind, subject, verifier, receipt_hash)`. Subject
and verifier must be in the same frozen snapshot, and the verifier must be
selected for that subject and round. A byte-identical slot is idempotent; a
different receipt hash for the same slot is a conflict. Receipts are accepted
from `start(E)` through `evidence_deadline(E)`, inclusive.

At exactly `evidence_deadline(E)`, qualification closes once. The generic
committee policy is constructor-supplied because CO-03 has not approved
mainnet values. It never falls back to v1's dynamic small-network quorum. The
closed qualification hash binds network, genesis, parameter set, snapshot,
epoch, close height, and the ordered qualified keys.

The complete pipeline state hash commits to admitted leases, snapshots,
accepted receipt slots, and closed qualification sets. Copy/replay tests use it
as the current in-memory reorg oracle. Persistent LMDB/undo integration remains
CO-08.

## CO-04 authenticated receipt core

`src/epose/service_receipt_v2.*` implements the bounded cryptographic transcript
between CO-02's frozen membership layer and the future probe/carrier layers. It
does not perform DNS, socket, HTTP, RPC, or wall-clock operations.

The currently supported service kind is `canonical_object = 1`. A verifier
requests an object by its canonical cryptographic hash. The live probe layer
must retrieve bounded canonical bytes from the frozen endpoint descriptor and
verify that their hash equals `requested_object_hash` before signing. The
on-chain receipt carries the digest, not the returned bytes. Consequently the
chain verifies a dual-signed claim about successful retrieval; colluding keys
can still synthesize that claim and remain covered by the CO-03 threat model.

The challenge commits to version 2, service kind, epoch, round, snapshot and
anchor hashes, both service public keys, the frozen endpoint-descriptor hash, a
fresh 32-byte nonce, and the requested canonical-object hash.

`QWC_EPOSE_CHALLENGE_V2` additionally binds network ID, genesis hash, and
parameter-set hash. `QWC_EPOSE_SUBJECT_RESPONSE_V2` binds that challenge hash
and the returned object hash; the subject signs it. `QWC_EPOSE_RECEIPT_V2`
binds the subject-response hash and subject signature; the verifier signs it.
There is no circular transcript.

Validation rejects a missing or bad subject signature, bad verifier signature,
self-vote, role swap, unsupported service kind, null context/commitment, wrong
object, or any context transplant. Only a fully valid receipt can be converted
to CO-02's `prevalidated_receipt_slot_v2`.

The legacy HF17 automation that fabricated its response locally and set
`service_ok = true` is disabled. Historical HF17 blocks retain their original
validity, but this software no longer originates automatic positive HF17
attestations. The eventual live probe transport depends on the bounded carrier
and endpoint/lifecycle work in CO-05/CO-07; until then automation fails closed
instead of fabricating service.

## CO-05 bounded envelope primitive

`src/epose/envelope_v2.*` implements the dedicated `0x05` field and `QEP2`
envelope format specified above without adding `0x05` to the historical HF17
generic `tx_extra` variant. This preserves pre-activation parser behavior.

The parser accepts constructor-supplied limits only; no fallback mainnet values
exist. It validates canonical outer varints, magic, versions, zero flags, exact
record count and byte lengths, per-record payload size, per-envelope byte and
record counts, and signature/RandomX operation budgets. Empty envelopes and
reserved record versions are invalid. Costs are charged by record type before
payload semantics or duplicate handling.

`parse_transaction_envelope_fields_v2` is intentionally carrier-neutral: a
version-aware caller supplies exact `0x05` fields from either the miner
transaction or an ordinary fee-funded transaction and receives the same
records and budget. `charge_block_budget_v2` accumulates byte, record,
signature, and admission costs transaction by transaction without mutating the
committed budget on failure.

The primitive is non-activating. Version-aware integration into generic
`tx_extra`, actual record payload codecs, miner template batching, wallet-funded
submission, queue fairness, and measured manifest limits remain required before
HF18 can recognize tag `0x05` in a block.

## CO-06 reward and payment-proof candidate

`src/epose/reward_v2.*` implements non-activating v2 reward arithmetic,
epoch-relative payee rotation, and a scoped payment-proof candidate. V2 service
rewards are calculated from scheduled subsidy only; fees remain entirely with
the miner. The service share remains 1,000 BPS.

The empty-qualified-set policy has no default. The reference implementation
models both miner fallback and permanent non-issuance, but returns
`unresolved_empty_policy` when the caller supplies `unset`. Neither option is
wired to block construction or validation pending an explicit economic ADR.

The scoped proof publishes a recipient-specific key derivation and uses the
inherited transaction-proof primitive to bind it to the coinbase transaction
secret. Its non-circular transcript commits to the network/genesis, parameter
set, height, parent, payout/qualification context, public reward keys, exact
service outputs, transaction public key, and a coinbase commitment calculated
with the proof record omitted. Validators can recompute the prescribed
one-time output keys without a private view key.

This payment-proof construction supports primary standard reward addresses
only and remains non-activating until independent cryptographic review. See
`review/ADR-0003-REWARD-SEMANTICS.md`.

## CO-07 identity lifecycle candidate

`src/epose/lifecycle_v2.*` implements a future-effective lifecycle state
machine for registration, renewal, descriptor update, deregistration, and
online service-key recovery. A stable identity ID is derived from the
network/genesis, parameter set, and an offline operator-authorization public
key. The online service key and public reward address remain separate.

Every transition commits its predecessor hash and exact next sequence. Both
the offline operator authority and the service key named by the next descriptor
must sign. Consequently, an online-key compromise alone cannot redirect future
rewards or rewrite the endpoint, while recovery does not depend on the old
compromised key. Effective epochs are supplied with the cutoff-derived minimum
and cannot be retroactive.

Epoch lookup returns the descriptor valid for that epoch, so later records do
not mutate a descriptor already frozen into a membership/reward snapshot. This
primitive remains non-activating pending record serialization, envelope and
persistent-index integration. See `review/ADR-0004-IDENTITY-LIFECYCLE.md`.
No implementation may guess missing fields or accept an opaque payload merely
because its outer envelope parses.

### Duplicate and conflict semantics

Every record has a protocol-defined semantic key. At minimum the key contains
network/genesis identity, protocol version, parameter-set hash, target epoch,
record type, service identity, and sequence or receipt slot as applicable.

- A byte-identical repeat of an already valid record is idempotent and does not
  add influence, but it still consumes all block parsing and verification
  budgets.
- A different byte sequence with the same semantic key is a conflict and is
  invalid unless a later lifecycle specification explicitly defines a unique
  sequence ordering before activation.
- An invalid record is never ignored because a valid record with the same key
  appeared earlier.
- Receipt retries map to one slot `(snapshot, epoch, round, subject, verifier,
  service_kind)` and can contribute at most one positive vote.
- Canonical block order resolves only operations that the protocol explicitly
  declares independent. Hash-map or mempool arrival order never resolves a
  conflict.

### Domain separation and context

Every v2 hash or signature transcript begins with an ASCII domain string ending
in `_V2`, followed by the fixed context below:

```text
bytes16 network_id
bytes32 genesis_hash
uint8   epose_protocol_version = 2
bytes32 parameter_set_hash
uint64  epoch
```

Reserved domains are:

```text
QWC_EPOSE_DESCRIPTOR_V2
QWC_EPOSE_ADMISSION_V2
QWC_EPOSE_SNAPSHOT_V2
QWC_EPOSE_SELECT_V2
QWC_EPOSE_CHALLENGE_V2
QWC_EPOSE_SUBJECT_RESPONSE_V2
QWC_EPOSE_RECEIPT_V2
QWC_EPOSE_QUALIFICATION_V2
QWC_EPOSE_PAYOUT_V2
QWC_EPOSE_STATE_V2
QWC_EPOSE_PAYMENT_PROOF_V2
```

Strings are encoded as their exact ASCII bytes without a NUL terminator.
Variable-length fields are prefixed by an unsigned 32-bit little-endian length.
Public keys, hashes, signatures, and fixed identifiers use their canonical
fixed-size byte representation. No host-endian serialization is permitted.

### Resource accounting

The activation manifest MUST assign all limits below. A `null` value means the
protocol is intentionally not activatable:

- maximum envelope bytes per transaction;
- maximum envelopes per transaction;
- maximum EPoSE bytes per block;
- maximum records per envelope and per block;
- maximum descriptor, admission, receipt, lifecycle, and payment-proof records
  per block;
- maximum signature verifications and RandomX admission verifications per block;
- maximum descriptor and proof payload sizes;
- maximum active population and lease interval;
- retained state/undo requirements needed to validate and reorganize the chain.

Budget charges occur after structural length checks but before signature,
RandomX, duplicate, or semantic checks. Both miner-transaction batches and
fee-funded transaction carriers consume the same block-wide budgets and produce
the same canonical transition.

### Activation manifest and parameter-set hash

The canonical activation manifest is JSON for review and release tooling, but
consensus embeds the SHA-256 of its canonical UTF-8 representation. Canonical
JSON uses sorted object keys, no insignificant whitespace, UTF-8, integer
numbers only, and a single trailing line feed. Arrays retain order.

The manifest MUST bind at least:

- network ID and genesis hash;
- source commit and dependency/submodule commits;
- activation hardfork and height;
- EPoSE/envelope/record versions and tag assignments;
- epoch timing and all cutoff formulas;
- all resource budgets;
- committee, threshold, rounds, and admission parameters;
- reward BPS, fee policy, empty-set policy, emission accounting, payout schedule;
- state schema and minimum validating/pruning requirements.

The checked-in CO-01 manifest is a reservation manifest, not an activation
manifest. Its `null` fields make accidental activation a specification error.

### Relay and template policy boundary

Nodes may apply stricter local relay and template limits than consensus permits,
prioritize deadlines, or omit all EPoSE records from locally produced blocks.
Those policies cannot make a complete block invalid. A miner requires no
service key, and a service node requires no mining role. Both coinbase and
fee-funded carriers must use the same envelope parser and state transition.

### Reorg behavior

Disconnecting a block restores the exact parent EPoSE state, including active
descriptor sequences, leases, frozen snapshots, accepted receipt slots, closed
qualification sets, payout schedules, and any scheduled-emission accounting.
Reorg `A -> B -> A` MUST reproduce the original state root byte-for-byte. If a
reorg crosses a cutoff or anchor, all dependent objects are discarded and
derived again from the replacement canonical blocks. A cache or index failure
must trigger explicit rebuild or failure, never an empty qualification shortcut.
