// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include "epose/reward_v2.h"

namespace
{
  using namespace qwertycoin::epose;

  crypto::hash hash_text(const char *text)
  {
    return crypto::cn_fast_hash(text, std::strlen(text));
  }

  cryptonote::account_public_address make_address()
  {
    cryptonote::account_public_address address{};
    crypto::secret_key ignored{};
    crypto::generate_keys(address.m_spend_public_key, ignored);
    crypto::generate_keys(address.m_view_public_key, ignored);
    return address;
  }

  crypto::public_key make_public_key()
  {
    crypto::public_key key{};
    crypto::secret_key secret{};
    crypto::generate_keys(key, secret);
    return key;
  }

  service_payment_context_v2 make_payment_context(crypto::secret_key &transaction_secret)
  {
    service_payment_context_v2 context{};
    context.nettype = cryptonote::TESTNET;
    context.genesis_hash = hash_text("genesis");
    context.parameter_set_hash = hash_text("parameters");
    context.height = 2160;
    context.parent_hash = hash_text("parent");
    context.payout_epoch = 3;
    context.qualification_hash = hash_text("qualification");
    context.payee_service_public_key = make_public_key();
    context.reward_address = make_address();
    context.service_reward = 100;
    crypto::generate_keys(context.transaction_public_key, transaction_secret);
    context.coinbase_commitment = hash_text("coinbase-without-proof");

    crypto::key_derivation derivation{};
    EXPECT_TRUE(crypto::generate_key_derivation(
        context.reward_address.m_view_public_key, transaction_secret, derivation));
    for (const auto &entry : std::vector<std::pair<uint32_t, uint64_t>>{{1, 40}, {3, 60}})
    {
      service_payment_output_v2 output{};
      output.output_index = entry.first;
      output.amount = entry.second;
      EXPECT_TRUE(crypto::derive_public_key(
          derivation, output.output_index, context.reward_address.m_spend_public_key, output.output_public_key));
      context.outputs.push_back(output);
    }
    return context;
  }
}

TEST(epose_reward_v2, subsidy_only_split_never_shares_fees)
{
  reward_allocation_v2 allocation{};
  ASSERT_EQ(reward_status_v2::accepted,
      calculate_reward_allocation_v2(1000, 250, true, empty_qualification_policy_v2::unset, allocation));
  EXPECT_EQ(900u, allocation.miner_subsidy);
  EXPECT_EQ(250u, allocation.miner_fees);
  EXPECT_EQ(100u, allocation.service_reward);
  EXPECT_EQ(0u, allocation.permanently_unissued);
  EXPECT_EQ(1000u, allocation.issued_subsidy);
  EXPECT_EQ(1250u, allocation.coinbase_total);
  EXPECT_EQ(1000u, allocation.emission_advance);
}

TEST(epose_reward_v2, empty_set_policy_is_explicit_and_models_both_decisions)
{
  reward_allocation_v2 allocation{};
  EXPECT_EQ(reward_status_v2::unresolved_empty_policy,
      calculate_reward_allocation_v2(1000, 250, false, empty_qualification_policy_v2::unset, allocation));
  EXPECT_EQ(0u, allocation.scheduled_subsidy);

  ASSERT_EQ(reward_status_v2::accepted,
      calculate_reward_allocation_v2(1000, 250, false, empty_qualification_policy_v2::miner_fallback, allocation));
  EXPECT_EQ(1000u, allocation.miner_subsidy);
  EXPECT_EQ(0u, allocation.permanently_unissued);
  EXPECT_EQ(1000u, allocation.issued_subsidy);
  EXPECT_EQ(1250u, allocation.coinbase_total);

  ASSERT_EQ(reward_status_v2::accepted,
      calculate_reward_allocation_v2(1000, 250, false, empty_qualification_policy_v2::permanent_nonissuance, allocation));
  EXPECT_EQ(900u, allocation.miner_subsidy);
  EXPECT_EQ(100u, allocation.permanently_unissued);
  EXPECT_EQ(900u, allocation.issued_subsidy);
  EXPECT_EQ(1150u, allocation.coinbase_total);
  EXPECT_EQ(1000u, allocation.emission_advance);
}

TEST(epose_reward_v2, split_rounding_and_overflow_fail_deterministically)
{
  reward_allocation_v2 allocation{};
  ASSERT_EQ(reward_status_v2::accepted,
      calculate_reward_allocation_v2(9, 0, true, empty_qualification_policy_v2::unset, allocation));
  EXPECT_EQ(0u, allocation.service_reward);
  EXPECT_EQ(9u, allocation.miner_subsidy);
  EXPECT_EQ(reward_status_v2::invalid_basis_points,
      calculate_reward_allocation_v2(1, 0, true, empty_qualification_policy_v2::unset, allocation, 10001));
  EXPECT_EQ(reward_status_v2::arithmetic_overflow,
      calculate_reward_allocation_v2(std::numeric_limits<uint64_t>::max(), 1, true, empty_qualification_policy_v2::unset, allocation));
  EXPECT_EQ(0u, allocation.scheduled_subsidy);
}

