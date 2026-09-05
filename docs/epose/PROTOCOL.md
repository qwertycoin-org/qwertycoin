# EPoSE v2 Protocol

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
- `EPOSE_VERIFIER_COMMITTEE_SIZE = 5`

EPoSE consensus is active from genesis for `HF_VERSION_QWC_EPOSE_V1 = 17`.

Mainnet, testnet, and stagenet start directly at QWC protocol v17 from height 0. QWC v17 inherits Monero's current v16 consensus rules and adds EPoSE; historical Monero hardforks are not scheduled as later QWC activations. Low-height activation for deterministic regression tests uses local `HardFork` test fixtures.

`EPOSE_MIN_ATTESTATIONS = 2`, verifier committee size `5`, and admission target
`8` leading zero bits are the current bootstrap parameters in `main`. They are
not final long-term values; the next hardening PR evaluates a dynamic 2/3 quorum,
larger committees, and a benchmarked admission target.

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
