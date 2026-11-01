// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <cstring>
#include <fstream>
#include <boost/filesystem.hpp>
#include "epose/relay_pool_v2.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"

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
    out.committee = {3, 2, 3, 2,
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
  ASSERT_TRUE(enrollment.endpoint_update.present);
  EXPECT_EQ(resource_status_v2::accepted,
      validate_endpoint_descriptor_v2(
          policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
          enrollment.endpoint_update.descriptor));
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

TEST(epose_service_producer_v2, launch_worker_bound_covers_sixty_four_mean_search_spaces)
{
  EXPECT_EQ(UINT64_C(1) << 24,
      EPOSE_V2_LAUNCH_ADMISSION_MAX_NONCE_ATTEMPTS);
  EXPECT_EQ(UINT64_C(64),
      EPOSE_V2_LAUNCH_ADMISSION_MAX_NONCE_ATTEMPTS / (UINT64_C(1) << 18));
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

TEST(epose_service_producer_v2, admission_nonce_changes_randomx_work_with_expected_spread)
{
  const auto policy = parameters(16);
  const auto config = configuration();
  service_enrollment_v2 enrollment{};
  ASSERT_EQ(service_producer_status_v2::accepted,
      build_initial_service_enrollment_v2(
          parameters(1), config, policy.genesis_hash, 1000, enrollment));

  admission_lease_v2 lease = enrollment.admission;
  const admission_context_v2 context{
      policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
      0, policy.genesis_hash};
  crypto::hash previous{};
  unsigned best = 0;
  for (uint64_t nonce = 0; nonce < 256; ++nonce)
  {
    lease.nonce = nonce;
    const crypto::hash work = calculate_admission_work_v2(lease, context);
    if (nonce != 0)
    {
      EXPECT_NE(previous, work);
    }
    previous = work;
    for (unsigned bits = 1; bits <= 32; ++bits)
    {
      if (!admission_work_meets_target_v2(work, bits))
        break;
      if (bits > best)
        best = bits;
    }
  }
  EXPECT_GE(best, 2u);
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
  auto renewal_config = config;
  renewal_config.target_epoch = 2;
  renewal_config.expiry_epoch = 4;
  ASSERT_EQ(service_producer_status_v2::accepted,
      build_service_renewal_enrollment_v2(
          policy, renewal_config, initial.lifecycle.next_descriptor,
          epoch_one_hash, 1000, renewal));
  ASSERT_EQ(2u, renewal.records.size());
  ASSERT_TRUE(renewal.endpoint_update.present);
  EXPECT_EQ(lifecycle_action_v2::update_descriptor, renewal.lifecycle.action);
  EXPECT_EQ(1u, renewal.lifecycle.next_descriptor.sequence);
  EXPECT_EQ(2u, renewal.lifecycle.next_descriptor.effective_epoch);
  EXPECT_EQ(4u, renewal.lifecycle.next_descriptor.expiry_epoch);
  EXPECT_EQ(1u, renewal.endpoint_update.descriptor.sequence);
  EXPECT_EQ(4u, renewal.endpoint_update.descriptor.expiry_epoch);
  EXPECT_NE(initial.lifecycle.next_descriptor.endpoint_descriptor_hash,
      renewal.lifecycle.next_descriptor.endpoint_descriptor_hash);
  EXPECT_EQ(hash_endpoint_descriptor_v2(
      policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
      renewal.endpoint_update.descriptor),
      renewal.lifecycle.next_descriptor.endpoint_descriptor_hash);
  EXPECT_EQ(resource_status_v2::accepted,
      validate_endpoint_descriptor_v2(
          policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
          initial.endpoint_update.descriptor));
  EXPECT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          renewal.records, {721, false, nullptr, nullptr}, source, summary));
  EXPECT_TRUE(state.membership().has_admission(
      initial.lifecycle.next_descriptor.identity_id, 2));

  service_enrollment_v2 second_renewal{};
  auto second_config = config;
  second_config.target_epoch = 3;
  second_config.expiry_epoch = 5;
  ASSERT_EQ(service_producer_status_v2::accepted,
      build_service_renewal_enrollment_v2(
          policy, second_config, renewal.lifecycle.next_descriptor,
          hash_text("epoch-two"), 1000, second_renewal));
  ASSERT_TRUE(second_renewal.endpoint_update.present);
  EXPECT_EQ(2u, second_renewal.endpoint_update.descriptor.sequence);
  EXPECT_EQ(5u, second_renewal.endpoint_update.descriptor.expiry_epoch);
}

TEST(epose_service_producer_v2, renewal_rejects_silent_reward_redirection)
{
  const auto policy = parameters();
  const auto config = configuration();
  service_enrollment_v2 initial{};
  ASSERT_EQ(service_producer_status_v2::accepted,
      build_initial_service_enrollment_v2(
          policy, config, policy.genesis_hash, 1000, initial));

  auto changed = config;
  changed.target_epoch = 2;
  changed.expiry_epoch = 4;
  crypto::secret_key ignored{};
  crypto::generate_keys(changed.reward_address.m_spend_public_key, ignored);
  service_enrollment_v2 renewal{};
  EXPECT_EQ(service_producer_status_v2::invalid_configuration,
      build_service_renewal_enrollment_v2(
          policy, changed, initial.lifecycle.next_descriptor,
          hash_text("epoch-one"), 1000, renewal));
  EXPECT_FALSE(renewal.endpoint_update.present);
  EXPECT_TRUE(renewal.records.empty());
}

TEST(epose_service_producer_v2, receipt_retries_are_fair_and_bounded)
{
  receipt_retry_tracker_v2 tracker(2, 10, 40, 20);
  const crypto::hash anchor = hash_text("round-anchor");
  const crypto::hash first = hash_text("unreachable-first-subject");
  const crypto::hash second = hash_text("reachable-second-subject");
  const crypto::hash third = hash_text("capacity-bound-subject");
  ASSERT_TRUE(tracker.begin_context(1, 0, anchor));

  ASSERT_TRUE(tracker.start(first, 100));
  tracker.failed(first, 100);
  const auto failed = tracker.status(first);
  EXPECT_TRUE(failed.found);
  EXPECT_EQ(1u, failed.failures);
  EXPECT_EQ(110u, failed.next_attempt_ms);
  EXPECT_FALSE(failed.in_flight);
  EXPECT_FALSE(tracker.can_attempt(first, 109));
  EXPECT_TRUE(tracker.can_attempt(second, 101));
  ASSERT_TRUE(tracker.start(second, 101));
  tracker.submitted(second, 101);
  const auto submitted = tracker.status(second);
  EXPECT_TRUE(submitted.found);
  EXPECT_EQ(121u, submitted.next_attempt_ms);
  EXPECT_FALSE(submitted.in_flight);
  EXPECT_FALSE(submitted.has_envelope);
  EXPECT_FALSE(tracker.can_attempt(second, 120));
  EXPECT_FALSE(tracker.start(third, 101));
  EXPECT_TRUE(tracker.can_attempt(first, 110));

  tracker.canonical(second);
  EXPECT_FALSE(tracker.status(second).found);
  EXPECT_TRUE(tracker.start(third, 121));
  EXPECT_EQ(2u, tracker.size());
}

