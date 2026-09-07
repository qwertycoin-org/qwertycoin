// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <cstring>

#include "epose/service_producer_v2.h"
#include "epose/semantic_batch_v2.h"

namespace
{
  using namespace qwertycoin::epose;

  crypto::hash hash_text(const char *text)
  {
    return crypto::cn_fast_hash(text, std::strlen(text));
  }

  consensus_parameters_v2 parameters(uint8_t difficulty = 1)
  {
    consensus_parameters_v2 out{};
    out.nettype = cryptonote::MAINNET;
    out.genesis_hash = hash_text("producer-genesis");
    out.parameter_set_hash = hash_text("producer-parameters");
    out.timing = {0, 720, 60};
    out.admission = {admission_work_algorithm_v2::randomx, difficulty, 1};
    out.committee = {3, 3, 3, 2,
        static_cast<uint8_t>(service_kind_v2::canonical_object),
        {0, 200, 400}, 1000};
    out.limits.max_envelopes_per_transaction = 4;
    out.limits.envelope.max_envelope_bytes = 65536;
    out.limits.envelope.max_records = 256;
    out.limits.envelope.max_record_payload_bytes = 65536;
    out.limits.envelope.max_signature_verifications = 2048;
    out.limits.envelope.max_admission_verifications = 8;
    out.limits.envelope.supported_record_versions = {0, 1, 1, 1, 1, 1};
    out.limits.block = {262144, 1024, 2048, 8};
    out.limits.max_recent_undo_blocks = 2160;
    out.empty_policy = empty_qualification_policy_v2::miner_fallback;
    out.state_commitment_schema = 1;
    return out;
  }

  service_enrollment_config_v2 configuration()
  {
    service_enrollment_config_v2 out{};
    crypto::generate_keys(
        out.keystore.operator_public_key, out.keystore.operator_secret_key);
    crypto::generate_keys(
        out.keystore.service_public_key, out.keystore.service_secret_key);
    crypto::secret_key ignored{};
    crypto::generate_keys(out.reward_address.m_view_public_key, ignored);
    crypto::generate_keys(out.reward_address.m_spend_public_key, ignored);
    out.endpoint_transport = endpoint_transport_v2::tcp_ipv4;
    out.endpoint_host = "8.8.8.8";
    out.endpoint_port = 8198;
    out.target_epoch = 1;
    out.expiry_epoch = 3;
    return out;
  }

  class contexts final : public canonical_context_source_v2
  {
  public:
    explicit contexts(const crypto::hash &genesis)
      : genesis_(genesis), epoch_one_(hash_text("epoch-one")) {}
    bool block_hash(uint64_t height, crypto::hash &hash) const override
    {
      if (height == 0)
        hash = genesis_;
      else if (height == 720)
        hash = epoch_one_;
      else
        hash = crypto::null_hash;
      return height == 0 || height == 720;
    }
    bool round_anchor(uint64_t, uint64_t, crypto::hash &) const override
    {
      return false;
    }
  private:
    crypto::hash genesis_{};
    crypto::hash epoch_one_{};
  };
}

TEST(epose_service_producer_v2, builds_real_signed_endpoint_lifecycle_and_randomx_admission)
{
  const auto policy = parameters();
  const auto config = configuration();
  service_enrollment_v2 enrollment{};
  ASSERT_EQ(service_producer_status_v2::accepted,
      build_initial_service_enrollment_v2(
          policy, config, policy.genesis_hash, 1000, enrollment));
  ASSERT_EQ(2u, enrollment.records.size());
  EXPECT_EQ(resource_status_v2::accepted,
      validate_endpoint_descriptor_v2(
          policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
          enrollment.endpoint));
  EXPECT_EQ(lifecycle_status_v2::accepted,
      validate_lifecycle_record_authorization_v2(
          policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
          enrollment.lifecycle));
  const admission_context_v2 admission_context{
      policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
      0, policy.genesis_hash};
  EXPECT_TRUE(validate_admission_lease_v2(
      enrollment.admission, admission_context, policy.admission));

  semantic_state_v2 state{
      policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
      policy.timing, policy.admission, policy.committee};
  semantic_apply_summary_v2 summary{};
  const contexts source{policy.genesis_hash};
  EXPECT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          enrollment.records, {1, false, nullptr, nullptr}, source, summary));
  EXPECT_EQ(1u, summary.lifecycle_records);
  EXPECT_EQ(1u, summary.admission_records);
  EXPECT_EQ(2u, summary.verifications.signatures);
  EXPECT_EQ(1u, summary.verifications.randomx);
}

