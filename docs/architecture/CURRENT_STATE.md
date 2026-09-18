# Qwertycoin Core current state

This page summarizes the implementation on the current branch. Exact EPoSE
rules are documented under [`../epose/`](../epose/README.md); the source code
and consensus tests are authoritative.

## Network baseline

Qwertycoin v2 derives from the Monero 0.18 code family but starts an independent
chain and uses QWC-owned network identifiers, address prefixes, ports, genesis,
and release artifacts.

| Item | Current value |
| --- | --- |
| Daemon | `qwertycoind` |
| Wallet CLI | `qwertycoin-wallet-cli` |
| Wallet RPC | `qwertycoin-wallet-rpc` |
| Proof of work | RandomX |
| Target block time | `120` seconds |
| Display decimals | `8` |
| Atomic units per QWC | `100,000,000` |
| Mainnet P2P | `8196` |
| Daemon RPC | `8197` |
| Restricted/wallet RPC convention | `8198` |
| ZMQ RPC | `8199` |
| Mainnet standard-address prefix | `0x14820c` |

RandomX remains responsible for block production and cumulative-work chain
selection. EPoSE neither mines blocks nor replaces proof of work.

## Activation profile

QWC does not replay Monero's historical hardfork schedule. The public chain
starts at QWC hardfork version `17` at height `0`, with EPoSE protocol version
`2` bound to:

- genesis `4f95857586e2c66063c277370eda99cd75897d773af09f0c3cd1e22f7e87db39`;
- parameter set `2c26755094535871dd3ede7bd1b50aba82a9fb6831f0a17f32968eb0385145c6`.

`compiled_consensus_parameters_v2()` accepts this profile only for mainnet and
the exact genesis. Runtime flags cannot change its consensus values. Older
Monero hardfork constants and retired EPoSE-v1 helpers remain in source where
inherited validation, compatibility errors, or tests still reference them;
they are not an alternate public-chain mode.

## EPoSE v2

The active implementation provides:

- offline operator identities and rotating online service keys;
- signed, future-effective lifecycle and endpoint updates;
- epoch-scoped RandomX admission with an 18-leading-zero-bit target;
- 720-block epochs and a 60-block anchor depth;
- frozen, canonically ordered membership capped at 100 identities;
- subject-specific verifier committees of at most 9 non-subject members;
- a dynamic `ceil(2n/3)` receipt quorum for the committee actually formed;
- three rounds at offsets `0`, `200`, and `400`, with two passing rounds
  required;
- canonical-object challenges and dual-signed service receipts;
- transaction-extra `0x05` / `QEP2` envelopes with bounded parsing, relay, and
  template policies;
- fail-atomic state transitions, persisted commitments, bounded undo, replay,
  and reorg handling;
- a 10% service allocation from scheduled subsidy only, with all fees paid to
  the miner and full miner fallback for an empty qualification set;
- deterministic payee rotation and a scoped Coinbase payment proof;
- restricted read RPCs plus an unrestricted-only envelope submission method.

Epoch `1` is the first service epoch. The first possible service payout is at
height `1440`, the start of epoch `2`.

Exact values and limits are in
[`../epose/SECURITY_PARAMETERS.md`](../epose/SECURITY_PARAMETERS.md). Protocol
semantics are in [`../epose/PROTOCOL.md`](../epose/PROTOCOL.md).

## Service-node runtime

Service-node production is opt-in through `--epose-v2-service` and the
`--epose-v2-*` options. The protected v2 keystore contains operator and service
identity material bound to network, genesis, and parameter set. It does not
contain wallet spend or view keys. Rewards use a normal public QWC address.

The repository contains portable operator examples and the public network
defaults required by clients. Private deployment topology, host inventories,
administrative endpoints, access methods, monitoring, and secret-bearing
configuration are intentionally outside this public documentation and must not
be committed. Deployment inputs are not consensus configuration.

## Validation surface

The repository includes focused unit, negative, replay, reorg, resource-limit,
manifest, release-gate, integration, fuzz, and benchmark targets. Historical
review evidence is retained under [`../epose/review/`](../epose/review/), but
its snapshot verdicts must not be read as a description of later code.

The machine-readable parameter manifest is an activation artifact whose
dependency commit fields intentionally preserve the reviewed snapshot. Current
runtime behavior is checked against the compiled profile and current tests,
not inferred from those historical commit IDs.

## Documentation rule

Canonical documents describe only implemented behavior. Proposed changes and
past release assessments belong in review records or Git history and must be
clearly labelled non-normative. If a document disagrees with validation code,
the code wins and the document should be corrected in the same change that
finds the mismatch.