TEST(epose_service_producer_v2, receipt_retry_context_reconciles_reorgs)
{
  receipt_retry_tracker_v2 tracker(4, 10, 40, 20);
  const crypto::hash old_anchor = hash_text("old-anchor");
  const crypto::hash new_anchor = hash_text("new-anchor");
  const crypto::hash slot = hash_text("receipt-slot");
  ASSERT_TRUE(tracker.begin_context(1, 0, old_anchor));
  ASSERT_TRUE(tracker.start(slot, 100));
  tracker.submitted(slot, 100);
  EXPECT_FALSE(tracker.can_attempt(slot, 119));
  EXPECT_TRUE(tracker.can_attempt(slot, 120));

  ASSERT_TRUE(tracker.begin_context(1, 0, new_anchor));
  EXPECT_EQ(0u, tracker.size());
  EXPECT_TRUE(tracker.can_attempt(slot, 101));
  ASSERT_TRUE(tracker.start(slot, 101));
  tracker.failed(slot, 101);
  EXPECT_FALSE(tracker.can_attempt(slot, 110));
  EXPECT_TRUE(tracker.can_attempt(slot, 111));

  EXPECT_FALSE(tracker.begin_context(1, 1, crypto::null_hash));
}

TEST(epose_service_producer_v2, transport_retries_reuse_one_envelope_after_bounded_cooldown)
{
  receipt_retry_tracker_v2 tracker(4, 10, 40, 20, 3);
  const crypto::hash anchor = hash_text("stable-envelope-anchor");
  const crypto::hash slot = hash_text("stable-envelope-slot");
  ASSERT_TRUE(tracker.begin_context(3, 1, anchor));
  ASSERT_TRUE(tracker.start(slot, 100));
  tracker.submitted(slot, 100, "signed-envelope");

  std::string envelope;
  EXPECT_FALSE(tracker.transport_retry(slot, 119, envelope));
  ASSERT_TRUE(tracker.transport_retry(slot, 120, envelope));
  EXPECT_EQ("signed-envelope", envelope);
  EXPECT_FALSE(tracker.can_attempt(slot, 140));
  ASSERT_TRUE(tracker.transport_retry(slot, 140, envelope));
  EXPECT_EQ("signed-envelope", envelope);
  EXPECT_FALSE(tracker.transport_retry(slot, 160, envelope));
  ASSERT_TRUE(tracker.transport_retry(slot, 180, envelope));
  EXPECT_EQ("signed-envelope", envelope);

  const auto status = tracker.status(slot);
  ASSERT_TRUE(status.found);
  EXPECT_TRUE(status.has_envelope);
  EXPECT_EQ(4u, status.transport_attempts);

  tracker.canonical(slot);
  EXPECT_FALSE(tracker.status(slot).found);
  EXPECT_FALSE(tracker.transport_retry(slot, 200, envelope));
}

TEST(epose_service_producer_v2, multi_hop_redelivery_is_per_peer_bounded)
{
  relay_delivery_limiter_v2 intermediary(8, 60, 3);
  const std::string verifier_to_intermediary = "receipt/peer-intermediary";
  const std::string intermediary_to_miner = "receipt/peer-miner";

  ASSERT_TRUE(intermediary.allow(verifier_to_intermediary, 100));
  ASSERT_TRUE(intermediary.allow(intermediary_to_miner, 100));
  // The miner misses this first delivery. The same wave cannot loop.
  EXPECT_FALSE(intermediary.allow(intermediary_to_miner, 101));

  // A later delivery of the identical envelope can cross the intermediary.
  ASSERT_TRUE(intermediary.allow(verifier_to_intermediary, 160));
  ASSERT_TRUE(intermediary.allow(intermediary_to_miner, 160));
  EXPECT_FALSE(intermediary.allow(intermediary_to_miner, 161));

  // A second envelope has an independent per-peer budget. A sender cannot
  // evade the first envelope's bound by changing only the surrounding batch.
  ASSERT_TRUE(intermediary.allow("second-receipt/peer-miner", 161));
  EXPECT_FALSE(intermediary.allow(intermediary_to_miner, 161));

  ASSERT_TRUE(intermediary.allow(intermediary_to_miner, 220));
  EXPECT_FALSE(intermediary.allow(intermediary_to_miner, 280));
  EXPECT_TRUE(intermediary.allow(intermediary_to_miner, 400));
  EXPECT_EQ(3u, intermediary.size());

  // Reconnecting the same peer must not reset the stable peer budget. A
  // genuinely different downstream peer still receives its own bounded
  // delivery opportunity.
  EXPECT_TRUE(intermediary.allow("receipt/other-miner-peer", 221));
}

TEST(epose_service_producer_v2, eight_failed_deliveries_recover_through_a_new_intermediary)
{
  receipt_retry_tracker_v2 producer;
  relay_delivery_limiter_v2 relay;
  const crypto::hash anchor = hash_text("production-recovery-anchor");
  const crypto::hash slot = hash_text("production-recovery-slot");
  const std::string signed_envelope = "same-authenticated-envelope";
  ASSERT_TRUE(producer.begin_context(30, 0, anchor));
  ASSERT_TRUE(producer.start(slot, 0));
  producer.submitted(slot, 0, signed_envelope);
  ASSERT_TRUE(relay.allow("envelope/old-unreachable-peer", 0));

  std::string retry;
  for (uint64_t attempt = 1; attempt < 8; ++attempt)
  {
    const uint64_t now_ms = attempt * 60000;
    ASSERT_TRUE(producer.transport_retry(slot, now_ms, retry));
    EXPECT_EQ(signed_envelope, retry);
    ASSERT_TRUE(relay.allow("envelope/old-unreachable-peer", now_ms));
  }
  EXPECT_FALSE(producer.transport_retry(slot, 480000, retry));

  // The eighth failed transport starts the bounded production cooldown. Once
  // it expires, the producer reuses the exact signed bytes. A newly available
  // intermediary and its miner-facing hop each have independent bounded
  // delivery state, so the same envelope can still reach a template daemon
  // during the unchanged round context.
  ASSERT_TRUE(producer.transport_retry(slot, 540000, retry));
  EXPECT_EQ(signed_envelope, retry);
  ASSERT_TRUE(relay.allow("envelope/new-intermediary", 540000));
  ASSERT_TRUE(relay.allow("envelope/miner", 540000));
  EXPECT_EQ(3u, relay.size());

  producer.canonical(slot);
  EXPECT_FALSE(producer.transport_retry(slot, 600000, retry));
}

