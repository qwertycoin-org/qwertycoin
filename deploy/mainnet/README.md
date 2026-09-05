# Qwertycoin Mainnet Node Deployment

These files start a Qwertycoin v2 EPoSE mainnet seed with one shared Docker
Compose definition and one host-specific env file. `QWC_REWARD_ADDRESS` is the
public reward address embedded in new service-node registrations.
`QWC_REWARD_VIEW_KEY` is the reward wallet private view key disclosed for
consensus reward validation. It does not allow spending, but it makes incoming
reward activity observable.

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

Before starting a public node, replace the `CHANGE_ME_*` values in the host env
file with operator-specific values. Do not commit real operator reward
addresses, reward view keys, or private host details to this repository.

The service-node identity key is stored outside the chain volume at
`QWC_SERVICE_NODE_KEY_PATH`, mounted through `QWC_IDENTITY_VOLUME`. Keep that
identity volume during chain resets.

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
