// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_core/contract_state.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "device/device.hpp"
#include "epose/envelope_v2.h"
#include "epose/reward_v2.h"

namespace
{
  using namespace qwertycoin::epose;

  constexpr uint8_t payment_hard_fork = HF_VERSION_QWC_CONTRACTS;
  constexpr uint64_t coinbase_height = 1440;
  constexpr uint64_t block_fees = 25;
  constexpr uint64_t minted_per_destination = 7000;

  crypto::hash hash_text(const char *text)
  {
    return crypto::cn_fast_hash(text, std::strlen(text));
  }

  crypto::public_key random_public_key()
  {
    return cryptonote::keypair::generate(hw::get_device("default")).pub;
  }

  cryptonote::account_public_address random_address()
  {
    cryptonote::account_public_address address{};
    address.m_spend_public_key = random_public_key();
    address.m_view_public_key = random_public_key();
    return address;
  }

  envelope_limits_v2 test_limits()
  {
    envelope_limits_v2 limits{};
    limits.max_envelope_bytes = 4096;
    limits.max_records = 8;
    limits.max_record_payload_bytes = 2048;
    limits.max_signature_verifications = 16;
    limits.max_admission_verifications = 4;
    limits.supported_record_versions = {0, 1, 1, 1, 1, 1};
    return limits;
  }

  cryptonote::contract_mint random_mint(uint8_t view_tag)
  {
    cryptonote::contract_mint mint;
    mint.amount = minted_per_destination;
    mint.destination.key = random_public_key();
    mint.destination.view_tag.data = view_tag;
    mint.destination.tx_key = random_public_key();
    return mint;
  }

  struct paid_coinbase_with_mints
  {
    envelope_limits_v2 limits = test_limits();
    service_payment_expectation_v2 expectation{};
    cryptonote::miner_contract_outputs contract_outputs;
    service_payment_context_v2 generated{};
    cryptonote::transaction coinbase{};
    size_t first_mint_output = 0;

    paid_coinbase_with_mints()
    {
      uint64_t subsidy = 0;
      EXPECT_TRUE(cryptonote::get_block_reward(
          CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5, 1, 0, subsidy, payment_hard_fork));

      expectation.nettype = cryptonote::TESTNET;
      expectation.genesis_hash = hash_text("contract-mints-genesis");
      expectation.parameter_set_hash = hash_text("contract-mints-parameters");
      expectation.height = coinbase_height;
      expectation.parent_hash = hash_text("contract-mints-parent");
      expectation.payout_epoch = 2;
      expectation.qualification_hash = hash_text("contract-mints-qualification");
      expectation.payee_service_public_key = random_public_key();
      expectation.reward_address = random_address();
      expectation.service_reward = subsidy / 10;

      contract_outputs.state_root = hash_text("contract-mints-state-root");
      contract_outputs.mints = {random_mint(0x11), random_mint(0x22)};

      const cryptonote::miner_service_payment_v2 payment{
          &expectation, 0, subsidy + block_fees, 2, &limits, nullptr, &generated};
      EXPECT_TRUE(cryptonote::construct_miner_tx(
          coinbase_height, CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5, 0, 1, block_fees,
          random_address(), coinbase, {}, 1, payment_hard_fork, &payment, &contract_outputs));
      first_mint_output = coinbase.vout.size() - contract_outputs.mints.size();
    }

    reward_status_v2 verify(const cryptonote::transaction &candidate, service_payment_context_v2 &context) const
    {
      return verify_coinbase_service_payment_v2(candidate, payment_hard_fork, 2, limits, expectation, context);
    }
  };
}

TEST(epose_reward_contract_mints, payment_proof_verifies_with_mints_in_the_coinbase)
{
  const paid_coinbase_with_mints block;
  ASSERT_GT(block.first_mint_output, 0u);

  service_payment_context_v2 verified{};
  ASSERT_EQ(reward_status_v2::accepted, block.verify(block.coinbase, verified));
  EXPECT_EQ(block.generated.coinbase_commitment, verified.coinbase_commitment);
  EXPECT_EQ(block.expectation.service_reward, [&] {
    uint64_t paid = 0;
    for (const service_payment_output_v2 &output : verified.outputs)
      paid += output.amount;
    return paid;
  }());
}

TEST(epose_reward_contract_mints, proof_claims_no_mint_output)
{
  const paid_coinbase_with_mints block;
  service_payment_context_v2 verified{};
  ASSERT_EQ(reward_status_v2::accepted, block.verify(block.coinbase, verified));
  for (const service_payment_output_v2 &output : verified.outputs)
    EXPECT_LT(output.output_index, block.first_mint_output);
}

