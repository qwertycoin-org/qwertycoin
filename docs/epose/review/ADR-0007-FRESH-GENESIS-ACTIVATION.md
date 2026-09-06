# ADR-0007: Fresh-genesis EPoSE v2 activation profile

- **Status:** Accepted target profile; activation still gated
- **Date:** 2026-09-06
- **Scope:** Public mainnet genesis, protocol version, and EPoSE warm-up
- **Supersedes:** ADR-0001 chain-preserving activation decision
- **Current implementation status:** Reserved and non-activating

## Context

ADR-0001 conservatively treated the observed HF17 pre-publication chain as
potentially carrying state that had to survive a later hardfork. The owner has
since confirmed that the public mainnet has not launched, the existing chain is
disposable, and no balances, registrations, qualifications, or other chain
state need to be preserved.

The inherited code already starts at protocol version 17 rather than replaying
the historical Monero hardfork schedule. Renumbering the new start rules to
version 1 would disturb many inherited version thresholds. Protocol version 18
therefore remains the unambiguous identifier for the hardened QWC start rules,
but it is scheduled from genesis rather than at a later height.

## Decision

1. The intended public mainnet is a new chain with a newly reviewed genesis.
2. Protocol version 18 and EPoSE protocol version 2 are the intended rules from
   height 0. No protocol-17 block is valid on that public chain.
3. The manifest activation mode is `fresh-genesis` and its activation height is
   exactly 0.
4. Epoch 0 is enrollment/bootstrap only. It cannot qualify service or create an
   EPoSE v2 payout.
5. Epoch 1 is the first measurable service epoch.
6. Epoch 2 is the first payout epoch; with `B = 720`, the first possible service
   payout is height 1440.
7. No HF17 identity, admission, receipt, qualification, reward entitlement,
   balance, or chain-state migration exists.
8. The new genesis hash, network identities, checkpoints, seed bootstrap, and
   release binaries must be committed together as candidate-bound release
   evidence. The currently observed disposable genesis is not reused by
   implication.
9. DNS checkpoints remain disabled unless a separate reviewed trust,
   generation, signing, rotation, conflict, and operational design is approved.
10. This ADR does not schedule version 18 in `hardforks.cpp`. Runtime activation
    remains blocked until block/coinbase validation, atomic LMDB state, bounded
    transport, wallet funds safety, final parameters, and every mandatory
    release gate are complete.

## Consensus consequences

- There is no mixed HF17/HF18 transition window on the intended public chain.
- There is no pre-activation height and no activation-block migration state.
- Genesis and every descendant must use the exact scheduled version 18 until a
  future separately approved protocol version.
- Reorg handling remains required from genesis onward. A fresh genesis does not
  weaken replay, crash-recovery, or deterministic-state requirements.
- RandomX remains the only block-production and chain-selection mechanism;
  EPoSE v2 remains a bounded service-reward protocol.

## Activation gates

The existing release-gate policy remains mandatory. Setting
`activation.height` to 0 does not make a reservation manifest activatable.
Genesis identity, source revision, final resource/security/economic parameters,
integrated state transitions, independent review, reproducible artifacts, and
multi-node wallet spend/reorg evidence remain required.

## Rejected alternatives

### Preserve the disposable HF17 chain

Rejected because the owner has explicitly stated that no pre-publication chain
or balances need preservation. Carrying migration machinery into genesis would
increase consensus complexity without preserving required state.

### Rename the start protocol to version 1

Rejected because inherited CryptoNote/Monero rules use ordered version
thresholds. Keeping version 18 avoids broad unrelated rewrites and makes the
new QWC rules distinguishable from every inherited historical rule set.

### Activate the current incomplete v2 stack immediately

Rejected. A genesis decision fixes the intended transition topology, not the
readiness of the implementation. The runtime schedule remains unchanged until
the required integrated code and evidence exist.
