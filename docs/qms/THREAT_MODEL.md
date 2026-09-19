# QMS MVP threat model

## Protected assets

Message plaintext, long-lived private messenger keys, contact discovery secrets,
contact identity pins, prepared transaction bytes and the local outgoing plaintext copy.

## Trust and validation

The invitation is transferred through an already trusted channel and its fingerprint is
confirmed out of band. A discovery secret never substitutes for the pinned sender's
Ed25519 signature. The receiver validates size and canonical structure, discovery hint,
fragment MAC, duplicate consistency, full ciphertext hash, sealed-box authentication,
sender signature, genesis, IDs, fingerprints, content type and UTF-8 before display.

## Adversaries

- Nodes and miners can omit, reorder or reorg carrier transactions. They learn carrier
  sizes/timing but do not receive plaintext or messenger private keys.
- A chain observer can copy/replay fragments. Message ID, signed context and local
  idempotency prevent a replay from becoming a distinct message.
- An attacker knowing no invitation secret cannot create fragments that pass the early
  MAC gate. Implementations must still enforce 64 incomplete messages, 8 MiB total and
  a smaller per-contact budget before persistence.
- A malicious contact possessing its invitation secret can consume its per-contact
  allowance; it cannot forge another pinned sender's inner signature.
- Loss of the recipient's long-lived private encryption key compromises confidentiality
  of retained historical ciphertext. There is no forward secrecy or ratchet recovery.
- The profile is not post-quantum and the combined protocol has not been externally
  audited.

## Wallet/funds invariants

Carrier transactions are ordinary self-payments with dynamically calculated fees.
Preparation must reserve distinct mature inputs for the complete batch and broadcast
nothing. Commit is explicit and idempotently reuses stored signed bytes. No repair
transaction may spend a new fee without another review. Consensus and relay limits are
never relaxed for QMS.

Hardware wallets, multisig, integrated addresses, user payment IDs and light-wallet
services are unsupported in the MVP.

