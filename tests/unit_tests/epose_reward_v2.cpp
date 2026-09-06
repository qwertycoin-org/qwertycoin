// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "epose/envelope_v2.h"
#include "epose/reward_v2.h"
#include "epose/record_codec_v2.h"
#include "epose/verification_v2.h"

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

  envelope_limits_v2 envelope_limits()
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

TEST(epose_reward_v2, typed_payment_proof_codec_is_context_bound_and_atomic)
{
  crypto::secret_key transaction_secret{};
  const service_payment_context_v2 context = make_payment_context(transaction_secret);
  scoped_payment_proof_v2 expected{};
  ASSERT_EQ(reward_status_v2::accepted,
      generate_scoped_payment_proof_v2(context, transaction_secret, expected));
  envelope_record_v2 encoded{};
  ASSERT_EQ(record_codec_status_v2::accepted,
      encode_payment_proof_record_v2(expected, context, encoded));
  EXPECT_EQ(static_cast<uint8_t>(record_type_v2::service_payment_proof), encoded.type);
  EXPECT_EQ(EPOSE_PAYMENT_PROOF_RECORD_VERSION_V2, encoded.version);
  EXPECT_EQ(EPOSE_PAYMENT_PROOF_PAYLOAD_BYTES_V2, encoded.payload.size());

  scoped_payment_proof_v2 actual{};
  ASSERT_EQ(record_codec_status_v2::accepted,
      decode_payment_proof_record_v2(encoded, context, actual));
  EXPECT_EQ(expected.derivation, actual.derivation);
  EXPECT_EQ(0, std::memcmp(&expected.proof, &actual.proof, sizeof(expected.proof)));

  auto wrong_context = context;
  ++wrong_context.height;
  actual = expected;
  EXPECT_EQ(record_codec_status_v2::invalid_record,
      decode_payment_proof_record_v2(encoded, wrong_context, actual));
  EXPECT_EQ(crypto::null_pkey, actual.derivation);

  encoded.payload.pop_back();
  actual = expected;
  EXPECT_EQ(record_codec_status_v2::wrong_size,
      decode_payment_proof_record_v2(encoded, context, actual));
  EXPECT_EQ(crypto::null_pkey, actual.derivation);
}

TEST(epose_reward_v2, coinbase_commitment_omits_only_payment_proof_records)
{
  cryptonote::transaction coinbase{};
  coinbase.version = 2;
  coinbase.unlock_time = 60;
  cryptonote::txin_gen input{};
  input.height = 0;
  coinbase.vin.push_back(input);
  ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(coinbase, make_public_key()));

  envelope_record_v2 receipt{};
  receipt.type = static_cast<uint8_t>(record_type_v2::service_receipt);
  receipt.version = 1;
  receipt.payload = "receipt";
  envelope_budget_v2 budget{};
  ASSERT_EQ(envelope_status_v2::accepted,
      append_transaction_envelope_v2(
          {receipt}, HF_VERSION_QWC_EPOSE, 2,
          envelope_limits(), coinbase.extra, budget));

  crypto::hash before_proof{};
  size_t removed = 9;
  ASSERT_EQ(reward_status_v2::accepted,
      canonical_coinbase_commitment_v2(
          coinbase, HF_VERSION_QWC_EPOSE, 2,
          envelope_limits(), before_proof, removed));
  EXPECT_EQ(0u, removed);

  envelope_record_v2 proof{};
  proof.type = static_cast<uint8_t>(record_type_v2::service_payment_proof);
  proof.version = 1;
  proof.payload = "proof";
  ASSERT_EQ(envelope_status_v2::accepted,
      append_transaction_envelope_v2(
          {proof}, HF_VERSION_QWC_EPOSE, 2,
          envelope_limits(), coinbase.extra, budget));

  crypto::hash after_proof{};
  ASSERT_EQ(reward_status_v2::accepted,
      canonical_coinbase_commitment_v2(
          coinbase, HF_VERSION_QWC_EPOSE, 2,
          envelope_limits(), after_proof, removed));
  EXPECT_EQ(1u, removed);
  EXPECT_EQ(before_proof, after_proof);

  ++coinbase.unlock_time;
  crypto::hash changed{};
  ASSERT_EQ(reward_status_v2::accepted,
      canonical_coinbase_commitment_v2(
          coinbase, HF_VERSION_QWC_EPOSE, 2,
          envelope_limits(), changed, removed));
  EXPECT_NE(before_proof, changed);

  cryptonote::transaction ordinary{};
  crypto::hash stale = before_proof;
  removed = 9;
  EXPECT_EQ(reward_status_v2::invalid_context,
      canonical_coinbase_commitment_v2(
          ordinary, HF_VERSION_QWC_EPOSE, 2,
          envelope_limits(), stale, removed));
  EXPECT_EQ(crypto::null_hash, stale);
  EXPECT_EQ(0u, removed);
}

