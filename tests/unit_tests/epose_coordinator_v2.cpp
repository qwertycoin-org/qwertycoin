// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include "epose_coordinator_fixture.h"

using namespace epose_coordinator_fixture;


TEST(epose_coordinator_v2, incomplete_or_wrong_version_configuration_fails_closed)
{
  consensus_parameters_v2 compiled{};
  for (const cryptonote::network_type nettype : {
           cryptonote::MAINNET, cryptonote::TESTNET, cryptonote::STAGENET})
  {
    EXPECT_FALSE(compiled_consensus_parameters_v2(
        nettype, hash_text("candidate-genesis"), compiled));
    EXPECT_FALSE(compiled.valid());
  }

  consensus_parameters_v2 invalid = parameters(empty_qualification_policy_v2::miner_fallback);
  invalid.committee.committee_size = 0;
  consensus_coordinator_v2 disabled(invalid);
  EXPECT_FALSE(disabled.valid());
  invalid = parameters(empty_qualification_policy_v2::miner_fallback);
  invalid.empty_policy = static_cast<empty_qualification_policy_v2>(255);
  consensus_coordinator_v2 unknown_policy(invalid);
  EXPECT_FALSE(unknown_policy.valid());

  const consensus_parameters_v2 valid = parameters(empty_qualification_policy_v2::miner_fallback);
  consensus_coordinator_v2 coordinator(valid);
  ASSERT_TRUE(coordinator.valid());
  contexts source{};
  coordinator_result_v2 result{};
  EXPECT_EQ(coordinator_status_v2::invalid_block,
      connect_empty(coordinator, source, 0, 1025, result, 16));
  EXPECT_EQ(crypto::null_hash, result.state_hash);
}

TEST(epose_coordinator_v2, future_hardfork_continues_same_coordinator_state)
{
  consensus_coordinator_v2 coordinator(
      parameters(empty_qualification_policy_v2::miner_fallback));
  contexts source{};
  coordinator_result_v2 result{};
  ASSERT_EQ(coordinator_status_v2::accepted,
      connect_empty(coordinator, source, 0, 1025, result,
          HF_VERSION_QWC_EPOSE));
  const crypto::hash at_hf17 = result.state_hash;
  ASSERT_EQ(coordinator_status_v2::accepted,
      connect_empty(coordinator, source, 1, 1025, result,
          HF_VERSION_QWC_EPOSE + 1));
  EXPECT_NE(at_hf17, result.state_hash);
  EXPECT_EQ(QWC_EPOSE_ROLLING_STATE_SCHEMA, result.state_commitment_schema);
  EXPECT_EQ(coordinator.state().window_parameter_hash(), result.parameter_set_hash);
  const crypto::hash at_hf18 = result.state_hash;
  ASSERT_EQ(block_transition_status_v2::accepted, coordinator.disconnect_tip(1));
  EXPECT_EQ(at_hf17, coordinator.state().state_hash());
  ASSERT_EQ(coordinator_status_v2::accepted,
      connect_empty(coordinator, source, 1, 1025, result,
          HF_VERSION_QWC_EPOSE + 1));
  EXPECT_EQ(at_hf18, result.state_hash);
}

TEST(epose_coordinator_v2, contract_mints_extend_coinbase_total)
{
  const consensus_parameters_v2 configuration = parameters(empty_qualification_policy_v2::miner_fallback);
  contexts source{};
  coordinator_result_v2 result{};
  consensus_coordinator_v2 unminted(configuration);
  EXPECT_EQ(coordinator_status_v2::invalid_coinbase_amount,
      connect_empty(unminted, source, 0, 1125, result, HF_VERSION_QWC_CONTRACTS));
  consensus_coordinator_v2 unpaid(configuration);
  EXPECT_EQ(coordinator_status_v2::invalid_coinbase_amount,
      connect_empty(unpaid, source, 0, 1025, result, HF_VERSION_QWC_CONTRACTS, 100));
  consensus_coordinator_v2 minted(configuration);
  EXPECT_EQ(coordinator_status_v2::accepted,
      connect_empty(minted, source, 0, 1125, result, HF_VERSION_QWC_CONTRACTS, 100));
}