TEST(epose_service_producer_v2, rejects_prohibited_endpoint_and_wrong_context)
{
  const auto policy = parameters();
  auto config = configuration();
  config.endpoint_host = "127.0.0.1";
  service_enrollment_v2 enrollment{};
  EXPECT_EQ(service_producer_status_v2::invalid_endpoint,
      build_initial_service_enrollment_v2(
          policy, config, policy.genesis_hash, 1000, enrollment));
  EXPECT_TRUE(enrollment.records.empty());

  config = configuration();
  EXPECT_EQ(service_producer_status_v2::invalid_context,
      build_initial_service_enrollment_v2(
          policy, config, crypto::null_hash, 1000, enrollment));
  EXPECT_TRUE(enrollment.records.empty());
}

TEST(epose_service_producer_v2, admission_search_is_bounded_and_atomic)
{
  const auto policy = parameters(255);
  const auto config = configuration();
  service_enrollment_v2 enrollment{};
  EXPECT_EQ(service_producer_status_v2::admission_search_exhausted,
      build_initial_service_enrollment_v2(
          policy, config, policy.genesis_hash, 1, enrollment));
  EXPECT_TRUE(enrollment.records.empty());
  EXPECT_EQ(crypto::null_hash, enrollment.admission.lease_hash);
}

TEST(epose_service_producer_v2, admission_search_honors_cancellation_atomically)
{
  const auto policy = parameters(255);
  const auto config = configuration();
  std::atomic<bool> cancel{true};
  service_enrollment_v2 enrollment{};
  EXPECT_EQ(service_producer_status_v2::cancelled,
      build_initial_service_enrollment_v2(
          policy, config, policy.genesis_hash, 1000000, enrollment, &cancel));
  EXPECT_TRUE(enrollment.records.empty());
  EXPECT_EQ(crypto::null_hash, enrollment.admission.lease_hash);
}

TEST(epose_service_producer_v2, renewal_extends_lifecycle_and_admits_the_next_epoch)
{
  const auto policy = parameters();
  const auto config = configuration();
  const contexts source{policy.genesis_hash};
  service_enrollment_v2 initial{};
  ASSERT_EQ(service_producer_status_v2::accepted,
      build_initial_service_enrollment_v2(
          policy, config, policy.genesis_hash, 1000, initial));
  semantic_state_v2 state{
      policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
      policy.timing, policy.admission, policy.committee};
  semantic_apply_summary_v2 summary{};
  ASSERT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          initial.records, {1, false, nullptr, nullptr}, source, summary));

  service_enrollment_v2 renewal{};
  const crypto::hash epoch_one_hash = hash_text("epoch-one");
  ASSERT_EQ(service_producer_status_v2::accepted,
      build_service_renewal_enrollment_v2(
          policy, config.keystore, initial.lifecycle.next_descriptor,
          2, epoch_one_hash, 1000, renewal));
  ASSERT_EQ(2u, renewal.records.size());
  EXPECT_EQ(lifecycle_action_v2::renew_lease, renewal.lifecycle.action);
  EXPECT_EQ(1u, renewal.lifecycle.next_descriptor.sequence);
  EXPECT_EQ(2u, renewal.lifecycle.next_descriptor.effective_epoch);
  EXPECT_EQ(4u, renewal.lifecycle.next_descriptor.expiry_epoch);
  EXPECT_EQ(initial.lifecycle.next_descriptor.endpoint_descriptor_hash,
      renewal.lifecycle.next_descriptor.endpoint_descriptor_hash);
  EXPECT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          renewal.records, {721, false, nullptr, nullptr}, source, summary));
  EXPECT_TRUE(state.membership().has_admission(
      initial.lifecycle.next_descriptor.identity_id, 2));
}
