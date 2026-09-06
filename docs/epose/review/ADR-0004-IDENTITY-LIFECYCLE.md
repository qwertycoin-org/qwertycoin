# ADR-0004: EPoSE v2 Identity Lifecycle and Recovery

- **Status:** Candidate implementation; non-activating
- **Date:** 2026-09-06
- **Scope:** CO-07

## Decision

EPoSE v2 separates three authorities:

- the persistent identity ID, derived from network/genesis, parameter set, and
  an offline operator-authorization public key;
- the online service key used for service participation;
- the public reward address, whose spend key remains exclusively in the wallet.

Every registration, renewal, descriptor update, deregistration, and recovery is
an explicitly typed lifecycle record. Records bind the previous descriptor
hash, monotonically increasing sequence, future effective epoch, expiry,
service key, operator key, public reward keys, and endpoint descriptor hash.

All records require both the offline operator signature and the service
signature named by the next descriptor. Recovery therefore proves possession
of the replacement service key and needs no signature from a lost or
compromised old online key. The operator key itself is fixed for this first
candidate; operator-authority rotation needs a separate recovery design.

## Security consequences

- Theft of the online service key alone cannot redirect rewards, change an
  endpoint, renew a lease, or deregister the identity.
- A replacement online key cannot participate before its future effective
  epoch.
- Changes cannot rewrite earlier descriptor views used by frozen snapshots or
  already settled rewards.
- Exact valid duplicates are idempotent, but their signatures are verified
  before duplicate handling.
- Descriptor history remains committed to deterministic state.

## Operational requirements

The operator-authorization secret belongs in offline custody, never in the
daemon, command-line arguments, logs, repository, or service-identity volume.
The online `service-node.key` remains persistent with restrictive permissions
and separate from chain state and wallets. Atomic key-file creation, backup,
and recovery UI are required before activation.

## Compatibility and activation

HF17 registration and key files on the disposable development chain are
unchanged. The public chain imports none of them. V2 record serialization,
envelope integration, durable storage, and fresh dedicated reward-wallet
operational guidance remain open. This state machine alone does not complete
QWC-HF17 EPoSE-v2 integration.
