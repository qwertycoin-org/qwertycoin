// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <limits>

#include "epose_reward_payee_fixture.h"

using namespace epose_reward_payee_fixture;

namespace
{
  constexpr uint8_t contracts_hard_fork = HF_VERSION_QWC_CONTRACTS;

  struct indexed_payment
  {
    payee_coinbase block{contracts_hard_fork, cryptonote::DEVNET, false};
    crypto::key_derivation derivation{};
    uint32_t start = 0;

    bool ready()
    {
      if (!block.built || !block.generated.service_output_start || !block.service_derivation(derivation))
        return false;
      start = *block.generated.service_output_start;
      return start > 0;
    }

    reward_status_v2 run(
        uint32_t service_output_start,
        uint64_t service_reward,
        const crypto::public_key &spend_public_key) const
    {
      std::vector<service_payment_output_v2> outputs;
      return collect_service_output_run_v2(
          block.coinbase, derivation, spend_public_key, service_output_start, service_reward, outputs);
    }

    reward_status_v2 run(uint32_t service_output_start) const
    {
      return run(service_output_start, block.expectation.service_reward, spend());
    }

    reward_status_v2 run_with_reward(uint64_t service_reward) const
    {
      return run(start, service_reward, spend());
    }

    const crypto::public_key &spend() const
    {
      return block.expectation.reward_address.m_spend_public_key;
    }

    void append_output(const crypto::public_key &output_public_key, uint64_t amount)
    {
      cryptonote::tx_out output = block.coinbase.vout.back();
      const bool tagged = output.target.type() == typeid(cryptonote::txout_to_tagged_key);
      cryptonote::set_tx_out(amount, output_public_key, tagged, crypto::view_tag{}, output);
      block.coinbase.vout.push_back(output);
      block.coinbase.invalidate_hashes();
    }

    bool append_service_output(uint64_t amount)
    {
      crypto::public_key output_public_key{};
      if (!crypto::derive_public_key(derivation, block.coinbase.vout.size(), spend(), output_public_key))
        return false;
      append_output(output_public_key, amount);
      return true;
    }
  };
}

TEST(epose_reward_service_start_rejections, wrong_start_index)
{
  indexed_payment payment;
  ASSERT_TRUE(payment.ready());
  ASSERT_EQ(reward_status_v2::accepted, payment.run(payment.start));
  EXPECT_EQ(reward_status_v2::invalid_output_allocation, payment.run(payment.start - 1));
  EXPECT_EQ(reward_status_v2::invalid_output_allocation, payment.run(payment.start + 1));
  EXPECT_EQ(reward_status_v2::invalid_output_allocation, payment.run(0));
}

TEST(epose_reward_service_start_rejections, sum_mismatch)
{
  indexed_payment payment;
  ASSERT_TRUE(payment.ready());
  const uint64_t reward = payment.block.expectation.service_reward;
  EXPECT_EQ(reward_status_v2::invalid_output_allocation, payment.run_with_reward(reward - 1));
  EXPECT_EQ(reward_status_v2::invalid_output_allocation, payment.run_with_reward(reward + 1));

  ++payment.block.coinbase.vout[payment.start].amount;
  EXPECT_EQ(reward_status_v2::invalid_output_allocation, payment.run(payment.start));
}

TEST(epose_reward_service_start_rejections, wrong_address)
{
  indexed_payment payment;
  ASSERT_TRUE(payment.ready());
  EXPECT_EQ(reward_status_v2::invalid_output_allocation,
      payment.run(payment.start, payment.block.expectation.service_reward, random_public_key()));
  EXPECT_EQ(reward_status_v2::invalid_output_allocation,
      payment.run(payment.start, payment.block.expectation.service_reward, fresh_address().m_spend_public_key));
}

TEST(epose_reward_service_start_rejections, start_index_out_of_range)
{
  indexed_payment payment;
  ASSERT_TRUE(payment.ready());
  const uint32_t end = static_cast<uint32_t>(coinbase_reward_output_end_v2(payment.block.coinbase));
  EXPECT_EQ(reward_status_v2::invalid_output_allocation, payment.run(end));
  EXPECT_EQ(reward_status_v2::invalid_output_allocation, payment.run(end + 1));
  EXPECT_EQ(reward_status_v2::invalid_output_allocation,
      payment.run(std::numeric_limits<uint32_t>::max()));
}

TEST(epose_reward_service_start_rejections, empty_coinbase)
{
  indexed_payment payment;
  ASSERT_TRUE(payment.ready());
  payment.block.coinbase.vout.clear();
  EXPECT_EQ(0u, coinbase_reward_output_end_v2(payment.block.coinbase));
  EXPECT_EQ(reward_status_v2::invalid_output_allocation, payment.run(0));
  EXPECT_EQ(reward_status_v2::invalid_output_allocation, payment.run(payment.start));
}

TEST(epose_reward_service_start_rejections, non_service_output_after_the_run)
{
  indexed_payment payment;
  ASSERT_TRUE(payment.ready());
  payment.append_output(random_public_key(), 1);
  EXPECT_EQ(reward_status_v2::invalid_output_allocation, payment.run(payment.start));
  EXPECT_EQ(reward_status_v2::invalid_output_allocation,
      payment.run_with_reward(payment.block.expectation.service_reward + 1));
  EXPECT_NE(reward_status_v2::accepted, payment.block.verify());
}

TEST(epose_reward_service_start_rejections, run_sum_overflow)
{
  indexed_payment payment;
  ASSERT_TRUE(payment.ready());
  ASSERT_TRUE(payment.append_service_output(std::numeric_limits<uint64_t>::max()));
  EXPECT_EQ(reward_status_v2::arithmetic_overflow, payment.run(payment.start));
}

TEST(epose_reward_service_start_rejections, zero_amount_output_in_the_run)
{
  indexed_payment payment;
  ASSERT_TRUE(payment.ready());
  ASSERT_TRUE(payment.append_service_output(0));
  EXPECT_EQ(reward_status_v2::invalid_output_allocation, payment.run(payment.start));
}

TEST(epose_reward_service_start_rejections, rewritten_start_index_is_rejected_by_the_validator)
{
  indexed_payment reference;
  ASSERT_TRUE(reference.ready());
  ASSERT_EQ(reward_status_v2::accepted, reference.block.verify());
  const uint32_t end = static_cast<uint32_t>(reference.block.coinbase.vout.size());

  for (const uint32_t start : {reference.start - 1, reference.start + 1, end,
           std::numeric_limits<uint32_t>::max()})
  {
    payee_coinbase block = reference.block;
    ASSERT_TRUE(block.rewrite_service_output_start(start));
    EXPECT_EQ(reward_status_v2::invalid_output_allocation, block.verify()) << "start " << start;
  }

  payee_coinbase unindexed = reference.block;
  ASSERT_TRUE(unindexed.rewrite_service_output_start(boost::none));
  EXPECT_EQ(reward_status_v2::invalid_payment_proof, unindexed.verify());
}
