// Copyright (c) 2014-2022, The Monero Project
// Copyright (c) 2026 The Qwertycoin Project
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#pragma once

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <string>
#include <boost/uuid/uuid.hpp>

#define CRYPTONOTE_DNS_TIMEOUT_MS                       20000

#define CRYPTONOTE_MAX_BLOCK_NUMBER                     500000000
#define CRYPTONOTE_MAX_TX_SIZE                          1000000
#define CRYPTONOTE_MAX_TX_PER_BLOCK                     0x10000000
#define CRYPTONOTE_PUBLIC_ADDRESS_TEXTBLOB_VER          0
#define CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW            60
#define CURRENT_TRANSACTION_VERSION                     2
#define CURRENT_BLOCK_MAJOR_VERSION                     1
#define CURRENT_BLOCK_MINOR_VERSION                     0
#define CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT              60*60*2
#define CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE             10

#define BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW               60

// MONEY_SUPPLY - total number coins to be generated
#define MONEY_SUPPLY                                    UINT64_C(18446744073709551)
#define EMISSION_SPEED_FACTOR_PER_MINUTE                (20)
#define FINAL_SUBSIDY_PER_MINUTE                        UINT64_C(30000000) // 0.3 QWC

#define CRYPTONOTE_REWARD_BLOCKS_WINDOW                 100
#define CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2    60000 //size of block (bytes) after which reward for block calculated using block size
#define CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1    20000 //size of block (bytes) after which reward for block calculated using block size - before first fork
#define CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5    300000 //size of block (bytes) after which reward for block calculated using block size - second change, from v5
#define CRYPTONOTE_LONG_TERM_BLOCK_WEIGHT_WINDOW_SIZE   100000 // size in blocks of the long term block weight median window
#define CRYPTONOTE_SHORT_TERM_BLOCK_WEIGHT_SURGE_FACTOR 50
#define CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE          600
#define CRYPTONOTE_DISPLAY_DECIMAL_POINT                8
// COIN - number of smallest units in one coin
#define COIN                                            UINT64_C(100000000) // pow(10, 8)

#define FEE_PER_KB_OLD                                  UINT64_C(1000000) // 0.01 QWC
#define FEE_PER_KB                                      UINT64_C(200000) // 0.002 QWC
#define FEE_PER_BYTE                                    UINT64_C(30)
#define DYNAMIC_FEE_PER_KB_BASE_FEE                     UINT64_C(200000) // 0.002 QWC
#define DYNAMIC_FEE_PER_KB_BASE_BLOCK_REWARD            UINT64_C(1000000000) // 10 QWC
#define DYNAMIC_FEE_PER_KB_BASE_FEE_V5                  (UINT64_C(200000) * (uint64_t)CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2 / CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5)
#define DYNAMIC_FEE_REFERENCE_TRANSACTION_WEIGHT         ((uint64_t)3000)

#define ORPHANED_BLOCKS_MAX_COUNT                       100


#define DIFFICULTY_TARGET_V2                            120  // seconds
#define DIFFICULTY_TARGET_V1                            60  // seconds - before first fork
#define DIFFICULTY_WINDOW                               720 // blocks
#define DIFFICULTY_LAG                                  15  // !!!
#define DIFFICULTY_CUT                                  60  // timestamps to cut after sorting
#define DIFFICULTY_BLOCKS_COUNT                         DIFFICULTY_WINDOW + DIFFICULTY_LAG


#define CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS_V1   DIFFICULTY_TARGET_V1 * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS
#define CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS_V2   DIFFICULTY_TARGET_V2 * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS
#define CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS       1


#define DIFFICULTY_BLOCKS_ESTIMATE_TIMESPAN             DIFFICULTY_TARGET_V1 //just alias; used by tests


#define BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT          10000  //by default, blocks ids count in synchronizing
#define BLOCKS_IDS_SYNCHRONIZING_MAX_COUNT              25000  //max blocks ids count in synchronizing
#define BLOCKS_SYNCHRONIZING_DEFAULT_COUNT_PRE_V4       100    //by default, blocks count in blocks downloading
#define BLOCKS_SYNCHRONIZING_DEFAULT_COUNT              20     //by default, blocks count in blocks downloading
#define BLOCKS_SYNCHRONIZING_MAX_COUNT                  2048   //must be a power of 2, greater than 128, equal to SEEDHASH_EPOCH_BLOCKS

#define CRYPTONOTE_MEMPOOL_TX_LIVETIME                    (86400*3) //seconds, three days
#define CRYPTONOTE_MEMPOOL_TX_FROM_ALT_BLOCK_LIVETIME     604800 //seconds, one week


#define CRYPTONOTE_DANDELIONPP_STEMS              2 // number of outgoing stem connections per epoch
#define CRYPTONOTE_DANDELIONPP_FLUFF_PROBABILITY 20 // out of 100
#define CRYPTONOTE_DANDELIONPP_MIN_EPOCH         10 // minutes
#define CRYPTONOTE_DANDELIONPP_EPOCH_RANGE       30 // seconds
#define CRYPTONOTE_DANDELIONPP_FLUSH_AVERAGE      5 // seconds average for poisson distributed fluff flush
#define CRYPTONOTE_DANDELIONPP_EMBARGO_AVERAGE   39 // seconds (see tx_pool.cpp for more info)

