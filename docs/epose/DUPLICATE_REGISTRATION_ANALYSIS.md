# Duplicate Registration Analysis

## Observed State

The internal mainnet-test network showed six active EPoSE registrations while
only three physical seed nodes were expected. The duplicate registrations shared
endpoint commitments by seed host, which means each physical endpoint had more
than one active service public key.

## Likely Root Cause

The default service-node key path lived inside the same Docker volume as the
chain database. Resetting or recreating that volume deleted the service-node
identity. On restart, the daemon generated a new service key and could broadcast
or mine a fresh registration, while the chain still contained another active
registration for the same endpoint from an earlier key.

## Fix

Deployment now mounts a separate identity volume and passes:

```text
--service-node-key=/service-node/service-node.key
```

Chain/LMDB resets can remove only the chain volume while retaining the identity
volume. The service public key must remain identical before and after a chain
reset when that identity volume is preserved.

The registry also rejects overlapping active registrations that reuse the same
endpoint commitment. Service-node identity remains the service public key; the
endpoint rule is an operational duplicate guard, not a complete Sybil-resistance
mechanism and not an IP identity model.

## Lifecycle

Current lifecycle:

```text
REGISTER -> active until expiry_epoch -> EXPIRE -> prune after retention window
```

Renewal/update/deregister semantics are still intentionally conservative. A
future renewal/update should replace or extend an existing active identity
without creating a conflicting second active registration.