TEST(epose_service_producer_v2, epoch47_two_lifecycles_and_admission_final_size)
{
  const auto policy = parameters();
  const auto config = configuration();
  service_enrollment_v2 own{}, unrelated{};
  ASSERT_EQ(service_producer_status_v2::accepted, build_initial_service_enrollment_v2(
      policy, config, policy.genesis_hash, 1000, own));
  ASSERT_EQ(service_producer_status_v2::accepted, build_initial_service_enrollment_v2(
      policy, configuration(), policy.genesis_hash, 1000, unrelated));
  const auto lifecycle = own.records.front();
  const auto admission = own.records.back();
  ASSERT_EQ(772u, 2 * (lifecycle.payload.size() + 8));
  ASSERT_EQ(330u, admission.payload.size() + 8);
  ASSERT_GT(772u + 330u, MAX_TX_EXTRA_SIZE);
  uint64_t reward = 0;
  ASSERT_TRUE(cryptonote::get_block_reward(CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5,
      1, 0, reward, HF_VERSION_QWC_EPOSE));
  // Every ordering includes admission-before-dependency and hash-order inversions.
  const std::vector<envelope_record_v2> input{unrelated.records.front(), lifecycle, admission};
  std::vector<size_t> order{0, 1, 2};
  do
  {
    std::vector<envelope_record_v2> candidates;
    for (auto i : order) candidates.push_back(input[i]);
    const cryptonote::miner_service_payment_v2 payment{
        nullptr, 0, reward, 2, &policy.limits.envelope, &candidates, nullptr, policy.nettype, policy.genesis_hash, policy.parameter_set_hash};
    cryptonote::transaction tx{};
    ASSERT_TRUE(cryptonote::construct_miner_tx(1,
        CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5, 0, 1, 0,
        config.reward_address, tx, {}, 1, HF_VERSION_QWC_EPOSE, nullptr, 0, &payment));
    EXPECT_LE(tx.extra.size(), MAX_TX_EXTRA_SIZE);
    std::vector<envelope_record_v2> selected;
    envelope_budget_v2 budget{};
    ASSERT_EQ(envelope_status_v2::accepted, parse_transaction_extra_v2(
        tx.extra, HF_VERSION_QWC_EPOSE, 2, policy.limits.envelope, selected, budget));
    ASSERT_EQ(2u, selected.size());
    EXPECT_EQ(lifecycle.payload, selected.front().payload);
    EXPECT_EQ(admission.payload, selected.back().payload);
    semantic_state_v2 state{policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
        policy.timing, policy.admission, policy.committee};
    semantic_apply_summary_v2 summary{};
    ASSERT_EQ(semantic_status_v2::accepted, state.apply_transaction(
        selected, {1, true, nullptr, nullptr}, contexts{policy.genesis_hash}, summary));
  } while (std::next_permutation(order.begin(), order.end()));

  // Restart re-signing creates different bytes for the same dependency. It
  // must not turn a fitting pair into two lifecycles plus one admission.
  service_enrollment_v2 resigned{};
  ASSERT_EQ(service_producer_status_v2::accepted, build_initial_service_enrollment_v2(
      policy, config, policy.genesis_hash, 1000, resigned));
  std::vector<envelope_record_v2> duplicates{lifecycle, resigned.records.front(), admission};
  const cryptonote::miner_service_payment_v2 duplicate_payment{
      nullptr, 0, reward, 2, &policy.limits.envelope, &duplicates, nullptr, policy.nettype, policy.genesis_hash, policy.parameter_set_hash};
  cryptonote::transaction duplicate_tx{};
  ASSERT_TRUE(cryptonote::construct_miner_tx(1,
      CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5, 0, 1, 0,
      config.reward_address, duplicate_tx, {}, 1, HF_VERSION_QWC_EPOSE,
      nullptr, 0, &duplicate_payment));
  std::vector<envelope_record_v2> deduplicated;
  envelope_budget_v2 duplicate_budget{};
  ASSERT_EQ(envelope_status_v2::accepted, parse_transaction_extra_v2(
      duplicate_tx.extra, HF_VERSION_QWC_EPOSE, 2, policy.limits.envelope,
      deduplicated, duplicate_budget));
  ASSERT_EQ(2u, deduplicated.size());
  EXPECT_EQ(admission.payload, deduplicated.back().payload);

  // A payment proof plus nonce leaves room for the lifecycle alone. Neither
  // member of the pair may survive when the complete serialized pair cannot.
  service_payment_expectation_v2 expected{};
  expected.nettype = policy.nettype;
  expected.genesis_hash = policy.genesis_hash;
  expected.parameter_set_hash = policy.parameter_set_hash;
  expected.height = 1440;
  expected.parent_hash = hash_text("parent");
  expected.payout_epoch = 2;
  expected.qualification_hash = hash_text("qualified");
  expected.payee_service_public_key = config.keystore.service_public_key;
  expected.reward_address = config.reward_address;
  expected.service_reward = reward / 10;
  std::vector<envelope_record_v2> pair{lifecycle, admission};
  const cryptonote::miner_service_payment_v2 payment{
      &expected, 0, reward, 2, &policy.limits.envelope, &pair, nullptr, policy.nettype, policy.genesis_hash, policy.parameter_set_hash};
  cryptonote::transaction tx{};
  ASSERT_TRUE(cryptonote::construct_miner_tx(expected.height,
      CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5, 0, 1, 0,
      configuration().reward_address, tx, std::string(255, 'x'), 1,
      HF_VERSION_QWC_EPOSE, nullptr, 0, &payment));
  std::vector<envelope_record_v2> selected;
  envelope_budget_v2 budget{};
  ASSERT_EQ(envelope_status_v2::accepted, parse_transaction_extra_v2(
      tx.extra, HF_VERSION_QWC_EPOSE, 2, policy.limits.envelope, selected, budget));
  ASSERT_EQ(1u, selected.size());
  EXPECT_EQ(static_cast<uint8_t>(record_type_v2::service_payment_proof), selected.front().type);

}

TEST(epose_service_producer_v2, epoch47_queue_prioritizes_complete_admission_pair)
{
  const auto policy = parameters();
  service_enrollment_v2 own{}, unrelated{};
  ASSERT_EQ(service_producer_status_v2::accepted, build_initial_service_enrollment_v2(
      policy, configuration(), policy.genesis_hash, 1000, own));
  ASSERT_EQ(service_producer_status_v2::accepted, build_initial_service_enrollment_v2(
      policy, configuration(), policy.genesis_hash, 1000, unrelated));
  semantic_state_v2 canonical{policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
      policy.timing, policy.admission, policy.committee};
  // The queue budget fits two records; a lifecycle ID must not outrank admission.
  for (size_t max_bytes : {size_t(600), size_t(1060)})
  {
    relay_record_pool_v2 pool{policy.timing, policy.committee, policy.limits.envelope,
        {16, 16384, 1, 1, 512, 512}, {2, max_bytes, 1, 1, 100, 100}};
    for (const auto &record : {unrelated.records.front(), own.records.front(), own.records.back()})
      ASSERT_EQ(relay_record_status_v2::accepted, pool.enqueue(record, 1));
    std::vector<relay_record_selection_v2> selected;
    ASSERT_EQ(relay_record_status_v2::accepted, pool.select_for_template(1, selected, &canonical));
    if (max_bytes == 1060)
    {
      ASSERT_EQ(2u, selected.size());
      EXPECT_EQ(own.records.front().payload, selected.front().record.payload);
      EXPECT_EQ(own.records.back().payload, selected.back().record.payload);
    }
    else
    {
      ASSERT_EQ(1u, selected.size());
      EXPECT_EQ(unrelated.records.front().payload, selected.front().record.payload);
    }
  }
}

