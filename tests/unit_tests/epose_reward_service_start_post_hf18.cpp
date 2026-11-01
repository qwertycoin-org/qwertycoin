// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include "epose_reward_payee_fixture.h"

using namespace epose_reward_payee_fixture;

namespace
{
  constexpr uint8_t contracts_hard_fork = HF_VERSION_QWC_CONTRACTS;

  void expect_run_to_reward_end(
      const payee_coinbase &block,
      const service_payment_context_v2 &verified)
  {
    ASSERT_TRUE(verified.service_output_start);
    ASSERT_FALSE(verified.outputs.empty());
    EXPECT_EQ(block.generated.service_output_start, verified.service_output_start);
    EXPECT_EQ(*verified.service_output_start, verified.outputs.front().output_index);
    for (size_t i = 1; i < verified.outputs.size(); ++i)
      EXPECT_EQ(verified.outputs[i - 1].output_index + 1, verified.outputs[i].output_index);
    EXPECT_EQ(coinbase_reward_output_end_v2(block.coinbase), verified.outputs.back().output_index + 1u);
    EXPECT_EQ(block.expectation.service_reward, paid_amount(verified));
  }
}

TEST(epose_reward_service_start_post_hf18, indexed_proof_verifies_for_distinct_and_miner_payees)
{
  for (const cryptonote::network_type nettype : {cryptonote::MAINNET, cryptonote::TESTNET, cryptonote::DEVNET})
    for (const bool miner_is_payee : {false, true})
    {
      const payee_coinbase block(contracts_hard_fork, nettype, miner_is_payee);
      ASSERT_TRUE(block.built) << "miner_is_payee " << miner_is_payee;

      service_payment_context_v2 verified{};
      ASSERT_EQ(reward_status_v2::accepted, block.verify(contracts_hard_fork, verified));
      expect_run_to_reward_end(block, verified);
      EXPECT_EQ(block.coinbase.vout.size(), coinbase_reward_output_end_v2(block.coinbase));
    }
}

TEST(epose_reward_service_start_post_hf18, proof_record_carries_the_start_index)
{
  const payee_coinbase block(contracts_hard_fork, cryptonote::DEVNET, true);
  ASSERT_TRUE(block.built);
  ASSERT_TRUE(block.generated.service_output_start);

  envelope_record_v2 record{};
  ASSERT_TRUE(block.proof_record(record));
  ASSERT_EQ(EPOSE_PAYMENT_PROOF_INDEXED_PAYLOAD_BYTES_V2, record.payload.size());
  scoped_payment_proof_v2 proof{};
  ASSERT_EQ(record_codec_status_v2::accepted,
      decode_payment_proof_record_structure_v2(record, proof));
  EXPECT_EQ(block.generated.service_output_start, proof.service_output_start);
}

TEST(epose_reward_service_start_post_hf18, run_ends_before_the_contract_mint_tail)
{
  const cryptonote::miner_contract_outputs contract_outputs = contract_outputs_with_two_mints();
  for (const bool miner_is_payee : {false, true})
  {
    const payee_coinbase block(contracts_hard_fork, cryptonote::DEVNET, miner_is_payee, &contract_outputs);
    ASSERT_TRUE(block.built);
    EXPECT_EQ(block.coinbase.vout.size() - contract_outputs.mints.size(),
        coinbase_reward_output_end_v2(block.coinbase));
    EXPECT_TRUE(cryptonote::check_contract_mints_in_miner_tx(block.coinbase, contract_outputs.mints));

    service_payment_context_v2 verified{};
    ASSERT_EQ(reward_status_v2::accepted, block.verify(contracts_hard_fork, verified));
    expect_run_to_reward_end(block, verified);
  }
}

TEST(epose_reward_service_start_post_hf18, validator_and_producer_share_the_run_helper)
{
  const payee_coinbase block(contracts_hard_fork, cryptonote::DEVNET, true);
  ASSERT_TRUE(block.built);
  crypto::key_derivation derivation{};
  ASSERT_TRUE(block.service_derivation(derivation));

  std::vector<service_payment_output_v2> run;
  ASSERT_EQ(reward_status_v2::accepted,
      collect_service_output_run_v2(
          block.coinbase, derivation, block.expectation.reward_address.m_spend_public_key,
          *block.generated.service_output_start, block.expectation.service_reward, run));
  ASSERT_EQ(block.generated.outputs.size(), run.size());
  for (size_t i = 0; i < run.size(); ++i)
  {
    EXPECT_EQ(block.generated.outputs[i].output_index, run[i].output_index);
    EXPECT_EQ(block.generated.outputs[i].amount, run[i].amount);
    EXPECT_EQ(block.generated.outputs[i].output_public_key, run[i].output_public_key);
  }
}