// see src/cryptonote_protocol/levin_notify.cpp
#define CRYPTONOTE_NOISE_MIN_EPOCH                      5      // minutes
#define CRYPTONOTE_NOISE_EPOCH_RANGE                    30     // seconds
#define CRYPTONOTE_NOISE_MIN_DELAY                      10     // seconds
#define CRYPTONOTE_NOISE_DELAY_RANGE                    5      // seconds
#define CRYPTONOTE_NOISE_BYTES                          3*1024 // 3 KiB
#define CRYPTONOTE_NOISE_CHANNELS                       2      // Max outgoing connections per zone used for noise/covert sending

// Both below are in seconds. The idea is to delay forwarding from i2p/tor
// to ipv4/6, such that 2+ incoming connections _could_ have sent the tx
#define CRYPTONOTE_FORWARD_DELAY_BASE (CRYPTONOTE_NOISE_MIN_DELAY + CRYPTONOTE_NOISE_DELAY_RANGE)
#define CRYPTONOTE_FORWARD_DELAY_AVERAGE (CRYPTONOTE_FORWARD_DELAY_BASE + (CRYPTONOTE_FORWARD_DELAY_BASE / 2))

#define CRYPTONOTE_MAX_FRAGMENTS                        20 // ~20 * NOISE_BYTES max payload size for covert/noise send

#define COMMAND_RPC_GET_BLOCKS_FAST_MAX_BLOCK_COUNT     1000
#define COMMAND_RPC_GET_BLOCKS_FAST_MAX_TX_COUNT        20000
#define DEFAULT_RPC_MAX_CONNECTIONS_PER_PUBLIC_IP       3
#define DEFAULT_RPC_MAX_CONNECTIONS_PER_PRIVATE_IP      25
#define DEFAULT_RPC_MAX_CONNECTIONS                     100
#define DEFAULT_RPC_SOFT_LIMIT_SIZE                     25 * 1024 * 1024 // 25 MiB
#define MAX_RPC_CONTENT_LENGTH                          1048576 // 1 MB

#define P2P_LOCAL_WHITE_PEERLIST_LIMIT                  1000
#define P2P_LOCAL_GRAY_PEERLIST_LIMIT                   5000

#define P2P_DEFAULT_CONNECTIONS_COUNT                   12
#define P2P_DEFAULT_HANDSHAKE_INTERVAL                  60           //secondes
#define P2P_DEFAULT_PACKET_MAX_SIZE                     50000000     //50000000 bytes maximum packet size
#define P2P_DEFAULT_PEERS_IN_HANDSHAKE                  250
#define P2P_MAX_PEERS_IN_HANDSHAKE                      250
#define P2P_DEFAULT_CONNECTION_TIMEOUT                  5000       //5 seconds
#define P2P_DEFAULT_SOCKS_CONNECT_TIMEOUT               45         // seconds
#define P2P_DEFAULT_PING_CONNECTION_TIMEOUT             2000       //2 seconds
#define P2P_DEFAULT_INVOKE_TIMEOUT                      60*2*1000  //2 minutes
#define P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT            5000       //5 seconds
#define P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT       70
#define P2P_DEFAULT_ANCHOR_CONNECTIONS_COUNT            2
#define P2P_DEFAULT_SYNC_SEARCH_CONNECTIONS_COUNT       2
#define P2P_DEFAULT_LIMIT_RATE_UP                       8192       // kB/s
#define P2P_DEFAULT_LIMIT_RATE_DOWN                     32768       // kB/s

#define P2P_FAILED_ADDR_FORGET_SECONDS                  (60*60)     //1 hour
#define P2P_IP_BLOCKTIME                                (60*60*24)  //24 hour
#define P2P_IP_FAILS_BEFORE_BLOCK                       10
#define P2P_IDLE_CONNECTION_KILL_INTERVAL               (5*60) //5 minutes

#define P2P_SUPPORT_FLAG_FLUFFY_BLOCKS                  0x01
#define P2P_SUPPORT_FLAG_EPOSE_V2                       0x02
#define P2P_SUPPORT_FLAG_CHAINLOCK                      0x04
#define P2P_SUPPORT_FLAGS                               (P2P_SUPPORT_FLAG_FLUFFY_BLOCKS | P2P_SUPPORT_FLAG_EPOSE_V2 | P2P_SUPPORT_FLAG_CHAINLOCK)

#define RPC_IP_FAILS_BEFORE_BLOCK                       3

#define CRYPTONOTE_NAME                         "qwertycoin"
#define CRYPTONOTE_BLOCKCHAINDATA_FILENAME      "data.mdb"
#define CRYPTONOTE_BLOCKCHAINDATA_LOCK_FILENAME "lock.mdb"
#define P2P_NET_DATA_FILENAME                   "p2pstate.bin"
#define RPC_PAYMENTS_DATA_FILENAME              "rpcpayments.bin"
#define MINER_CONFIG_FILE_NAME                  "miner_conf.json"

