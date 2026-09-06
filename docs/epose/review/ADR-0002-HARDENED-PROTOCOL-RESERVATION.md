# ADR-0002: Hardened EPoSE Protocol Reservation

- **Status:** Accepted design reservation; not activated
- **Date:** 2026-09-06
- **Scope:** CO-01 protocol namespace and transition contract
- **Depends on:** ADR-0001
- **Baseline:** `1c4c1bf10c387887a42243dc690a65abb6c6e786`
- **Reservation manifest canonical SHA-256:**
  `3cc33c916af6cd377485e163dd41006a9ea6ff8899ae4e31205cd01f104fe5f1`

## Context

HF17/EPoSE v1 is active from genesis and has accepted history. The hardened
design needs different epoch ordering, immutable snapshots, subject-authenticated
receipts, a carrier larger than the legacy 255-byte nonce, and separately
reviewed reward semantics. Reusing v1 encodings or changing their interpretation
would create a validation split.

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
5. Require a future activation height aligned to 720 blocks and strictly above
   the release-freeze mainnet tip.
   The preactivation commitment may use a known earlier reference height/hash,
   but MUST NOT require the unknowable future activation-block hash.  The latter
   is a postactivation observation only.
6. Preserve every pre-activation HF17/v1 rule byte-for-byte. No v1 object is
   implicitly promoted into v2.
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
  without guessing the activation height or tokenomics.
- A later feature that needs HF18 or tx-extra tag `0x05` must coordinate with
  this reservation instead of silently reusing it.
- Activation remains impossible until CO-03, CO-05, CO-06, CO-08, CO-10, and
  CO-11 supply the missing manifest values and evidence.

## Rejected alternatives

### Reuse HF17

Rejected because old and new nodes could assign different meaning to the same
historical bytes.

### Extend `tx_extra_nonce`

Rejected because the current attestation already consumes 235 of 255 bytes and
a subject signature alone exceeds the remaining capacity.

### Choose final limits and economics in CO-01

Rejected because the required capacity, attacker-cost, emission, payment-proof,
and funds-safety evidence does not yet exist. Reservation is not approval.

### Activate at the next arbitrary height

Rejected because the pipeline depends on explicit epoch boundaries and a
two-epoch warm-up. Activation requires a release-specific, signed manifest.
