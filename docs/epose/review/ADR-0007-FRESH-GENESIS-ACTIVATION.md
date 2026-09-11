# ADR-0007: Fresh-genesis QWC-HF17 / EPoSE-v2 activation profile

- **Status:** Accepted target profile; activation still gated
- **Date:** 2026-09-06
- **Scope:** Public mainnet genesis, protocol version, and EPoSE warm-up
- **Supersedes:** ADR-0001 chain-preserving activation decision
- **Current implementation status:** Final identity bound; exact rehearsal pending

## Context

ADR-0001 conservatively treated the observed HF17 pre-publication chain as
potentially carrying state that had to survive a later hardfork. The owner has
since confirmed that the public mainnet has not launched, the existing chain is
disposable, and no balances, registrations, qualifications, or other chain
state need to be preserved.

The inherited code already starts at QWC block version 17 rather than replaying
the historical Monero hardfork schedule. Monero HF16 is the retained technical
rule baseline, not an earlier phase of the new chain. Block major version 17 and
EPoSE protocol/format version 2 are separate version domains.

## Decision

1. The intended public mainnet is a new chain with a newly reviewed genesis.
2. QWC block version 17 and EPoSE protocol version 2 are the intended rules from
   height 0. No block version 16, 18, or other unscheduled version is valid.
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
10. The hardfork table schedules QWC version 17 from genesis, but the hardened
    v2 economic path is not release-ready until block/coinbase validation,
    atomic LMDB state, bounded transport, wallet funds safety, final parameters,
    and every mandatory release gate are complete.
11. Future scheduled QWC block versions continue the parameter-bound EPoSE-v2
    state by default.  ADR-0008 defines that no-op continuation rule; it does
    not schedule a future fork or permit an implicit parameter migration.

## Final public chain identity

The one permitted post-rehearsal reset is bound to these reviewed values:

- Mainnet network ID: `QWC2MAIN2026FIN` plus terminal byte `0x01`
  (`515743324d41494e3230323646494e01`);
- Genesis transaction template: unchanged from the reviewed HF17 relaunch
  template, so this identity change creates no special allocation or premine;
- Genesis nonce: `10003`;
- Canonical genesis hash:
  `906629482787e94cb00463696a0e95ec75a480da09257c6270c65ba1a74a76b0`;
- EPoSE consensus-parameter commitment:
  `e5654b4f5fa27faa51a80ca1e93bb877c3bd3345d0a803b6e7ab55c05189c20d`.

The network ID prevents accidental P2P association with the disposable
`QWC2MAIN2026POC` rehearsal network.  The genesis hash is independently
recomputed by the focused C++ test through the same production constructor
used by daemon and wallet.  Neither value authorizes announcement or mining;
the exact-source isolated rehearsal and release-artifact binding remain gates.

## Consensus consequences

- There is no Monero-HF16 operating phase or QWC-HF17 migration window.
- There is no pre-activation height and no activation-block migration state.
- Genesis and every descendant must use the block version selected by the
  hardfork schedule. Version 17 remains the only scheduled version until a
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

### Use block version 18 for EPoSE v2

Rejected because QWC HF17 is the launch-rule layer over the retained Monero-HF16
baseline. EPoSE's `2` is its protocol/format version and does not require the
block major version to be incremented again.

### Activate the current incomplete v2 stack immediately

Rejected. A genesis decision fixes the intended transition topology, not the
readiness of the implementation. The runtime schedule remains unchanged until
the required integrated code and evidence exist.
