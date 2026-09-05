# EPoSE Attestation Relay

## Goal

EPoSE service-node qualification must not require local mining. A service node may
generate signed EPoSE payloads, but a normal full node or mining-pool daemon must be
able to relay those registrations and attestations and include them in block templates without having
a service-node private key.

## Data Flow

```text
Service node
 -> signed service_node_identity / service_attestation
 -> local attestation pool
 -> NOTIFY_NEW_EPOSE_PAYLOADS
 -> peer attestation pools
 -> normal miner get_block_template
 -> miner_tx.extra
 -> on-chain EPoSE state
```

The relay pool is candidate data only. Canonical EPoSE state is still derived only
from registrations and attestations committed on-chain and accepted by
`chain_state::apply_tx_extra`.

## P2P Message

`NOTIFY_NEW_EPOSE_PAYLOADS` carries bounded batches of serialized
`service_node_identity` registration blobs and serialized `service_attestation`
blobs.

Current limits:

- maximum batch size: `EPOSE_ATTESTATION_RELAY_MAX_BATCH`
- maximum registration item size: `EPOSE_IDENTITY_BLOB_SIZE`
- maximum attestation item size: `EPOSE_ATTESTATION_BLOB_SIZE`
- maximum wire message size: 16 KiB

Invalid batches are rejected before relay. Accepted new payloads are relayed to
other peers; duplicates are ignored and not amplified.

## Pool Policy

The attestation pool is:

- in-memory only,
- bounded,
- deduplicated by registration epoch/service key or attestation epoch/subject/verifier/challenge,
- epoch aware,
- chain-duplicate aware,
- validated before insertion.

Validation checks:

- supported registration/attestation version,
- registration admission proof and signature,
- registration epoch and TTL,
- duplicate active registration rejection,
- supported attestation version,
- current epoch binding,
- network-bound signature,
- active subject registration,
- active verifier registration,
- selected verifier committee membership,
- deterministic challenge hash,
- deterministic response hash,
- duplicate on-chain vote rejection.

## Template Policy

`get_block_template` selects valid pool entries deterministically by:

```text
registration_epoch
service_public_key

epoch
subject_public_key
verifier_public_key
challenge_hash
```

The current tx-extra nonce format can carry one EPoSE payload per miner
transaction. Therefore the template policy currently includes at most one relayed
registration or attestation per block. Registrations are prioritized because
attestations require registered subjects and verifiers. This is a transport-policy
limit, not a consensus rule.

Miners may omit relayed payloads. A block is not invalid merely because it does
not include all EPoSE payloads known to a validator's local pool.

## Roles

Normal node:

- validates and relays EPoSE registrations and attestations,
- maintains an attestation pool,
- can build block templates with relayed EPoSE payloads.

Miner node:

- same as normal node,
- provides block templates to miners.

Service node:

- same as normal node,
- owns a service-node key,
- signs and broadcasts its registration independently from mining,
- signs attestations for selected subjects,
- broadcasts attestations independently from mining.

Hybrid node:

- service node plus miner,
- allowed but not required.