TEST(epose_reward_v2, coinbase_payment_proof_is_derived_from_actual_outputs)
{
  crypto::secret_key transaction_secret{};
  crypto::public_key transaction_public{};
  crypto::generate_keys(transaction_public, transaction_secret);
  const cryptonote::account_public_address reward_address = make_address();

  cryptonote::transaction coinbase{};
  coinbase.version = 2;
  coinbase.unlock_time = 60;
  cryptonote::txin_gen input{};
  input.height = 1440;
  coinbase.vin.push_back(input);
  ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(coinbase, transaction_public));

  cryptonote::tx_out miner_output{};
  miner_output.amount = 900;
  cryptonote::txout_to_key miner_target{};
  miner_target.key = make_public_key();
  miner_output.target = miner_target;
  coinbase.vout.push_back(miner_output);

  crypto::key_derivation derivation{};
  ASSERT_TRUE(crypto::generate_key_derivation(
      reward_address.m_view_public_key, transaction_secret, derivation));
  service_payment_context_v2 signing_context{};
  signing_context.nettype = cryptonote::TESTNET;
  signing_context.genesis_hash = hash_text("genesis");
  signing_context.parameter_set_hash = hash_text("parameters");
  signing_context.height = 1440;
  signing_context.parent_hash = hash_text("parent");
  signing_context.payout_epoch = 2;
  signing_context.qualification_hash = hash_text("qualification");
  signing_context.payee_service_public_key = make_public_key();
  signing_context.reward_address = reward_address;
  signing_context.service_reward = 100;
  signing_context.transaction_public_key = transaction_public;

  for (const uint64_t amount : {40u, 60u})
  {
    const uint32_t output_index = static_cast<uint32_t>(coinbase.vout.size());
    cryptonote::txout_to_key service_target{};
    ASSERT_TRUE(crypto::derive_public_key(
        derivation, output_index, reward_address.m_spend_public_key,
        service_target.key));
    cryptonote::tx_out service_output{};
    service_output.amount = amount;
    service_output.target = service_target;
    coinbase.vout.push_back(service_output);
    signing_context.outputs.push_back({output_index, amount, service_target.key});
  }
  const cryptonote::transaction proofless_coinbase = coinbase;

  size_t removed = 9;
  ASSERT_EQ(reward_status_v2::accepted,
      canonical_coinbase_commitment_v2(
          coinbase, HF_VERSION_QWC_EPOSE, 2,
          envelope_limits(), signing_context.coinbase_commitment, removed));
  ASSERT_EQ(0u, removed);
  scoped_payment_proof_v2 proof{};
  ASSERT_EQ(reward_status_v2::accepted,
      generate_scoped_payment_proof_v2(signing_context, transaction_secret, proof));
  envelope_record_v2 proof_record{};
  ASSERT_EQ(record_codec_status_v2::accepted,
      encode_payment_proof_record_v2(proof, signing_context, proof_record));
  envelope_budget_v2 budget{};
  ASSERT_EQ(envelope_status_v2::accepted,
      append_transaction_envelope_v2(
          {proof_record}, HF_VERSION_QWC_EPOSE, 2,
          envelope_limits(), coinbase.extra, budget));

  const service_payment_expectation_v2 expected{
      signing_context.nettype,
      signing_context.genesis_hash,
      signing_context.parameter_set_hash,
      signing_context.height,
      signing_context.parent_hash,
      signing_context.payout_epoch,
      signing_context.qualification_hash,
      signing_context.payee_service_public_key,
      signing_context.reward_address,
      signing_context.service_reward};
  service_payment_context_v2 actual{};
  ASSERT_EQ(reward_status_v2::accepted,
      verify_coinbase_service_payment_v2(
          coinbase, HF_VERSION_QWC_EPOSE, 2,
          envelope_limits(), expected, actual));
  EXPECT_EQ(signing_context.coinbase_commitment, actual.coinbase_commitment);
  EXPECT_EQ(signing_context.outputs.size(), actual.outputs.size());

  verification_counters_v2 validation_calls{};
  validated_service_payment_v2 validated{};
  ASSERT_EQ(reward_status_v2::accepted,
      validate_coinbase_service_payment_v2(
          coinbase, HF_VERSION_QWC_EPOSE, 2,
          envelope_limits(), expected, validated, &validation_calls));
  EXPECT_EQ(0u, validation_calls.randomx);
  EXPECT_EQ(1u, validation_calls.signatures);
  EXPECT_TRUE(validated.matches(proof_record));
  envelope_record_v2 changed_record = proof_record;
  changed_record.payload.back() ^= 0x01;
  EXPECT_FALSE(validated.matches(changed_record));

  cryptonote::transaction produced = proofless_coinbase;
  service_payment_context_v2 produced_context{};
  ASSERT_EQ(reward_status_v2::accepted,
      append_coinbase_service_payment_proof_v2(
          produced, transaction_secret, HF_VERSION_QWC_EPOSE, 2,
          envelope_limits(), expected, produced_context));
  EXPECT_EQ(signing_context.coinbase_commitment, produced_context.coinbase_commitment);
  ASSERT_EQ(reward_status_v2::accepted,
      verify_coinbase_service_payment_v2(
          produced, HF_VERSION_QWC_EPOSE, 2,
          envelope_limits(), expected, actual));

  crypto::public_key wrong_public{};
  crypto::secret_key wrong_secret{};
  crypto::generate_keys(wrong_public, wrong_secret);
  produced = proofless_coinbase;
  const cryptonote::transaction unchanged = produced;
  EXPECT_EQ(reward_status_v2::invalid_transaction_key,
      append_coinbase_service_payment_proof_v2(
          produced, wrong_secret, HF_VERSION_QWC_EPOSE, 2,
          envelope_limits(), expected, produced_context));
  EXPECT_EQ(cryptonote::tx_to_blob(unchanged), cryptonote::tx_to_blob(produced));
  EXPECT_TRUE(produced_context.outputs.empty());

  cryptonote::transaction wrong_amount = coinbase;
  ++wrong_amount.vout.back().amount;
  EXPECT_EQ(reward_status_v2::invalid_payment_proof,
      verify_coinbase_service_payment_v2(
          wrong_amount, HF_VERSION_QWC_EPOSE, 2,
          envelope_limits(), expected, actual));
  EXPECT_TRUE(actual.outputs.empty());

  cryptonote::transaction duplicate_proof = coinbase;
  ASSERT_EQ(envelope_status_v2::accepted,
      append_transaction_envelope_v2(
          {proof_record}, HF_VERSION_QWC_EPOSE, 2,
          envelope_limits(), duplicate_proof.extra, budget));
  EXPECT_EQ(reward_status_v2::invalid_payment_proof,
      verify_coinbase_service_payment_v2(
          duplicate_proof, HF_VERSION_QWC_EPOSE, 2,
          envelope_limits(), expected, actual));
  EXPECT_TRUE(actual.outputs.empty());
}