TEST(epose_reward_v2, payout_rotation_is_epoch_relative_and_context_bound)
{
  qualification_set_v2 qualification{};
  qualification.epoch = 2;
  qualification.closed_height = 2099;
  qualification.snapshot_hash = hash_text("snapshot");
  qualification.qualification_hash = hash_text("closed-set");
  qualification.qualified_nodes = {make_public_key(), make_public_key(), make_public_key()};
  const crypto::hash seed = hash_text("payout-seed");
  const crypto::hash genesis = hash_text("genesis");
  const crypto::hash parameters = hash_text("parameters");
  const epoch_timing_v2 timing{0, 720, 60};
  std::vector<crypto::public_key> selected;
  for (uint64_t height = 2160; height < 2163; ++height)
  {
    crypto::public_key key{};
    ASSERT_EQ(reward_status_v2::accepted,
        select_service_payee_v2(qualification, seed, genesis, parameters, timing, 3, height, key));
    selected.push_back(key);
  }
  std::sort(selected.begin(), selected.end(), [](const crypto::public_key &left, const crypto::public_key &right) {
    return std::memcmp(&left, &right, sizeof(left)) < 0;
  });
  EXPECT_EQ(selected.end(), std::unique(selected.begin(), selected.end()));
  crypto::public_key ignored{};
  EXPECT_EQ(reward_status_v2::invalid_height,
      select_service_payee_v2(qualification, seed, genesis, parameters, timing, 3, 2159, ignored));
  EXPECT_EQ(reward_status_v2::invalid_height,
      select_service_payee_v2(qualification, seed, genesis, parameters, timing, 3, 2880, ignored));
  qualification.qualified_nodes.clear();
  EXPECT_EQ(reward_status_v2::empty_qualification,
      select_service_payee_v2(qualification, seed, genesis, parameters, timing, 3, 2160, ignored));
}

TEST(epose_reward_v2, payout_rejects_wrong_source_epoch_and_duplicate_members)
{
  qualification_set_v2 qualification{};
  qualification.epoch = 2;
  qualification.closed_height = 2099;
  qualification.snapshot_hash = hash_text("snapshot");
  qualification.qualification_hash = hash_text("closed-set");
  qualification.qualified_nodes = {make_public_key()};
  crypto::public_key selected{};
  const epoch_timing_v2 timing{0, 720, 60};
  EXPECT_EQ(reward_status_v2::invalid_context,
      select_service_payee_v2(qualification, hash_text("seed"), hash_text("genesis"),
          hash_text("parameters"), timing, 4, 2160, selected));
  qualification.qualified_nodes.push_back(qualification.qualified_nodes.front());
  EXPECT_EQ(reward_status_v2::invalid_context,
      select_service_payee_v2(qualification, hash_text("seed"), hash_text("genesis"),
          hash_text("parameters"), timing, 3, 2160, selected));
}

TEST(epose_reward_v2, payout_epoch_bounds_are_derived_from_canonical_timing)
{
  qualification_set_v2 qualification{};
  qualification.epoch = 3;
  qualification.closed_height = 2819;
  qualification.snapshot_hash = hash_text("snapshot");
  qualification.qualification_hash = hash_text("closed-set");
  qualification.qualified_nodes = {make_public_key()};
  const epoch_timing_v2 timing{0, 720, 60};
  crypto::public_key selected{};
  EXPECT_EQ(reward_status_v2::invalid_height,
      select_service_payee_v2(qualification, hash_text("seed"), hash_text("genesis"),
          hash_text("parameters"), timing, 4, 2879, selected));
  EXPECT_EQ(reward_status_v2::accepted,
      select_service_payee_v2(qualification, hash_text("seed"), hash_text("genesis"),
          hash_text("parameters"), timing, 4, 2880, selected));
  EXPECT_EQ(reward_status_v2::accepted,
      select_service_payee_v2(qualification, hash_text("seed"), hash_text("genesis"),
          hash_text("parameters"), timing, 4, 3599, selected));
  EXPECT_EQ(reward_status_v2::invalid_height,
      select_service_payee_v2(qualification, hash_text("seed"), hash_text("genesis"),
          hash_text("parameters"), timing, 4, 3600, selected));
}

