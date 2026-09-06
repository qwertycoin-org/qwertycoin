# Qwertycoin DNS Records

This document records the DNS records expected by QWC v2 and separates safe
operational records from release-gated metadata.

## DNS Checkpoint Decision

DNS checkpoints are **not available** in the current daemon. This is an
intentional fail-closed release state:

- the daemon does not query checkpoint records periodically;
- `--disable-dns-checkpoints` remains accepted for operator compatibility;
- `--enforce-dns-checkpointing` is rejected instead of silently enforcing an
  empty checkpoint set;
- local JSON checkpoints and hardcoded QWC checkpoints remain independent of
  the DNS setting.

Do not publish checkpoint TXT records yet. DNS checkpoint activation requires
its own reviewed PR and all of these gates:

1. QWC-owned checkpoint generation tooling with reproducible input/output;
2. a documented signer and key-rotation/revocation procedure;
3. multiple independently administered DNS zones and a defined quorum rule;
4. authenticated DNS responses, including validated DNSSEC behavior and a
   documented resolver/failure policy;
5. monotonic height and conflict/reorg handling with positive and negative
   tests;
6. operator runbooks for publication, rollback, outage, compromise, and audit;
7. live validation proving that unavailable or conflicting DNS cannot split
   honest nodes or silently disable JSON/hardcoded checkpoints.

DNS is bootstrap/hardening metadata, never an EPoSE oracle and never a
replacement for RandomX chain selection.

## Update Metadata

Do not copy inherited `updates.moneropulse.org` TXT records.

Current code deliberately disables runtime update checks until QWC-owned signed
release metadata exists. Publishing inherited Monero records would make QWC
tooling point at unrelated binaries and hashes.

Recommended current state:

```text
updates.qwertycoin.org. 300 IN TXT "qwc:update-metadata-not-yet-published"
```

The placeholder is informational only. Current QWC update code does not consume
it for update decisions.

Future release-gated records should only be published after QWC release
artifacts exist and their SHA-256 hashes are known. Candidate format:

```text
updates.qwertycoin.org. 300 IN TXT "qwertycoin:source:<version>:<sha256>"
updates.qwertycoin.org. 300 IN TXT "qwertycoin:linux-x64:<version>:<sha256>"
updates.qwertycoin.org. 300 IN TXT "qwertycoin:mac-armv8:<version>:<sha256>"
updates.qwertycoin.org. 300 IN TXT "qwertycoin:mac-x64:<version>:<sha256>"
updates.qwertycoin.org. 300 IN TXT "qwertycoin:win-x64:<version>:<sha256>"
```

Do not publish fake hashes, inherited Monero version numbers, or records for
artifacts that were not built and verified.

Records with empty version or hash fields, such as `qwertycoin:mac-armv8::`,
are invalid release metadata and must not be published.

## OpenAlias Donation Record

QWC OpenAlias parsing uses the `oa1:qwc` marker. Inherited `oa1:xmr` records are
intentionally rejected.

Do not publish `donate.qwertycoin.org` as an OpenAlias record until a real QWC
donation wallet exists. A valid record needs a QWC address for the intended
network.

Mainnet template:

```text
donate.qwertycoin.org. 300 IN TXT "oa1:qwc recipient_address=<QWC_MAINNET_DONATION_ADDRESS>; recipient_name=Qwertycoin Development Fund; tx_description=Donation to Qwertycoin Development Fund;"
```

For testnet-only donation testing, use a clearly separate hostname instead of
the public donation name:

```text
donate-testnet.qwertycoin.org. 300 IN TXT "oa1:qwc recipient_address=<QWC_TESTNET_DONATION_ADDRESS>; recipient_name=Qwertycoin Testnet Faucet; tx_description=Testnet-only QWC donation/faucet address;"
```

Do not publish inherited `donate.getmonero.org` addresses or non-QWC `oa1:xmr`
records under Qwertycoin domains.

## Other QWC-Owned DNS Names

The debug DNS utility checks these QWC-owned names:

```text
seeds.qwertycoin.org
updates.qwertycoin.org
checkpoints.qwertycoin.org
segheights.qwertycoin.org
```

`seeds.qwertycoin.org` should resolve to QWC seed node addresses when stable
seed nodes are available.

The daemon's built-in mainnet DNS seed list contains these individual seed
hostnames:

```text
seed-00.qwertycoin.org
seed-01.qwertycoin.org
seed-02.qwertycoin.org
seed-03.qwertycoin.org
```

Each seed hostname should publish `A` and, where available, `AAAA` records for
publicly reachable QWC P2P nodes. The daemon appends the network's default P2P
port from `src/cryptonote_config.h` when it turns those DNS answers into peer
addresses.

Current P2P default:

```text
8196
```

DNS checkpoints, DNS blocklists, and segregation-height records must remain
disabled or unpublished until the gates above are implemented and reviewed.
These records must not be copied from Monero infrastructure.
