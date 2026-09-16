# EPoSE v2 compiled parameters

These values are compiled into the public-chain consensus profile by
`compiled_consensus_parameters_v2()` in `src/epose/coordinator_v2.cpp`.
Command-line arguments, environment files, DNS, RPC responses, and this
document cannot override them.

## Profile binding

| Parameter | Compiled value |
| --- | --- |
| Network | mainnet |
| QWC hardfork version | `17` from height `0` |
| EPoSE protocol version | `2` |
| Genesis hash | `4f95857586e2c66063c277370eda99cd75897d773af09f0c3cd1e22f7e87db39` |
| Parameter-set hash | `2c26755094535871dd3ede7bd1b50aba82a9fb6831f0a17f32968eb0385145c6` |
| State commitment schema | `1` |

The genesis and parameter-set hashes are also compiled in
`src/epose/compiled_profile_v2.h`. A different genesis or network does not load
this profile.

## Timing

| Parameter | Value |
| --- | ---: |
| Activation height | `0` |
| Epoch length | `720` blocks |
| Anchor/finality depth | `60` blocks |
| First service epoch | `1` |
| First payout height | `1440` (start of epoch `2`) |
| Admission context offset | `1` epoch |
| Admission lease | target epoch only |
| In-memory undo minimum | `2160` blocks |

For epoch `E`, the admission cutoff is `E*720 - 61`, the committee anchor is
`E*720 - 60`, and the inclusive evidence deadline is
`(E+1)*720 - 61`.

## Admission and membership

| Parameter | Value |
| --- | ---: |
| Work algorithm | RandomX |
| Leading-zero target | `18` bits |
| Maximum active population | `100` |
| Committee target | `9` non-subject members |
| Full-size threshold | `6` |
| Round offsets | `0`, `200`, `400` |
| Rounds required | `2` of `3` |
| Service kind | canonical object (`1`) |

Small-network quorum is derived from the committee that can actually be
formed: `ceil(2*n/3)`, implemented as `n - n/3`. The subject is excluded. Thus
the target is 6-of-9, while a four-member snapshot forms a three-verifier
committee requiring two receipts per passing round.

## Rewards

| Parameter | Value |
| --- | --- |
| Service reward | `1000` basis points (10%) |
| Reward base | scheduled subsidy only |
| Transaction fees | 100% miner |
| Empty qualification | miner fallback |
| Payout source | immediately preceding closed epoch |
| Payment proof | scoped Coinbase proof, record version `1` |

## Envelope and block limits

| Limit | Value |
| --- | ---: |
| Envelopes per transaction | `4` |
| Bytes per envelope | `65,536` |
| Records per envelope | `256` |
| Bytes per record payload | `65,536` |
| Signature verifications per envelope | `2,048` |
| RandomX admission verifications per envelope | `8` |
| EPoSE bytes per block | `262,144` |
| EPoSE records per block | `1,024` |
| Signature verifications per block | `2,048` |
| RandomX admission verifications per block | `8` |

Structural and cryptographic costs are charged before duplicate elimination or
semantic application.

## Relay and template limits

The non-consensus relay policy is compiled by `compiled_relay_policy_v2()` in
`src/epose/relay_pool_v2.cpp` and bounded by the consensus envelope limits.
The checked-in manifest records the current policy ceilings:

| Limit | Value |
| --- | ---: |
| Relay queue items | `2,048` |
| Relay queue bytes | `1,048,576` |
| Template records | `256` |
| Template EPoSE bytes | `32,768` |
| Reserved enrollment queue | `512` items / `262,144` bytes |
| Reserved evidence queue | `512` items / `262,144` bytes |
| Reserved enrollment template | `64` records / `8,192` bytes |
| Reserved evidence template | `64` records / `8,192` bytes |

These values control local queueing and template selection. They cannot make an
otherwise invalid block valid or a valid complete block invalid.

## Change procedure

Consensus values require a new reviewed parameter commitment and an explicit
activation/migration rule. Editing `PARAMETER_MANIFEST_V2.json` alone does not
change runtime consensus. Editing only C++ without updating the manifest and
its tests breaks the source/profile consistency checks.

Benchmarks in [`BENCHMARKS.md`](BENCHMARKS.md) are evidence, not parameter
sources. Never infer a new admission target or population ceiling from one
host.
