# Qwertycoin Mainnet Node Deployment

These files start a Qwertycoin v2 EPoSE mainnet seed with one shared Docker
Compose definition and one host-specific env file. `QWC_REWARD_ADDRESS` is the
public reward address embedded in new service registrations. The hardened v2
path never takes or discloses a reward private view key.

The release-candidate launch profile expects a node image built from the current
release branch and tagged as:

```text
qwertycoin-v2-node:v2.0.0-rc.1
```

Do not start public mainnet nodes from older PR or testphase image tags.

Build the release-candidate image from a clean worktree:

```bash
./deploy/mainnet/build-rc-image.sh
```

For disposable local validation builds only, a dirty worktree can be allowed
explicitly:

```bash
QWC_ALLOW_DIRTY_BUILD=1 ./deploy/mainnet/build-rc-image.sh
```

Start a node with Docker Compose:

```bash
docker compose --env-file deploy/mainnet/seed-00.env -f deploy/mainnet/docker-compose.yml up -d
```

On hosts without the Docker Compose plugin, use the Docker-only helper script
below.

Start a node without deleting chain data:

```bash
./deploy/mainnet/start-qwertycoin-node.sh deploy/mainnet/seed-00.env
```

Start with a fresh chain volume for a coordinated network restart:

```bash
QWC_RESET_CHAIN=1 ./deploy/mainnet/start-qwertycoin-node.sh deploy/mainnet/seed-00.env
```

Use the matching env file for each host:

- `seed-00.env` for `seed-00.qwertycoin.org`
- `seed-01.env` for `seed-01.qwertycoin.org`
- `seed-02.env` for `seed-02.qwertycoin.org`
- `seed-03.env` for `seed-03.qwertycoin.org`

Before starting a public node, replace the `CHANGE_ME_*` values in the host env
file with operator-specific values. Do not commit real operator reward
addresses, wallet secrets, keystore contents, or private host details to this
repository.

The genesis- and parameter-bound v2 operator/service keystore is stored outside
the chain volume at `QWC_EPOSE_V2_KEYSTORE_PATH`, mounted through
`QWC_IDENTITY_VOLUME`. A distinct final-launch genesis requires a newly bound
keystore; do not copy an older v1 identity into this path.

The unrestricted admin RPC is published only on loopback at port 8197. The
restricted listener is intentionally public at port 8198 so assigned committee
members can fetch signed endpoint descriptors and answer bounded service
challenges. It must not expose `/submit_epose_envelope`; that method belongs only
to the unrestricted listener. `QWC_EPOSE_V2_DISCOVERY_ENDPOINTS` is the
comma-separated bootstrap list used to discover signed endpoint descriptors.
It does not authorize participants and is not an admission allowlist.

## Bootstrap Modes

The host env files contain priority peers for controlled seed-cluster startup.
The Compose profile treats them as optional so public launch validation can also
run a seed-only bootstrap through DNS records.

For the initial coordinated seed start, keep `QWC_PRIORITY_NODE_1` and
`QWC_PRIORITY_NODE_2` set. For the public bootstrap gate, unset both values and
verify a fresh node can find peers through:

```text
seed-00.qwertycoin.org
seed-01.qwertycoin.org
seed-02.qwertycoin.org
```

Do not announce public mining until an independent fresh node can sync without
manual `--add-priority-node` values.
