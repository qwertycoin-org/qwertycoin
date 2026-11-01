// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <cstring>

#include "gtest/gtest.h"

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_config.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "epose/compiled_profile_v2.h"
#include "epose/coordinator_v2.h"
#include "epose/relay_pool_v2.h"
#include "hardforks/hardforks.h"
#include "string_tools.h"

namespace
{
  crypto::hash devnet_genesis_hash()
  {
    cryptonote::block genesis;
    EXPECT_TRUE(cryptonote::generate_genesis_block(
        genesis, cryptonote::get_config(cryptonote::DEVNET).GENESIS_TX,
        cryptonote::get_config(cryptonote::DEVNET).GENESIS_NONCE,
        HF_VERSION_QWC_EPOSE, HF_VERSION_QWC_EPOSE));
    return cryptonote::get_block_hash(genesis);
  }
}

TEST(devnet_profile, hardfork_schedule_reaches_contracts_at_low_height)
{
  ASSERT_EQ(2u, num_devnet_hard_forks);
  EXPECT_EQ(HF_VERSION_QWC_EPOSE, devnet_hard_forks[0].version);
  EXPECT_EQ(0u, devnet_hard_forks[0].height);
  EXPECT_EQ(HF_VERSION_QWC_CONTRACTS, devnet_hard_forks[1].version);
  EXPECT_EQ(QWC_DEVNET_CONTRACTS_ACTIVATION_HEIGHT, devnet_hard_forks[1].height);
  EXPECT_LT(devnet_hard_forks[1].height, 1000u);
}

TEST(devnet_profile, mainnet_schedule_has_no_contracts_row)
{
  for (size_t n = 0; n < num_mainnet_hard_forks; ++n)
    EXPECT_LT(mainnet_hard_forks[n].version, HF_VERSION_QWC_CONTRACTS);
}

TEST(devnet_profile, network_identity_is_distinct)
{
  const cryptonote::config_t &devnet = cryptonote::get_config(cryptonote::DEVNET);
  for (const cryptonote::network_type other : {cryptonote::MAINNET, cryptonote::TESTNET, cryptonote::STAGENET})
  {
    const cryptonote::config_t &config = cryptonote::get_config(other);
    EXPECT_NE(devnet.CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX, config.CRYPTONOTE_PUBLIC_ADDRESS_BASE58_PREFIX);
    EXPECT_NE(devnet.P2P_DEFAULT_PORT, config.P2P_DEFAULT_PORT);
    EXPECT_NE(devnet.RPC_DEFAULT_PORT, config.RPC_DEFAULT_PORT);
    EXPECT_NE(0, std::memcmp(&devnet.NETWORK_ID, &config.NETWORK_ID, sizeof(config.NETWORK_ID)));
  }
}

TEST(devnet_profile, compiled_genesis_hash_matches_generated_genesis)
{
  crypto::hash expected{};
  ASSERT_TRUE(epee::string_tools::hex_to_pod(qwertycoin::epose::DEVNET_GENESIS_HASH_V2, expected));
  EXPECT_EQ(expected, devnet_genesis_hash());
}

TEST(devnet_profile, consensus_parameters_are_valid_and_shortened)
{
  qwertycoin::epose::consensus_parameters_v2 devnet;
  ASSERT_TRUE(qwertycoin::epose::compiled_consensus_parameters_v2(
      cryptonote::DEVNET, devnet_genesis_hash(), devnet));
  EXPECT_TRUE(devnet.valid());
  EXPECT_EQ(cryptonote::DEVNET, devnet.nettype);

  crypto::hash mainnet_genesis{};
  ASSERT_TRUE(epee::string_tools::hex_to_pod(qwertycoin::epose::MAINNET_FINAL_GENESIS_HASH_V2, mainnet_genesis));
  qwertycoin::epose::consensus_parameters_v2 mainnet;
  ASSERT_TRUE(qwertycoin::epose::compiled_consensus_parameters_v2(
      cryptonote::MAINNET, mainnet_genesis, mainnet));
  EXPECT_LT(devnet.timing.epoch_length, mainnet.timing.epoch_length);
  EXPECT_LT(devnet.admission.leading_zero_bits, mainnet.admission.leading_zero_bits);
  EXPECT_NE(devnet.parameter_set_hash, mainnet.parameter_set_hash);
}

TEST(devnet_profile, consensus_parameters_reject_foreign_genesis)
{
  qwertycoin::epose::consensus_parameters_v2 parameters;
  EXPECT_FALSE(qwertycoin::epose::compiled_consensus_parameters_v2(
      cryptonote::DEVNET, crypto::hash{}, parameters));
  EXPECT_FALSE(qwertycoin::epose::compiled_consensus_parameters_v2(
      cryptonote::TESTNET, devnet_genesis_hash(), parameters));
  EXPECT_FALSE(qwertycoin::epose::compiled_consensus_parameters_v2(
      cryptonote::STAGENET, devnet_genesis_hash(), parameters));
}

TEST(devnet_profile, relay_policy_is_compiled_for_devnet_only)
{
  qwertycoin::epose::consensus_parameters_v2 parameters;
  ASSERT_TRUE(qwertycoin::epose::compiled_consensus_parameters_v2(
      cryptonote::DEVNET, devnet_genesis_hash(), parameters));
  qwertycoin::epose::relay_policy_v2 policy;
  EXPECT_TRUE(qwertycoin::epose::compiled_relay_policy_v2(
      cryptonote::DEVNET, devnet_genesis_hash(), parameters.parameter_set_hash, policy));
  EXPECT_TRUE(policy.valid());
  EXPECT_FALSE(qwertycoin::epose::compiled_relay_policy_v2(
      cryptonote::TESTNET, devnet_genesis_hash(), parameters.parameter_set_hash, policy));
}
