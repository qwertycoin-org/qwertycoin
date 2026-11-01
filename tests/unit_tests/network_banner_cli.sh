#!/bin/sh
set -eu

wallet_cli=$1

check_version()
{
  expected=$1
  shift
  banner=$("$wallet_cli" "$@" --version)
  case "$banner" in
    "Qwertycoin 'QWC v2 $expected' (v"*) ;;
    *) exit 1 ;;
  esac
}

check_version Mainnet
check_version Testnet --testnet
check_version Devnet --devnet

help=$("$wallet_cli" --devnet --help)
case "$help" in
  *"Qwertycoin 'QWC v2 Devnet' (v"*) ;;
  *) exit 1 ;;
esac

scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT HUP INT TERM
if startup=$("$wallet_cli" --devnet --wallet-file="$scratch/missing" --log-file="$scratch/wallet.log" 2>&1); then
  exit 1
fi
case "$startup" in
  *"Qwertycoin 'QWC v2 Devnet' (v"*) ;;
  *) exit 1 ;;
esac