TEST(epose_service_producer_v2, epoch47_restart_after_lifecycle_only_builds_bound_admission)
{
  const auto policy = parameters();
  const auto config = configuration();
  service_enrollment_v2 original{};
  ASSERT_EQ(service_producer_status_v2::accepted, build_initial_service_enrollment_v2(
      policy, config, policy.genesis_hash, 1000, original));
  semantic_state_v2 state{policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
      policy.timing, policy.admission, policy.committee};
  semantic_apply_summary_v2 summary{};
  const contexts source{policy.genesis_hash};
  ASSERT_EQ(semantic_status_v2::accepted, state.apply_transaction(
      {original.records.front()}, {1, true, nullptr, nullptr}, source, summary));
  service_enrollment_v2 recovered{};
  ASSERT_EQ(service_producer_status_v2::accepted, build_service_renewal_enrollment_v2(
      policy, config, original.lifecycle.next_descriptor, policy.genesis_hash, 1000, recovered));
  ASSERT_EQ(1u, recovered.records.size());
  EXPECT_FALSE(recovered.endpoint_update.present);
  EXPECT_EQ(original.admission.member.descriptor_hash, recovered.admission.member.descriptor_hash);
  EXPECT_EQ(original.admission.member.sequence, recovered.admission.member.sequence);
  ASSERT_EQ(semantic_status_v2::accepted, state.apply_transaction(
      recovered.records, {2, true, nullptr, nullptr}, source, summary));
  EXPECT_TRUE(state.membership().has_admission(original.admission.member.identity_id, 1));
  auto next_config = config;
  next_config.target_epoch = 2;
  next_config.expiry_epoch = 4;
  service_enrollment_v2 renewal{};
  ASSERT_EQ(service_producer_status_v2::accepted, build_service_renewal_enrollment_v2(
      policy, next_config, original.lifecycle.next_descriptor, hash_text("epoch-one"), 1000, renewal));
  ASSERT_EQ(semantic_status_v2::accepted, state.apply_transaction(
      {renewal.records.front()}, {721, true, nullptr, nullptr}, source, summary));
  recovered = {};
  ASSERT_EQ(service_producer_status_v2::accepted, build_service_renewal_enrollment_v2(
      policy, next_config, renewal.lifecycle.next_descriptor, hash_text("epoch-one"), 1000, recovered));
  ASSERT_EQ(1u, recovered.records.size());
  EXPECT_EQ(1u, recovered.admission.member.sequence);
  EXPECT_EQ(renewal.admission.member.descriptor_hash, recovered.admission.member.descriptor_hash);
  ASSERT_EQ(semantic_status_v2::accepted, state.apply_transaction(
      recovered.records, {722, true, nullptr, nullptr}, source, summary));

}

TEST(epose_service_producer_v2, epoch47_completed_work_survives_restart_and_rejects_stale_file)
{
  const auto policy = parameters();
  const auto config = configuration();
  service_enrollment_v2 own{};
  ASSERT_EQ(service_producer_status_v2::accepted, build_initial_service_enrollment_v2(
      policy, config, policy.genesis_hash, 1000, own));
  const auto directory = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
  ASSERT_TRUE(boost::filesystem::create_directory(directory));
  struct cleanup { boost::filesystem::path path; ~cleanup() { boost::filesystem::remove_all(path); } } guard{directory};
  const auto path = (directory / "pending.bin").string();
  ASSERT_TRUE(save_pending_enrollment_v2(path, policy, own.records));
  EXPECT_FALSE(boost::filesystem::exists(path + ".tmp"));
  std::vector<envelope_record_v2> restored;
  const auto identity = own.admission.member.identity_id;
  ASSERT_TRUE(load_pending_enrollment_v2(path, policy, identity, 1, 2, policy.genesis_hash, restored));
  ASSERT_EQ(2u, restored.size());
  EXPECT_EQ(own.records.front().payload, restored.front().payload);
  EXPECT_EQ(own.records.back().payload, restored.back().payload); // includes found nonce/work
  // Force publication's temporary-file open to fail for a changed job. The
  // previous published pair must survive, and the next successful save replaces it.
  ASSERT_TRUE(boost::filesystem::create_directory(path + ".tmp"));
  EXPECT_FALSE(save_pending_enrollment_v2(path, policy, {own.records.back()}));
  ASSERT_TRUE(load_pending_enrollment_v2(path, policy, identity, 1, 2, policy.genesis_hash, restored));
  ASSERT_EQ(2u, restored.size());
  EXPECT_EQ(own.records.front().payload, restored.front().payload);
  boost::filesystem::remove(path + ".tmp");
  ASSERT_TRUE(load_pending_enrollment_v2(path, policy, identity, 1, 659, policy.genesis_hash, restored));
  semantic_state_v2 state{policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
      policy.timing, policy.admission, policy.committee};
  semantic_apply_summary_v2 summary{};
  const contexts source{policy.genesis_hash};
  ASSERT_EQ(semantic_status_v2::accepted, state.apply_transaction(
      {own.records.front()}, {1, true, nullptr, nullptr}, source, summary));
  EXPECT_EQ(semantic_status_v2::accepted, state.apply_transaction(
      restored, {2, true, nullptr, nullptr}, source, summary));
  // Valid signatures/work in a cache file are insufficient after a conflicting
  // canonical descriptor: the same semantic validation used by core rejects it.
  auto changed_config = config;
  changed_config.endpoint_host = "9.9.9.9";
  service_enrollment_v2 changed{};
  ASSERT_EQ(service_producer_status_v2::accepted, build_initial_service_enrollment_v2(
      policy, changed_config, policy.genesis_hash, 1000, changed));
  semantic_state_v2 reorg{policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
      policy.timing, policy.admission, policy.committee};
  ASSERT_EQ(semantic_status_v2::accepted, reorg.apply_transaction(
      {changed.records.front()}, {1, true, nullptr, nullptr}, source, summary));
  EXPECT_NE(semantic_status_v2::accepted, reorg.apply_transaction(
      restored, {2, true, nullptr, nullptr}, source, summary));
  EXPECT_FALSE(load_pending_enrollment_v2(path, policy, identity, 2, 721, hash_text("epoch-one"), restored));
  EXPECT_FALSE(load_pending_enrollment_v2(path, policy, identity, 1, 660, policy.genesis_hash, restored));
  EXPECT_FALSE(load_pending_enrollment_v2(path, policy, identity, 1, 2, hash_text("reorg"), restored));
  EXPECT_FALSE(load_pending_enrollment_v2(path, policy, hash_text("other-identity"), 1, 2, policy.genesis_hash, restored));
  ASSERT_TRUE(save_pending_enrollment_v2(path, policy, {own.records.back()}));
  ASSERT_TRUE(load_pending_enrollment_v2(path, policy, identity, 1, 2, policy.genesis_hash, restored));
  ASSERT_EQ(1u, restored.size());
  EXPECT_EQ(own.records.back().payload, restored.front().payload);
  { std::ofstream corrupt(path, std::ios::binary | std::ios::trunc); corrupt << "bad"; }
  EXPECT_FALSE(load_pending_enrollment_v2(path, policy, identity, 1, 2, policy.genesis_hash, restored));
  EXPECT_TRUE(restored.empty());
}

