// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include "epose_reward_payee_fixture.h"

using namespace epose_reward_payee_fixture;

namespace
{
  constexpr uint8_t contracts_hard_fork = HF_VERSION_QWC_CONTRACTS;
}

TEST(epose_reward_miner_is_payee_v2, miner_paid_to_the_payee_address_still_yields_a_verifiable_proof)
{
  const payee_coinbase block(contracts_hard_fork, cryptonote::TESTNET, true);
  ASSERT_TRUE(block.built);

  service_payment_context_v2 verified{};
  ASSERT_EQ(reward_status_v2::accepted, block.verify(contracts_hard_fork, verified));
  EXPECT_EQ(block.expectation.service_reward, paid_amount(verified));
  EXPECT_LT(verified.outputs.size(), block.coinbase.vout.size());
  EXPECT_EQ(block.generated.coinbase_commitment, verified.coinbase_commitment);
}

TEST(epose_reward_miner_is_payee_v2, service_outputs_are_the_trailing_matched_outputs)
{
  const payee_coinbase block(contracts_hard_fork, cryptonote::TESTNET, true);
  ASSERT_TRUE(block.built);

  service_payment_context_v2 verified{};
  ASSERT_EQ(reward_status_v2::accepted, block.verify(contracts_hard_fork, verified));
  ASSERT_FALSE(verified.outputs.empty());
  EXPECT_EQ(block.coinbase.vout.size() - 1, verified.outputs.back().output_index);
}

TEST(epose_reward_miner_is_payee_v2, tampered_service_amount_is_rejected_when_miner_is_payee)
{
  payee_coinbase block(contracts_hard_fork, cryptonote::TESTNET, true);
  ASSERT_TRUE(block.built);

  ++block.expectation.service_reward;
  service_payment_context_v2 verified{};
  EXPECT_NE(reward_status_v2::accepted, block.verify(contracts_hard_fork, verified));
}

TEST(epose_reward_miner_is_payee_v2, contract_mints_after_the_service_outputs_are_never_claimed)
{
  const cryptonote::miner_contract_outputs contract_outputs = contract_outputs_with_two_mints();
  const payee_coinbase block(contracts_hard_fork, cryptonote::TESTNET, true, &contract_outputs);
  ASSERT_TRUE(block.built);

  service_payment_context_v2 verified{};
  ASSERT_EQ(reward_status_v2::accepted, block.verify(contracts_hard_fork, verified));
  EXPECT_EQ(block.expectation.service_reward, paid_amount(verified));
  const size_t first_mint_output = block.coinbase.vout.size() - contract_outputs.mints.size();
  for (const service_payment_output_v2 &output : verified.outputs)
    EXPECT_LT(output.output_index, first_mint_output);
}
