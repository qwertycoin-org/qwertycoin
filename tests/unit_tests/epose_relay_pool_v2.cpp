// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>

#include "epose/lifecycle_v2.h"
#include "epose/record_codec_v2.h"
#include "epose/relay_pool_v2.h"
#include "string_tools.h"

namespace
{
  using namespace qwertycoin::epose;

  struct key_pair
  {
    crypto::public_key public_key{};
    crypto::secret_key secret_key{};
  };

  key_pair keys()
  {
    key_pair pair{};
    crypto::generate_keys(pair.public_key, pair.secret_key);
    return pair;
  }

  crypto::hash hash_text(const char *text)
  {
    return crypto::cn_fast_hash(text, std::strlen(text));
  }

  crypto::hash relay_genesis() { return hash_text("relay-genesis"); }
  crypto::hash relay_parameters() { return hash_text("relay-parameters"); }

  envelope_limits_v2 envelope_limits()
  {
    envelope_limits_v2 limits{};
    limits.max_envelope_bytes = 2048;
    limits.max_records = 4;
    limits.max_record_payload_bytes = 512;
    limits.max_signature_verifications = 8;
    limits.max_admission_verifications = 4;
    limits.supported_record_versions = {0, 1, 1, 1, 1, 1};
    return limits;
  }

  relay_record_pool_v2 pool()
  {
    return relay_record_pool_v2(
        epoch_timing_v2{0, 720, 60}, envelope_limits(),
        relay_queue_limits_v2{4, 8192, 1, 1, 2048, 2048},
        relay_template_limits_v2{2, 4096, 1, 1, 2048, 2048});
  }

  envelope_record_v2 lifecycle_record(uint64_t effective_epoch)
  {
    const key_pair service = keys();
    const key_pair authority = keys();
    const key_pair reward_view = keys();
    const key_pair reward_spend = keys();
    const crypto::hash genesis = relay_genesis();
    const crypto::hash parameters = relay_parameters();
    lifecycle_record_v2 lifecycle{};
    lifecycle.action = lifecycle_action_v2::register_identity;
    lifecycle.next_descriptor.identity_id = derive_identity_id_v2(
        cryptonote::TESTNET, genesis, parameters, authority.public_key);
    lifecycle.next_descriptor.service_public_key = service.public_key;
    lifecycle.next_descriptor.operator_authorization_public_key = authority.public_key;
    lifecycle.next_descriptor.reward_address.m_view_public_key = reward_view.public_key;
    lifecycle.next_descriptor.reward_address.m_spend_public_key = reward_spend.public_key;
    lifecycle.next_descriptor.endpoint_descriptor_hash = hash_text("relay-endpoint");
    lifecycle.next_descriptor.effective_epoch = effective_epoch;
    lifecycle.next_descriptor.expiry_epoch = effective_epoch + 4;
    EXPECT_TRUE(sign_lifecycle_record_v2(
        cryptonote::TESTNET, genesis, parameters, lifecycle,
        authority.secret_key, service.secret_key));
    envelope_record_v2 record{};
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_lifecycle_record_v2(
            lifecycle, cryptonote::TESTNET, genesis, parameters, record));
    return record;
  }

  std::vector<envelope_record_v2> dependent_lifecycle_records()
  {
    const key_pair service = keys();
    const key_pair authority = keys();
    const key_pair reward_view = keys();
    const key_pair reward_spend = keys();
    identity_descriptor_v2 descriptor{};
    descriptor.identity_id = derive_identity_id_v2(
        cryptonote::TESTNET, relay_genesis(), relay_parameters(),
        authority.public_key);
    descriptor.service_public_key = service.public_key;
    descriptor.operator_authorization_public_key = authority.public_key;
    descriptor.reward_address.m_view_public_key = reward_view.public_key;
    descriptor.reward_address.m_spend_public_key = reward_spend.public_key;
    descriptor.endpoint_descriptor_hash = hash_text("relay-dependent-endpoint");
    descriptor.effective_epoch = 1;
    descriptor.expiry_epoch = 5;

    lifecycle_record_v2 registration{};
    registration.action = lifecycle_action_v2::register_identity;
    registration.next_descriptor = descriptor;
    EXPECT_TRUE(sign_lifecycle_record_v2(
        cryptonote::TESTNET, relay_genesis(), relay_parameters(), registration,
        authority.secret_key, service.secret_key));

    lifecycle_record_v2 update{};
    update.action = lifecycle_action_v2::update_descriptor;
    update.previous_descriptor_hash = hash_identity_descriptor_v2(
        cryptonote::TESTNET, relay_genesis(), relay_parameters(), descriptor);
    update.next_descriptor = descriptor;
    update.next_descriptor.sequence = 1;
    update.next_descriptor.effective_epoch = 2;
    update.next_descriptor.expiry_epoch = 6;
    EXPECT_TRUE(sign_lifecycle_record_v2(
        cryptonote::TESTNET, relay_genesis(), relay_parameters(), update,
        authority.secret_key, service.secret_key));

    std::vector<envelope_record_v2> records(2);
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_lifecycle_record_v2(
            registration, cryptonote::TESTNET, relay_genesis(),
            relay_parameters(), records[0]));
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_lifecycle_record_v2(
            update, cryptonote::TESTNET, relay_genesis(), relay_parameters(),
            records[1]));
    return records;
  }

  envelope_record_v2 receipt_record(uint64_t epoch)
  {
    envelope_record_v2 record{};
    record.type = static_cast<uint8_t>(record_type_v2::service_receipt);
    record.version = EPOSE_SERVICE_RECEIPT_RECORD_VERSION_V2;
    record.payload.assign(EPOSE_SERVICE_RECEIPT_PAYLOAD_BYTES_V2, '\0');
    record.payload[0] = 1;
    record.payload[1] = 1;
    for (unsigned shift = 0; shift < 64; shift += 8)
      record.payload[2 + shift / 8] = static_cast<char>((epoch >> shift) & 0xff);
    return record;
  }

  relay_policy_v2 relay_policy()
  {
    return {
        relay_queue_limits_v2{4, 8192, 1, 1, 2048, 2048},
        relay_template_limits_v2{2, 4096, 1, 1, 2048, 2048}};
  }

  class fixed_contexts final : public canonical_context_source_v2
  {
  public:
    bool block_hash(uint64_t, crypto::hash &hash) const override
    {
      hash = hash_text("relay-context");
      return true;
    }
    bool round_anchor(uint64_t, uint64_t, crypto::hash &hash) const override
    {
      hash = hash_text("relay-anchor");
      return true;
    }
  };
}

