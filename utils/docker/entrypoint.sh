#!/bin/bash

cmd="qwertycoind --data-dir /blockchain --log-file /logs/qwertycoind.log"

if [ -n "${LOG_LEVEL}" ]; then 
	cmd="$cmd --log-level=${LOG_LEVEL}" ;
fi

if [ "${NO_CONSOLE}" = "1" ]; then 
	cmd="$cmd --no-console" ;
fi

if [ "${RESTRICTED_RPC}" != "0" ]; then 
	cmd="$cmd --restricted-rpc" ;
fi

if [ "${TESTNET}" = "1" ]; then 
	cmd="$cmd --testnet" ;
fi

if [ -n "${FIXED_DIFFICULTY}" ]; then 
	cmd="$cmd --fixed-difficulty" ;
fi

if [ -n "${ENABLE_CORS}" ]; then 
	cmd="$cmd --enable-cors=${ENABLE_CORS}" ;
else
	cmd="$cmd --enable-cors=*" ;
fi

if [ -n "${FEE_ADDRESS}" ]; then 
	cmd="$cmd --fee-address ${FEE_ADDRESS}" ;
else
	cmd="$cmd --fee-address=QWC1Dae7oKkhk2mxueQvtDJJWeowVB1i1fhfSnL4n3GnG3KgdbE9tS4iPgVns2Gj7JgrSjQt6W3oP4ZeRyE1sdAp7KxUMQCDoT" ;
fi

if [ -n "${VIEW_KEY}" ]; then 
	cmd="$cmd --view-key ${VIEW_KEY}" ;
fi

if [ "${ENABLE_BLOCKCHAIN_INDEXES}" = "1" ]; then 
	cmd="$cmd --enable-blockchain-indexes" ;
fi

if [ "${PRINT_GENESIS_TX}"  = "1" ]; then 
	cmd="$cmd --print-genesis-tx" ;
fi

if [ -n "${LOAD_CHECKPOINTS}" ]; then 
	cmd="$cmd --load-checkpoints ${LOAD_CHECKPOINTS}" ;
fi

if [ "${WITHOUT_CHECKPOINTS}" = "1" ]; then 
	cmd="$cmd --without-checkpoints" ;
fi

if [ -n "${ROLLBACK}" ]; then 
	cmd="$cmd --rollback=${ROLLBACK}" ;
fi

if [ -n "${CONTACT}" ]; then 
	cmd="$cmd --contact=${CONTACT}" ;
fi

if [ -n "${RPC_BIND_IP}" ]; then 
	cmd="$cmd --rpc-bind-ip=${RPC_BIND_IP}" ;
else
	cmd="$cmd --rpc-bind-ip=0.0.0.0" ;
fi

if [ -n "${RPC_BIND_PORT}" ]; then 
	cmd="$cmd --rpc-bind-port=${RPC_BIND_PORT}" ;
fi

if [ -n "${P2P_BIND_IP}" ]; then 
	cmd="$cmd --p2p-bind-ip=${P2P_BIND_IP}" ;
fi

if [ -n "${P2P_BIND_PORT}" ]; then 
	cmd="$cmd --p2p-bind-port=${P2P_BIND_PORT}" ;
fi

if [ -n "${P2P_EXTERNAL_PORT}" ]; then 
	cmd="$cmd --p2p-external-port=${P2P_EXTERNAL_PORT}" ;
fi

if [ "${ALLOW_LOCAL_IP}" = "1" ]; then 
	cmd="$cmd --allow-local-ip" ;
fi

if [ -n "${ADD_PEER}" ]; then 
	cmd="$cmd --add-peer=${ADD_PEER}" ;
fi

if [ -n "${ADD_PRIORITY_NODE}" ]; then 
	cmd="$cmd --add-priority-node=${ADD_PRIORITY_NODE}" ;
fi

if [ -n "${ADD_EXCLUSIVE_NODE}" ]; then 
	cmd="$cmd --add-exclusive-node=${ADD_EXCLUSIVE_NODE}" ;
fi

if [ -n "${SEED_NODE}" ]; then 
	cmd="$cmd --seed-node=${SEED_NODE}" ;
fi

if [ -n "${HIDE_MY_PORT}" ]; then 
	cmd="$cmd --hide-my-port" ;
fi

if [ -n "${EXCLUSIVE_VERSION}" ]; then 
	cmd="$cmd --exclusive-version=${EXCLUSIVE_VERSION}" ;
fi

if [ -n "${EXTRA_MESSAGES_FILE}" ]; then 
	cmd="$cmd --extra-messages-file=${EXTRA_MESSAGES_FILE}" ;
fi

if [ -n "${START_MINING}" ]; then 
	cmd="$cmd --start-mining=${START_MINING}" ;
fi

if [ -n "${MINING_THREADS}" ]; then 
	cmd="$cmd --mining-threads=${MINING_THREADS}" ;
fi

echo "uid: $(id -u)"
echo "gid: $(id -g)"
echo "cmd: ${cmd}"
#exec qwertycoind $cmd