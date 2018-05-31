// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2018, The Monero project
// Copyright (c) 2014-2018, The Forknote developers
// Copyright (c) 2016-2018, The Karbowanec developers
// Copyright (c) 2018, The Qwertycoin developers
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

const uint64_t DIFFICULTY_TARGET                             = 120; // seconds
const uint64_t CRYPTONOTE_MAX_BLOCK_NUMBER                   = 500000000;
const size_t   CRYPTONOTE_MAX_BLOCK_BLOB_SIZE                = 500000000;
const size_t   CRYPTONOTE_MAX_TX_SIZE                        = 1000000000;
const uint64_t CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX       = 0x14820c; // addresses start with "QWC"
const size_t   CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW          = 60;
const size_t   CRYPTONOTE_TX_SPENDABLE_AGE                   = 6;

const uint64_t CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT            = DIFFICULTY_TARGET * 60;
const uint64_t CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT_V1         = DIFFICULTY_TARGET * 11; 
const size_t   BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW             = 60;
const size_t   BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW_V1          = 11; 

const uint64_t MONEY_SUPPLY                                  = (uint64_t)(-1);
const uint64_t TAIL_EMISSION_REWARD                          = 100;
const size_t CRYPTONOTE_COIN_VERSION                         = 1;
const unsigned EMISSION_SPEED_FACTOR                         = 19;
static_assert(EMISSION_SPEED_FACTOR <= 8 * sizeof(uint64_t), "Bad EMISSION_SPEED_FACTOR");

const size_t   CRYPTONOTE_REWARD_BLOCKS_WINDOW               = 100;
const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE     = 10000; //size of block (bytes) after which reward for block calculated using block size
const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2  = 1000000;
const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1  = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE;
const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_CURRENT = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE;
const size_t   CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE        = 600;
const size_t   CRYPTONOTE_DISPLAY_DECIMAL_POINT              = 8;
const uint64_t MINIMUM_FEE                                   = UINT64_C(100000000); // 1 QWC
const uint64_t DEFAULT_DUST_THRESHOLD                        = UINT64_C(100000);    // 0.001
const uint64_t MIN_TX_MIXIN_SIZE                             = 1;
const uint64_t MAX_TX_MIXIN_SIZE							               = 20;
const uint64_t MAX_TRANSACTION_SIZE_LIMIT                    = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2 / 4 - CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE;

const uint64_t EXPECTED_NUMBER_OF_BLOCKS_PER_DAY             = 24 * 60 * 60 / DIFFICULTY_TARGET;
const size_t   DIFFICULTY_WINDOW                             = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY; // blocks
const size_t   DIFFICULTY_WINDOW_V2                          = DIFFICULTY_WINDOW;  // blocks
const size_t   DIFFICULTY_WINDOW_V3                          = 70;  // blocks
const size_t   DIFFICULTY_CUT                                = 60;  // timestamps to cut after sorting
const size_t   DIFFICULTY_LAG                                = 15;  // !!!
static_assert(2 * DIFFICULTY_CUT <= DIFFICULTY_WINDOW - 2, "Bad DIFFICULTY_WINDOW or DIFFICULTY_CUT");

const size_t   MAX_BLOCK_SIZE_INITIAL                        = 20 * 1024;
const uint64_t MAX_BLOCK_SIZE_GROWTH_SPEED_NUMERATOR         = 100 * 1024;
const uint64_t MAX_BLOCK_SIZE_GROWTH_SPEED_DENOMINATOR       = 365 * 24 * 60 * 60 / DIFFICULTY_TARGET;

const uint64_t CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS     = 1;
const uint64_t CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS    = DIFFICULTY_TARGET * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS;

const uint64_t CRYPTONOTE_MEMPOOL_TX_LIVETIME                = 60 * 60 * 24 * 1;     //seconds, one day
const uint64_t CRYPTONOTE_MEMPOOL_TX_FROM_ALT_BLOCK_LIVETIME = 60 * 60 * 24 * 7; //seconds, one week
const uint64_t CRYPTONOTE_NUMBER_OF_PERIODS_TO_FORGET_TX_DELETED_FROM_POOL = 7;  // CRYPTONOTE_NUMBER_OF_PERIODS_TO_FORGET_TX_DELETED_FROM_POOL * CRYPTONOTE_MEMPOOL_TX_LIVETIME = time to forget tx

const size_t   FUSION_TX_MAX_SIZE                            = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1 * 30 / 100;
const size_t   FUSION_TX_MIN_INPUT_COUNT                     = 12;
const size_t   FUSION_TX_MIN_IN_OUT_COUNT_RATIO              = 4;

const uint32_t UPGRADE_HEIGHT_V2                             = 40000;
const uint32_t UPGRADE_HEIGHT_V3                             = 46000;
const uint32_t UPGRADE_HEIGHT_V4                             = 4294967294;

const unsigned UPGRADE_VOTING_THRESHOLD                      = 90; // percent
const uint32_t UPGRADE_VOTING_WINDOW                         = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY;  // blocks
const uint32_t UPGRADE_WINDOW                                = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY;  // blocks
static_assert(0 < UPGRADE_VOTING_THRESHOLD && UPGRADE_VOTING_THRESHOLD <= 100, "Bad UPGRADE_VOTING_THRESHOLD");
static_assert(UPGRADE_VOTING_WINDOW > 1, "Bad UPGRADE_VOTING_WINDOW");

