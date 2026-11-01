// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <vector>

#include "epose_reward_payee_fixture.h"

using namespace epose_reward_payee_fixture;

namespace
{
  constexpr uint8_t last_hard_fork_without_start_index = HF_VERSION_QWC_CONTRACTS - 1;

  const std::vector<cryptonote::network_type> &all_networks()
  {
    static const std::vector<cryptonote::network_type> networks{
        cryptonote::MAINNET, cryptonote::TESTNET, cryptonote::STAGENET, cryptonote::DEVNET};
    return networks;
  }
}

TEST(epose_reward_service_start_pre_hf18, miner_is_payee_cannot_produce_a_proof_on_any_network)
{
  for (const cryptonote::network_type nettype : all_networks())
  {
    payee_coinbase block(last_hard_fork_without_start_index, nettype, true);
    ASSERT_TRUE(block.built) << "nettype " << static_cast<int>(nettype);
    service_payment_context_v2 verified{};
    ASSERT_EQ(reward_status_v2::accepted, block.verify(last_hard_fork_without_start_index, verified));
    EXPECT_EQ(block.expectation.service_reward, paid_amount(verified));
    EXPECT_FALSE(verified.service_output_start);
    EXPECT_FALSE(block.generated.service_output_start);
    envelope_record_v2 record{};
    ASSERT_TRUE(block.proof_record(record));
    EXPECT_EQ(EPOSE_PAYMENT_PROOF_PAYLOAD_BYTES_V2, record.payload.size());
    EXPECT_EQ(reward_status_v2::invalid_context, block.rebuild_with_primary_miner_key());
    EXPECT_FALSE(block.built) << "nettype " << static_cast<int>(nettype);
  }
}

TEST(epose_reward_service_start_pre_hf18, distinct_payee_pays_with_an_unindexed_proof_on_any_network)
{
  for (const cryptonote::network_type nettype : all_networks())
  {
    const payee_coinbase block(last_hard_fork_without_start_index, nettype, false);
    ASSERT_TRUE(block.built) << "nettype " << static_cast<int>(nettype);

    service_payment_context_v2 verified{};
    ASSERT_EQ(reward_status_v2::accepted, block.verify(last_hard_fork_without_start_index, verified));
    EXPECT_EQ(block.expectation.service_reward, paid_amount(verified));
    EXPECT_FALSE(verified.service_output_start);
    EXPECT_FALSE(block.generated.service_output_start);

    envelope_record_v2 record{};
    ASSERT_TRUE(block.proof_record(record));
    EXPECT_EQ(EPOSE_PAYMENT_PROOF_PAYLOAD_BYTES_V2, record.payload.size());
  }
}

TEST(epose_reward_service_start_pre_hf18, indexed_coinbase_is_rejected_by_the_old_validator_on_any_network)
{
  for (const cryptonote::network_type nettype : all_networks())
  {
    const payee_coinbase block(HF_VERSION_QWC_CONTRACTS, nettype, true);
    ASSERT_TRUE(block.built) << "nettype " << static_cast<int>(nettype);

    service_payment_context_v2 verified{};
    EXPECT_EQ(reward_status_v2::invalid_payment_proof,
        block.verify(last_hard_fork_without_start_index, verified))
        << "nettype " << static_cast<int>(nettype);
    EXPECT_TRUE(verified.outputs.empty());
  }
}

TEST(epose_reward_service_start_pre_hf18, index_added_to_an_old_proof_is_rejected_on_any_network)
{
  for (const cryptonote::network_type nettype : all_networks())
  {
    payee_coinbase block(last_hard_fork_without_start_index, nettype, false);
    ASSERT_TRUE(block.built) << "nettype " << static_cast<int>(nettype);
    ASSERT_FALSE(block.generated.outputs.empty());
    ASSERT_TRUE(block.rewrite_service_output_start(block.generated.outputs.front().output_index));

    EXPECT_EQ(reward_status_v2::invalid_payment_proof, block.verify())
        << "nettype " << static_cast<int>(nettype);
  }
}
