# Qwertycoin Docker image

The public image is `docker.io/qwertycoin/qwertycoin`. It packages the three
programs from one verified Linux Core release archive; Docker does not compile
Core a second time. Images are currently published for `linux/amd64` only.

Use an immutable version tag or digest in production. The examples below use
the current prerelease tag; replace it with the release you have reviewed.

## Program selection

The daemon is the default. `daemon` is an optional explicit selector, while
`wallet` and `wallet-rpc` select the other two programs. Arguments are passed
unchanged to the selected program.

```sh
docker run --rm docker.io/qwertycoin/qwertycoin:2.0.1-rc1 --version
docker run --rm -it docker.io/qwertycoin/qwertycoin:2.0.1-rc1 wallet --help
docker run --rm docker.io/qwertycoin/qwertycoin:2.0.1-rc1 wallet-rpc --help
```

These commands only verify program selection. They do not persist data or run a
production node.

## Full node with Compose

Copy the example files and pin the desired image in `.env`:

```sh
cp docker/.env.example docker/.env
docker compose --env-file docker/.env -f docker/compose.yml up -d daemon
docker compose --env-file docker/.env -f docker/compose.yml logs -f daemon
```

Mainnet is the image and Compose default. The daemon stores its chain in the
named `chain` volume, publishes P2P on TCP 8196, and exposes administrative RPC
only on host loopback port 8197. ZMQ is disabled. The Compose network is created
automatically; no pre-existing Docker network or local compilation is needed.

The short-lived `volume-init` service prepares only the roots of fresh named
volumes. The long-running programs run as UID/GID `10001:10001`, with all Linux
capabilities dropped and a read-only root filesystem. Do not use `chmod 777` or
run the daemon permanently as root to work around host-volume permissions.

Stop cleanly without deleting volumes:

```sh
docker compose --env-file docker/.env -f docker/compose.yml stop
```

## Interactive CLI wallet

Initialize or open the wallet interactively:

```sh
docker compose --env-file docker/.env -f docker/compose.yml \
  --profile tools run --rm wallet-cli
```

The wallet is stored in the named `wallets` volume. Keep its password and seed
offline. Never open the same wallet concurrently in CLI and wallet RPC.

## Optional wallet RPC

A normal full node or EPoSe service node does not need wallet RPC. To enable it,
create protected runtime files that are never built into the image:

```sh
mkdir -p docker/secrets
cp docker/wallet-rpc.conf.example docker/secrets/wallet-rpc.conf
# Edit wallet-rpc.conf and replace the RPC password.
# Put the existing wallet password in wallet-password.txt (one line).
# Stop wallet-cli, then grant only the container account access to these files.
sudo chown 10001:10001 docker/secrets/wallet-rpc.conf docker/secrets/wallet-password.txt
sudo chmod 400 docker/secrets/wallet-rpc.conf docker/secrets/wallet-password.txt
docker compose --env-file docker/.env -f docker/compose.yml \
  --profile wallet-rpc up -d daemon wallet-rpc
```

The example keeps authentication enabled, reads credentials from protected
runtime files, reaches `daemon:8197` over the private Compose network, and
publishes wallet RPC only at `127.0.0.1:8198`. Do not expose administrative
daemon RPC, ZMQ, or wallet RPC to the public Internet. To use an existing daemon
instead, set `daemon-address` in the protected config to its reachable URL and
remove the Compose dependency only after verifying that route and its trust
boundary. The wallet-RPC login is a runtime service credential and is unrelated
to the Docker Hub token used only by release automation.

## EPoSe service node

Use the separate service-node profile so chain data and the EPoSe identity are
persistent and independently backed up:

```sh
cp docker/.env.example docker/.env
# Set QWC_IMAGE, QWC_EPOSE_REWARD_ADDRESS and QWC_EPOSE_PUBLIC_HOST in docker/.env.
docker compose --env-file docker/.env -f docker/compose.epose.yml up -d
docker compose --env-file docker/.env -f docker/compose.epose.yml logs -f service-node
```

The reward address is public; use a dedicated primary QWC mainnet address. The
public host must be a canonical public IPv4, IPv6 or lowercase DNS name. Allow
inbound TCP 8196 (P2P) and 8198 (restricted EPoSe probe RPC). Administrative RPC
remains on `127.0.0.1:8197`, and wallet RPC is not started.

The identity file is `/service-node/epose-v2-keystore` in the
`service-identity` volume. Never delete or replace it during restart, container
recreation or image update: doing so creates a new identity. Starting a process
successfully does not prove registration, qualification or rewards.

Inspect status locally:

```sh
curl -s http://127.0.0.1:8197/get_epose_info
curl -s http://127.0.0.1:8197/get_epose_service_endpoint_v2
```

## Update, rollback and backup

Pull and recreate containers without removing volumes:

```sh
# Set QWC_IMAGE in docker/.env to a reviewed version tag or image@sha256 digest.
docker compose --env-file docker/.env -f docker/compose.yml pull
docker compose --env-file docker/.env -f docker/compose.yml up -d daemon
```

An image pull never upgrades running containers automatically. The first node
start still needs to synchronize the blockchain. Before an update, stop the
services cleanly and back up the `chain`, `wallets`, and (for EPoSe)
`service-identity` volumes as applicable. Protect wallet and identity backups as
secrets. Roll back binaries only when the older release supports the on-disk
data formats; never remove volumes as an update or rollback step.

For example, after stopping the matching Compose project, back up a volume with
the same pinned Qwertycoin image (confirm the exact volume name first with
`docker volume ls`):

```sh
mkdir -p backups
docker run --rm --user 0:0 \
  -v qwertycoin_chain:/source:ro -v "$PWD/backups:/backup" \
  --entrypoint tar docker.io/qwertycoin/qwertycoin:2.0.1-rc1 \
  -C /source -czf /backup/qwertycoin-chain.tar.gz .
```

Repeat for `qwertycoin_wallets` and
`qwertycoin-epose_service-identity` when used. Store those archives with
restricted permissions and offline protection. Chain data can be resynchronized;
wallet and EPoSe identity backups cannot be reconstructed from the image.

## Provenance

Every image records the exact release tag, full Core source revision, Linux
archive SHA-256, packaging-workflow revision and pinned base-image digest in OCI
labels. Stable releases publish `<version>` and, only for the newest stable
release, `latest`. Prereleases publish only their complete prerelease version.
Version tags are immutable; later packaging-only changes use an explicit
`<version>-image.N` tag.
