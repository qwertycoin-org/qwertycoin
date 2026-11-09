// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include "cryptonote_config.h"
#include "epose_reward_payee_fixture.h"
#include "hardforks/hardforks.h"

using namespace epose_reward_payee_fixture;

namespace
{
  constexpr uint8_t contracts_hard_fork = HF_VERSION_QWC_CONTRACTS;
  constexpr uint8_t last_hard_fork_without_start_index = contracts_hard_fork - 1;

  size_t proof_payload_bytes(const payee_coinbase &block)
  {
    envelope_record_v2 record{};
    return block.proof_record(record) ? record.payload.size() : 0;
  }
}

TEST(epose_reward_service_start_boundary, producer_switches_to_the_indexed_proof_at_the_fork)
{
  const payee_coinbase before(last_hard_fork_without_start_index, cryptonote::DEVNET, false);
  const payee_coinbase after(contracts_hard_fork, cryptonote::DEVNET, false);
  ASSERT_TRUE(before.built);
  ASSERT_TRUE(after.built);
  EXPECT_EQ(EPOSE_PAYMENT_PROOF_PAYLOAD_BYTES_V2, proof_payload_bytes(before));
  EXPECT_EQ(EPOSE_PAYMENT_PROOF_INDEXED_PAYLOAD_BYTES_V2, proof_payload_bytes(after));
  EXPECT_FALSE(before.generated.service_output_start);
  EXPECT_TRUE(after.generated.service_output_start);

  payee_coinbase shared_before(last_hard_fork_without_start_index, cryptonote::DEVNET, true);
  ASSERT_TRUE(shared_before.built);
  EXPECT_EQ(reward_status_v2::accepted, shared_before.verify());
  EXPECT_FALSE(shared_before.generated.service_output_start);
  EXPECT_EQ(EPOSE_PAYMENT_PROOF_PAYLOAD_BYTES_V2, proof_payload_bytes(shared_before));
  EXPECT_EQ(reward_status_v2::invalid_context, shared_before.rebuild_with_primary_miner_key());
  EXPECT_FALSE(shared_before.built);
  EXPECT_TRUE(payee_coinbase(contracts_hard_fork, cryptonote::DEVNET, true).built);
}

TEST(epose_reward_service_start_boundary, validator_switches_at_the_fork)
{
  const payee_coinbase before(last_hard_fork_without_start_index, cryptonote::DEVNET, false);
  const payee_coinbase after(contracts_hard_fork, cryptonote::DEVNET, false);
  ASSERT_TRUE(before.built);
  ASSERT_TRUE(after.built);

  service_payment_context_v2 verified{};
  EXPECT_EQ(reward_status_v2::accepted, before.verify(last_hard_fork_without_start_index, verified));
  EXPECT_EQ(reward_status_v2::invalid_payment_proof, before.verify(contracts_hard_fork, verified));
  EXPECT_EQ(reward_status_v2::invalid_payment_proof, after.verify(last_hard_fork_without_start_index, verified));
  EXPECT_EQ(reward_status_v2::accepted, after.verify(contracts_hard_fork, verified));
}

TEST(epose_reward_service_start_boundary, producer_and_validator_agree_on_both_sides)
{
  for (const uint8_t hard_fork : {last_hard_fork_without_start_index, contracts_hard_fork})
  {
    const payee_coinbase block(hard_fork, cryptonote::DEVNET, false);
    ASSERT_TRUE(block.built);
    service_payment_context_v2 verified{};
    ASSERT_EQ(reward_status_v2::accepted, block.verify(hard_fork, verified));
    EXPECT_EQ(block.generated.service_output_start, verified.service_output_start);
    EXPECT_EQ(hash_service_payment_context_v2(block.generated, crypto::public_key{}),
        hash_service_payment_context_v2(verified, crypto::public_key{}));
  }
}

TEST(epose_reward_service_start_boundary, zero_reward_rule_flips_at_the_fork)
{
  EXPECT_TRUE(service_payout_due_v2(last_hard_fork_without_start_index, true, 0));
  EXPECT_FALSE(service_payout_due_v2(contracts_hard_fork, true, 0));
}

TEST(epose_reward_service_start_boundary, devnet_activation_height_is_the_first_indexed_block)
{
  const hardfork_t &activation = devnet_hard_forks[1];
  EXPECT_EQ(contracts_hard_fork, activation.version);
  EXPECT_EQ(static_cast<uint64_t>(QWC_DEVNET_CONTRACTS_ACTIVATION_HEIGHT), activation.height);
  EXPECT_EQ(last_hard_fork_without_start_index, devnet_hard_forks[0].version);
  EXPECT_LT(devnet_hard_forks[0].height, activation.height);
}
