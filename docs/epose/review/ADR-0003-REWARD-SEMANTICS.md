# ADR-0003: EPoSE v2 Reward Semantics

- **Status:** Rehearsal candidate decision; final launch evidence required
- **Date:** 2026-09-07
- **Scope:** QWC HF17 / EPoSE v2 from fresh genesis

## Context

The new Qwertycoin chain starts at block major version 17 with EPoSE protocol
version 2. There is no earlier Qwertycoin history or v1 service state to
migrate. The inherited Monero HF16 behavior remains the technical baseline;
the v2 reward rule is enforced from genesis while actual service payouts begin
only after completed qualification.

The approved rehearsal economics preserve the 1,000 BPS service share when a
qualified payee exists, keep fees with the miner, and return the full scheduled
subsidy to the miner when qualification is empty. This keeps inherited emission
accounting and PoW revenue deterministic during bootstrap and outages.

## Candidate implemented by CO-06

`reward_v2.*` provides a non-activating reference implementation with these
properties:

1. The service allocation is `floor(scheduled_subsidy * 1000 / 10000)`.
2. Transaction fees always belong to the miner.
3. A qualified payee receives the service allocation; the miner receives the
   remaining subsidy and all fees.
4. With no qualified payee, the caller must explicitly choose either historical
   miner fallback or permanent non-issuance. `unset` fails closed.
5. Permanent non-issuance advances scheduled emission by the full scheduled
   subsidy while issued supply excludes the withheld amount. Integration must
   prove that inherited emission code cannot reissue it later.
6. The service share remains 1,000 BPS. This PR does not authorize changing it.
7. Payout rotation is relative to the payout epoch start and binds genesis,
   parameter set, closed qualification hash, source/payout epochs, payout seed,
   and service key.

No option is connected to block construction or validation in this PR.

## Scoped payment proof candidate

The candidate uses the inherited transaction-proof primitive. The miner
publishes the recipient-specific derivation `D = 8*r*A` and proves knowledge of
the coinbase secret `r` binding `R = r*G` and `D = r*(8*A)`. The transcript also
commits to network/genesis, parameter set, height, parent, payout epoch, closed
qualification set, service identity, public reward keys, amount, transaction
public key, explicit output indices/amounts/keys, and a canonical coinbase
commitment calculated with the proof record omitted.

Validators derive the prescribed one-time output keys from `D` and the public
spend key. A private view key is neither published nor consumed. Proof bytes are
excluded from the coinbase commitment, avoiding a circular transcript.

This construction is limited to primary standard reward addresses. It remains
a candidate until an independent cryptographic review covers canonical point
encoding, small-order/cofactor behavior, transcript completeness, output
allocation, and inherited proof assumptions. It must not be activated merely
because its tests pass.

## Rehearsal decision

The embedded rehearsal profile selects:

- **miner fallback** for an empty qualification set;
- **actual-issued-subsidy** emission accounting;
- **subsidy-only** service allocation, with transaction fees retained by the
  miner; and
- the scoped transaction proof candidate for an actual service payout.

These values make the resettable rehearsal configuration complete. They do not
close the independent payment-proof review, integrated emission-continuity,
payout-fork, maturity, spend, recipient-rescan, or final-launch review gates.
The incentive to suppress all qualification is an accepted consequence of this
bootstrap-safe choice and remains part of the economic/security assessment.

## Compatibility

There is no Qwertycoin pre-launch history to preserve. Wallet transaction and
metadata compatibility remain inherited, while legacy v1 registrations,
attestations and reward-view-key interfaces do not create v2 eligibility.