TEST(epose_relay_pool_v2, compiled_mainnet_rehearsal_policy_matches_manifest)
{
  crypto::hash genesis{};
  crypto::hash parameters{};
  ASSERT_TRUE(epee::string_tools::hex_to_pod(
      "e791e506200ba3a221b87b6c78359c2bbb13c3ef622e1c90b3b3fbb52f4943f5",
      genesis));
  ASSERT_TRUE(epee::string_tools::hex_to_pod(
      "404be09ba6ec45b4124f348255c0bf9c2ab743d7b173132e678a691cf7cd97b3",
      parameters));

  relay_policy_v2 policy{};
  ASSERT_TRUE(compiled_relay_policy_v2(
      cryptonote::MAINNET, genesis, parameters, policy));
  EXPECT_TRUE(policy.valid());
  EXPECT_EQ(2048u, policy.queue.max_items);
  EXPECT_EQ(1048576u, policy.queue.max_bytes);
  EXPECT_EQ(256u, policy.mining_template.max_items);
  EXPECT_EQ(32768u, policy.mining_template.max_bytes);

  crypto::hash wrong = parameters;
  reinterpret_cast<unsigned char *>(&wrong)[0] ^= 1;
  EXPECT_FALSE(compiled_relay_policy_v2(
      cryptonote::MAINNET, genesis, wrong, policy));
  EXPECT_FALSE(policy.valid());
}

