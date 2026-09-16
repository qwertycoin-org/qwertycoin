# Qwertycoin Core

Official `linux/amd64` runtime image for Qwertycoin mainnet. One image contains
the release-matched daemon, interactive CLI wallet and wallet RPC together with
their verified runtime libraries.

```sh
docker run --rm qwertycoin/qwertycoin:latest --version
docker run --rm -it qwertycoin/qwertycoin:latest wallet --help
docker run --rm qwertycoin/qwertycoin:latest wallet-rpc --help
```

The daemon is the default. Mainnet is the default. `latest` follows the newest
stable image; audited production deployments may pin its resolved digest.
Full-node, wallet-RPC and EPoSe Compose examples,
persistence, ports, backup and update guidance are maintained in the repository:

https://github.com/qwertycoin-org/qwertycoin/tree/main/docker