namespace {
std::string env1(const envelope_record_v2 &r, const consensus_parameters_v2 &p)
{
  std::string blob; envelope_budget_v2 b{};
  EXPECT_EQ(envelope_status_v2::accepted, encode_envelope_v2({r}, p.limits.envelope, blob, b));
  return blob;
}
}

// R3: replay must not re-queue, forward or mine a canonical admission.
TEST(review, R3_canonical_admission_replay_is_suppressed)
{
  const auto policy = parameters();
  service_enrollment_v2 own{};
  ASSERT_EQ(service_producer_status_v2::accepted, build_initial_service_enrollment_v2(
      policy, configuration(), policy.genesis_hash, 1000, own));
  const relay_policy_v2 rp{{16, 16384, 1, 1, 512, 512}, {4, 4096, 1, 1, 512, 512}, 0};
  relay_record_pool_v2 pool{policy.timing, policy.committee, policy.limits.envelope, rp.queue, rp.mining_template, 0};
  semantic_state_v2 state{policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
      policy.timing, policy.admission, policy.committee};
  semantic_apply_summary_v2 summary{};
  const contexts source{policy.genesis_hash};
  ASSERT_EQ(semantic_status_v2::accepted, state.apply_transaction(
      own.records, {20, true, nullptr, nullptr}, source, summary));
  pool.erase_confirmed_records(own.records, 20);
  std::vector<std::string> accepted;
  ASSERT_EQ(relay_ingress_status_v2::accepted, admit_relay_envelopes_v2(
      {env1(own.records.back(), policy)}, 21, policy.limits.envelope, rp, state, source, pool, accepted));
  EXPECT_EQ(0u, accepted.size()) << "forwarded";
  EXPECT_EQ(0u, pool.size()) << "re-queued";
  std::vector<relay_record_selection_v2> selected;
  ASSERT_EQ(relay_record_status_v2::accepted, pool.select_for_template(21, selected));
  EXPECT_EQ(0u, selected.size()) << "re-mined";
  // Consensus accepts the redundant inclusion.
  if (!selected.empty())
    EXPECT_EQ(semantic_status_v2::accepted, state.apply_transaction(
        {selected.front().record}, {21, true, nullptr, nullptr}, source, summary));
}

// R4: competing endpoint variants must leave a valid bound pair selectable.
TEST(review, R4_conflicting_same_sequence_lifecycle_does_not_starve_identity)
{
  const auto policy = parameters();
  const auto config = configuration();
  service_enrollment_v2 own{}, changed{};
  ASSERT_EQ(service_producer_status_v2::accepted, build_initial_service_enrollment_v2(
      policy, config, policy.genesis_hash, 1000, own));
  auto changed_config = config;
  changed_config.endpoint_host = "9.9.9.9";
  ASSERT_EQ(service_producer_status_v2::accepted, build_initial_service_enrollment_v2(
      policy, changed_config, policy.genesis_hash, 1000, changed));
  ASSERT_EQ(own.admission.member.identity_id, changed.admission.member.identity_id);
  ASSERT_EQ(own.admission.member.sequence, changed.admission.member.sequence);
  for (auto order : {0, 1})
  {
    std::vector<envelope_record_v2> candidates = order == 0
        ? std::vector<envelope_record_v2>{changed.records.front(), own.records.front(), own.records.back()}
        : std::vector<envelope_record_v2>{own.records.front(), own.records.back(), changed.records.front(), changed.records.back()};
    semantic_state_v2 semantic{policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
        policy.timing, policy.admission, policy.committee};
    std::vector<bool> included(candidates.size());
    std::vector<envelope_record_v2> records;
    // Mirrors Blockchain::select_epose_template_records_v2.
    for (const auto &group : enrollment_template_groups_v2(candidates, policy.nettype, policy.genesis_hash, policy.parameter_set_hash))
    {
      std::vector<envelope_record_v2> batch;
      for (auto i : group) if (!included[i]) batch.push_back(candidates[i]);
      semantic_apply_summary_v2 summary{};
      if (semantic.apply_transaction(batch, {1, true, nullptr, nullptr}, contexts{policy.genesis_hash}, summary)
          != semantic_status_v2::accepted)
        continue;
      records.insert(records.end(), batch.begin(), batch.end());
      for (auto i : group) included[i] = true;
    }
    ASSERT_EQ(2u, records.size()) << "order " << order;
    EXPECT_EQ(own.records.front().payload, records.front().payload);
    EXPECT_EQ(own.records.back().payload, records.back().payload);
  }
}

// R1 also runs against the unmodified v2.0.5 relay_pool translation unit.
TEST(review, R1_complete_pair_accepted_with_canonical_lifecycle)
{
  const auto policy = parameters();
  service_enrollment_v2 own{};
  ASSERT_EQ(service_producer_status_v2::accepted, build_initial_service_enrollment_v2(
      policy, configuration(), policy.genesis_hash, 1000, own));
  const relay_policy_v2 rp{{16, 16384, 1, 1, 512, 512}, {4, 4096, 1, 1, 512, 512}, 0};
  relay_record_pool_v2 pool{policy.timing, policy.committee, policy.limits.envelope,
      rp.queue, rp.mining_template, 0};
  semantic_state_v2 state{policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
      policy.timing, policy.admission, policy.committee};
  const contexts source{policy.genesis_hash};
  semantic_apply_summary_v2 summary{};
  ASSERT_EQ(semantic_status_v2::accepted, state.apply_transaction(
      {own.records.front()}, {20, true, nullptr, nullptr}, source, summary));
  // Model a peer that already cached L before learning its canonical inclusion.
  ASSERT_EQ(relay_record_status_v2::accepted, pool.enqueue(own.records.front(), 20));
  const std::vector<std::string> complete{env1(own.records.front(), policy), env1(own.records.back(), policy)};
  std::vector<std::string> accepted;
  ASSERT_EQ(relay_ingress_status_v2::accepted, admit_relay_envelopes_v2(
      complete, 21, policy.limits.envelope, rp, state, source, pool, accepted));
  ASSERT_FALSE(accepted.empty());
  EXPECT_EQ(complete.back(), accepted.back());
  // Also cover confirmation having already erased L from the relay pool.
  // v2.0.5 may cache L again, but its unchanged semantic path must still
  // recognize the canonical duplicate and accept the following admission.
  relay_record_pool_v2 empty_pool{policy.timing, policy.committee, policy.limits.envelope,
      rp.queue, rp.mining_template, 0};
  ASSERT_EQ(relay_ingress_status_v2::accepted, admit_relay_envelopes_v2(
      complete, 21, policy.limits.envelope, rp, state, source, empty_pool, accepted));
  ASSERT_FALSE(accepted.empty());
  EXPECT_EQ(complete.back(), accepted.back());
}

