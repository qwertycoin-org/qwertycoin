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
| 4319 | Epoch 5 end | `b1be6607441fdd441264f5827401207e58052937f7a600287b442c1808d4160e` | `0x39566a3802` |
| 5039 | Epoch 6 end | `55ac2256275255e99628c19e9fbfb3eb8708016a68af87f9d327f253ea150ecc` | `0x4a90d5fca8` |
| 5759 | Epoch 7 end | `a39a08b8d95157e6ea39e33e91a2a7c1f25141db36f931911629b9e96d61737b` | `0x5b4dc1a177` |
| 6479 | Epoch 8 end | `4c4d825d6c1d56e4a173658c7e5dd001d76050ff6b620407d74f5069f3d12996` | `0x6c49228c02` |
| 7199 | Epoch 9 end | `5f69ac8432e5572d55a4cfca05c4cdef3964a95a09fe4b61ecb384908075072e` | `0x7d83b8cccc` |
| 7919 | Epoch 10 end | `ae157dc07f24fb8075cfda96bcff3e0c2c303c07f09b37674465ba47a3719b9e` | `0x908fdebe23` |
| 8639 | Epoch 11 end | `d60ea59c24852ef0c1bc12e25954550bae4592e94437182c0fc14c3755f3015c` | `0xa30db806e8` |
| 9359 | Epoch 12 end | `1a19876876537d20a2eea9ff6bfb1e55beb9687a666737ad462c28d870a31c4c` | `0xb153f7b7cd` |
| 10079 | Epoch 13 end | `24a4a4dca58f94bcdeac71795ce0e434023d9dfc086b72b40238dadd5f690998` | `0xbb0d648773` |
| 10799 | Epoch 14 end | `4c9b3093213620d56a5a26bf58a2a7423e212cd2bc0483eb3c8ca84f8346f5ed` | `0xc4611179c0` |
| 11519 | Epoch 15 end | `65abb7baad939246dc1dd9c0b5ac04f0e3cd5bc0ee1dca34a6abd873eabda935` | `0xcde8b8a389` |
| 12239 | Epoch 16 end | `e515a18cdcb584963c19ce289ed064c8bb78d8b51920363e97847da9d6ca01dd` | `0xe00312c3d8` |

The 17 post-genesis entries are the ends of completed 720-block EPoSE epochs.
The first five entries were verified on 2026-09-18. On 2026-10-01, every new
hash and cumulative difficulty was independently queried from six operator Core
daemons. The block hashes were also checked against the public Qwertycoin
Explorer. All queried headers were canonical, reported hard-fork version 17,
and the newest included entry was 817 blocks deep at block count 13057.

Epoch 17 ended at height 12959, but that block was only 97 blocks deep during
the 2026-10-01 review. It was deliberately not embedded yet because it was
below the 178-block confirmation precedent used for the previous update.

## Activation and compatibility

Hardcoded checkpoints take effect when a node starts a Core binary containing
them. There is no hard-fork height, database migration, or on-chain activation.
Existing databases are checked against the embedded hashes and cumulative
difficulties; fresh nodes enforce them while syncing.

Nodes running older binaries do not gain these checkpoints. During a mixed-version
rollout, an older node can follow a proof-of-work reorganization that a newer node
rejects if it conflicts at or below height 12239. Operators should therefore roll
the update out consistently across public infrastructure.

## Reorganization behavior

An upgraded node rejects a block whose hash does not match a checkpoint at that
height and does not retain alternative blocks at or below the latest applicable
checkpoint. Reorganizations wholly above height 12239 remain governed by normal
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
