#!/bin/sh
set -eu

case "${1:-}" in
  daemon)
    shift
    exec /opt/qwertycoin/qwertycoind "$@"
    ;;
  wallet)
    shift
    exec /opt/qwertycoin/qwertycoin-wallet-cli "$@"
    ;;
  wallet-rpc)
    shift
    exec /opt/qwertycoin/qwertycoin-wallet-rpc "$@"
    ;;
  *)
    exec /opt/qwertycoin/qwertycoind "$@"
    ;;
esac
