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
block.major_version = scheduled_version(H) for every height H
block.major_version = 17 immediately before A
block.major_version = 18 from A until the next scheduled fork
```

Historical HF17 blocks and v1 payloads retain their original meaning. At and
after `A`, new v1 registration and attestation payloads are rejected; historical
v1 state is read-only and cannot supply a v2 committee or payee. There is no
implicit migration of an identity, descriptor, admission lease, receipt, or
reward destination from v1 to v2.

The activation height, a known earlier reference height/hash, parameter-set
hash, reward-policy identifier, and final resource limits remain deliberately
unassigned.  The hash of the future activation block is not a preactivation
input: it is recorded only as a postactivation observation.  A binary MUST
refuse to advertise v2 activation while any required manifest field is
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

All five registered record types now have canonical codecs. A type `0x01`
record is the dual-authorized `register_identity` lifecycle action; type `0x04`
carries every later lifecycle action. Both use the same 378-byte payload and
cannot be relabelled because the outer type must agree with the encoded action.
The 96-byte type `0x05` payload contains only the scoped derivation and proof;
all payee, amount, output, Coinbase, height and parent context is reconstructed
from the candidate miner transaction before verification.

Structural envelope parsing and typed semantic decoding are separate atomic
steps. A structurally valid record with an invalid signature, RandomX proof,
context, or outer-type/action combination invalidates the semantic batch; no
decoded prefix may be applied. Which record versions are enabled comes only
from the active manifest, not from a peer or transaction.

### CO-02 frozen membership implementation

`src/epose/membership_v2.*` implements the deterministic, non-activating v2
membership pipeline. It is not called by HF17 block validation. The typed
admission codec parses the complete lease, then recomputes its RandomX work and
lease commitment before the pipeline can mutate membership state. The pipeline
also ingests complete dual-signed receipts and validates their canonical
context; no public caller-supplied “verified” Boolean or hash-only admission
boundary remains.

For target service epoch `E`, the only valid admission context is the canonical
block at `start(E - 1)`. The fixed v2 manifest value
`admission.context_epoch_offset = 1` is part of the rule, not a tunable security
parameter. A different earlier block is invalid even if its RandomX work and
lease hash are recomputed. This prevents an applicant from choosing among old
contexts while keeping the work seed independent of the later
`committee_anchor(E)`. The lease must still be included no later than
`enrollment_cutoff(E)`.

An admitted member freezes these selection-relevant commitments:

```text
service_public_key
identity_id
operator_authorization_public_key
descriptor_hash
reward_binding_hash
endpoint_descriptor_hash
sequence
target_epoch
work_algorithm
leading_zero_bits
admission_context_height
admission_context_hash
nonce
work_hash
lease_hash
inclusion_height
```

The admission record payload is exactly 322 bytes in the order above, excluding
`inclusion_height` (which is derived from its containing block). Integer fields
are little-endian and all keys and hashes use their fixed canonical byte forms.
`work_hash` is RandomX over domain `QWC_EPOSE_ADMISSION_WORK_V2`, the complete
network/genesis/parameter/context and member bindings, target epoch, algorithm,
target, and nonce, using the admission-context block hash as the RandomX seed.
`lease_hash` is the fast hash of domain `QWC_EPOSE_ADMISSION_LEASE_V2`, the same
bound lease fields and the verified work hash.

The lifecycle payload is exactly:

```text
uint8   action
bytes32 previous_descriptor_hash
uint8   descriptor_version
bytes32 identity_id
bytes32 service_public_key
bytes32 operator_authorization_public_key
bytes32 reward_view_public_key
bytes32 reward_spend_public_key
bytes32 endpoint_descriptor_hash
uint64  sequence
uint64  effective_epoch
uint64  expiry_epoch
bytes64 operator_signature
bytes64 service_signature
```

The two signatures cover the domain-separated lifecycle hash and are checked
before a decoded record is exposed. Registry application then enforces the
history-dependent predecessor, sequence, timing and action transition.

The payment-proof payload is exactly:

```text
bytes32 derivation
bytes64 proof
```

It is exposed only when the scoped proof verifies against the fully derived
Coinbase context.

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
directly to CO-02's membership pipeline together with its canonical receipt
context.  No caller-supplied `cryptographically_verified` Boolean or publicly
constructible aggregate is accepted.

The legacy HF17 automation that fabricated its response locally and set
`service_ok = true` is disabled. Historical HF17 blocks retain their original
validity, but this software no longer originates automatic positive HF17
attestations. The eventual live probe transport depends on the bounded carrier
and endpoint/lifecycle work in CO-05/CO-07; until then automation fails closed
instead of fabricating service.

### Canonical block service boundary

`src/epose/canonical_service_v2.*` implements the transport-independent data
exchange behind `canonical_object = 1`. The only currently permitted object is
an exact canonical block blob identified by its CryptoNote block hash. A
subject-side adapter must authorize the complete challenge against the frozen
snapshot, selected verifier, endpoint and round before any database lookup or
signature. It then retrieves the block from its canonical chain, enforces the
explicit byte limit, parses and canonically reserializes it, verifies the block
hash and signs the response transcript.

The verifier independently parses the returned bytes, checks their canonical
serialization and block hash, and compares them byte-for-byte with the same
block from its own canonical chain before accepting the subject signature and
creating its signature. Unknown, alternative-chain, malformed, truncated,
oversized or locally divergent objects therefore produce no receipt. All
failure outputs are cleared atomically.

The two injected boundaries are intentionally strict:

- `challenge_authorizer_v2` must be backed by canonical frozen membership,
  committee and lifecycle state; and
- `canonical_block_source_v2` must return main-chain bytes, never an
  alternative-chain or caller-supplied blob.

No public RPC adapter is enabled while those canonical v2 state adapters are
absent. This prevents the service key from becoming a generic signing oracle.
DNS, connection setup, redirects, streaming limits and request concurrency
remain local transport work for CO-09; they never enter block validation.

## CO-05 bounded envelope primitive

`src/epose/envelope_v2.*` implements the dedicated `0x05` field and `QEP2`
envelope format specified above. `tx_extra_epose_v2` is present in the generic
variant so unrelated fields can be parsed and preserved, but the ordinary
parser rejects it unless a version-aware caller opts in. The shared carrier
adapter opts in only for the exact reserved HF18 major version; HF17 and an
unscheduled version 19 therefore reject the field. HF18 remains absent from
the active hard-fork schedule.

The parser accepts constructor-supplied limits only; no fallback mainnet values
exist. It validates canonical outer varints, magic, versions, zero flags, exact
record count and byte lengths, per-record payload size, per-envelope byte and
record counts, and signature/RandomX operation budgets. Empty envelopes and
reserved record versions are invalid. Costs are charged by record type before
payload semantics or duplicate handling.

`parse_transaction_extra_v2` is the common parser for a miner transaction and
an ordinary fee-funded transaction. It extracts every dedicated field from the
normal transaction extra, preserves unrelated tags, applies the same envelope
semantics, and accounts for the outer tag and canonical length prefix.
`append_transaction_envelope_v2` appends without modifying the caller's bytes
on failure. `charge_block_budget_v2` accumulates byte, record, signature, and
admission costs transaction by transaction without mutating the committed
budget on failure.

`src/epose/record_codec_v2.*` currently defines the canonical service-receipt
payload. It is exactly 402 bytes in this order: version and service kind
(`uint8` each); epoch and round (`uint64` little-endian each); snapshot hash,
round-anchor hash, subject key, verifier key, endpoint-descriptor hash, nonce,
requested-object hash, and response-object hash (32 bytes each); then subject
and verifier signatures (64 bytes each). Decode is atomic and returns a receipt
only after the complete context-bound signature validation succeeds.

The carrier and receipt codec remain non-activating: there is no HF18 schedule
or block-state adapter. Codecs for lifecycle, admission, descriptors and the
payment proof, miner template batching, wallet-funded submission, queue
fairness, and measured manifest limits remain required before HF18 can affect
block validity.

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

## CO-08 state-index and replay boundary

`src/epose/state_index_v2.*` defines a schema-versioned, checksummed checkpoint
journal. Checkpoints bind the block/parent, state root, and canonical EPoSE
payload hash. Connect is contiguous and fail-atomic; exact tip duplicates are
idempotent; same-height conflicts fail. Undo is bounded and a deeper disconnect
returns `rebuild_required` rather than substituting empty state.

Serialized state validates schema, parameter-set identity, checksum, exact
lengths, and parent links before replacing committed memory. Canonical block
replay remains the correctness oracle. The journal is non-activating and still
requires LMDB tables updated in the same write transaction as block state,
startup tip checks, payload/object reconstruction, and pruning integration.
See `review/ADR-0005-PERSISTENT-STATE.md`.

## CO-09 endpoint and resource policy

`src/epose/resource_policy_v2.*` defines signed, typed endpoint descriptors,
canonical host rules, public-address filtering, bounded DNS answer validation,
probe concurrency/byte/time budgets, admission-context cache admission, and
paginated RPC scan limits.

DNS answers and live probes remain local policy and are rechecked at connection
time; block validation performs no DNS or network I/O. Unknown admission
contexts are rejected before any cache or RandomX VM allocation. Local overload
may shed probe/relay work but cannot alter complete-block validity.

These primitives remain non-activating pending handler integration, streaming
limits, no-redirect enforcement, credential isolation, fuzzing, and sustained
malicious-load benchmarks. See `review/ADR-0006-RESOURCE-POLICY.md`.
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
QWC_EPOSE_ADMISSION_WORK_V2
QWC_EPOSE_ADMISSION_LEASE_V2
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
- maximum relay-queue items and bytes, with nonzero enrollment and evidence
  reservations whose sums do not exceed the queue totals;
- retained state/undo requirements needed to validate and reorganize the chain.

Budget charges occur after structural length checks but before signature,
RandomX, duplicate, or semantic checks. Both miner-transaction batches and
fee-funded transaction carriers consume the same block-wide budgets and produce
the same canonical transition.

Relay/template policy is not consensus. The local deadline queue reserves both
item and byte capacity for enrollment and evidence so either class can continue
to enter under a flood of the other. Duplicate IDs are idempotent only when all
queue metadata agree; conflicts fail closed. Deadlines are inclusive, expired
items are pruned before capacity checks, and template selection first fills the
configured reservation for each class before considering the remaining records
in `(deadline, id, class)` order. These local choices cannot invalidate an
otherwise complete block.

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