TEST(epose_coordinator_v2, compiled_mainnet_final_profile_matches_manifest)
{
  crypto::hash genesis{};
  ASSERT_TRUE(epee::string_tools::hex_to_pod(
      "4f95857586e2c66063c277370eda99cd75897d773af09f0c3cd1e22f7e87db39",
      genesis));
  consensus_parameters_v2 compiled{};
  ASSERT_TRUE(compiled_consensus_parameters_v2(
      cryptonote::MAINNET, genesis, compiled));
  EXPECT_TRUE(compiled.valid());
  EXPECT_EQ(17, HF_VERSION_QWC_EPOSE);
  EXPECT_EQ(0u, compiled.timing.activation_height);
  EXPECT_EQ(720u, compiled.timing.epoch_length);
  EXPECT_EQ(60u, compiled.timing.anchor_depth);
  EXPECT_EQ(18u, compiled.admission.leading_zero_bits);
  EXPECT_EQ(9u, compiled.committee.committee_size);
  EXPECT_EQ(6u, compiled.committee.threshold);
  EXPECT_EQ(100u, compiled.committee.max_active_population);
  EXPECT_EQ((std::vector<uint64_t>{0, 200, 400}),
      compiled.committee.round_offsets);

  crypto::hash wrong = genesis;
  reinterpret_cast<unsigned char *>(&wrong)[0] ^= 1;
  EXPECT_FALSE(compiled_consensus_parameters_v2(
      cryptonote::MAINNET, wrong, compiled));
  EXPECT_FALSE(compiled.valid());
  EXPECT_FALSE(compiled_consensus_parameters_v2(
      cryptonote::TESTNET, genesis, compiled));
}

TEST(epose_coordinator_v2, configured_mainnet_genesis_identity_is_final)
{
  cryptonote::block genesis{};
  ASSERT_TRUE(cryptonote::generate_genesis_block(
      genesis, config::GENESIS_TX, config::GENESIS_NONCE,
      HF_VERSION_QWC_EPOSE, HF_VERSION_QWC_EPOSE));
  EXPECT_EQ(20000u, config::GENESIS_NONCE);
  EXPECT_EQ("515743324d41494e3230323652303102",
      epee::string_tools::pod_to_hex(config::NETWORK_ID));
  EXPECT_EQ("4f95857586e2c66063c277370eda99cd75897d773af09f0c3cd1e22f7e87db39",
      epee::string_tools::pod_to_hex(cryptonote::get_block_hash(genesis)));
}

TEST(epose_coordinator_v2, genesis_bootstrap_and_empty_miner_fallback_are_deterministic)
{
  consensus_coordinator_v2 coordinator(
      parameters(empty_qualification_policy_v2::miner_fallback));
  contexts source{};
  connect_through(coordinator, source, 1439);

  coordinator_result_v2 result{};
  ASSERT_EQ(coordinator_status_v2::accepted,
      connect_empty(coordinator, source, 1440, 1025, result));
  EXPECT_FALSE(result.has_service_payee);
  EXPECT_EQ(1000u, result.reward.miner_subsidy);
  EXPECT_EQ(25u, result.reward.miner_fees);
  EXPECT_EQ(0u, result.reward.service_reward);
  EXPECT_EQ(1025u, result.reward.coinbase_total);
  EXPECT_EQ(1u, result.state_commitment_schema);
  EXPECT_NE(crypto::null_hash, result.state_hash);
}

TEST(epose_coordinator_v2, empty_nonissuance_changes_only_post_bootstrap_allocation)
{
  consensus_coordinator_v2 coordinator(
      parameters(empty_qualification_policy_v2::permanent_nonissuance));
  contexts source{};
  connect_through(coordinator, source, 1439);

  coordinator_result_v2 result{};
  EXPECT_EQ(coordinator_status_v2::invalid_coinbase_amount,
      connect_empty(coordinator, source, 1440, 1025, result));
  EXPECT_EQ(crypto::null_hash, result.state_hash);

  ASSERT_EQ(coordinator_status_v2::accepted,
      connect_empty(coordinator, source, 1440, 925, result));
  EXPECT_EQ(900u, result.reward.miner_subsidy);
  EXPECT_EQ(100u, result.reward.permanently_unissued);
  EXPECT_EQ(1000u, result.reward.emission_advance);
  EXPECT_EQ(900u, result.reward.issued_subsidy);
  EXPECT_EQ(925u, result.reward.coinbase_total);
}

