// Copyright (c) 2012-2016, The CryptoNote developers, The Bytecoin developers, The Qwertycoin developers
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
const uint64_t CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT            = DIFFICULTY_TARGET * 60;

const size_t   BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW             = 60;

// MONEY_SUPPLY - total number coins to be generated in atomic units
const uint64_t MONEY_SUPPLY                                  = (uint64_t)(-1);
const uint64_t TAIL_EMISSION_REWARD                          = 100;
const size_t CRYPTONOTE_COIN_VERSION                         = 1;
const unsigned EMISSION_SPEED_FACTOR                         = 19;
static_assert(EMISSION_SPEED_FACTOR <= 8 * sizeof(uint64_t), "Bad EMISSION_SPEED_FACTOR");

const size_t   CRYPTONOTE_REWARD_BLOCKS_WINDOW               = 100;
const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE     = 10000; //size of block (bytes) after which reward 
const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V2  = 1000000; // for block calculated using block sizes
const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V1  = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE;
const size_t   CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_CURRENT = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE;
const size_t   CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE        = 600;
const size_t   CRYPTONOTE_DISPLAY_DECIMAL_POINT              = 8;
const uint64_t MAX_TX_MIXIN_SIZE                             = 20;  // max mixin size
const uint64_t MINIMUM_FEE                                   = UINT64_C(100000000); // 1 QWC
const uint64_t DEFAULT_DUST_THRESHOLD                        = UINT64_C(100000);    // 0.001

const uint64_t EXPECTED_NUMBER_OF_BLOCKS_PER_DAY             = 24 * 60 * 60 / DIFFICULTY_TARGET;
const size_t   DIFFICULTY_WINDOW                             = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY; // 720 blocks
const size_t   DIFFICULTY_WINDOW_V2                          = DIFFICULTY_WINDOW; // blocks
const size_t   DIFFICULTY_WINDOW_V3                          = 70; // 70 blocks
const size_t   DIFFICULTY_CUT                                = 60;  // timestamps to cut after sorting
const size_t   DIFFICULTY_LAG                                = 15;  // !!!
static_assert(2 * DIFFICULTY_CUT <= DIFFICULTY_WINDOW - 2, "Bad DIFFICULTY_WINDOW or DIFFICULTY_CUT");

const size_t   MAX_BLOCK_SIZE_INITIAL                        = 20 * 1024;
const uint64_t MAX_BLOCK_SIZE_GROWTH_SPEED_NUMERATOR         = 100 * 1024;
const uint64_t MAX_BLOCK_SIZE_GROWTH_SPEED_DENOMINATOR       = 365 * 24 * 60 * 60 / DIFFICULTY_TARGET;

const uint64_t CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS     = 1;
const uint64_t CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_SECONDS    = DIFFICULTY_TARGET * CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS;

const uint64_t CRYPTONOTE_MEMPOOL_TX_LIVETIME                = 60 * 60 * 24;     //seconds, one day
const uint64_t CRYPTONOTE_MEMPOOL_TX_FROM_ALT_BLOCK_LIVETIME = 60 * 60 * 24 * 7; //seconds, one week
const uint64_t CRYPTONOTE_NUMBER_OF_PERIODS_TO_FORGET_TX_DELETED_FROM_POOL = 7;  // CRYPTONOTE_NUMBER_OF_PERIODS_TO_FORGET_TX_DELETED_FROM_POOL * CRYPTONOTE_MEMPOOL_TX_LIVETIME = time to forget tx

const size_t   FUSION_TX_MAX_SIZE                            = CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_CURRENT * 30 / 100;
const size_t   FUSION_TX_MIN_INPUT_COUNT                     = 12;
const size_t   FUSION_TX_MIN_IN_OUT_COUNT_RATIO              = 4;

const uint32_t UPGRADE_HEIGHT_V2                             = 40000;
const uint32_t UPGRADE_HEIGHT_V3                             = 46000;
const unsigned UPGRADE_VOTING_THRESHOLD                      = 90; // percent
const uint32_t   UPGRADE_VOTING_WINDOW                       = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY;  // blocks
const uint32_t   UPGRADE_WINDOW                              = EXPECTED_NUMBER_OF_BLOCKS_PER_DAY;  // blocks
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

