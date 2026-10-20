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
| 12959 | Epoch 17 end | `6959c12d91fecf28a50d629ac5a2a18b37a64d7cde3a5327605792ced3f0300f` | `0xfbfe2feaa0` |
| 13679 | Epoch 18 end | `a6b9075826b44e14aeb4c3fb2c55d440fe79ef6b5601d7655ca90f1c6ef52ff2` | `0x118e0339a47` |
| 14399 | Epoch 19 end | `56cbd63c9d17270f553f5ecb56b211f183494e13bb225672b02dda76a7e4f9ee` | `0x136f1dacfa2` |
| 15119 | Epoch 20 end | `7220cff68f89e75718948e2208cc61426180f5388fd2de5193d22ce284620526` | `0x15a0ea5d9da` |
| 15839 | Epoch 21 end | `da196c6bfe5d25ca7a2c292ed7569add1a52baffd59ca2bc1b023dd4658708b2` | `0x17be5c8170a` |
| 16559 | Epoch 22 end | `8d5f2b38f90260e2f1845e3987ec9f471bf58564b2f0d93602b7f0dbdac81f58` | `0x1a52f36c67d` |
| 17279 | Epoch 23 end | `530dfec4570e59d75b6c91138c04b2eb1ac371f453f670c21af47cfbbdf13898` | `0x1caa4ca2d0b` |
| 17999 | Epoch 24 end | `0872682cfa0da18a74e120a87aabfc3e577cb4788b6b205a58d93420f79bd662` | `0x1ed19db6cbd` |
| 18719 | Epoch 25 end | `945a7de00e4b7b068126798c0ec09ec4efae8fd7ff9e674888be37bca8735ae6` | `0x208d442be99` |
| 19439 | Epoch 26 end | `420601b1815153173c93218329123a9880b45adf119ef9ee0b23ced4dfef5457` | `0x233cea81a8a` |
| 20159 | Epoch 27 end | `4398911ef51c5b811ec9cb1d0d5573e0daa032888e648d2591429f7a25642799` | `0x24e07830570` |
| 20879 | Epoch 28 end | `8e47cad1a2ec1fdc5279cb1edfb4b257f29086a2028bf3288eba6947cd0771ef` | `0x2600457e393` |
| 21599 | Epoch 29 end | `e37634450115b43429452dc120d9ee26c2aba74326c4a594db8ea1d85c31dc00` | `0x2700e6d87d7` |
| 21600 | Relay-hardening activation | `459877a4083844295a54c52197e2e74f9e912ca055f9262841f7ab340bac35a2` | `0x27013be15c9` |
| 22319 | Epoch 30 end | `45b311b0a6be0cfa50661494228021d9ff9a41f00d5d18487ec4b24c54835ad1` | `0x28065e5b322` |
| 23039 | Epoch 31 end | `cb30509efe6dbe1641578dd6a8f28944272f7281228ce4fcf00f7b6c38b78345` | `0x2923e77ff2e` |
| 23759 | Epoch 32 end | `400d01aac8813d4266eacd95c9ad539f77f377787e5e5d09fb3d5cb8be4258ad` | `0x29f548995f3` |
| 24479 | Epoch 33 end | `8bf8433b1bd11881acf71f750af9da264749126865fc8b170b41cc2979711e28` | `0x2ad2f01f417` |
| 25199 | Epoch 34 end | `28d4f0c224cd7ba84ecca9c64c36763ad38cdd4b8c830b2e9a0338e719e0b19c` | `0x2ba9342a571` |
| 25919 | Epoch 35 end | `152427834b4352c836b94620114c9594a8f1e4394669ce594b9db0b153606e23` | `0x2c86a5fc17f` |