#define THREAD_STACK_SIZE                       5 * 1024 * 1024

#define HF_VERSION_DYNAMIC_FEE                  4
#define HF_VERSION_MIN_MIXIN_4                  6
#define HF_VERSION_MIN_MIXIN_6                  7
#define HF_VERSION_MIN_MIXIN_10                 8
#define HF_VERSION_MIN_MIXIN_15                 15
#define HF_VERSION_ENFORCE_RCT                  6
#define HF_VERSION_PER_BYTE_FEE                 8
#define HF_VERSION_SMALLER_BP                   10
#define HF_VERSION_LONG_TERM_BLOCK_WEIGHT       10
#define HF_VERSION_MIN_2_OUTPUTS                12
#define HF_VERSION_MIN_V2_COINBASE_TX           12
#define HF_VERSION_SAME_MIXIN                   12
#define HF_VERSION_REJECT_SIGS_IN_COINBASE      12
#define HF_VERSION_ENFORCE_MIN_AGE              12
#define HF_VERSION_EFFECTIVE_SHORT_TERM_MEDIAN_IN_PENALTY 12
#define HF_VERSION_EXACT_COINBASE               13
#define HF_VERSION_CLSAG                        13
#define HF_VERSION_DETERMINISTIC_UNLOCK_TIME    13
#define HF_VERSION_BULLETPROOF_PLUS             15
#define HF_VERSION_VIEW_TAGS                    15
#define HF_VERSION_2021_SCALING                 15
// QWC v2 starts as a new chain on the current Monero consensus rule set.
// Monero's v16 rules are represented by the post-v15 checks in this codebase.
#define HF_VERSION_MONERO_CURRENT_CONSENSUS     16
#define HF_VERSION_QWC_RELAUNCH_BASE            HF_VERSION_MONERO_CURRENT_CONSENSUS
// QWC starts directly from the inherited Monero-v16 rule baseline and assigns
// block major version 17 to the hardened EPoSE-v2 launch rules. Block-version
// and EPoSE wire-format versions are separate domains.
#define HF_VERSION_QWC_EPOSE                    17
#define HF_VERSION_QWC_CONTRACTS                18
// Height-gated local relay/template policy. This is intentionally not a hard
// fork: block versions and every consensus rule remain HF17 on both sides of
// the boundary, so mixed old/new deployments continue following one chain.
constexpr uint64_t QWC_EPOSE_RELAY_HARDENING_HEIGHT = 21600;
constexpr uint64_t QWC_DEVNET_CONTRACTS_ACTIVATION_HEIGHT = 60;

// EPoSE-v2 is the launch state machine, not a one-version feature flag.  A
// later QWC hardfork continues processing the same parameter-bound state until
// an explicitly versioned migration replaces it.  Keeping this predicate in
// one place prevents a future HF from silently disabling registration,
// receipts, qualification, or rewards.
inline constexpr bool is_qwc_epose_v2_hardfork(const uint8_t version) noexcept
{
  return version >= HF_VERSION_QWC_EPOSE;
}

inline constexpr bool is_qwc_contracts_hardfork(const uint8_t version) noexcept
{
  return version >= HF_VERSION_QWC_CONTRACTS;
}

constexpr uint64_t QWC_WINDOW_POOL_LEAD_BLOCKS = 10;
constexpr uint64_t QWC_WINDOW_UNLOCK_DELAY_BLOCKS = 10;
constexpr uint64_t QWC_WALLET_DEFAULT_WINDOW_BLOCKS = 720;
constexpr uint64_t QWC_WALLET_DEFAULT_WINDOW_ROUNDING = 100;
constexpr uint64_t QWC_WINDOW_EXPIRED_TX_RETENTION_BLOCKS = 720;
constexpr size_t QWC_WINDOW_EXPIRED_TX_CACHE_MAX_BYTES = 32 * 1024 * 1024;

