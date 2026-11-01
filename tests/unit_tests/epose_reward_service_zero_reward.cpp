// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include "epose_coordinator_fixture.h"
#include "epose_reward_payee_fixture.h"

namespace
{
  constexpr uint8_t epose_hard_fork = HF_VERSION_QWC_EPOSE;
  constexpr uint8_t contracts_hard_fork = HF_VERSION_QWC_CONTRACTS;
  constexpr uint64_t subsidy_without_service_share = 9;
}

TEST(epose_reward_service_zero_reward, payout_is_due_only_for_a_positive_reward_from_hf18)
{
  using qwertycoin::epose::service_payout_due_v2;
  EXPECT_FALSE(service_payout_due_v2(epose_hard_fork, false, 0));
  EXPECT_FALSE(service_payout_due_v2(epose_hard_fork, false, 1));
  EXPECT_TRUE(service_payout_due_v2(epose_hard_fork, true, 0));
  EXPECT_TRUE(service_payout_due_v2(epose_hard_fork, true, 1));
  EXPECT_FALSE(service_payout_due_v2(contracts_hard_fork, false, 0));
  EXPECT_FALSE(service_payout_due_v2(contracts_hard_fork, false, 1));
  EXPECT_FALSE(service_payout_due_v2(contracts_hard_fork, true, 0));
  EXPECT_TRUE(service_payout_due_v2(contracts_hard_fork, true, 1));
}

TEST(epose_reward_service_zero_reward, helpers_reject_a_zero_reward)
{
  using namespace epose_reward_payee_fixture;
  payee_coinbase block(contracts_hard_fork, cryptonote::DEVNET, false);
  ASSERT_TRUE(block.built);
  crypto::key_derivation derivation{};
  ASSERT_TRUE(block.service_derivation(derivation));

  std::vector<service_payment_output_v2> outputs;
  EXPECT_EQ(reward_status_v2::invalid_context,
      collect_service_output_run_v2(
          block.coinbase, derivation, block.expectation.reward_address.m_spend_public_key,
          *block.generated.service_output_start, 0, outputs));
  EXPECT_TRUE(outputs.empty());

  block.expectation.service_reward = 0;
  service_payment_context_v2 verified{};
  EXPECT_EQ(reward_status_v2::invalid_context, block.verify(contracts_hard_fork, verified));
}

TEST(epose_reward_service_zero_reward, producer_refuses_a_zero_reward_expectation)
{
  using namespace epose_reward_payee_fixture;
  for (const uint8_t hard_fork : {epose_hard_fork, contracts_hard_fork})
  {
    service_payment_expectation_v2 expectation{};
    expectation.nettype = cryptonote::DEVNET;
    expectation.height = coinbase_height;
    expectation.payout_epoch = 2;
    expectation.reward_address = fresh_address();
    expectation.payee_service_public_key = random_public_key();
    const envelope_limits_v2 envelope_limits = limits();
    service_payment_context_v2 generated{};
    const cryptonote::miner_service_payment_v2 payment{
        &expectation, 0, block_fees, 2, &envelope_limits, nullptr, &generated};
    cryptonote::transaction coinbase{};
    EXPECT_FALSE(cryptonote::construct_miner_tx(
        coinbase_height, CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5, 0, 1,
        block_fees, fresh_address(), coinbase, {}, 1, hard_fork, &payment))
        << "hard fork " << static_cast<int>(hard_fork);
  }
}

TEST(epose_reward_service_zero_reward, selected_payee_with_zero_reward_is_void_from_hf18)
{
  using namespace epose_coordinator_fixture;
  const consensus_parameters_v2 configuration =
      parameters(empty_qualification_policy_v2::miner_fallback);
  consensus_coordinator_v2 coordinator(configuration);
  contexts source{};
  std::vector<enrollment> members;
  ASSERT_NO_FATAL_FAILURE(
      settle_single_qualified_payee(configuration, coordinator, source, members));

  const uint64_t miner_total = subsidy_without_service_share + 25;
  coordinator_result_v2 result{};
  cryptonote::transaction proofless = coinbase(1440, miner_total);
  EXPECT_EQ(coordinator_status_v2::invalid_payout_state,
      connect_with_transactions(
          coordinator, source, 1440, proofless, {}, result,
          epose_hard_fork, subsidy_without_service_share));

  envelope_record_v2 proof_record{};
  proof_record.type = static_cast<uint8_t>(record_type_v2::service_payment_proof);
  proof_record.version = 1;
  proof_record.payload.assign(EPOSE_PAYMENT_PROOF_INDEXED_PAYLOAD_BYTES_V2, '\0');
  cryptonote::transaction carrying_proof = proofless;
  envelope_budget_v2 budget{};
  ASSERT_EQ(envelope_status_v2::accepted,
      append_transaction_envelope_v2(
          {proof_record}, contracts_hard_fork,
          configuration.limits.max_envelopes_per_transaction,
          configuration.limits.envelope, carrying_proof.extra, budget));
  EXPECT_EQ(coordinator_status_v2::transition_failed,
      connect_with_transactions(
          coordinator, source, 1440, carrying_proof, {}, result,
          contracts_hard_fork, subsidy_without_service_share));

  ASSERT_EQ(coordinator_status_v2::accepted,
      connect_with_transactions(
          coordinator, source, 1440, proofless, {}, result,
          contracts_hard_fork, subsidy_without_service_share));
  EXPECT_FALSE(result.has_service_payee);
  EXPECT_EQ(0u, result.reward.service_reward);
  EXPECT_EQ(subsidy_without_service_share, result.reward.miner_subsidy);
  EXPECT_EQ(miner_total, result.reward.coinbase_total);
  EXPECT_TRUE(result.payment.outputs.empty());
}
