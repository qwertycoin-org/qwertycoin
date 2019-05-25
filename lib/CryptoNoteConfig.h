// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero project
// Copyright (c) 2014-2018, The Forknote developers
// Copyright (c) 2018, Ryo Currency Project
// Copyright (c) 2016-2018, The Karbowanec developers
// Copyright (c) 2018-2019, The Qwertycoin developers
//
// This file is part of Qwertycoin.
//
// Qwertycoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Qwertycoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Qwertycoin.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <cstdint>

namespace CryptoNote {
namespace parameters {

const uint64_t DIFFICULTY_TARGET                              = 120;
const uint64_t CRYPTONOTE_MAX_BLOCK_NUMBER                    = 500000000;
const size_t   CRYPTONOTE_MAX_BLOCK_BLOB_SIZE                 = 500000000;
const size_t   CRYPTONOTE_MAX_TX_SIZE                         = 1000000000;
const uint64_t CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX        = 0x14820c;
const size_t   CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW           = 60;
const size_t   CRYPTONOTE_TX_SPENDABLE_AGE                    = 10;

const uint64_t CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT             = DIFFICULTY_TARGET * 60; // 7200
const uint64_t CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V1          = DIFFICULTY_TARGET * 6;  // 720 //5.0
const size_t   BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW              = 60;
const size_t   BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V1           = 11;                     //5.0

const uint64_t MONEY_SUPPLY                                   = (uint64_t)(-1);
const uint64_t COIN                                           = 100000000;
const uint64_t TAIL_EMISSION_REWARD                           = 100000000;
const size_t CRYPTONOTE_COIN_VERSION                          = 1;
const unsigned EMISSION_SPEED_FACTOR                          = 19;
static_assert(EMISSION_SPEED_FACTOR <= 8 * sizeof(uint64_t), "Bad EMISSION_SPEED_FACTOR");

const size_t   CRYPTONOTE_REWARD_BLOCKS_WINDOW                = 100;
const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE      = 10000;                  //1.0;3.0
const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2   = 1000000;                //2.0;4.0;5.0
const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1   = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE;
const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_CURRENT = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE;
const size_t   CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE         = 600;
const size_t   CRYPTONOTE_DISPLAY_DECIMAL_POINT               = 8;

const uint64_t MINIMUM_FEE_V1                                 = UINT64_C(100000000);
const uint64_t MINIMUM_FEE_V2                                 = UINT64_C(100000000);
const uint32_t MINIMUM_FEE_V2_HEIGHT                          = 800000;
const uint64_t MINIMUM_FEE                                    = MINIMUM_FEE_V2;
const uint64_t MAXIMUM_FEE                                    = UINT64_C(100000000);
const double   REMOTE_NODE_FEE_FACTOR                         = 0.25; // percent
const uint64_t MAX_REMOTE_NODE_FEE                            = UINT64_C(10000000000); //max fee

const uint64_t DEFAULT_DUST_THRESHOLD                         = UINT64_C(100000);
const uint64_t MIN_TX_MIXIN_SIZE                              = 2;
const uint64_t MAX_TX_MIXIN_SIZE_V1                           = 20;
const uint64_t MAX_TX_MIXIN_SIZE_V2                           = 20;
const uint64_t MAX_TX_MIXIN_SIZE                              = MAX_TX_MIXIN_SIZE_V1;
const uint32_t MIN_TX_MIXIN_V1_HEIGHT                         = 200000;
const uint32_t MIN_TX_MIXIN_V2_HEIGHT                         = 800000;
const uint64_t MAX_TRANSACTION_SIZE_LIMIT                     = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2 / 2 - CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE;

const uint64_t EXPECTED_NUMBER_OF_BLOCKS_PER_DAY              = 24 * 60 * 60 / DIFFICULTY_TARGET;
const size_t   DIFFICULTY_WINDOW                              = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY; // blocks
const size_t   DIFFICULTY_WINDOW_V2                           = DIFFICULTY_WINDOW;  // blocks
const size_t   DIFFICULTY_WINDOW_V3                           = 70;  // blocks
const size_t   DIFFICULTY_CUT                                 = 60;  // timestamps to cut after sorting
const size_t   DIFFICULTY_LAG                                 = 15;  // !!!
static_assert(2 * DIFFICULTY_CUT <= DIFFICULTY_WINDOW - 2, "Bad DIFFICULTY_WINDOW or DIFFICULTY_CUT");

static constexpr uint64_t POISSON_CHECK_TRIGGER               = 10;   // Reorg size that triggers poisson timestamp check
static constexpr uint64_t POISSON_CHECK_DEPTH                 = 60;   // Main-chain depth of the poisson check. The attacker will have to tamper 50% of those blocks
static constexpr double   POISSON_LOG_P_REJECT                = -75.0;// Reject reorg if the probablity that the timestamps are genuine is below e^x, -75 = 10^-33

const size_t   MAX_BLOCK_SIZE_INITIAL                         = 1000000;
const uint64_t MAX_BLOCK_SIZE_GROWTH_SPEED_NUMERATOR          = 100 * 1024;
const uint64_t MAX_BLOCK_SIZE_GROWTH_SPEED_DENOMINATOR        = 365 * 24 * 60 * 60 / DIFFICULTY_TARGET;

const uint64_t CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS      = 1;
const uint64_t CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS     = DIFFICULTY_TARGET * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS;

const uint64_t CRYPTONOTE_MEMPOOL_TX_LIVETIME                 = 60 * 60 * 14; // 14 hours
const uint64_t CRYPTONOTE_MEMPOOL_TX_FROM_ALT_BLOCK_LIVETIME  = 60 * 60 * 24; // 24 hours
const uint64_t CRYPTONOTE_NUMBER_OF_PERIODS_TO_FORGET_TX_DELETED_FROM_POOL = 7;

const size_t   FUSION_TX_MAX_SIZE                             = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2 * 30 / 100;
const size_t   FUSION_TX_MIN_INPUT_COUNT                      = 12;
const size_t   FUSION_TX_MIN_IN_OUT_COUNT_RATIO               = 4;

const uint32_t UPGRADE_HEIGHT_V2                              = 40000;
const uint32_t UPGRADE_HEIGHT_V3                              = 46000;
const uint32_t UPGRADE_HEIGHT_V4                              = 110520;
const uint32_t UPGRADE_HEIGHT_V5                              = 250720;
const uint32_t UPGRADE_HEIGHT_V6                              = 4294967295;

const unsigned UPGRADE_VOTING_THRESHOLD                      = 90; // percent
const uint32_t UPGRADE_VOTING_WINDOW                         = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY;  // blocks
const uint32_t UPGRADE_WINDOW                                = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY;  // blocks
static_assert(0 < UPGRADE_VOTING_THRESHOLD && UPGRADE_VOTING_THRESHOLD <= 100, "Bad UPGRADE_VOTING_THRESHOLD");
static_assert(UPGRADE_VOTING_WINDOW > 1, "Bad UPGRADE_VOTING_WINDOW");

const char     CRYPTONOTE_BLOCKS_FILENAME[]                  = "blocks.bin";
const char     CRYPTONOTE_BLOCKINDEXES_FILENAME[]            = "blockindexes.bin";
const char     CRYPTONOTE_BLOCKSCACHE_FILENAME[]             = "blockscache.bin";
const char     CRYPTONOTE_POOLDATA_FILENAME[]                = "poolstate.dat";
const char     P2P_NET_DATA_FILENAME[]                       = "p2pstate.dat";
const char     CRYPTONOTE_BLOCKCHAIN_INDICES_FILENAME[]      = "blockchainindices.bin";
const char     MINER_CONFIG_FILE_NAME[]                      = "miner_conf.json";
} // parameters

const char     CRYPTONOTE_NAME[]                             = "Qwertycoin";
const char     GENESIS_COINBASE_TX_HEX[]                     = "013c01ff0001ffffffffffff07029b2e4c0281c0b02e7c53291a94d1d0cbff8883f8024f5142ee494ffbbd0880712101eddf1e272c1ffa70f49ca4eaad918578bc3b59689e53e48a1bc670fbdea08478";
const char     GENESIS_COINBASE_TX_FIX[]                     = "013c01ff0001ffffffffffff07029b2e4c0281c0b02e7c53291a94d1d0cbff8883f8024f5142ee494ffbbd0880712101eddf1e272c1ffa70f49ca4eaad918578bc3b59689e53e48a1bc670fbdea08478.5.1.2";

const uint8_t  CURRENT_TRANSACTION_VERSION                   =  1;
const uint8_t  BLOCK_MAJOR_VERSION_1                         =  1;
const uint8_t  BLOCK_MAJOR_VERSION_2                         =  2;
const uint8_t  BLOCK_MAJOR_VERSION_3                         =  3;
const uint8_t  BLOCK_MAJOR_VERSION_4                         =  4;
const uint8_t  BLOCK_MAJOR_VERSION_5                         =  5;
const uint8_t  BLOCK_MAJOR_VERSION_6                         =  6;

const uint8_t  BLOCK_MINOR_VERSION_0                         =  0;
const uint8_t  BLOCK_MINOR_VERSION_1                         =  1;

const size_t   BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT        =  10000;
const size_t   BLOCKS_SYNCHRONIZING_DEFAULT_COUNT            =  128;
const size_t   COMMAND_RPC_GET_BLOCKS_FAST_MAX_COUNT         =  1000;

const int      P2P_DEFAULT_PORT                              =  8196;
const int      RPC_DEFAULT_PORT                              =  8197;

const size_t   P2P_LOCAL_WHITE_PEERLIST_LIMIT                =  1000;
const size_t   P2P_LOCAL_GRAY_PEERLIST_LIMIT                 =  5000;

const size_t   P2P_CONNECTION_MAX_WRITE_BUFFER_SIZE          = 64 * 1024 * 1024; // 64 MB
const uint32_t P2P_DEFAULT_CONNECTIONS_COUNT                 = 8;
const size_t   P2P_DEFAULT_WHITELIST_CONNECTIONS_PERCENT     = 70;
const uint32_t P2P_DEFAULT_HANDSHAKE_INTERVAL                = 60;            // seconds
const uint32_t P2P_DEFAULT_PACKET_MAX_SIZE                   = 50000000;      // 50000000 bytes maximum packet size
const uint32_t P2P_DEFAULT_PEERS_IN_HANDSHAKE                = 250;
const uint32_t P2P_DEFAULT_CONNECTION_TIMEOUT                = 5000;          // 5 seconds
const uint32_t P2P_DEFAULT_PING_CONNECTION_TIMEOUT           = 2000;          // 2 seconds
const uint64_t P2P_DEFAULT_INVOKE_TIMEOUT                    = 60 * 2 * 1000; // 2 minutes
const size_t   P2P_DEFAULT_HANDSHAKE_INVOKE_TIMEOUT          = 5000;          // 5 seconds
const uint32_t P2P_FAILED_ADDR_FORGET_SECONDS                = (60 * 60);     //1 hour
const uint32_t P2P_IP_BLOCKTIME                              = (60 * 60 * 24);//24 hour
const uint32_t P2P_IP_FAILS_BEFORE_BLOCK                     = 10;
const uint32_t P2P_IDLE_CONNECTION_KILL_INTERVAL             = (5 * 60);      //5 minutes

const char     P2P_STAT_TRUSTED_PUB_KEY[]                    = "deaddeadbeef04d37a9499c67ccb730dc4734950f414cdb332b28c5ce764beaf";

/*
*   Modules
*/

// P2P Messages
const bool     P2P_MESSAGES                                  =  true;
const uint16_t P2P_MESSAGES_CHAR_COUNT                       =  160;

const char* const SEED_NODES[] = {
  "node-00.qwertycoin.org:8196",
  "node-01.qwertycoin.org:8196",
  "node-02.qwertycoin.org:8196",
  "node-03.qwertycoin.org:8196",
  "node-04.qwertycoin.org:8196",
  "node-05.qwertycoin.org:8196",
  "198.147.30.115:8196",  //loop
  "198.147.30.116:8196"   //pool
};

} // CryptoNote

#define ALLOW_DEBUG_COMMANDS
