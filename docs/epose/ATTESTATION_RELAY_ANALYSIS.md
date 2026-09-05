# EPoSE Attestation Relay Analysis

## Current Behavior

EPoSE registrations and attestations are encoded as typed `tx_extra_nonce` payloads. The
on-chain representation is currently shared by registrations and attestations:

- `EPOSE_TX_EXTRA_NONCE_REGISTRATION` stores a serialized `service_node_identity`.
- `EPOSE_TX_EXTRA_NONCE_ATTESTATION` stores a serialized `service_attestation`.
- `chain_state::apply_tx_extra(...)` parses both payload types from a transaction extra and applies
  them to the canonical EPoSE chain state.

The current registration and attestation generation path is coupled to mining:

1. `core::get_block_template(...)` calls `core::build_epose_miner_extra_nonce(...)`.
2. `build_epose_miner_extra_nonce(...)` only runs useful EPoSE logic when the local daemon is started
   with `--service-node`, has a loaded service key, and the caller did not provide a custom extra nonce.
3. If the local service node is not registered in the current epoch, the method creates one signed
   registration and returns it as the effective miner extra nonce.
4. If the local service node is registered in the current epoch, the method creates one signed
   attestation for a selected subject and returns it as the effective miner extra nonce.
5. The block template builder puts that nonce into `miner_tx.extra`.
6. The EPoSE payload becomes canonical only if a miner actually finds a block from that template.

Normal full nodes currently do not create, store, relay, or include foreign EPoSE payloads. The
cryptonote P2P protocol has handlers for blocks, transactions, chain sync, fluffy-block recovery, and
txpool complements, but no EPoSE payload relay message.

## Root Cause

Service-node EPoSE transport is missing.

The consensus parser already accepts EPoSE payloads from any transaction extra that reaches the chain,
but the only production path that creates those extras is the service-node-local miner-template path.
That means a service node's ability to become qualified is indirectly dependent on at least some
service-node block templates being mined.

This mixes two roles that should be independent:

- Miner: performs RandomX proof of work and creates blocks.
- Service node: provides service, verifies other service nodes, signs attestations, and becomes
  reward eligible.

## Required Change

Keep the existing on-chain attestation representation where possible, but add an off-chain relay layer:

```text
Service node
 -> signed EPoSE registration or attestation
 -> local bounded attestation pool
 -> P2P relay
 -> normal full node attestation pool
 -> normal miner block template
 -> existing miner_tx.extra EPoSE payload
 -> canonical chain state
```

The key functional change is that `get_block_template` on a normal non-service-node daemon must be able
to include valid relayed EPoSE registrations and attestations from its local attestation pool.

## Consensus Impact

The pool must not be consensus state. Consensus remains based only on EPoSE payloads that are included
on-chain and validated by `chain_state::apply_tx_extra(...)`.

Nodes may have different EPoSE pool contents without causing a consensus split. A block is valid
if every included EPoSE payload passes the existing deterministic validation rules against the canonical
chain state. A block must not be invalid merely because it omitted a payload known to a validator's
local pool.

If the on-chain `tx_extra_nonce` encoding is preserved, this can be a compatible node upgrade rather
than a hardfork. Older nodes that already understand the current EPoSE extra format should continue to
validate blocks containing EPoSE payloads in `miner_tx.extra`.

## P2P Impact

A new bounded P2P notify is needed for candidate EPoSE registrations and attestations. It should carry
serialized EPoSE blobs rather than wallet/money transactions.

Required properties:

- Maximum batch count.
- Maximum item sizes equal to `EPOSE_IDENTITY_BLOB_SIZE` and `EPOSE_ATTESTATION_BLOB_SIZE`.
- Parse before expensive validation.
- Drop stale/future epochs.
- Reject duplicates before relay amplification.
- Relay accepted new items only.
- Never relay invalid payloads.

## Security Impact

The pool admits remote input, so it needs explicit DoS controls:

- bounded memory and entry count,
- deterministic registration keying by epoch and service key,
- deterministic keying by epoch/subject/verifier/challenge,
- registration signature/admission validation,
- signature validation,
- active subject/verifier validation,
- selected-verifier validation,
- challenge/response validation,
- stale/future epoch filtering,
- duplicate and replay rejection.

The pool must not persist secrets and does not require a service-node private key. Normal nodes must be
able to validate, store, relay, and include candidate attestations without being service nodes.

## Migration / Compatibility

The first implementation target is a compatible relay/template-policy change:

- preserve the current `EPOSE_TX_EXTRA_NONCE_ATTESTATION` on-chain payload,
- preserve the current `EPOSE_TX_EXTRA_NONCE_REGISTRATION` on-chain payload,
- add a non-consensus in-memory attestation pool for EPoSE relay candidates,
- add P2P relay for serialized registrations and attestations,
- make service nodes publish signed EPoSE payloads independently from mining,
- make normal miner daemons include valid pool payloads in `miner_tx.extra`.

No genesis reset should be required. A hardfork is only required if later changes alter block validity
rules or the on-chain attestation format.