TEST(epose_reward_contract_mints, coinbase_keeps_state_root_and_mint_layout)
{
  const paid_coinbase_with_mints block;
  crypto::hash root;
  ASSERT_EQ(1, cryptonote::get_tx_vm_state_root(block.coinbase.extra, root));
  EXPECT_EQ(block.contract_outputs.state_root, root);
  EXPECT_TRUE(cryptonote::check_contract_mints_in_miner_tx(block.coinbase, block.contract_outputs.mints));
  EXPECT_EQ(block.coinbase.vout.size(), cryptonote::get_additional_tx_pub_keys_from_extra(block.coinbase).size());
}

TEST(epose_reward_contract_mints, tampered_mint_key_breaks_the_commitment)
{
  const paid_coinbase_with_mints block;
  cryptonote::transaction tampered = block.coinbase;
  cryptonote::tx_out &mint_out = tampered.vout[block.first_mint_output];
  cryptonote::set_tx_out(mint_out.amount, random_public_key(), true, *cryptonote::get_output_view_tag(mint_out), mint_out);
  tampered.invalidate_hashes();

  service_payment_context_v2 verified{};
  EXPECT_EQ(reward_status_v2::invalid_payment_proof, block.verify(tampered, verified));
}

TEST(epose_reward_contract_mints, removed_mint_breaks_the_commitment)
{
  const paid_coinbase_with_mints block;
  cryptonote::transaction tampered = block.coinbase;
  tampered.vout.pop_back();
  tampered.invalidate_hashes();

  service_payment_context_v2 verified{};
  EXPECT_EQ(reward_status_v2::invalid_payment_proof, block.verify(tampered, verified));
}

TEST(epose_reward_contract_mints, tampered_mint_tx_key_is_rejected)
{
  const paid_coinbase_with_mints block;
  cryptonote::transaction tampered = block.coinbase;
  std::vector<crypto::public_key> keys = cryptonote::get_additional_tx_pub_keys_from_extra(tampered);
  ASSERT_EQ(tampered.vout.size(), keys.size());
  keys.back() = random_public_key();
  ASSERT_TRUE(cryptonote::remove_field_from_tx_extra(tampered.extra, typeid(cryptonote::tx_extra_additional_pub_keys)));
  ASSERT_TRUE(cryptonote::add_additional_tx_pub_keys_to_extra(tampered.extra, keys));
  tampered.invalidate_hashes();

  service_payment_context_v2 verified{};
  EXPECT_EQ(reward_status_v2::invalid_output_allocation, block.verify(tampered, verified));
}

TEST(epose_reward_contract_mints, indexed_proof_budget_defers_a_carrier_at_the_extra_limit)
{
  const paid_coinbase_with_mints block;
  auto limits = block.limits;
  const size_t extra_limit = get_coinbase_extra_size_limit(payment_hard_fork);
  limits.max_envelope_bytes = 2 * extra_limit;
  limits.max_record_payload_bytes = extra_limit;
  std::vector<envelope_record_v2> carriers{{
      static_cast<uint8_t>(record_type_v2::descriptor_lifecycle), 1,
      std::string(extra_limit - block.coinbase.extra.size(), 'x')}};
  for (;;)
  {
    auto extra = block.coinbase.extra;
    envelope_budget_v2 budget{};
    ASSERT_EQ(envelope_status_v2::accepted, append_transaction_envelope_v2(
        carriers, payment_hard_fork, 2, limits, extra, budget));
    ASSERT_GE(extra.size(), extra_limit + 1);
    if (extra.size() == extra_limit + 1)
      break;
    carriers.front().payload.resize(carriers.front().payload.size() - (extra.size() - extra_limit - 1));
  }
  uint64_t subsidy = 0;
  ASSERT_TRUE(cryptonote::get_block_reward(
      CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5, 1, 0, subsidy, payment_hard_fork));
  const cryptonote::miner_service_payment_v2 payment{
      &block.expectation, 0, subsidy + block_fees, 2, &limits, &carriers};
  cryptonote::transaction coinbase{};
  ASSERT_TRUE(cryptonote::construct_miner_tx(coinbase_height,
      CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5, 0, 1, block_fees,
      random_address(), coinbase, {}, 1, payment_hard_fork, &payment, &block.contract_outputs));
  EXPECT_LE(coinbase.extra.size(), extra_limit);
  EXPECT_TRUE(cryptonote::check_contract_mints_in_miner_tx(coinbase, block.contract_outputs.mints));
  service_payment_context_v2 verified{};
  EXPECT_EQ(reward_status_v2::accepted, block.verify(coinbase, verified));
  std::vector<envelope_record_v2> selected;
  envelope_budget_v2 budget{};
  ASSERT_EQ(envelope_status_v2::accepted, parse_transaction_extra_v2(
      coinbase.extra, payment_hard_fork, 2, limits, selected, budget));
  ASSERT_EQ(1u, selected.size());
  EXPECT_EQ(static_cast<uint8_t>(record_type_v2::service_payment_proof), selected.front().type);
}