TEST(epose_reward_v2, scoped_payment_proof_uses_no_private_view_key)
{
  crypto::secret_key transaction_secret{};
  const service_payment_context_v2 context = make_payment_context(transaction_secret);
  scoped_payment_proof_v2 proof{};
  ASSERT_EQ(reward_status_v2::accepted,
      generate_scoped_payment_proof_v2(context, transaction_secret, proof));
  EXPECT_EQ(reward_status_v2::accepted, verify_scoped_payment_proof_v2(context, proof));
}

TEST(epose_reward_v2, payment_proof_is_bound_to_coinbase_height_parent_and_payee)
{
  crypto::secret_key transaction_secret{};
  const service_payment_context_v2 context = make_payment_context(transaction_secret);
  scoped_payment_proof_v2 proof{};
  ASSERT_EQ(reward_status_v2::accepted,
      generate_scoped_payment_proof_v2(context, transaction_secret, proof));

  auto changed = context;
  ++changed.height;
  EXPECT_EQ(reward_status_v2::invalid_payment_proof, verify_scoped_payment_proof_v2(changed, proof));
  changed = context;
  changed.parent_hash = hash_text("other-parent");
  EXPECT_EQ(reward_status_v2::invalid_payment_proof, verify_scoped_payment_proof_v2(changed, proof));
  changed = context;
  changed.coinbase_commitment = hash_text("other-coinbase");
  EXPECT_EQ(reward_status_v2::invalid_payment_proof, verify_scoped_payment_proof_v2(changed, proof));
  changed = context;
  changed.payee_service_public_key = make_public_key();
  EXPECT_EQ(reward_status_v2::invalid_payment_proof, verify_scoped_payment_proof_v2(changed, proof));
}

TEST(epose_reward_v2, malformed_proof_and_output_allocation_fail_closed)
{
  crypto::secret_key transaction_secret{};
  const service_payment_context_v2 context = make_payment_context(transaction_secret);
  scoped_payment_proof_v2 proof{};
  ASSERT_EQ(reward_status_v2::accepted,
      generate_scoped_payment_proof_v2(context, transaction_secret, proof));

  auto changed_proof = proof;
  reinterpret_cast<unsigned char *>(&changed_proof.proof)[0] ^= 1;
  EXPECT_EQ(reward_status_v2::invalid_payment_proof, verify_scoped_payment_proof_v2(context, changed_proof));
  changed_proof = proof;
  changed_proof.derivation = make_public_key();
  EXPECT_EQ(reward_status_v2::invalid_payment_proof, verify_scoped_payment_proof_v2(context, changed_proof));

  auto changed = context;
  ++changed.outputs[0].output_index;
  EXPECT_EQ(reward_status_v2::invalid_payment_proof, verify_scoped_payment_proof_v2(changed, proof));
  changed = context;
  changed.outputs[0].output_public_key = make_public_key();
  EXPECT_EQ(reward_status_v2::invalid_payment_proof, verify_scoped_payment_proof_v2(changed, proof));
  changed = context;
  changed.outputs.push_back(changed.outputs.back());
  EXPECT_EQ(reward_status_v2::invalid_context, verify_scoped_payment_proof_v2(changed, proof));
}

TEST(epose_reward_v2, wrong_transaction_secret_and_repeat_payments_do_not_collide)
{
  crypto::secret_key first_secret{};
  const service_payment_context_v2 first = make_payment_context(first_secret);
  scoped_payment_proof_v2 first_proof{};
  crypto::public_key wrong_public{};
  crypto::secret_key wrong_secret{};
  crypto::generate_keys(wrong_public, wrong_secret);
  EXPECT_EQ(reward_status_v2::invalid_transaction_key,
      generate_scoped_payment_proof_v2(first, wrong_secret, first_proof));
  ASSERT_EQ(reward_status_v2::accepted,
      generate_scoped_payment_proof_v2(first, first_secret, first_proof));

  crypto::secret_key second_secret{};
  auto second = make_payment_context(second_secret);
  second.reward_address = first.reward_address;
  crypto::key_derivation second_derivation{};
  ASSERT_TRUE(crypto::generate_key_derivation(second.reward_address.m_view_public_key, second_secret, second_derivation));
  for (auto &output : second.outputs)
    ASSERT_TRUE(crypto::derive_public_key(second_derivation, output.output_index,
        second.reward_address.m_spend_public_key, output.output_public_key));
  scoped_payment_proof_v2 second_proof{};
  ASSERT_EQ(reward_status_v2::accepted,
      generate_scoped_payment_proof_v2(second, second_secret, second_proof));
  EXPECT_NE(first.outputs[0].output_public_key, second.outputs[0].output_public_key);
  EXPECT_NE(first_proof.derivation, second_proof.derivation);
  EXPECT_EQ(reward_status_v2::accepted, verify_scoped_payment_proof_v2(second, second_proof));
}