const char     CRYPTONOTE_NAME[]                             = "qwertycoin";
const char     GENESIS_COINBASE_TX_HEX[]                     = "013c01ff0001ffffffffffff07029b2e4c0281c0b02e7c53291a94d1d0cbff8883f8024f5142ee494ffbbd0880712101eddf1e272c1ffa70f49ca4eaad918578bc3b59689e53e48a1bc670fbdea08478";

const uint8_t  CURRENT_TRANSACTION_VERSION                   =  1;
const uint8_t  BLOCK_MAJOR_VERSION_1                         =  1;
const uint8_t  BLOCK_MAJOR_VERSION_2                         =  2;
const uint8_t  BLOCK_MAJOR_VERSION_3                         =  3;
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
  "node-00.qwertycoin.org:8196", //00
  "195.201.25.118:8196",  //01
  "node-02.qwertycoin.org:8196", //02
  "78.47.85.215:8196", //03
  "46.38.242.66:8196", //op
  "78.47.87.215:8196", //04
  "195.201.27.148:8196", //05
  "94.16.122.128:8196"//nc
};

struct CheckpointData {
  uint32_t height;
  const char* blockId;
};

const std::initializer_list<CheckpointData> CHECKPOINTS = { 
  { 8473,"e131c96f6105604ea3f7dd2b55d73d430b28bed133112c9992a38c26b5239e14"},
  {10000,"fb021fd69f78a60a365c16692777e7f699215404366545e072eba7dddbf1d61d"},
  {20000,"d32286163e2a5cfbbab35007438e7bf90564afee70c715930606710e96f2ce19"},
  {30000,"1d32bbca2149eeb27ff9e2c19d1b6ace4a160764839a4da7031328f7ea49e6f6"},
  {35000,"173269a8b2ed188ef95a876acaf807cbbc4817191f036063ecf92112f46b5cbf"},
  {40001,"17132deac10a3661f56eb4b19dbaea77cbf2ef8a131f78df04c7df023b687158"},
  {45000,"0e504e82e6fe247b0b5dae9fc5bd3e82982e61610fd674d8e1f8ee3c85000c15"},
  {45758,"18fb884f14d819f79021b6112ab8252e75543b15550dcae644f8ed276723766a"},
  {46001,"15853b6a46d3fcf1554935dadc8988a0de95b0a9e711cfb845d6fe3fde5aa98a"},
  {46240,"0ff6bc1bfe6de184bc0afba48a6d41d482268b4434405554e333b88f809b5483"},
  {46561,"96d6238ea56650dc4da27d144bc11a17cc2d75942eec027da25686fa846d6034"},
  {46864,"56d957a3683e91b497a74eca662c053b2af2d0e93fa751935ed4d45c0e2b4616"},
  {47155,"18ac09e380f80588fc0119688bb26c767500558c2871854528680d3914e66af2"},
  {50500,"16c29b2587ab3ee74ab0f7b7b537ed12f380d9a03bebc89c7a7fa440945004ea"},
  {50620,"503fa20afba97a012c13c22e101bc7391ff27ab57ee57c58f68782ab3cf6e5a7"},
  {50740,"51104fc0326c17b10071c6c06adde9fce8e7bb3c360f8206512de84a77c711a3"},
  {50800,"ebe3067a8903fe944f8f6109827f97337da3582c2fcdf7c415f13a9184304304"},
  {51070,"fdaf9b5c0743b6ad853b3926e2170aca0b06c20293714191a30390bd797a80af"},
  {51266,"2cdf71f9a4c8d0ef27f8cb31f69e46a5fd015a6123e2c9ad00e5ceba53f0c297"},
  {52900,"38b29e4db5308db1dbe0ded77b9081e8f1e9ba0f51936015e9dc66cdf861eefe"},
  {55000,"b90e836c7ad95a6184869f16406161ac541c4ee3f2436447668a7f43362da733"},
  {55733,"fec26ed484443f16b1883859dd7844aea36e3ca748ec350300232f66fa793261"}
};

} // CryptoNote

#define ALLOW_DEBUG_COMMANDS



