# EPoSE v2 Design

## Source Research

Historical QWC material defines the intent:

- QWC should reward public node operation and uptime.
- EPoSE should not require classic capital-heavy masternode staking.
- Sentinel, Node Map, and Explorer were planned as measurement and display components.
- A 24-hour reward maturity period was proposed.
- Accumulated transaction fees were proposed as service-node rewards.

The historical design is not directly consensus-safe because it relies on central Sentinel measurements, node IP/location metadata, and insufficient Sybil resistance.

## Selected Direction

EPoSE v2 is a PoW-secured service-node reward layer:

1. RandomX PoW remains canonical chain consensus.
2. Service nodes register a long-lived service key and reward address.
3. Admission requires identity-bound work, so identities are not free.
4. Service quality is proven through epoch challenges and signed attestations.
5. Qualification and payout rotation are deterministic from finalized chain state.
6. Sentinel v2 is only a monitor over consensus state.

## Admission

The first implementation adds an identity-bound admission hash:

```text
H(
  network_id
  || service_public_key
  || reward_address
  || endpoint_commitment
  || registration_epoch
  || previous_epoch_hash
  || nonce
)
```

For production consensus this admission work should be upgraded to a RandomX-bound proof before activation. The current module intentionally isolates the rule so that tests and later RandomX admission verification can replace the hash primitive without touching wallet, RPC, or P2P code.

## Epochs

`EPOSE_EPOCH_LENGTH = 720` blocks. At 120-second block target this is about 24 hours, matching the historical reward maturity idea.

```text
epoch = floor(height / 720)
epoch_start = epoch * 720
epoch_end = epoch_start + 719
epoch_seed_height = max(0, epoch_start - 60)
```

`EPOSE_FINALITY_DEPTH = 60` avoids selecting committees from immediately reorgable tip data.

## Attestations

An attestation binds:

- verifier service key
- subject service key
- epoch
- challenge hash
- response hash
- observed tip hash
- service result

Self-attestation is invalid. Duplicate verifier votes count once. Invalid signatures and negative observations do not add qualification weight.

## Rewards

The testnet rule splits `base_reward + fees` using `EPOSE_SERVICE_REWARD_BPS = 1000` for a 10% service reward pool. This is a testnet engineering constant, not a final mainnet tokenomics decision.

Only one qualified service node is selected per block by deterministic rotation over an epoch-seeded sorted set. This keeps Coinbase size bounded even with many qualified nodes.