TEST(review, R2_two_pool_origin_delivery_stops_at_confirmation_or_cutoff)
{
  const auto policy = parameters();
  service_enrollment_v2 own{};
  ASSERT_EQ(service_producer_status_v2::accepted, build_initial_service_enrollment_v2(
      policy, configuration(), policy.genesis_hash, 1000, own));
  const auto dir = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
  ASSERT_TRUE(boost::filesystem::create_directory(dir));
  struct cleanup { boost::filesystem::path p; ~cleanup() { boost::filesystem::remove_all(p); } } guard{dir};
  const auto path = (dir / "pending.bin").string();
  const std::vector<std::string> complete{env1(own.records.front(), policy), env1(own.records.back(), policy)};
  const relay_policy_v2 rp{{16, 16384, 1, 1, 512, 512}, {4, 4096, 1, 1, 512, 512}, 0};
  const contexts source{policy.genesis_hash};
  uint64_t cutoff = 0;
  ASSERT_TRUE(policy.timing.enrollment_cutoff(1, cutoff));
  for (bool confirm : {false, true})
  {
    relay_record_pool_v2 origin{policy.timing, policy.committee, policy.limits.envelope, rp.queue, rp.mining_template, 0};
    auto receiver = origin;
    semantic_state_v2 origin_state{policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
        policy.timing, policy.admission, policy.committee};
    auto receiver_state = origin_state;
    relay_delivery_limiter_v2 limiter(2, 60000, 2);
    EXPECT_EQ(complete, filter_envelope_delivery_v2(limiter, complete, 7, 0, false));
    EXPECT_TRUE(filter_envelope_delivery_v2(limiter, complete, 7, 1, false).empty());
    EXPECT_TRUE(filter_envelope_delivery_v2(limiter, {complete.back()}, 7, 1, false).empty());
    EXPECT_TRUE(filter_envelope_delivery_v2(limiter, complete, 7, 30000, false).empty());
    bool confirmed = false, save_failed = false, newly_accepted = false;
    size_t deliveries = 0, peer_drops = 0, forwards = 0;
    uint64_t height = 1, now_ms = 1;
    const auto tick = [&] {
      return redeliver_pending_enrollment_v2(path, policy, complete, height, confirmed,
          [&](const std::vector<std::string> &batch, std::vector<std::string> &accepted) {
            return admit_relay_envelopes_v2(batch, height, policy.limits.envelope, rp,
                origin_state, source, origin, accepted);
          }, [&](const std::vector<std::string> &batch) {
            EXPECT_EQ(complete, batch);
            ASSERT_EQ(batch, filter_envelope_delivery_v2(limiter, batch, 7, now_ms, true));
            ++deliveries;
            std::vector<std::string> accepted;
            auto status = admit_relay_envelopes_v2(batch, height + 1, policy.limits.envelope,
                rp, receiver_state, source, receiver, accepted);
            if (status == relay_ingress_status_v2::invalid_batch) ++peer_drops;
            EXPECT_EQ(relay_ingress_status_v2::accepted, status);
            if (!accepted.empty())
            {
              ++forwards;
              std::vector<std::string> returned;
              ASSERT_EQ(relay_ingress_status_v2::accepted, admit_relay_envelopes_v2(
                  accepted, height, policy.limits.envelope, rp, origin_state, source, origin, returned));
              EXPECT_TRUE(returned.empty()); // no third-party replay loop
            }
          }, save_failed, newly_accepted);
    };
    // Divergent single-envelope budgets must not split a set, including at
    // the old limiter's retry exhaustion/eviction boundaries.
    for (unsigned attempt = 0; attempt < 12; ++attempt)
    {
      filter_envelope_delivery_v2(limiter, {complete.front()}, 7, now_ms, false);
      ASSERT_EQ(relay_ingress_status_v2::accepted, tick());
      EXPECT_FALSE(save_failed);
      now_ms += 30000;
    }
    EXPECT_EQ(12u, deliveries);
    EXPECT_EQ(1u, forwards);
    semantic_apply_summary_v2 summary{};
    // Origin knows L is canonical, receiver is still behind: origin sends L,A.
    ASSERT_EQ(semantic_status_v2::accepted, origin_state.apply_transaction(
        {own.records.front()}, {20, true, nullptr, nullptr}, source, summary));
    origin.erase_confirmed_records({own.records.front()}, 20);
    height = 21;
    ASSERT_EQ(relay_ingress_status_v2::accepted, tick());
    ASSERT_EQ(semantic_status_v2::accepted, receiver_state.apply_transaction(
        {own.records.front()}, {20, true, nullptr, nullptr}, source, summary));
    receiver.erase_confirmed_records({own.records.front()}, 20);
    height = cutoff - 1; // receiver is exactly at the cutoff
    ASSERT_EQ(relay_ingress_status_v2::accepted, tick());
    if (confirm)
    {
      ASSERT_EQ(semantic_status_v2::accepted, origin_state.apply_transaction(
          {own.records.back()}, {height, true, nullptr, nullptr}, source, summary));
      confirmed = origin_state.membership().has_admission(own.admission.member.identity_id, 1);
      origin.erase_confirmed_records(own.records, height);
    }
    else
      height = cutoff;
    const auto stopped_at = deliveries;
    for (unsigned attempt = 0; attempt < 12; ++attempt)
    {
      EXPECT_EQ(relay_ingress_status_v2::expired, tick());
      ++height;
      now_ms += 30000;
    }
    EXPECT_EQ(stopped_at, deliveries);
    EXPECT_EQ(0u, peer_drops);
  }
}

TEST(review, canonical_admission_is_pruned_and_excluded_from_template)
{
  const auto policy = parameters();
  service_enrollment_v2 own{};
  ASSERT_EQ(service_producer_status_v2::accepted, build_initial_service_enrollment_v2(
      policy, configuration(), policy.genesis_hash, 1000, own));
  const relay_policy_v2 rp{{16, 16384, 1, 1, 512, 512}, {4, 4096, 1, 1, 512, 512}, 0};
  relay_record_pool_v2 pool{policy.timing, policy.committee, policy.limits.envelope,
      rp.queue, rp.mining_template, 0};
  semantic_state_v2 state{policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
      policy.timing, policy.admission, policy.committee};
  const contexts source{policy.genesis_hash};
  ASSERT_EQ(relay_record_status_v2::accepted, pool.enqueue(own.records.back(), 1));
  semantic_apply_summary_v2 summary{};
  ASSERT_EQ(semantic_status_v2::accepted, state.apply_transaction(
      own.records, {20, true, nullptr, nullptr}, source, summary));
  std::vector<relay_record_selection_v2> selected;
  ASSERT_EQ(relay_record_status_v2::accepted, pool.select_for_template(21, selected, &state));
  EXPECT_TRUE(selected.empty());
  pool.prune_unusable_receipts(21, state, source);
  EXPECT_EQ(0u, pool.size());
  // Same identity/target with different lease is not silently classified canonical.
  auto other = own.admission;
  other.lease_hash = hash_text("different-work");
  EXPECT_FALSE(state.membership().contains_admission(other));
}