const char     CRYPTONOTE_BLOCKS_FILENAME[]                  = "blocks.dat";
const char     CRYPTONOTE_BLOCKINDEXES_FILENAME[]            = "blockindexes.dat";
const char     CRYPTONOTE_BLOCKSCACHE_FILENAME[]             = "blockscache.dat";
const char     CRYPTONOTE_POOLDATA_FILENAME[]                = "poolstate.bin";
const char     P2P_NET_DATA_FILENAME[]                       = "p2pstate.bin";
const char     CRYPTONOTE_BLOCKCHAIN_INDICES_FILENAME[]      = "blockchainindices.dat";
const char     MINER_CONFIG_FILE_NAME[]                      = "miner_conf.json";
} // parameters

const char     CRYPTONOTE_NAME[]                             = "qwertycoin";
const char     GENESIS_COINBASE_TX_HEX[]                     = "013c01ff0001ffffffffffff07029b2e4c0281c0b02e7c53291a94d1d0cbff8883f8024f5142ee494ffbbd0880712101eddf1e272c1ffa70f49ca4eaad918578bc3b59689e53e48a1bc670fbdea08478";
const char     GENESIS_COINBASE_TX_FIX[]                     = "013c01ff0001ffffffffffff07029b2e4c0281c0b02e7c53291a94d1d0cbff8883f8024f5142ee494ffbbd0880712101eddf1e272c1ffa70f49ca4eaad918578bc3b59689e53e48a1bc670fbdea08478.2.2";

const uint8_t  CURRENT_TRANSACTION_VERSION                   =  1;
const uint8_t  BLOCK_MAJOR_VERSION_1                         =  1;
const uint8_t  BLOCK_MAJOR_VERSION_2                         =  2;
const uint8_t  BLOCK_MAJOR_VERSION_3                         =  3;
const uint8_t  BLOCK_MAJOR_VERSION_4                         =  4;
const uint8_t  BLOCK_MINOR_VERSION_0                         =  0;
const uint8_t  BLOCK_MINOR_VERSION_1                         =  1;

const size_t   BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT        =  10000;  //by default, blocks ids count in synchronizing
const size_t   BLOCKS_SYNCHRONIZING_DEFAULT_COUNT            =  128;    //by default, blocks count in blocks downloading
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

const char     P2P_STAT_TRUSTED_PUB_KEY[]                    = "37ceebc436f3004d3739499c67ccb730cc4734950f414cdb332b24c5ce764317";

const char* const SEED_NODES[] = { 
  "node-00.qwertycoin.org:8196",//00P
  "195.201.25.118:8196",        //01P
  "198.147.30.116:8196",        //EXP
  "198.147.30.115:8196",        //POO
  "node-02.qwertycoin.org:8196",//02A
  "78.47.85.215:8196",          //03A
  "78.47.87.215:8196",          //04A
};

struct CheckpointData {
  uint32_t height;
  const char* blockId;
};

const std::initializer_list<CheckpointData> CHECKPOINTS = { 
  { 5000,"f44ad7e79c4dc4b8cce6100460444f8fc61096a6cf4d9a53107abde0b8750b4f"},
  { 8473,"e131c96f6105604ea3f7dd2b55d73d430b28bed133112c9992a38c26b5239e14"},
  {10000,"fb021fd69f78a60a365c16692777e7f699215404366545e072eba7dddbf1d61d"},
  {15000,"fb8ad336e6518aa6a5f874a15276561249d36a35afd08f0f1fab78333d9e4149"},
  {20000,"d32286163e2a5cfbbab35007438e7bf90564afee70c715930606710e96f2ce19"},
  {25000,"c064cd86be01c32c86e0c7a6295023628acfeaa2786e2873a03343bc6f6a8d41"},
  {30000,"1d32bbca2149eeb27ff9e2c19d1b6ace4a160764839a4da7031328f7ea49e6f6"},
  {35000,"173269a8b2ed188ef95a876acaf807cbbc4817191f036063ecf92112f46b5cbf"},
  {40000,"60a9694b18cf470bcfd9f36f32ad01f86b5538fe1f88a3bd2717ca6ad1c7ce80"},
  {40001,"17132deac10a3661f56eb4b19dbaea77cbf2ef8a131f78df04c7df023b687158"},
  {45000,"0e504e82e6fe247b0b5dae9fc5bd3e82982e61610fd674d8e1f8ee3c85000c15"},
  {46000,"51e2f9e09278cb66b08efcecfdf3208053ee53030c6fc5a6a647565a6b318cd5"},
  {46001,"15853b6a46d3fcf1554935dadc8988a0de95b0a9e711cfb845d6fe3fde5aa98a"},
  {50000,"5599ca3d8ac58f377187b5a43c00103496244ec95299e4ae3dff3be6016488bf"},
  {55000,"b90e836c7ad95a6184869f16406161ac541c4ee3f2436447668a7f43362da733"},
  {60000,"796e0037bed22ec317dc165bdfcc331322fe3fcc7ca162827ab374a7d17107f0"},
  {65000,"d03c367bc1d87bea553346245ae60865175371b449dea11b79b7cfa28453d892"},
  {70000,"9b035668d72b17ceaabbd4b341089f090aa5ebad37f33513509d9f6e102c7c0c"},
  {75000,"dfe99ff9e3eaa55fd24797cd2639e156e5222a10a0c14f690f4007cbe6ce3477"},
  {80000,"02bdbd6cab951c0685796598b353524d450f947c58d8e1a8efbc05c334663ad7"},
  {83590,"a483f182bda2bd51f02c3954a16fcefba5aeee60ef16061fe104b1c7a70894b0"}
};

} // CryptoNote

#define ALLOW_DEBUG_COMMANDS
