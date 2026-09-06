# Qwertycoin Public Mainnet Launch Readiness

Date: 2026-09-05
Goal: public mainnet start with users and public mining.

## Current Decision

The current branch prepares a release-candidate launch path. It does not mark
the public mainnet launch as complete.

## Release Candidate Baseline

```text
core branch: main, then release-candidate branch
release candidate: v2.0.0-rc.1
node image: qwertycoin-v2-node:v2.0.0-rc.1
public p2p: 8196
private daemon rpc: 8197
optional restricted rpc: 8198
private zmq rpc: 8199
```

The closed QWCP public-address prototype is excluded from the launch baseline.

## Public Launch Sequence

1. Build the RC node image from the release branch with
   `deploy/mainnet/build-rc-image.sh`.
2. Run EPoSE unit, fuzz, ASan, and deployment-config validation.
3. Push the RC image to each seed host or to the chosen registry.
4. Start `seed-00`, `seed-01`, `seed-02`, and `seed-03` with preserved service identity
   volumes and fresh chain volumes for the coordinated launch chain.
5. Confirm all seeds share genesis hash, top hash, height, network type, and
   EPoSE state hash.
6. Validate DNS records for all seed hostnames.
7. Start a fresh independent node without priority nodes and confirm DNS-only
   bootstrap and sync.
8. Start public mining from a non-seed wallet or miner.
9. Register at least one service node on-chain from an operator wallet.
10. Mine through qualification, reward creation, reward maturity, reward spend,
    and recipient receipt.
11. Deploy or restart the Explorer against the public network.
12. Publish release artifacts and checksums.
13. Announce public mining only after the above gates are green.

## Immediate Blockers

- Produce a current RC image from `main` with `deploy/mainnet/build-rc-image.sh`;
  old `epose-pr*` images are not valid for public launch.
- Repair or replace red GitHub Actions with recorded local validation.
- Complete DNS-only bootstrap from an independent host.
- Keep DNS checkpoints fail-closed until the trust model, tooling, independent
  zones, authentication, and operational procedures in
  `docs/epose/DNS_RECORDS.md` pass review.
- Complete matured EPoSE reward spend and recipient receipt.
- Record public mining from a non-seed miner.
- Publish real release artifacts and hashes.

## Resource Priority

Seed nodes and mining validation have priority over Explorer layout work. The
Explorer is launch-supporting monitoring, not a consensus component.