constexpr uint64_t QWC_CHAINLOCK_QUORUM_MAX = 50;
constexpr uint64_t QWC_CHAINLOCK_MIN_NODES = 10;
constexpr uint64_t QWC_CHAINLOCK_THRESHOLD_PERCENT = 68;
inline constexpr uint64_t qwc_chainlock_threshold(const uint64_t quorum_size) noexcept
{
  return (quorum_size * QWC_CHAINLOCK_THRESHOLD_PERCENT + 99) / 100;
}
constexpr uint64_t QWC_LOCK_MAX_AGE_BLOCKS = 720;
inline constexpr bool qwc_chainlock_age_expired(uint64_t locked_height, uint64_t current_height) noexcept
{
  return locked_height < current_height && current_height - locked_height > QWC_LOCK_MAX_AGE_BLOCKS;
}
constexpr size_t QWC_CHAINLOCK_SIGNER_BITMAP_SIZE = 7;
constexpr size_t QWC_CHAINLOCK_HEADER_SIZE = 8 + 32 + 8 + QWC_CHAINLOCK_SIGNER_BITMAP_SIZE;
constexpr size_t QWC_CHAINLOCK_SIGNATURE_SIZE = 64;
constexpr size_t QWC_CHAINLOCK_MAX_SIZE = QWC_CHAINLOCK_HEADER_SIZE + QWC_CHAINLOCK_QUORUM_MAX * QWC_CHAINLOCK_SIGNATURE_SIZE;
constexpr uint64_t QWC_CHAINLOCK_STORE_RETENTION_BLOCKS = 2160;
constexpr size_t QWC_CHAINLOCK_PENDING_MAX = 64;
constexpr size_t QWC_CHAINLOCK_P2P_MAX_PACKET_SIZE = 4096;
static_assert(QWC_CHAINLOCK_MAX_SIZE < QWC_CHAINLOCK_P2P_MAX_PACKET_SIZE, "chainlock must fit one P2P packet");
constexpr size_t QWC_EPOSE_ENDPOINTS_V2_P2P_MAX_PACKET_SIZE = 16 * 1024;
constexpr uint64_t QWC_EPOSE_RETAINED_EPOCHS = 8;
constexpr size_t QWC_EPOSE_MIN_COMPATIBLE_COMMITTEE = 5;
constexpr size_t QWC_EPOSE_CHALLENGE_RPC_BUCKET_CAPACITY = 8;
constexpr size_t QWC_EPOSE_CHALLENGE_RPC_BUCKETS_MAX = 4096;
constexpr size_t QWC_EPOSE_CHALLENGE_RPC_IPV6_48_CAPACITY = 32;
constexpr uint64_t QWC_EPOSE_CHALLENGE_RPC_IPV6_48_REQUESTS_PER_SECOND = 4;
constexpr size_t QWC_EPOSE_CHALLENGE_RPC_GLOBAL_HOSTS = 64;
static_assert(QWC_EPOSE_CHALLENGE_RPC_BUCKET_CAPACITY > 0);
static_assert(QWC_EPOSE_CHALLENGE_RPC_GLOBAL_HOSTS > 0);
static_assert(QWC_EPOSE_CHALLENGE_RPC_GLOBAL_HOSTS <=
    std::numeric_limits<size_t>::max() / QWC_EPOSE_CHALLENGE_RPC_BUCKET_CAPACITY);
constexpr size_t QWC_EPOSE_CHALLENGE_RPC_GLOBAL_CAPACITY = QWC_EPOSE_CHALLENGE_RPC_GLOBAL_HOSTS * QWC_EPOSE_CHALLENGE_RPC_BUCKET_CAPACITY;
constexpr uint64_t QWC_EPOSE_CHALLENGE_RPC_REFILL_MS = 1000;
static_assert(QWC_EPOSE_CHALLENGE_RPC_REFILL_MS % QWC_EPOSE_CHALLENGE_RPC_IPV6_48_REQUESTS_PER_SECOND == 0);
constexpr uint64_t QWC_EPOSE_CHALLENGE_RPC_IPV6_48_REFILL_MS =
    QWC_EPOSE_CHALLENGE_RPC_REFILL_MS / QWC_EPOSE_CHALLENGE_RPC_IPV6_48_REQUESTS_PER_SECOND;
constexpr uint64_t QWC_EPOSE_CHALLENGE_RPC_GLOBAL_REFILL_MS = QWC_EPOSE_CHALLENGE_RPC_REFILL_MS / QWC_EPOSE_CHALLENGE_RPC_GLOBAL_HOSTS;
constexpr size_t QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_CAPACITY = 32 * 1024 * 1024;
constexpr size_t QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_REFILL = 1024 * 1024;
constexpr uint64_t QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_REFILL_MS = 250;
constexpr size_t QWC_EPOSE_CHALLENGE_RPC_HOST_BYTE_CAPACITY = 5 * 1024 * 1024;
constexpr size_t QWC_EPOSE_CHALLENGE_RPC_HOST_BYTE_REFILL = 512 * 1024;
constexpr size_t QWC_EPOSE_CHALLENGE_RPC_IPV6_48_BYTE_CAPACITY = 20 * 1024 * 1024;
constexpr size_t QWC_EPOSE_CHALLENGE_RPC_IPV6_48_BYTE_REFILL = 2 * 1024 * 1024;
constexpr uint64_t QWC_EPOSE_CHALLENGE_RPC_BYTE_REFILL_MS = 1000;
constexpr size_t QWC_EPOSE_V2_MAX_CANONICAL_BLOCK_RESPONSE_BYTES = 2 * 1024 * 1024;
constexpr size_t QWC_EPOSE_CHALLENGE_RPC_JSON_OVERHEAD = 256;
static_assert(QWC_EPOSE_V2_MAX_CANONICAL_BLOCK_RESPONSE_BYTES <=
    (std::numeric_limits<size_t>::max() - QWC_EPOSE_CHALLENGE_RPC_JSON_OVERHEAD) / 2);
