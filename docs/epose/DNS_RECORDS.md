# Qwertycoin DNS Records

This document records the DNS records expected by the current QWC v2 / EPoSE
testnet branch and separates safe placeholders from release-gated metadata.

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
disabled or unpublished until QWC-owned signing keys, generation tooling, and
operational procedures are defined. These records must not be copied from
Monero infrastructure.