TEST(review, save_failure_does_not_block_submission_and_context_loss_preserves_work)
{
  const auto policy = parameters();
  service_enrollment_v2 own{};
  ASSERT_EQ(service_producer_status_v2::accepted, build_initial_service_enrollment_v2(
      policy, configuration(), policy.genesis_hash, 1000, own));
  const auto dir = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
  ASSERT_TRUE(boost::filesystem::create_directory(dir));
  struct cleanup { boost::filesystem::path p; ~cleanup() { boost::filesystem::remove_all(p); } } guard{dir};
  const auto path = (dir / "missing" / "pending.bin").string();
  const std::vector<std::string> complete{env1(own.records.front(), policy), env1(own.records.back(), policy)};
  const relay_policy_v2 rp{{16, 16384, 1, 1, 512, 512}, {4, 4096, 1, 1, 512, 512}, 0};
  relay_record_pool_v2 pool{policy.timing, policy.committee, policy.limits.envelope, rp.queue, rp.mining_template, 0};
  semantic_state_v2 state{policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
      policy.timing, policy.admission, policy.committee};
  struct missing_context : canonical_context_source_v2 {
    bool block_hash(uint64_t, crypto::hash &) const override { return false; }
    bool round_anchor(uint64_t, uint64_t, crypto::hash &) const override { return false; }
  } missing;
  const contexts present{policy.genesis_hash};
  bool save_failed = false, newly_accepted = false;
  size_t deliveries = 0;
  for (bool available : {false, true})
  {
    const canonical_context_source_v2 &source = available
        ? static_cast<const canonical_context_source_v2 &>(present) : missing;
    EXPECT_EQ(available ? relay_ingress_status_v2::accepted : relay_ingress_status_v2::context_unavailable,
        redeliver_pending_enrollment_v2(path, policy, complete, 1, false,
            [&](const std::vector<std::string> &batch, std::vector<std::string> &accepted) {
              return admit_relay_envelopes_v2(batch, 1, policy.limits.envelope, rp, state, source, pool, accepted);
            }, [&](const std::vector<std::string> &batch) { EXPECT_EQ(complete, batch); ++deliveries; },
            save_failed, newly_accepted));
    EXPECT_TRUE(save_failed);
    EXPECT_EQ(available ? 2u : 0u, pool.size());
  }
  EXPECT_EQ(1u, deliveries);
}

TEST(review, persistence_unchanged_file_and_4096_byte_cap)
{
  const auto policy = parameters();
  service_enrollment_v2 own{};
  ASSERT_EQ(service_producer_status_v2::accepted, build_initial_service_enrollment_v2(
      policy, configuration(), policy.genesis_hash, 1000, own));
  const auto dir = boost::filesystem::temp_directory_path() / boost::filesystem::unique_path();
  ASSERT_TRUE(boost::filesystem::create_directory(dir));
  struct cleanup { boost::filesystem::path p; ~cleanup() { boost::filesystem::remove_all(p); } } guard{dir};
  const auto path = (dir / "pending.bin").string();
  ASSERT_TRUE(save_pending_enrollment_v2(path, policy, own.records));
  boost::filesystem::last_write_time(path, 123456789);
  // A blocked temporary path makes any attempted rewrite fail.
  ASSERT_TRUE(boost::filesystem::create_directory(path + ".tmp"));
  EXPECT_TRUE(save_pending_enrollment_v2(path, policy, own.records));
  EXPECT_EQ(123456789, boost::filesystem::last_write_time(path));
  boost::filesystem::remove(path + ".tmp");
  std::string encoded;
  envelope_budget_v2 budget{};
  auto record = own.records.back();
  ASSERT_EQ(envelope_status_v2::accepted, encode_envelope_v2({record}, policy.limits.envelope, encoded, budget));
  record.payload.append(4096 - encoded.size(), '\0');
  ASSERT_TRUE(save_pending_enrollment_v2(path, policy, {record}));
  EXPECT_EQ(4096u, boost::filesystem::file_size(path));
  record.payload.push_back('\0');
  EXPECT_FALSE(save_pending_enrollment_v2(path, policy, {record}));
  EXPECT_EQ(4096u, boost::filesystem::file_size(path));
  { std::ofstream oversized(path, std::ios::binary | std::ios::app); oversized.put('\0'); }
  std::vector<envelope_record_v2> restored;
  EXPECT_FALSE(load_pending_enrollment_v2(path, policy, own.admission.member.identity_id,
      1, 2, policy.genesis_hash, restored));
  EXPECT_TRUE(restored.empty());
}

TEST(review, binding_chain_chooses_one_predecessor_per_sequence)
{
  const auto policy = parameters();
  const auto config = configuration();
  auto changed_config = config;
  changed_config.endpoint_host = "9.9.9.9";
  service_enrollment_v2 original{}, variant{}, renewal{};
  ASSERT_EQ(service_producer_status_v2::accepted, build_initial_service_enrollment_v2(
      policy, config, policy.genesis_hash, 1000, original));
  ASSERT_EQ(service_producer_status_v2::accepted, build_initial_service_enrollment_v2(
      policy, changed_config, policy.genesis_hash, 1000, variant));
  auto next_config = config;
  next_config.target_epoch = 2;
  next_config.expiry_epoch = 4;
  ASSERT_EQ(service_producer_status_v2::accepted, build_service_renewal_enrollment_v2(
      policy, next_config, original.lifecycle.next_descriptor, hash_text("epoch-one"), 1000, renewal));
  const std::vector<envelope_record_v2> candidates{
      variant.records.front(), renewal.records.back(), renewal.records.front(), original.records.front()};
  const auto groups = enrollment_template_groups_v2(candidates,
      policy.nettype, policy.genesis_hash, policy.parameter_set_hash);
  ASSERT_EQ(2u, groups.size());
  EXPECT_EQ((std::vector<size_t>{3, 2, 1}), groups.front());
  EXPECT_EQ((std::vector<size_t>{0}), groups.back());
  const std::vector<envelope_record_v2> rooted{renewal.records.back(), renewal.records.front()};
  const auto rooted_groups = enrollment_template_groups_v2(rooted,
      policy.nettype, policy.genesis_hash, policy.parameter_set_hash);
  ASSERT_EQ(1u, rooted_groups.size());
  EXPECT_EQ((std::vector<size_t>{1, 0}), rooted_groups.front());
  semantic_state_v2 canonical{policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
      policy.timing, policy.admission, policy.committee};
  semantic_apply_summary_v2 summary{};
  const contexts source{policy.genesis_hash};
  ASSERT_EQ(semantic_status_v2::accepted, canonical.apply_transaction(
      {original.records.front()}, {1, true, nullptr, nullptr}, source, summary));
  ASSERT_EQ(semantic_status_v2::accepted, canonical.apply_transaction(
      renewal.records, {721, true, nullptr, nullptr}, source, summary));
}