TEST(epose_coordinator_v2, settled_qualification_controls_actual_coinbase_payment)
{
  const consensus_parameters_v2 configuration =
      parameters(empty_qualification_policy_v2::miner_fallback);
  consensus_coordinator_v2 coordinator(configuration);
  contexts source{};
  coordinator_result_v2 result{};
  std::vector<enrollment> members;
  ASSERT_NO_FATAL_FAILURE(
      settle_single_qualified_payee(configuration, coordinator, source, members));
  const enrollment &subject = members.front();

  const qualification_set_v2 *qualification =
      coordinator.state().membership().qualification(1);
  ASSERT_NE(nullptr, qualification);
  ASSERT_EQ(1u, qualification->qualified_nodes.size());
  EXPECT_EQ(subject.service.public_key, qualification->qualified_nodes.front());

  crypto::public_key selected{};
  ASSERT_EQ(reward_status_v2::accepted,
      select_service_payee_v2(
          *qualification, source.hash_for(1380), configuration.genesis_hash,
          configuration.parameter_set_hash, configuration.timing,
          2, 1440, selected));
  ASSERT_EQ(subject.service.public_key, selected);

  crypto::public_key transaction_public{};
  crypto::secret_key transaction_secret{};
  crypto::generate_keys(transaction_public, transaction_secret);
  cryptonote::transaction miner = coinbase(1440, 925);
  ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(miner, transaction_public));
  crypto::key_derivation derivation{};
  ASSERT_TRUE(crypto::generate_key_derivation(
      subject.descriptor.reward_address.m_view_public_key,
      transaction_secret, derivation));
  cryptonote::txout_to_key service_target{};
  ASSERT_TRUE(crypto::derive_public_key(
      derivation, miner.vout.size(),
      subject.descriptor.reward_address.m_spend_public_key,
      service_target.key));
  cryptonote::tx_out service_output{100, service_target};
  miner.vout.push_back(service_output);

  const service_payment_expectation_v2 expected{
      configuration.nettype,
      configuration.genesis_hash,
      configuration.parameter_set_hash,
      1440,
      source.hash_for(1439),
      2,
      qualification->qualification_hash,
      selected,
      subject.descriptor.reward_address,
      100};
  service_payment_context_v2 generated{};
  ASSERT_EQ(reward_status_v2::accepted,
      append_coinbase_service_payment_proof_v2(
          miner, transaction_secret, HF_VERSION_QWC_EPOSE,
          configuration.limits.max_envelopes_per_transaction,
          configuration.limits.envelope, expected, 0, generated));

  cryptonote::transaction redirected = miner;
  ++redirected.vout.back().amount;
  EXPECT_EQ(coordinator_status_v2::invalid_coinbase_amount,
      connect_with_transactions(
          coordinator, source, 1440, redirected, {}, result));
  EXPECT_EQ(crypto::null_hash, result.state_hash);

  ASSERT_EQ(coordinator_status_v2::accepted,
      connect_with_transactions(
          coordinator, source, 1440, miner, {}, result));
  EXPECT_TRUE(result.has_service_payee);
  EXPECT_EQ(100u, result.reward.service_reward);
  EXPECT_EQ(selected, result.payment.payee_service_public_key);
  EXPECT_EQ(1u, result.payment.outputs.size());
  EXPECT_EQ(100u, result.payment.outputs.front().amount);
  EXPECT_EQ(1u, result.transition.charged.signature_verifications);
  EXPECT_EQ(1u, result.transition.semantic.verifications.signatures);
  EXPECT_EQ(0u, result.transition.semantic.verifications.randomx);
}