TEST(epose_reward_v2, production_miner_constructor_emits_verifiable_v2_payment)
{
  uint64_t scheduled_subsidy = 0;
  ASSERT_TRUE(cryptonote::get_block_reward(
      CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5, 1, 0,
      scheduled_subsidy, HF_VERSION_QWC_EPOSE));
  const uint64_t fees = 25;
  const uint64_t service_reward = scheduled_subsidy / 10;
  ASSERT_GT(service_reward, 0u);

  service_payment_expectation_v2 expected{};
  expected.nettype = cryptonote::TESTNET;
  expected.genesis_hash = hash_text("miner-constructor-genesis");
  expected.parameter_set_hash = hash_text("miner-constructor-parameters");
  expected.height = 1440;
  expected.parent_hash = hash_text("miner-constructor-parent");
  expected.payout_epoch = 2;
  expected.qualification_hash = hash_text("miner-constructor-qualification");
  expected.payee_service_public_key = make_public_key();
  expected.reward_address = make_address();
  expected.service_reward = service_reward;

  const cryptonote::account_public_address miner_address = make_address();
  const envelope_limits_v2 limits = envelope_limits();
  service_payment_context_v2 generated{};
  const cryptonote::miner_service_payment_v2 payment{
      &expected,
      0,
      scheduled_subsidy + fees,
      2,
      &limits,
      &generated};
  cryptonote::transaction coinbase{};
  ASSERT_TRUE(cryptonote::construct_miner_tx(
      expected.height,
      CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5,
      0,
      1,
      fees,
      miner_address,
      coinbase,
      {},
      1,
      HF_VERSION_QWC_EPOSE,
      nullptr,
      0,
      &payment));

  service_payment_context_v2 verified{};
  ASSERT_EQ(reward_status_v2::accepted,
      verify_coinbase_service_payment_v2(
          coinbase, HF_VERSION_QWC_EPOSE, 2, limits,
          expected, verified));
  EXPECT_EQ(generated.coinbase_commitment, verified.coinbase_commitment);
  EXPECT_EQ(service_reward, verified.service_reward);
  ASSERT_FALSE(verified.outputs.empty());
  uint64_t paid = 0;
  for (const service_payment_output_v2 &output : verified.outputs)
    paid += output.amount;
  EXPECT_EQ(service_reward, paid);
}