// R2-S1: the review's rotating-companion hazard must deliver the repeated
// envelope once, independently of notification composition and origin UUID.
TEST(review3, rotating_companions_cannot_bypass_envelope_limit)
{
  for (bool origin : {false, true})
  {
    relay_delivery_limiter_v2 limiter;
    const std::string a(330, 'a');
    size_t repeated = 0, companions = 0;
    for (int i = 0; i < 64; ++i)
    {
      const std::string companion(100, static_cast<char>(i));
      const auto delivered = filter_envelope_delivery_v2(
          limiter, {a, companion}, 7, 1000, origin);
      repeated += std::count(delivered.begin(), delivered.end(), a);
      companions += std::count(delivered.begin(), delivered.end(), companion);
    }
    EXPECT_EQ(1u, repeated);
    EXPECT_EQ(64u, companions);
    EXPECT_TRUE(filter_envelope_delivery_v2(limiter, {a}, 7, 1000, origin).empty());
    EXPECT_EQ(std::vector<std::string>{a},
        filter_envelope_delivery_v2(limiter, {a}, 8, 1000, origin));
    EXPECT_TRUE(filter_envelope_delivery_v2(limiter, {a}, 7, 60999, origin).empty());
    EXPECT_EQ(std::vector<std::string>{a},
        filter_envelope_delivery_v2(limiter, {a}, 7, 61000, origin));
  }
}

// A -> X -> Y, with X ahead of Y or holding pending L. The reviewer hazard
// forwarded only A. Exercise both ingress and the production transport helper.
TEST(review3, intermediary_preserves_enrollment_to_lagging_hop)
{
  const auto policy = parameters();
  service_enrollment_v2 own{};
  ASSERT_EQ(service_producer_status_v2::accepted, build_initial_service_enrollment_v2(
      policy, configuration(), policy.genesis_hash, 1000, own));
  const relay_policy_v2 rp{{16, 16384, 1, 1, 512, 512}, {4, 4096, 1, 1, 512, 512}, 0};
  const contexts source{policy.genesis_hash};
  const std::vector<std::string> complete{env1(own.records.front(), policy), env1(own.records.back(), policy)};
  for (bool canonical_l : {false, true})
  {
    relay_record_pool_v2 origin{policy.timing, policy.committee, policy.limits.envelope, rp.queue, rp.mining_template, 0};
    auto xpool = origin, ypool = origin;
    semantic_state_v2 a{policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
        policy.timing, policy.admission, policy.committee};
    auto x = a, y = a;
    semantic_apply_summary_v2 summary{};
    if (canonical_l)
      ASSERT_EQ(semantic_status_v2::accepted, x.apply_transaction(
          {own.records.front()}, {20, true, nullptr, nullptr}, source, summary));
    else
      ASSERT_EQ(relay_record_status_v2::accepted, xpool.enqueue(own.records.front(), 20));
    std::vector<std::string> outgoing, forwarded, accepted;
    ASSERT_EQ(relay_ingress_status_v2::accepted, admit_relay_envelopes_v2(
        complete, 21, policy.limits.envelope, rp, a, source, origin, outgoing));
    ASSERT_EQ(complete, outgoing);
    ASSERT_EQ(relay_ingress_status_v2::accepted, admit_relay_envelopes_v2(
        outgoing, 21, policy.limits.envelope, rp, x, source, xpool, forwarded));
    ASSERT_EQ(complete, forwarded);
    relay_delivery_limiter_v2 limiter;
    const auto delivered = filter_envelope_delivery_v2(limiter, forwarded, 9, 1000, false);
    ASSERT_EQ(complete, delivered);
    const auto status = admit_relay_envelopes_v2(delivered, 21,
        policy.limits.envelope, rp, y, source, ypool, accepted);
    EXPECT_EQ(relay_ingress_status_v2::accepted, status);
    EXPECT_NE(relay_ingress_status_v2::invalid_batch, status); // protocol's peer-drop condition
    EXPECT_EQ(2u, ypool.size());
    // Return and repeated waves stop at every node, even after the limiter resets.
    for (auto *pool : {&origin, &xpool, &ypool})
    {
      const auto &state = pool == &xpool ? x : a;
      ASSERT_EQ(relay_ingress_status_v2::accepted, admit_relay_envelopes_v2(
          complete, 21, policy.limits.envelope, rp, state, source, *pool, accepted));
      EXPECT_TRUE(accepted.empty());
    }
    // A prior L delivery may consume its budget. Never send its dependent A
    // alone; independent records still have their own per-envelope budgets.
    relay_delivery_limiter_v2 divergent;
    ASSERT_EQ(std::vector<std::string>{complete.front()}, filter_envelope_delivery_v2(
        divergent, {complete.front()}, 9, 1000, false));
    EXPECT_TRUE(filter_envelope_delivery_v2(divergent, complete, 9, 1001, false).empty());
    EXPECT_EQ(complete, filter_envelope_delivery_v2(divergent, complete, 9, 61000, false));
    // A blocked A does not hold back L: intermediary decisions are per envelope.
    relay_delivery_limiter_v2 admission_first;
    filter_envelope_delivery_v2(admission_first, {complete.back()}, 9, 1000, false);
    EXPECT_EQ(std::vector<std::string>{complete.front()},
        filter_envelope_delivery_v2(admission_first, complete, 9, 1001, false));
    EXPECT_EQ(complete, filter_envelope_delivery_v2(admission_first, complete, 9, 1001, true));
  }
}

// Compatibility control: a lone A is valid only where L is already canonical.
// This test is also linked against the unchanged v2.0.5 receiver.
TEST(review3, lone_admission_requires_canonical_lifecycle)
{
  const auto policy = parameters();
  service_enrollment_v2 own{};
  ASSERT_EQ(service_producer_status_v2::accepted, build_initial_service_enrollment_v2(
      policy, configuration(), policy.genesis_hash, 1000, own));
  const relay_policy_v2 rp{{16, 16384, 1, 1, 512, 512}, {4, 4096, 1, 1, 512, 512}, 0};
  const contexts source{policy.genesis_hash};
  for (bool canonical_l : {false, true})
  {
    relay_record_pool_v2 pool{policy.timing, policy.committee, policy.limits.envelope, rp.queue, rp.mining_template, 0};
    semantic_state_v2 state{policy.nettype, policy.genesis_hash, policy.parameter_set_hash,
        policy.timing, policy.admission, policy.committee};
    semantic_apply_summary_v2 summary{};
    if (canonical_l)
      ASSERT_EQ(semantic_status_v2::accepted, state.apply_transaction(
          {own.records.front()}, {20, true, nullptr, nullptr}, source, summary));
    std::vector<std::string> accepted;
    EXPECT_EQ(canonical_l ? relay_ingress_status_v2::accepted : relay_ingress_status_v2::invalid_batch,
        admit_relay_envelopes_v2({env1(own.records.back(), policy)}, 21,
            policy.limits.envelope, rp, state, source, pool, accepted));
    EXPECT_EQ(canonical_l ? 1u : 0u, pool.size());
  }
}