constexpr size_t QWC_EPOSE_CHALLENGE_RPC_MAX_RESPONSE_BYTES =
    QWC_EPOSE_V2_MAX_CANONICAL_BLOCK_RESPONSE_BYTES * 2 + QWC_EPOSE_CHALLENGE_RPC_JSON_OVERHEAD;
static_assert(QWC_EPOSE_CHALLENGE_RPC_MAX_RESPONSE_BYTES <= QWC_EPOSE_CHALLENGE_RPC_HOST_BYTE_CAPACITY);
static_assert(QWC_EPOSE_CHALLENGE_RPC_MAX_RESPONSE_BYTES <= QWC_EPOSE_CHALLENGE_RPC_IPV6_48_BYTE_CAPACITY);
static_assert(QWC_EPOSE_CHALLENGE_RPC_MAX_RESPONSE_BYTES <= QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_CAPACITY);
static_assert(QWC_EPOSE_CHALLENGE_RPC_IPV6_48_REQUESTS_PER_SECOND > 0);
static_assert(QWC_EPOSE_CHALLENGE_RPC_IPV6_48_REFILL_MS > 0);
static_assert(QWC_EPOSE_CHALLENGE_RPC_HOST_BYTE_REFILL > 0);
static_assert(QWC_EPOSE_CHALLENGE_RPC_HOST_BYTE_REFILL <= QWC_EPOSE_CHALLENGE_RPC_HOST_BYTE_CAPACITY);
static_assert(QWC_EPOSE_CHALLENGE_RPC_IPV6_48_BYTE_REFILL > 0);
static_assert(QWC_EPOSE_CHALLENGE_RPC_IPV6_48_BYTE_REFILL <= QWC_EPOSE_CHALLENGE_RPC_IPV6_48_BYTE_CAPACITY);
static_assert(QWC_EPOSE_CHALLENGE_RPC_BYTE_REFILL_MS > 0);
static_assert(QWC_EPOSE_CHALLENGE_RPC_REFILL_MS > 0);
static_assert(QWC_EPOSE_CHALLENGE_RPC_GLOBAL_REFILL_MS > 0);
static_assert(QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_REFILL_MS > 0);
static_assert(QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_REFILL > 0);
static_assert(QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_REFILL <=
    QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_CAPACITY);
namespace cryptonote { constexpr char EPOSE_SERVICE_CHALLENGE_V2_URI[] = "/epose_service_challenge_v2"; }
constexpr uint8_t QWC_EPOSE_ROLLING_STATE_SCHEMA = 2;

constexpr size_t QWC_CONTRACT_TX_EXTRA_MAX_SIZE = 64 * 1024;
constexpr uint8_t QWC_CONTRACT_SIGNATURE_SCHEME_VERSION = 1;
constexpr uint64_t QWC_VM_UNDO_RETENTION_BLOCKS = 2160;
constexpr uint64_t QWC_MINT_UNLOCK_BLOCKS = CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW;
constexpr uint64_t QWC_VM_TX_GAS_CAP = 10000000;
constexpr uint64_t QWC_VM_RESTRICTED_DRY_RUN_GAS_CAP = 100000;
constexpr uint64_t QWC_VM_BLOCK_GAS_LIMIT = 50000000;
constexpr bool QWC_POOL_DRY_RUN_KEPT_TXS = false; // QWC-10
constexpr size_t QWC_VM_MAX_MINTS_PER_BLOCK = 16; // QWC-10
constexpr size_t QWC_MINT_DERIVATION_INDEX = 0; // QWC-10
constexpr uint64_t QWC_WALLET_GAS_MARGIN_PERCENT = 10;
constexpr bool QWC_VM_DRY_RUN_ON_RESTRICTED_RPC = true; // QWC-10

constexpr char QWC_CONTRACTS_PARAMETER_SET_HASH[] =
    "e56242016ea9903587373837b1d4290633504c8636489c931d99b91fcf84cdaf";

#define PER_KB_FEE_QUANTIZATION_DECIMALS        8
#define CRYPTONOTE_SCALING_2021_FEE_ROUNDING_PLACES 2

#define HASH_OF_HASHES_STEP                     512

#define DEFAULT_TXPOOL_MAX_WEIGHT               648000000ull // 3 days at 300000, in bytes

#define BULLETPROOF_MAX_OUTPUTS                 16
#define BULLETPROOF_PLUS_MAX_OUTPUTS            16

#define CRYPTONOTE_PRUNING_STRIPE_SIZE          4096 // the smaller, the smoother the increase
#define CRYPTONOTE_PRUNING_LOG_STRIPES          3 // the higher, the more space saved
#define CRYPTONOTE_PRUNING_TIP_BLOCKS           5500 // the smaller, the more space saved

#define RPC_CREDITS_PER_HASH_SCALE ((float)(1<<24))

#define DNS_BLOCKLIST_LIFETIME (86400 * 8)

