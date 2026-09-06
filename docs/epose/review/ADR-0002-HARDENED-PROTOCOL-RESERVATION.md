# ADR-0002: Hardened EPoSE Protocol Reservation

- **Status:** Accepted design reservation; not activated
- **Date:** 2026-09-06
- **Scope:** CO-01 protocol namespace and transition contract
- **Depends on:** ADR-0007
- **Baseline:** `1c4c1bf10c387887a42243dc690a65abb6c6e786`
- **Reservation manifest canonical SHA-256:**
  `6377b1eb1eca7b44165cf64f66b1d686ec657e99ca057a0041ba4990c83e8699`

## Context

The disposable development chain used HF17/EPoSE v1, but the intended
public mainnet has no history to preserve. The hardened design needs immutable
snapshots, subject-authenticated receipts, a carrier larger than the legacy
255-byte nonce, and separately reviewed reward semantics. QWC block version 17
selects those start rules while the wire protocol remains EPoSE version 2.

The pinned transaction-extra registry uses tags `0x00` through `0x04` plus the
historical `0xDE` tag. Hardfork 17 is the highest version in the QWC schedule.
The project has no separate numeric-allocation process, so this ADR and the
machine-readable reservation manifest are the allocation record.

## Decision

1. Reserve hardfork version `18` for the first hardened EPoSE activation.
2. Reserve EPoSE protocol version `2`.
3. Reserve transaction-extra tag `0x05` for the dedicated EPoSE envelope.
4. Reserve envelope version `1` and record types `0x01` through `0x05` as listed
   in `PROTOCOL.md`.
5. Require `fresh-genesis` activation at height 0. The final genesis hash is a
   candidate-bound manifest and release input, not a future activation-block
   observation.
6. Import no HF17/v1 chain state or protocol objects into the public chain.
7. Use a two-epoch v2 warm-up: enrollment, then measured service, then payouts.
8. Freeze membership before the committee anchor and close qualification before
   the payout seed.
9. Keep all unresolved security/economic/resource fields `null` in the checked-in
   manifest. A manifest with a required `null` field is not activatable.
10. Require every implementation PR to add binary vectors and update the
    independent reference model before making its reserved record version valid.

## Consequences

- Current binaries continue to reject/ignore the reserved format according to
  existing HF17 parsing; this PR adds no runtime support.
- CO-02 through CO-09 can implement against stable namespaces and boundary rules
  with a fixed height-zero target while tokenomics and final limits remain gated.
- A later feature that needs QWC block version 17 or tx-extra tag `0x05` must coordinate with
  this reservation instead of silently reusing it.
- Activation remains impossible until CO-03, CO-05, CO-06, CO-08, CO-10, and
  CO-11 supply the missing manifest values and evidence.

## Rejected alternatives

### Reuse legacy EPoSE-v1 dispatch at QWC HF17

Rejected because the hardened records and state machine require an explicit
EPoSE protocol/format boundary. QWC block version 17 selects only v2 on the new
chain; the legacy-v1 production dispatch is retired.

### Extend `tx_extra_nonce`

Rejected because the current attestation already consumes 235 of 255 bytes and
a subject signature alone exceeds the remaining capacity.

### Choose final limits and economics in CO-01

Rejected because the required capacity, attacker-cost, emission, payment-proof,
and funds-safety evidence does not yet exist. Reservation is not approval.

### Activate after genesis

Rejected because there is no public history to preserve. The pipeline instead
uses epoch 0 for enrollment, epoch 1 for service, and epoch 2 for first payout.
