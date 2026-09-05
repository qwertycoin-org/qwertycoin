#!/bin/bash

rlwrap qwertycoin-wallet-cli --wallet-file wallet_m --password "" --testnet --trusted-daemon --daemon-address localhost:8197  --log-file wallet_miner.log stop_mining