//The limit is enough for the mandatory transaction content with 16 outputs (547 bytes),
//a custom tag (1 byte) and up to 32 bytes of custom data for each recipient.
// (1+32) + (1+1+16*32) + (1+16*32) = 1060
#define MAX_TX_EXTRA_SIZE                       1060

constexpr size_t QWC_COINBASE_STATE_ROOT_FIELD_SIZE = 1 + 32;
constexpr size_t QWC_COINBASE_MAX_REWARD_OUTPUTS = 1 + 20;
constexpr size_t QWC_COINBASE_MAX_OUTPUTS = QWC_COINBASE_MAX_REWARD_OUTPUTS + QWC_VM_MAX_MINTS_PER_BLOCK;
static_assert(QWC_COINBASE_MAX_OUTPUTS < 128, "mint key count must fit a one-byte varint");
constexpr size_t QWC_COINBASE_MINT_KEYS_FIELD_SIZE = 1 + 1 + 32 * QWC_COINBASE_MAX_OUTPUTS;
constexpr size_t QWC_COINBASE_MINT_OUTPUT_MAX_SIZE = 10 + 1 + 32 + 1;
constexpr size_t QWC_COINBASE_CHAINLOCK_FIELD_SIZE = 1 + 2 + QWC_CHAINLOCK_MAX_SIZE;
constexpr size_t QWC_CONTRACTS_COINBASE_EXTRA_MAX_SIZE = MAX_TX_EXTRA_SIZE + QWC_COINBASE_STATE_ROOT_FIELD_SIZE + QWC_COINBASE_MINT_KEYS_FIELD_SIZE
    + QWC_COINBASE_CHAINLOCK_FIELD_SIZE;
constexpr size_t QWC_CONTRACTS_COINBASE_BLOB_RESERVED_SIZE = CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE + QWC_CONTRACTS_COINBASE_EXTRA_MAX_SIZE
    + QWC_COINBASE_MINT_OUTPUT_MAX_SIZE * QWC_VM_MAX_MINTS_PER_BLOCK;
inline constexpr size_t get_coinbase_extra_size_limit(const uint8_t version) noexcept
{
  return is_qwc_contracts_hardfork(version) ? QWC_CONTRACTS_COINBASE_EXTRA_MAX_SIZE : MAX_TX_EXTRA_SIZE;
}
inline constexpr size_t get_coinbase_blob_reserved_size(const uint8_t version) noexcept
{
  return is_qwc_contracts_hardfork(version) ? QWC_CONTRACTS_COINBASE_BLOB_RESERVED_SIZE : CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE;
}

// New constants are intended to go here
namespace config
{
  uint64_t const DEFAULT_FEE_ATOMIC_QWC_PER_KB = 500; // Just a placeholder! Change me!
  uint8_t const FEE_CALCULATION_MAX_RETRIES = 10;
  uint64_t const DEFAULT_DUST_THRESHOLD = ((uint64_t)2000000000); // 2 * pow(10, 9)
  uint64_t const BASE_REWARD_CLAMP_THRESHOLD = ((uint64_t)100000000); // pow(10, 8)

  uint64_t const CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 0x14820c;
  uint64_t const CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX = 0x14820d;
  uint64_t const CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX = 0x14820e;
  uint16_t const P2P_DEFAULT_PORT = 8196;
  uint16_t const RPC_DEFAULT_PORT = 8197;
  uint16_t const ZMQ_RPC_DEFAULT_PORT = 8199;
  boost::uuids::uuid const NETWORK_ID = { {
      0x51, 0x57, 0x43, 0x32, 0x4d, 0x41, 0x49, 0x4e, 0x32, 0x30, 0x32, 0x36, 0x52, 0x30, 0x31, 0x02
    } }; // QWC2MAIN2026R01, chain identity revision 2
  std::string const GENESIS_TX = "013c01ff0001ffffffffffff03029b2e4c0281c0b02e7c53291a94d1d0cbff8883f8024f5142ee494ffbbd08807121017767aafcde9be00dcfd098715ebcf7f410daebc582fda69d24a28e9d0bc890d1";
  uint32_t const GENESIS_NONCE = 20000;