Each post-genesis `Epoch N end` entry is the end of a completed 720-block
EPoSE epoch. The table is intentionally append-only: existing height, hash, and
cumulative-difficulty values must never be rewritten by automation.
Height 21600 is additionally pinned as the first block at the EPoSE
relay-hardening activation height.
The first five entries were verified on 2026-09-18. On 2026-10-01, every new
hash and cumulative difficulty was independently queried from six operator Core
daemons. The block hashes were also checked against the public Qwertycoin
Explorer. All queried headers were canonical, reported hard-fork version 17,
and the newest included entry was 817 blocks deep at block count 13057. On
2026-10-02, epochs 17 and 18 were independently queried from the same six
operator daemons and cross-checked against the public Explorer. All six daemons
returned identical canonical HF17 hashes and cumulative difficulties. At block
count 14016, heights 12959 and 13679 were respectively 1056 and 336 blocks deep.
On 2026-10-14, epochs 19 through 29 were independently queried from three
operator Core daemons. All three returned identical canonical HF17 hashes and
cumulative difficulties, and every hash was cross-checked against the public
Qwertycoin Explorer. The activation checkpoint at height 21600 was verified by
the same three daemons and the public Explorer. At block count 21704, it was
104 blocks behind the reported current height. On 2026-10-15, epoch 30 was
queried from all six public operator Core RPCs and cross-checked against the
HTTPS public Explorer. Every source returned the same canonical HF17 block
hash; the Core RPCs also agreed on cumulative difficulty. The shallowest Core
observation placed height 22319 251 blocks behind its reported tip.

## Activation and compatibility

Hardcoded checkpoints take effect when a node starts a Core binary containing
them. There is no hard-fork height, database migration, or on-chain activation.
Existing databases are checked against the embedded hashes and cumulative
difficulties; fresh nodes enforce them while syncing.

Nodes running older binaries do not gain these checkpoints. During a mixed-version
rollout, an older node can follow a proof-of-work reorganization that a newer node
rejects if it conflicts at or below height 21600. The activation checkpoint makes
the observed post-activation chain authoritative for updated builds; it cannot
change the behavior of already-built old software. Operators should therefore roll
the update out consistently across public infrastructure.

## Reorganization behavior

An upgraded node rejects a block whose hash does not match a checkpoint at that
height and does not retain alternative blocks at or below the latest applicable
checkpoint. Reorganizations wholly above height 21600 remain governed by normal
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

### Automated epoch proposals

`.github/workflows/propose-epoch-checkpoints.yml` runs every six hours. It only
creates or refreshes the dedicated `automation/mainnet-epoch-checkpoints`
review branch after all of these gates pass:

1. the candidate is an exact 720-block epoch boundary;
2. every configured public Core RPC reports mainnet, synchronized state, and
   at least 60 blocks above the candidate;
3. all six Core RPCs return the same lowercase block hash, cumulative
   difficulty, and hard-fork major version;
4. the HTTPS public Explorer independently reports the same height and block
   hash; and
5. source, unit-test, and documentation tables agree before and after the
   append-only update.

The workflow may prepare a pull request, but it cannot approve or merge it.
Maintainers must still confirm the candidate from an additional independently
operated Core node and review CI. HTTP transport to the restricted seed RPCs is
not an authentication boundary; the multi-source and HTTPS checks are evidence
for review, not a replacement for it.

At one boundary per day, retaining each historical row is small (roughly 365
rows per year) and useful for releases, incident analysis, and bootstrap
review. If checkpoint review or release cadence becomes less frequent, the bot
keeps one rolling PR containing every missing mature boundary instead of
opening one PR per epoch.

### DNS checkpoint boundary

This workflow does not publish DNS. `checkpoints.qwertycoin.org` and Core's DNS
checkpoint loader remain disabled until a separate reviewed design defines
QWC-owned signing keys, a signed and versioned payload, generation and rollback
rules, bounded record retention, DNSSEC operations, and independent operator
approval. DNSSEC authenticates the zone response; it does not by itself prove
that a block is canonical.

Once that design exists, DNS should carry only a bounded rolling set of mature
reviewed checkpoints for update between binary releases. It should not mirror
the unbounded historical source table or publish a running-epoch tip.
