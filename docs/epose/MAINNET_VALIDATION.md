# Qwertycoin v2 Mainnet EPoSE Validation

Date: 2026-09-04
Source: current `main` at `60ccc9d08`

This document records the controlled mainnet-validation state after PR #8. It
does not claim final public mainnet readiness.

This is now a historical validation snapshot. The public mainnet launch
release-candidate gate is tracked in `docs/releases/v2.0.0-rc.1.md` and
`docs/MAINNET_LAUNCH_READINESS.md`.

## Current Validation Chain

The controlled three-seed validation deployment used the denominated PR #8
daemon image:

```text
qwertycoin-v2-node:epose-pr8-denom
```

After the coordinated reset:

- old `qwertycoin-mainnet` containers were removed,
- chain volumes were deleted,
- service-node identity volumes were deleted for a fresh-identity test,
- wallet volumes were preserved,
- all three nodes restarted from Genesis,
- explorers were restarted against the fresh chain.

Daemon height `1` means only Genesis block `0` exists.

Genesis top hash observed on all three hosts:

```text
e791e506200ba3a221b87b6c78359c2bbb13c3ef622e1c90b3b3fbb52f4943f5
```

## Hosts

| Node | Host | P2P |
| --- | --- | --- |
| seed-00 | `seed-00.qwertycoin.org` | `8196` |
| seed-01 | `seed-01.qwertycoin.org` | `8196` |
| seed-02 | `seed-02.qwertycoin.org` | `8196` |

## Service Public Keys After Fresh Identity Reset

```text
seed-00 ce0cedc6a2e7a26daec2bf27bb398d0696259c2373ceff7a2613f9d1c976437b
seed-01 ac771c42dba19af9a4234066b31e69b49e21fe1224ba510710f2ce3eef0376ca
seed-02 30b3c821d1f12ca8cc73ad3780e100cdb691ffe40707a0b4842a145a8da6f0ac
```

These keys are service-node identities only. They are not reward wallet spend
keys.

## Reward Output Validation

The current main chain no longer uses transparent public-spend-key service
reward outputs.

Current behavior:

- a service node registers a standard QWC reward address,
- registration discloses the matching private view key,
- the spend key remains secret,
- miner tx derives normal one-time service reward output keys,
- the service reward is decomposed into standard amount denominations,
- validators recompute expected output keys from the coinbase tx public key and
  registered view key,
- matching output amounts must equal the expected denomination set.

Example observed after mining restarted:

```text
block 10 coinbase tx:
0ecd55fe76e3dc7c6bd16e68145fd05dd31adc3020701305d5d2c68a21ca73c1
```

The coinbase had eight outputs:

- one miner output,
- seven denominated EPoSE service reward outputs.

For that block:

```text
total reward  = 351.83701005 QWC
miner reward  = 316.65330905 QWC
EPoSE reward  = 35.18370100 QWC
```

The service reward is exactly 10% of the total reward after atomic-unit
truncation.

## Wallet Validation Status

Observed:

- web wallet detected incoming seed-00 EPoSE reward balance,
- the earlier single-large-output design failed at send time because coin
  selection could not find enough decoys for unique amount classes,
- current `main` fixes that by denominating service reward outputs.

Still required as a release gate:

```text
mature denominated reward
=> spend from reward wallet
=> mined transaction
=> recipient wallet receives QWC
```

The agent must not hold reward-wallet spend keys. Spend validation should be
performed by the operator wallet, then verified on-chain.

## Explorer Status

Explorer integration is observer-only. For readability, the explorer may group
the denominated service reward outputs as one logical EPoSE reward in the UI.
That grouping does not change consensus: raw coinbase outputs remain
denominated on-chain.

## Deployment Notes

`deploy/mainnet` now uses:

- one shared `docker-compose.yml`,
- host-specific `seed-00.env`, `seed-01.env`, `seed-02.env`,
- `QWC_SERVICE_NODE_KEY_PATH=/service-node/service-node.key`,
- separate chain and identity volumes,
- `QWC_REWARD_ADDRESS` and matching `QWC_REWARD_VIEW_KEY`,
- two priority peers per seed.

For ordinary chain resets, preserve the identity volume. The fresh-identity
reset above intentionally deleted identity volumes because a clean test round
was requested.

## Current Open Gates

- Final reward spend/recipient receipt after denomination maturity.
- Dynamic 2/3 quorum hardening.
- Larger verifier committee evaluation.
- RandomX admission difficulty benchmark and hardening.
- Larger and longer multi-node validation.
- More adversarial partition/reorg testing.
- Broader sanitizer and long-fuzz runs.