  // Hash domain separators
  const char HASH_KEY_BULLETPROOF_EXPONENT[] = "bulletproof";
  const char HASH_KEY_BULLETPROOF_PLUS_EXPONENT[] = "bulletproof_plus";
  const char HASH_KEY_BULLETPROOF_PLUS_TRANSCRIPT[] = "bulletproof_plus_transcript";
  const char HASH_KEY_RINGDB[] = "ringdsb";
  const char HASH_KEY_SUBADDRESS[] = "SubAddr";
  const unsigned char HASH_KEY_ENCRYPTED_PAYMENT_ID = 0x8d;
  const unsigned char HASH_KEY_WALLET = 0x8c;
  const unsigned char HASH_KEY_WALLET_CACHE = 0x8d;
  const unsigned char HASH_KEY_BACKGROUND_CACHE = 0x8e;
  const unsigned char HASH_KEY_BACKGROUND_KEYS_FILE = 0x8f;
  const unsigned char HASH_KEY_RPC_PAYMENT_NONCE = 0x58;
  const unsigned char HASH_KEY_MEMORY = 'k';
  const unsigned char HASH_KEY_MULTISIG[] = {'M', 'u', 'l', 't' , 'i', 's', 'i', 'g', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  const unsigned char HASH_KEY_MULTISIG_KEY_AGGREGATION[] = "Multisig_key_agg";
  const unsigned char HASH_KEY_CLSAG_ROUND_MULTISIG[] = "CLSAG_round_ms_merge_factor";
  const unsigned char HASH_KEY_TXPROOF_V2[] = "TXPROOF_V2";
  const unsigned char HASH_KEY_CLSAG_ROUND[] = "CLSAG_round";
  const unsigned char HASH_KEY_CLSAG_AGG_0[] = "CLSAG_agg_0";
  const unsigned char HASH_KEY_CLSAG_AGG_1[] = "CLSAG_agg_1";
  const char HASH_KEY_MESSAGE_SIGNING[] = "QwertycoinMessageSignature";
  const unsigned char HASH_KEY_MM_SLOT = 'm';
  const constexpr char HASH_KEY_MULTISIG_TX_PRIVKEYS_SEED[] = "multisig_tx_privkeys_seed";
  const constexpr char HASH_KEY_MULTISIG_TX_PRIVKEYS[] = "multisig_tx_privkeys";
  const constexpr char HASH_KEY_TXHASH_AND_MIXRING[] = "txhash_and_mixring";

  // Multisig
  const uint32_t MULTISIG_MAX_SIGNERS{16};

  namespace testnet
  {
    uint64_t const CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 0x14821c;
    uint64_t const CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX = 0x14821d;
    uint64_t const CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX = 0x14821e;
    uint16_t const P2P_DEFAULT_PORT = 8196;
    uint16_t const RPC_DEFAULT_PORT = 8197;
    uint16_t const ZMQ_RPC_DEFAULT_PORT = 8199;
    boost::uuids::uuid const NETWORK_ID = { {
        0x51, 0x57, 0x43, 0x32, 0x54, 0x45, 0x53, 0x54, 0x32, 0x30, 0x32, 0x36, 0x50, 0x4f, 0x43, 0x02
      } }; // QWC2TEST2026POC
    std::string const GENESIS_TX = "013c01ff0001ffffffffffff03029b2e4c0281c0b02e7c53291a94d1d0cbff8883f8024f5142ee494ffbbd08807121017767aafcde9be00dcfd098715ebcf7f410daebc582fda69d24a28e9d0bc890d1";
    uint32_t const GENESIS_NONCE = 10001;
  }

  namespace stagenet
  {
    uint64_t const CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 0x14822c;
    uint64_t const CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX = 0x14822d;
    uint64_t const CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX = 0x14822e;
    uint16_t const P2P_DEFAULT_PORT = 8196;
    uint16_t const RPC_DEFAULT_PORT = 8197;
    uint16_t const ZMQ_RPC_DEFAULT_PORT = 8199;
    boost::uuids::uuid const NETWORK_ID = { {
        0x51, 0x57, 0x43, 0x32, 0x53, 0x54, 0x41, 0x47, 0x32, 0x30, 0x32, 0x36, 0x50, 0x4f, 0x43, 0x03
      } }; // QWC2STAG2026POC
    std::string const GENESIS_TX = "013c01ff0001ffffffffffff0302df5d56da0c7d643ddd1ce61901c7bdc5fb1738bfe39fbe69c28a3a7032729c0f2101168d0c4ca86fb55a4cf6a36d31431be1c53a3bd7411bb24e8832410289fa6f3b";
    uint32_t const GENESIS_NONCE = 10002;
  }

  namespace devnet
  {
    constexpr uint64_t DIFFICULTY_TARGET = 15;
    uint64_t const CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX = 0x14823c;
    uint64_t const CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX = 0x14823d;
    uint64_t const CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX = 0x14823e;
    uint16_t const P2P_DEFAULT_PORT = 28196;
    uint16_t const RPC_DEFAULT_PORT = 28197;
    uint16_t const ZMQ_RPC_DEFAULT_PORT = 28199;
    boost::uuids::uuid const NETWORK_ID = { {
        0x51, 0x57, 0x43, 0x32, 0x44, 0x45, 0x56, 0x4e, 0x32, 0x30, 0x32, 0x36, 0x50, 0x4f, 0x43, 0x04
      } }; // QWC2DEVN2026POC
    std::string const GENESIS_TX = "013c01ff0001ffffffffffff03029b2e4c0281c0b02e7c53291a94d1d0cbff8883f8024f5142ee494ffbbd08807121017767aafcde9be00dcfd098715ebcf7f410daebc582fda69d24a28e9d0bc890d1";
    uint32_t const GENESIS_NONCE = 30001;
  }

}

namespace cryptonote
{
  enum network_type : uint8_t
  {
    MAINNET = 0,
    TESTNET,
    STAGENET,
    FAKECHAIN,
    DEVNET,
    UNDEFINED = 255
  };
#define QWC_TRANSLATABLE_NETWORK_NAME(context, name) name