TEST(epose_relay_pool_v2, derives_deadlines_and_preserves_both_template_classes)
{
  auto relay = pool();
  const envelope_record_v2 enrollment_one = lifecycle_record(1);
  const envelope_record_v2 enrollment_two = lifecycle_record(2);
  const envelope_record_v2 evidence = receipt_record(1);
  ASSERT_TRUE(relay.valid());
  EXPECT_EQ(relay_record_status_v2::accepted, relay.enqueue(enrollment_one, 1));
  EXPECT_EQ(relay_record_status_v2::accepted, relay.enqueue(enrollment_two, 1));
  EXPECT_EQ(relay_record_status_v2::accepted, relay.enqueue(evidence, 1));
  EXPECT_EQ(relay_record_status_v2::idempotent_duplicate,
      relay.enqueue(evidence, 1));

  std::vector<relay_record_selection_v2> selected;
  ASSERT_EQ(relay_record_status_v2::accepted,
      relay.select_for_template(1, selected));
  ASSERT_EQ(2u, selected.size());
  EXPECT_TRUE(std::any_of(selected.begin(), selected.end(), [](const relay_record_selection_v2 &item) {
    return item.record.type == static_cast<uint8_t>(record_type_v2::service_receipt);
  }));
  EXPECT_TRUE(std::any_of(selected.begin(), selected.end(), [](const relay_record_selection_v2 &item) {
    return item.record.type == static_cast<uint8_t>(record_type_v2::identity_descriptor);
  }));
}

TEST(epose_relay_pool_v2, rejects_coinbase_proofs_and_prunes_derived_deadlines)
{
  auto relay = pool();
  envelope_record_v2 proof{};
  proof.type = static_cast<uint8_t>(record_type_v2::service_payment_proof);
  proof.version = EPOSE_PAYMENT_PROOF_RECORD_VERSION_V2;
  proof.payload.assign(EPOSE_PAYMENT_PROOF_PAYLOAD_BYTES_V2, '\0');
  EXPECT_EQ(relay_record_status_v2::payment_proof_forbidden,
      relay.enqueue(proof, 1));
  EXPECT_EQ(relay_record_status_v2::accepted,
      relay.enqueue(lifecycle_record(1), 1));
  EXPECT_EQ(relay_record_status_v2::accepted,
      relay.enqueue(receipt_record(1), 1));
  EXPECT_EQ(2u, relay.size());
  relay.prune_expired(660);
  EXPECT_EQ(1u, relay.size());
  relay.prune_expired(1380);
  EXPECT_EQ(0u, relay.size());
  EXPECT_EQ(0u, relay.bytes());
}

TEST(epose_relay_pool_v2, confirmed_records_are_removed_by_complete_record_id)
{
  auto relay = pool();
  const envelope_record_v2 receipt = receipt_record(1);
  ASSERT_EQ(relay_record_status_v2::accepted, relay.enqueue(receipt, 1));
  std::vector<relay_record_selection_v2> selected;
  ASSERT_EQ(relay_record_status_v2::accepted,
      relay.select_for_template(1, selected));
  ASSERT_EQ(1u, selected.size());
  relay.erase_confirmed({selected.front().id});
  EXPECT_EQ(0u, relay.size());
  EXPECT_EQ(0u, relay.bytes());

  ASSERT_EQ(relay_record_status_v2::accepted, relay.enqueue(receipt, 1));
  relay.erase_confirmed_records({receipt});
  EXPECT_EQ(0u, relay.size());
  EXPECT_EQ(0u, relay.bytes());
}

TEST(epose_relay_pool_v2, invalid_local_limits_fail_closed)
{
  relay_record_pool_v2 invalid(
      epoch_timing_v2{0, 720, 60}, envelope_limits(),
      relay_queue_limits_v2{},
      relay_template_limits_v2{2, 4096, 1, 1, 2048, 2048});
  EXPECT_FALSE(invalid.valid());
  EXPECT_EQ(relay_record_status_v2::invalid_configuration,
      invalid.enqueue(receipt_record(1), 1));
  std::vector<relay_record_selection_v2> selected;
  EXPECT_EQ(relay_record_status_v2::invalid_configuration,
      invalid.select_for_template(1, selected));
  EXPECT_TRUE(selected.empty());
}

TEST(epose_relay_pool_v2, relay_policy_must_fit_the_backing_queue)
{
  relay_policy_v2 policy = relay_policy();
  EXPECT_TRUE(policy.valid());
  policy.mining_template.max_items = 5;
  EXPECT_FALSE(policy.valid());
  policy.mining_template.max_items = 2;
  policy.mining_template.max_bytes = 8193;
  EXPECT_FALSE(policy.valid());
}

