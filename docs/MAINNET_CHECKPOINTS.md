# Mainnet hardcoded checkpoints

Qwertycoin Core embeds reviewed block-hash and cumulative-difficulty checkpoints
for Reset-1 mainnet. These checkpoints prevent an upgraded node from accepting a
history that conflicts with the known canonical chain at a checkpoint height.
They do not replace RandomX proof of work or change chain selection above the
latest checkpoint.

## Current checkpoints

| Height | EPoSE boundary | Block hash | Cumulative difficulty |
| ---: | ---: | --- | ---: |
| 0 | Genesis | `4f95857586e2c66063c277370eda99cd75897d773af09f0c3cd1e22f7e87db39` | `0x1` |
| 719 | Epoch 0 end | `69cf9a283099298d1ea9c71b14b0f1d73c7ca81c87e4a2303c1ea91ff8c5b68d` | `0xde51cae4` |
| 1439 | Epoch 1 end | `019b6a911b0fd41f6e83740f1c4c3fc83f5299339b62cb532b48c97b1e2c35f1` | `0x3705926df` |
| 2159 | Epoch 2 end | `011bdb8505cc457a3b4be6deccd8fb7fcd55849075b67cbac1f1601b034860a0` | `0x7a9125791` |
| 2879 | Epoch 3 end | `cc954d4cb4352affc224a9191fb356932e4a9ec405e89b4932654fb1737a50d0` | `0x13e28e554a` |
| 3599 | Epoch 4 end | `75c7cff8db59f803962fa86ab79a7429ebd3cd2025968bbe80d5744554ba35db` | `0x246cae087a` |

The five post-genesis entries are the ends of the completed 720-block EPoSE
epochs. On 2026-09-18, every hash and cumulative difficulty was independently
queried from three operator Core daemons. The block hashes were also checked
against the public Qwertycoin Explorer. All queried headers were canonical,
reported hard-fork version 17, and the newest entry was 178 blocks deep.

## Activation and compatibility

Hardcoded checkpoints take effect when a node starts a Core binary containing
them. There is no hard-fork height, database migration, or on-chain activation.
Existing databases are checked against the embedded hashes and cumulative
difficulties; fresh nodes enforce them while syncing.

Nodes running older binaries do not gain these checkpoints. During a mixed-version
rollout, an older node can follow a proof-of-work reorganization that a newer node
rejects if it conflicts at or below height 3599. Operators should therefore roll
the update out consistently across public infrastructure.

## Reorganization behavior

An upgraded node rejects a block whose hash does not match a checkpoint at that
height and does not retain alternative blocks at or below the latest applicable
checkpoint. Reorganizations wholly above height 3599 remain governed by normal
proof-of-work chain selection and all existing consensus rules.

These entries are separate from `src/blocks/checkpoints.dat`, which is the
per-block fast-sync hash mechanism, and from runtime `checkpoints.json` or DNS
checkpoint sources. This update changes none of those mechanisms.

## Updating checkpoints

Checkpoint additions require a separate review. At minimum, verify the exact
height, block hash, and cumulative difficulty against multiple independently
operated Core daemons; cross-check the hash with an independent public view;
record the observed depth; and add positive and negative regression tests. Do
not derive or advance checkpoints automatically from a single RPC endpoint.