  inline const char *network_type_name(network_type nettype)
  {
    switch (nettype)
    {
      case MAINNET: return QWC_TRANSLATABLE_NETWORK_NAME("cryptonote::simple_wallet", "Mainnet");
      case TESTNET: return QWC_TRANSLATABLE_NETWORK_NAME("cryptonote::simple_wallet", "Testnet");
      case STAGENET: return QWC_TRANSLATABLE_NETWORK_NAME("cryptonote::simple_wallet", "Stagenet");
      case FAKECHAIN: return QWC_TRANSLATABLE_NETWORK_NAME("cryptonote::simple_wallet", "Fakechain");
      case DEVNET: return QWC_TRANSLATABLE_NETWORK_NAME("cryptonote::simple_wallet", "Devnet");
      case UNDEFINED:
      default: return QWC_TRANSLATABLE_NETWORK_NAME("cryptonote::simple_wallet", "Undefined");
    }
  }

#undef QWC_TRANSLATABLE_NETWORK_NAME

  inline std::string network_type_name_lowercase(network_type nettype)
  {
    std::string name = network_type_name(nettype);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });
    return name;
  }

  constexpr uint64_t block_target(network_type nettype, uint8_t version = 2)
  {
    return version < 2 ? DIFFICULTY_TARGET_V1 : nettype == DEVNET ? ::config::devnet::DIFFICULTY_TARGET : DIFFICULTY_TARGET_V2;
  }

  constexpr uint64_t divide_round_up(uint64_t numerator, uint64_t denominator)
  {
    return numerator / denominator + (numerator % denominator != 0);
  }

  constexpr uint64_t blocks_for_duration(network_type nettype, uint64_t seconds)
  {
    return divide_round_up(seconds, block_target(nettype));
  }

  constexpr uint64_t locked_tx_allowed_delta_seconds(network_type nettype, uint8_t version)
  {
    return block_target(nettype, version) * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS;
  }

  struct config_t
  {
    uint64_t const CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX;
    uint64_t const CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX;
    uint64_t const CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX;
    uint16_t const P2P_DEFAULT_PORT;
    uint16_t const RPC_DEFAULT_PORT;
    uint16_t const ZMQ_RPC_DEFAULT_PORT;
    boost::uuids::uuid const NETWORK_ID;
    std::string const GENESIS_TX;
    uint32_t const GENESIS_NONCE;
  };
  inline const config_t& get_config(network_type nettype)
  {
    static const config_t mainnet = {
      ::config::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
      ::config::CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX,
      ::config::CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX,
      ::config::P2P_DEFAULT_PORT,
      ::config::RPC_DEFAULT_PORT,
      ::config::ZMQ_RPC_DEFAULT_PORT,
      ::config::NETWORK_ID,
      ::config::GENESIS_TX,
      ::config::GENESIS_NONCE
    };
    static const config_t testnet = {
      ::config::testnet::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
      ::config::testnet::CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX,
      ::config::testnet::CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX,
      ::config::testnet::P2P_DEFAULT_PORT,
      ::config::testnet::RPC_DEFAULT_PORT,
      ::config::testnet::ZMQ_RPC_DEFAULT_PORT,
      ::config::testnet::NETWORK_ID,
      ::config::testnet::GENESIS_TX,
      ::config::testnet::GENESIS_NONCE
    };
    static const config_t stagenet = {
      ::config::stagenet::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
      ::config::stagenet::CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX,
      ::config::stagenet::CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX,
      ::config::stagenet::P2P_DEFAULT_PORT,
      ::config::stagenet::RPC_DEFAULT_PORT,
      ::config::stagenet::ZMQ_RPC_DEFAULT_PORT,
      ::config::stagenet::NETWORK_ID,
      ::config::stagenet::GENESIS_TX,
      ::config::stagenet::GENESIS_NONCE
    };
    static const config_t devnet = {
      ::config::devnet::CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX,
      ::config::devnet::CRYPTONOTE_PUBLIC_INTEGRATED_ADDRESS_BASE58_PREFIX,
      ::config::devnet::CRYPTONOTE_PUBLIC_SUBADDRESS_BASE58_PREFIX,
      ::config::devnet::P2P_DEFAULT_PORT,
      ::config::devnet::RPC_DEFAULT_PORT,
      ::config::devnet::ZMQ_RPC_DEFAULT_PORT,
      ::config::devnet::NETWORK_ID,
      ::config::devnet::GENESIS_TX,
      ::config::devnet::GENESIS_NONCE
    };
    switch (nettype)
    {
      case MAINNET: return mainnet;
      case TESTNET: return testnet;
      case STAGENET: return stagenet;
      case FAKECHAIN: return mainnet;
      case DEVNET: return devnet;
      default: throw std::runtime_error("Invalid network type");
    }
  };
}
