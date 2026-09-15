# Qwertycoin Core

Official `linux/amd64` runtime image for Qwertycoin mainnet. One image contains
the release-matched daemon, interactive CLI wallet and wallet RPC together with
their verified runtime libraries.

```sh
docker run --rm qwertycoin/qwertycoin:2.0.1 --version
docker run --rm -it qwertycoin/qwertycoin:2.0.1 wallet --help
docker run --rm qwertycoin/qwertycoin:2.0.1 wallet-rpc --help
```

The daemon is the default. Mainnet is the default. Use immutable version tags or
digests in production. Full-node, wallet-RPC and EPoSe Compose examples,
persistence, ports, backup and update guidance are maintained in the repository:

https://github.com/qwertycoin-org/qwertycoin/tree/main/docker