TEST(epose_relay_pool_v2, semantic_ingress_is_authenticated_idempotent_and_batch_atomic)
{
  const epoch_timing_v2 timing{0, 720, 60};
  const admission_policy_v2 admission{
      admission_work_algorithm_v2::randomx, 1, 1};
  const committee_policy_v2 committee{1, 1, 1, 1, 1, {0}};
  semantic_state_v2 state(
      cryptonote::TESTNET, relay_genesis(), relay_parameters(),
      timing, admission, committee);
  ASSERT_TRUE(state.valid());
  auto relay = pool();
  const envelope_record_v2 record = lifecycle_record(1);
  envelope_budget_v2 ignored{};
  std::string encoded;
  ASSERT_EQ(envelope_status_v2::accepted,
      encode_envelope_v2({record}, envelope_limits(), encoded, ignored));
  std::string corrupted = encoded;
  ASSERT_FALSE(corrupted.empty());
  corrupted.back() ^= 1;
  const fixed_contexts contexts{};
  std::vector<std::string> accepted;

  EXPECT_EQ(relay_ingress_status_v2::invalid_batch,
      admit_relay_envelopes_v2(
          {encoded, corrupted}, 1, envelope_limits(), relay_policy(),
          state, contexts, relay, accepted));
  EXPECT_EQ(0u, relay.size());
  EXPECT_TRUE(accepted.empty());

  ASSERT_EQ(relay_ingress_status_v2::accepted,
      admit_relay_envelopes_v2(
          {encoded}, 1, envelope_limits(), relay_policy(),
          state, contexts, relay, accepted));
  ASSERT_EQ(1u, relay.size());
  ASSERT_EQ(1u, accepted.size());
  EXPECT_EQ(encoded, accepted.front());

  ASSERT_EQ(relay_ingress_status_v2::accepted,
      admit_relay_envelopes_v2(
          {encoded}, 1, envelope_limits(), relay_policy(),
          state, contexts, relay, accepted));
  EXPECT_EQ(1u, relay.size());
  EXPECT_TRUE(accepted.empty());

  EXPECT_EQ(relay_ingress_status_v2::invalid_batch,
      admit_relay_envelopes_v2(
          {corrupted}, 1, envelope_limits(), relay_policy(),
          state, contexts, relay, accepted));
  EXPECT_EQ(1u, relay.size());
  EXPECT_TRUE(accepted.empty());
}

TEST(epose_relay_pool_v2, ordered_single_record_envelopes_share_one_atomic_semantic_preview)
{
  const epoch_timing_v2 timing{0, 720, 60};
  const admission_policy_v2 admission{
      admission_work_algorithm_v2::randomx, 1, 1};
  const committee_policy_v2 committee{1, 1, 1, 1, 1, {0}};
  semantic_state_v2 state(
      cryptonote::TESTNET, relay_genesis(), relay_parameters(),
      timing, admission, committee);
  ASSERT_TRUE(state.valid());
  auto relay = pool();
  const auto records = dependent_lifecycle_records();
  std::vector<std::string> encoded;
  for (const auto &record : records)
  {
    envelope_budget_v2 ignored{};
    std::string envelope;
    ASSERT_EQ(envelope_status_v2::accepted,
        encode_envelope_v2({record}, envelope_limits(), envelope, ignored));
    encoded.push_back(std::move(envelope));
  }
  const fixed_contexts contexts{};
  std::vector<std::string> accepted;

  ASSERT_EQ(relay_ingress_status_v2::accepted,
      admit_relay_envelopes_v2(
          encoded, 1, envelope_limits(), relay_policy(), state, contexts,
          relay, accepted));
  EXPECT_EQ(2u, relay.size());
  EXPECT_EQ(encoded, accepted);

  auto reversed_pool = pool();
  std::reverse(encoded.begin(), encoded.end());
  EXPECT_EQ(relay_ingress_status_v2::invalid_batch,
      admit_relay_envelopes_v2(
          encoded, 1, envelope_limits(), relay_policy(), state, contexts,
          reversed_pool, accepted));
  EXPECT_EQ(0u, reversed_pool.size());
  EXPECT_TRUE(accepted.empty());
}
