# Qwertycoin Mainnet-Mode Testphase

> Historical snapshot - not current protocol documentation. This file records a
> previous validation phase. Current controlled mainnet validation is summarized
> in `MAINNET_VALIDATION.md`.

This document records the current non-final mainnet-mode deployment used for
live integration testing of PR #3.

## 2026-08-30 Two-Host Smoke

Source:

```text
branch: feature/epose-hardening-v2
commit: ae9bda3b1
image: qwertycoin-v2-node:mainnet-ae9bda3b1
```

Hosts:

```text
seed host A
seed host B
```

Container layout:

```text
container: qwertycoin-mainnet
volume: qwertycoin-mainnet-chain -> /root/.qwertycoin
P2P: 0.0.0.0:8196
daemon RPC: 127.0.0.1:8197
ZMQ RPC: 127.0.0.1:8199
mode: mainnet
```

The previous `qwertycoin-testnode` containers were stopped and removed, but
their Docker volumes were left intact.

Validation:

```text
seed host A image build: passed
seed host A EPoSE unit tests: 67/67 passed
seed host A EPoSE fuzz seeds: passed
seed host B image build: passed
seed host B EPoSE unit tests: 67/67 passed
seed host B EPoSE fuzz seeds: passed
```

Runtime smoke:

```text
seed host A nettype: mainnet
seed host A testnet: false
seed host A height: 1
seed host A EPoSE enabled: true
seed host A P2P: 1 incoming, 1 outgoing

seed host B nettype: mainnet
seed host B testnet: false
seed host B height: 1
seed host B EPoSE enabled: true
seed host B P2P: 1 incoming, 1 outgoing
```

The two hosts were connected with reciprocal priority nodes for the initial
smoke. Built-in DNS seed hostnames are present in the code, but seed-only
bootstrap still needs a fresh-node smoke without manual priority nodes.

The Docker image was built from a source export rather than a Git checkout on
both hosts to avoid touching dirty or SSH-restricted host repositories. The
binary therefore reports `v2.0.0-beta.1-unknown`; the source commit is tracked
by the image tag and this document for the testphase deployment.

## 2026-08-30 Mainnet-Mode Recheck

The running seed host A and seed host B daemons were rechecked after switching the public
explorer to mainnet mode.

Runtime state:

```text
seed host A nettype: mainnet
seed host A testnet: false
seed host A height: 1
seed host A top hash: e791e506200ba3a221b87b6c78359c2bbb13c3ef622e1c90b3b3fbb52f4943f5
seed host A EPoSE enabled: true
seed host A EPoSE state hash: d8f16d0b090f5755d3cd50f0ea6a28f46a5b3b13d9375b2dc1b0b42748a79e00
seed host A EPoSE service nodes: 0
seed host A EPoSE qualified nodes: 0

seed host B nettype: mainnet
seed host B testnet: false
seed host B height: 1
seed host B top hash: e791e506200ba3a221b87b6c78359c2bbb13c3ef622e1c90b3b3fbb52f4943f5
seed host B EPoSE enabled: true
seed host B EPoSE state hash: d8f16d0b090f5755d3cd50f0ea6a28f46a5b3b13d9375b2dc1b0b42748a79e00
seed host B EPoSE service nodes: 0
seed host B EPoSE qualified nodes: 0
```

EPoSE focused validation was repeated inside the deployed Docker image:

```text
seed host A EPoSE unit tests: passed
seed host A EPoSE fuzz seeds: passed
seed host B EPoSE unit tests: passed
seed host B EPoSE fuzz seeds: passed
```

The seed host A explorer now points at the mainnet-mode daemon and reports
`testnet:false` via `/api/networkinfo`. Its proxied `/qwc-rpc/get_epose_info`
endpoint reports EPoSE enabled with zero registered and zero qualified service
nodes, which matches the current height-1 chain state.

Seed DNS was also checked:

```text
seed-00.qwertycoin.org: resolves and accepts TCP on 8196, but P2P handshake reports wrong network
seed-01.qwertycoin.org: resolves to seed host A and accepts TCP on 8196
seed-02.qwertycoin.org: resolves to seed host B and accepts TCP on 8196
```

A fresh container without `--add-priority-node` currently does not complete a
seed-only bootstrap from the same public hosts used by seed host A/seed host B. Attempts
from those hosts are partially distorted by same-public-IP duplicate-peer
behavior, but `seed-00.qwertycoin.org` is clearly not running the current QWC
mainnet network id and must be updated or removed before public bootstrap can
be considered green.

## Non-Final Mainnet Gates

This deployment is suitable for mainnet-mode integration testing only. It is
not a final mainnet launch sign-off.

Open gates:

```text
final mainnet genesis / migration / activation decision
final admission-difficulty calibration
final service-reward privacy or explicit transparent-reward policy gate
final reward and fee tokenomics decision
longer fuzz and sanitizer passes
GitHub Actions after 2026-09-01
signed release artifacts and real update metadata
seed-00 on the correct QWC mainnet network id or removed from DNS
fresh seed-only bootstrap smoke from an independent third host
service-node registration and reward smoke on mainnet-mode chain
```
