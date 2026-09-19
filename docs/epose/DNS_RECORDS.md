# Qwertycoin DNS Records

This document records QWC-owned DNS names used by current Core and separates
safe placeholders from release-gated metadata.

## Update Metadata

Do not copy inherited `updates.moneropulse.org` TXT records.

Core and GUI update checks use this QWC-owned name only. Records are accepted
only when the complete DNS response validates through DNSSEC. The selected
artifact URL is not read from DNS: code maps a strict software/build-tag
allowlist to the corresponding `qwertycoin-org` GitHub release repository.

The live RRset contains exactly seven records: three Core channels and four GUI
channels.  The placeholder used before updater activation must not coexist with
them. Existing v2.0.1 binaries do not contain the active updater and therefore
require one manual update to enter this release channel.

Release records must only be published after the corresponding public artifact
exists and its SHA-256 has been independently verified. Core metadata uses:

```text
updates.qwertycoin.org. 300 IN TXT "qwertycoin:linux-x64:<version>:<sha256>"
updates.qwertycoin.org. 300 IN TXT "qwertycoin:mac-armv8:<version>:<sha256>"
updates.qwertycoin.org. 300 IN TXT "qwertycoin:win-x64:<version>:<sha256>"
```

GUI metadata uses distinct software and installed/portable Windows tags:

```text
updates.qwertycoin.org. 300 IN TXT "qwertycoin-gui:linux-x64:<version>:<sha256>"
updates.qwertycoin.org. 300 IN TXT "qwertycoin-gui:mac-armv8:<version>:<dmg-sha256>"
updates.qwertycoin.org. 300 IN TXT "qwertycoin-gui:install-win-x64:<version>:<setup-sha256>"
updates.qwertycoin.org. 300 IN TXT "qwertycoin-gui:win-x64:<version>:<portable-zip-sha256>"
```

`<version>` is the public three-component release version, for example
`2.0.2`, and maps to GitHub tag `v2.0.2`. Hashes are exactly 64 lowercase
hexadecimal characters. The macOS GUI channel deliberately selects the DMG as
the preferred package.

Source builds and macOS Intel currently have no supported update artifact.
Do not publish `source` or `mac-x64` records until release workflows produce and
verify matching public assets and the code allowlist is updated.

Do not publish fake hashes, inherited Monero version numbers, conflicting
hashes for the same version, or records for artifacts that were not built and
verified. The updater fails closed on malformed or conflicting matching
records.

Records with empty version or hash fields, such as `qwertycoin:mac-armv8::`,
are invalid release metadata and must not be published.

## Automated publication

`.github/workflows/update-release-dns.yml` owns the updater RRset. It runs when
a Core release is published and on a 30-minute reconciliation schedule. The
schedule is required because Core and GUI are separate repositories and their
release workflows may finish at different times. Publication waits until both
repositories expose the same stable `vMAJOR.MINOR.PATCH` tag.

The workflow:

1. selects the latest common stable Core/GUI release (or an exact tag supplied
   by a manual dispatch);
2. requires the complete, exact release asset set from both repositories;
3. downloads each `SHA256SUMS` file and compares every selected digest with
   GitHub's release-asset digest metadata;
4. requires exactly the seven compiled updater channels in Cloudflare;
5. rejects prereleases, downgrades, mixed versions and changed bytes under a
   version that is already published in DNS;
6. changes only the seven `updates.qwertycoin.org` TXT records, with automatic
   rollback on an API or verification failure; and
7. requires the exact new RRset with DNSSEC `AD=true` from both Cloudflare and
   Google public resolvers.

The publishing job uses the protected GitHub Environment `release-dns`. Its
only credential is the Environment secret `CLOUDFLARE_API_TOKEN`. The token
must be a scoped API token limited to `Zone:Read` and `DNS:Edit` for the
`qwertycoin.org` zone; never use a Cloudflare Global API Key. The zone and
record names are fixed in reviewed source, and the token is never accepted as a
workflow input, command-line argument or repository file.

Manual verification or recovery can dispatch `qwc/update-release-dns` with an
exact stable tag. Leave `apply` disabled for a release-only plan; enable it only
to run the same guarded publication path used by the schedule.

Initial activation order:

1. enable DNSSEC at the registrar and authoritative DNS provider;
2. verify `DS`/`DNSKEY` and an authenticated (`AD`) TXT response externally;
3. configure the `release-dns` Environment and its scoped token;
4. dispatch a plan for the intended stable tag;
5. dispatch with `apply` enabled and verify all seven authenticated records.

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
disabled or unpublished until QWC-owned signing keys, generation tooling, and
operational procedures are defined. These records must not be copied from
Monero infrastructure.
