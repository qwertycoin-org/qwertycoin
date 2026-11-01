#include <gtest/gtest.h>

#include "epose_journal_test_helpers.h"

#include <string>
#include <vector>

#include "epose/membership_v2.h"
#include "epose/lifecycle_v2.h"
#include "epose/record_codec_v2.h"
#include "epose/semantic_batch_v2.h"
#include "epose/service_receipt_v2.h"
#include "epose/state_image_v2.h"
#include "epose/state_index_v2.h"

namespace
{
  using namespace qwertycoin::epose;

  crypto::hash hash_text(const std::string &value)
  {
    return crypto::cn_fast_hash(value.data(), value.size());
  }

  const crypto::hash genesis = hash_text("rolling-soak-genesis");
  const crypto::hash parameters = hash_text("rolling-soak-parameters");
  const epoch_timing_v2 timing{0, 12, 2};
  const admission_policy_v2 admission{admission_work_algorithm_v2::randomx, 1, 1};
  const committee_policy_v2 committee{1, 1, 1, 1, 1, {0}};

  struct member
  {
    crypto::public_key service{};
    crypto::secret_key service_secret{};
    crypto::public_key authority{};
    crypto::secret_key authority_secret{};
    frozen_member_v2 frozen{};
  };

  member make_member(const std::string &name)
  {
    member result{};
    crypto::generate_keys(result.service, result.service_secret);
    crypto::generate_keys(result.authority, result.authority_secret);
    result.frozen.service_public_key = result.service;
    result.frozen.operator_authorization_public_key = result.authority;
    result.frozen.identity_id = derive_identity_id_v2(
        cryptonote::TESTNET, genesis, parameters, result.authority);
    result.frozen.descriptor_hash = hash_text(name + "-descriptor");
    result.frozen.reward_binding_hash = hash_text(name + "-reward");
    result.frozen.endpoint_descriptor_hash = hash_text(name + "-endpoint");
    return result;
  }

  admission_lease_v2 lease_for(
      const member &value, uint64_t epoch, uint64_t context_height,
      const crypto::hash &context_hash)
  {
    admission_lease_v2 lease{};
    lease.member = value.frozen;
    lease.target_epoch = epoch;
    lease.work_algorithm = static_cast<uint8_t>(admission.algorithm);
    lease.leading_zero_bits = admission.leading_zero_bits;
    lease.admission_context_height = context_height;
    lease.admission_context_hash = context_hash;
    const admission_context_v2 context{
        cryptonote::TESTNET, genesis, parameters, context_height, context_hash};
    do
    {
      lease.work_hash = calculate_admission_work_v2(lease, context);
      ++lease.nonce;
    } while (!admission_work_meets_target_v2(lease.work_hash, admission.leading_zero_bits));
    --lease.nonce;
    lease.lease_hash = calculate_admission_lease_hash_v2(lease, context);
    return lease;
  }

  authenticated_service_receipt_v2 receipt_for(
      const member &subject, const member &verifier, uint64_t epoch,
      const membership_snapshot_v2 &snapshot, const crypto::hash &anchor)
  {
    authenticated_service_receipt_v2 receipt{};
    receipt.challenge.service_kind = committee.service_kind;
    receipt.challenge.epoch = epoch;
    receipt.challenge.snapshot_hash = snapshot.snapshot_hash;
    receipt.challenge.anchor_hash = anchor;
    receipt.challenge.subject_public_key = subject.service;
    receipt.challenge.verifier_public_key = verifier.service;
    receipt.challenge.endpoint_descriptor_hash =
        subject.frozen.endpoint_descriptor_hash;
    receipt.challenge.nonce = hash_text("receipt-" + std::to_string(epoch));
    receipt.challenge.requested_object_hash = anchor;
    receipt.response_object_hash = anchor;
    const receipt_context_v2 context{cryptonote::TESTNET, genesis, parameters};
    sign_subject_response_v2(receipt, subject.service_secret, context);
    sign_verifier_receipt_v2(receipt, verifier.service_secret, context);
    return receipt;
  }

  struct history_contexts final : canonical_context_source_v2
  {
    bool block_hash(uint64_t height, crypto::hash &hash) const override
    {
      hash = hash_text("block-" + std::to_string(height));
      return true;
    }

    bool round_anchor(uint64_t epoch, uint64_t round, crypto::hash &hash) const override
    {
      if (round != 0 || epoch == 0)
        return false;
      hash = hash_text("anchor-" + std::to_string(epoch));
      return true;
    }
  };

  struct identity
  {
    member keys = make_member("identity");
    crypto::public_key reward_spend{};
    crypto::secret_key reward_spend_secret{};
    crypto::public_key reward_view{};
    crypto::secret_key reward_view_secret{};
    identity_descriptor_v2 descriptor{};
  };

  identity make_identity(uint64_t epoch)
  {
    identity value{};
    crypto::generate_keys(value.reward_spend, value.reward_spend_secret);
    crypto::generate_keys(value.reward_view, value.reward_view_secret);
    value.descriptor.identity_id = value.keys.frozen.identity_id;
    value.descriptor.service_public_key = value.keys.service;
    value.descriptor.operator_authorization_public_key = value.keys.authority;
    value.descriptor.reward_address.m_spend_public_key = value.reward_spend;
    value.descriptor.reward_address.m_view_public_key = value.reward_view;
    value.descriptor.endpoint_descriptor_hash = value.keys.frozen.endpoint_descriptor_hash;
    value.descriptor.effective_epoch = epoch;
    value.descriptor.expiry_epoch = epoch + 2;
    value.keys.frozen.descriptor_hash = hash_identity_descriptor_v2(
        cryptonote::TESTNET, genesis, parameters, value.descriptor);
    value.keys.frozen.reward_binding_hash = hash_reward_binding_v2(
        cryptonote::TESTNET, genesis, parameters, value.descriptor.reward_address);
    return value;
  }

  envelope_record_v2 lifecycle_record(
      const identity &value, lifecycle_action_v2 action,
      const identity_descriptor_v2 &descriptor, const crypto::hash &previous)
  {
    lifecycle_record_v2 lifecycle{};
    lifecycle.action = action;
    lifecycle.previous_descriptor_hash = previous;
    lifecycle.next_descriptor = descriptor;
    EXPECT_TRUE(sign_lifecycle_record_v2(cryptonote::TESTNET, genesis, parameters,
        lifecycle, value.keys.authority_secret, value.keys.service_secret));
    envelope_record_v2 encoded{};
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_lifecycle_record_v2(lifecycle, cryptonote::TESTNET,
            genesis, parameters, encoded));
    return encoded;
  }

  semantic_status_v2 apply_record(semantic_state_v2 &state,
      const canonical_context_source_v2 &contexts,
      uint64_t height, const envelope_record_v2 &record,
      semantic_apply_summary_v2 &summary)
  {
    return state.apply_transaction({record}, {height, false, nullptr, nullptr},
        contexts, summary);
  }
}

TEST(epose_memory_soak, rolling_window_stays_flat_with_admissions_and_receipts)
{
  membership_pipeline_v2 pipeline(cryptonote::TESTNET, genesis, parameters,
      timing, admission, committee);
  ASSERT_TRUE(pipeline.valid());
  const member alice = make_member("alice");
  const member bob = make_member("bob");
  size_t warmed_bytes = 0;
  for (uint64_t epoch = 1; epoch <= 48; ++epoch)
  {
    const uint64_t context_height = (epoch - 1) * timing.epoch_length;
    const crypto::hash context_hash = epoch == 1
        ? genesis : hash_text("context-" + std::to_string(context_height));
    const admission_context_v2 context{
        cryptonote::TESTNET, genesis, parameters,
        context_height, context_hash};
    for (const member *value : {&alice, &bob})
      ASSERT_EQ(pipeline_status_v2::accepted,
          epose_test_journal::apply_admission(pipeline,
              lease_for(*value, epoch, context_height, context_hash),
              context, context_height + 1));
    const crypto::hash anchor = hash_text("anchor-" + std::to_string(epoch));
    ASSERT_EQ(pipeline_status_v2::accepted,
        epose_test_journal::freeze_membership(pipeline,
            epoch, epoch * timing.epoch_length - timing.anchor_depth, anchor));
    const membership_snapshot_v2 *snapshot = pipeline.snapshot(epoch);
    ASSERT_NE(nullptr, snapshot);
    const receipt_context_v2 receipt_context{
        cryptonote::TESTNET, genesis, parameters};
    ASSERT_EQ(pipeline_status_v2::accepted,
        pipeline.apply_authenticated_receipt(
            receipt_for(alice, bob, epoch, *snapshot, anchor),
            receipt_context, epoch * timing.epoch_length, anchor));
    ASSERT_EQ(pipeline_status_v2::accepted,
        pipeline.apply_authenticated_receipt(
            receipt_for(bob, alice, epoch, *snapshot, anchor),
            receipt_context, epoch * timing.epoch_length, anchor));
    ASSERT_EQ(pipeline_status_v2::accepted,
        epose_test_journal::close_qualification(pipeline,
            epoch, (epoch + 1) * timing.epoch_length - timing.anchor_depth - 1));
    const uint64_t open_epoch = epoch + 1;
    const uint64_t oldest = open_epoch > QWC_EPOSE_RETAINED_EPOCHS
        ? open_epoch - QWC_EPOSE_RETAINED_EPOCHS : 0;
    while (pipeline.oldest_retained_epoch() < oldest)
    {
      crypto::hash digest{};
      ASSERT_TRUE(epose_test_journal::fold_epoch(pipeline, pipeline.oldest_retained_epoch(), digest));
      ASSERT_NE(crypto::null_hash, digest);
    }
    if (epoch == 12)
      warmed_bytes = pipeline.state_bytes();
  }
  EXPECT_EQ(41u, pipeline.oldest_retained_epoch());
  EXPECT_LE(pipeline.state_bytes(), warmed_bytes + 1024);
}

TEST(epose_memory_soak, semantic_state_stays_bounded_with_new_retired_identities)
{
  semantic_state_v2 state(cryptonote::TESTNET, genesis, parameters,
      timing, admission, committee);
  ASSERT_TRUE(state.valid());
  history_contexts contexts;
  semantic_apply_summary_v2 summary{};
  identity verifier = make_identity(1);
  verifier.descriptor.expiry_epoch = 100;
  verifier.keys.frozen.descriptor_hash = hash_identity_descriptor_v2(
      cryptonote::TESTNET, genesis, parameters, verifier.descriptor);
  ASSERT_EQ(semantic_status_v2::accepted,
      apply_record(state, contexts, 0,
          lifecycle_record(verifier, lifecycle_action_v2::register_identity,
              verifier.descriptor, crypto::null_hash), summary));
  identity previous{};
  identity first{};
  membership_snapshot_v2 first_snapshot{};
  size_t warmed_bytes = 0;
  for (uint64_t epoch = 1; epoch <= 48; ++epoch)
  {
    ASSERT_TRUE(epose_test_journal::advance_window(state, epoch));
    identity subject = make_identity(epoch);
    const uint64_t context_height = (epoch - 1) * timing.epoch_length;
    ASSERT_EQ(semantic_status_v2::accepted,
        apply_record(state, contexts, context_height,
            lifecycle_record(subject, lifecycle_action_v2::register_identity,
                subject.descriptor, crypto::null_hash), summary));
    if (epoch == 1)
      first = subject;
    if (epoch > 1)
    {
      identity_descriptor_v2 retired = previous.descriptor;
      retired.sequence = 1;
      retired.effective_epoch = epoch;
      retired.expiry_epoch = epoch;
      ASSERT_EQ(semantic_status_v2::accepted,
          apply_record(state, contexts, context_height,
              lifecycle_record(previous, lifecycle_action_v2::deregister_identity,
                  retired, hash_identity_descriptor_v2(cryptonote::TESTNET,
                      genesis, parameters, previous.descriptor)), summary));
    }
    crypto::hash context_hash{};
    ASSERT_TRUE(contexts.block_hash(context_height, context_hash));
    const admission_context_v2 lease_context{
        cryptonote::TESTNET, genesis, parameters, context_height, context_hash};
    for (const member *value : {&subject.keys, &verifier.keys})
    {
      envelope_record_v2 encoded{};
      ASSERT_EQ(record_codec_status_v2::accepted,
          encode_admission_lease_record_v2(lease_for(*value, epoch, context_height,
              context_hash), lease_context, admission, encoded));
      ASSERT_EQ(semantic_status_v2::accepted,
          apply_record(state, contexts, context_height + 1, encoded, summary));
    }
    const crypto::hash anchor = hash_text("anchor-" + std::to_string(epoch));
    ASSERT_EQ(pipeline_status_v2::accepted,
        epose_test_journal::freeze_membership(state, epoch, epoch * timing.epoch_length - timing.anchor_depth, anchor));
    const membership_snapshot_v2 *snapshot = state.membership().snapshot(epoch);
    ASSERT_NE(nullptr, snapshot);
    if (epoch == 1)
      first_snapshot = *snapshot;
    envelope_record_v2 encoded_receipt{};
    const receipt_context_v2 receipt_context{cryptonote::TESTNET, genesis, parameters};
    ASSERT_EQ(record_codec_status_v2::accepted,
        encode_service_receipt_record_v2(
            receipt_for(subject.keys, verifier.keys, epoch, *snapshot, anchor),
            receipt_context, encoded_receipt));
    ASSERT_EQ(semantic_status_v2::accepted,
        apply_record(state, contexts, epoch * timing.epoch_length,
            encoded_receipt, summary));
    ASSERT_EQ(pipeline_status_v2::accepted,
        epose_test_journal::close_qualification(state, epoch,
            (epoch + 1) * timing.epoch_length - timing.anchor_depth - 1));
    if (epoch == 12)
      warmed_bytes = state.state_bytes();
    previous = std::move(subject);
  }
  ASSERT_TRUE(epose_test_journal::advance_window(state, 49));
  const std::string image = state.serialize_state();
  ASSERT_FALSE(image.empty());
  semantic_state_v2 restarted(cryptonote::TESTNET, genesis, parameters,
      timing, admission, committee);
  ASSERT_TRUE(restarted.restore_state(image));
  EXPECT_EQ(image, restarted.serialize_state());
  EXPECT_EQ(state.state_hash(), restarted.state_hash());
  EXPECT_TRUE(epose_test_journal::advance_window(restarted, 50));
  EXPECT_TRUE(epose_test_journal::advance_window(state, 50));
  EXPECT_EQ(state.state_hash(), restarted.state_hash());
  EXPECT_NE(crypto::null_hash, state.history_root());
  EXPECT_LE(state.state_bytes(), warmed_bytes + 12000);
  EXPECT_EQ(pipeline_status_v2::outside_retained_window,
      epose_test_journal::freeze_membership(state, 1, timing.epoch_length - timing.anchor_depth,
          first_snapshot.anchor_hash));
  envelope_record_v2 stale_receipt{};
  const receipt_context_v2 receipt_context{cryptonote::TESTNET, genesis, parameters};
  ASSERT_EQ(record_codec_status_v2::accepted,
      encode_service_receipt_record_v2(
          receipt_for(first.keys, verifier.keys, 1, first_snapshot,
              first_snapshot.anchor_hash), receipt_context, stale_receipt));
  EXPECT_EQ(semantic_status_v2::outside_retained_window,
      apply_record(state, contexts, 49 * timing.epoch_length,
          stale_receipt, summary));
  identity_descriptor_v2 attempted = first.descriptor;
  attempted.effective_epoch = 50;
  attempted.expiry_epoch = 52;
  EXPECT_EQ(semantic_status_v2::invalid_lifecycle,
      apply_record(state, contexts, 49 * timing.epoch_length,
          lifecycle_record(first, lifecycle_action_v2::register_identity,
              attempted, crypto::null_hash), summary));
  EXPECT_EQ(lifecycle_status_v2::identity_already_exists,
      summary.rejected_lifecycle);
}

TEST(epose_memory_soak, expired_identities_and_live_renewals_keep_lifecycle_bounded)
{
  lifecycle_registry_v2 registry(cryptonote::TESTNET, genesis, parameters);
  epose_test_journal::activate_window(registry);
  identity live = make_identity(1);
  live.descriptor.expiry_epoch = 12;
  const auto signed_record = [](const identity &owner,
      lifecycle_action_v2 action, const identity_descriptor_v2 &descriptor,
      const crypto::hash &previous) {
    lifecycle_record_v2 record{};
    record.action = action;
    record.previous_descriptor_hash = previous;
    record.next_descriptor = descriptor;
    EXPECT_TRUE(sign_lifecycle_record_v2(cryptonote::TESTNET, genesis,
        parameters, record, owner.keys.authority_secret,
        owner.keys.service_secret));
    return record;
  };
  ASSERT_EQ(lifecycle_status_v2::accepted,
      epose_test_journal::apply(registry, signed_record(live, lifecycle_action_v2::register_identity,
          live.descriptor, crypto::null_hash), 0, 1));
  identity first_expired{};
  size_t warmed_bytes = 0;
  for (uint64_t epoch = 2; epoch <= 48; ++epoch)
  {
    identity expired = make_identity(epoch);
    if (epoch == 2)
      first_expired = expired;
    ASSERT_EQ(lifecycle_status_v2::accepted,
        epose_test_journal::apply(registry, signed_record(expired,
            lifecycle_action_v2::register_identity, expired.descriptor,
            crypto::null_hash), epoch - 1, epoch));
    identity_descriptor_v2 renewed = live.descriptor;
    ++renewed.sequence;
    renewed.effective_epoch = epoch;
    renewed.expiry_epoch = epoch + 11;
    const lifecycle_action_v2 action = epoch % 2 == 0
        ? lifecycle_action_v2::renew_lease
        : lifecycle_action_v2::update_descriptor;
    if (action == lifecycle_action_v2::update_descriptor)
      renewed.endpoint_descriptor_hash = hash_text("live-endpoint-" + std::to_string(epoch));
    ASSERT_EQ(lifecycle_status_v2::accepted,
        epose_test_journal::apply(registry, signed_record(live, action,
            renewed, hash_identity_descriptor_v2(cryptonote::TESTNET,
                genesis, parameters, live.descriptor)), epoch - 1, epoch));
    live.descriptor = renewed;
    if (epoch > QWC_EPOSE_RETAINED_EPOCHS)
      epose_test_journal::fold_retired(registry, epoch - QWC_EPOSE_RETAINED_EPOCHS - 1);
    if (epoch == 12)
      warmed_bytes = registry.state_bytes();
  }
  EXPECT_LE(registry.state_bytes(), warmed_bytes + 36 * sizeof(crypto::hash) + 2500);
  EXPECT_NE(nullptr, registry.latest(live.descriptor.identity_id));
  EXPECT_EQ(nullptr, registry.latest(first_expired.descriptor.identity_id));
  identity_descriptor_v2 duplicate = first_expired.descriptor;
  duplicate.effective_epoch = 50;
  duplicate.expiry_epoch = 52;
  EXPECT_EQ(lifecycle_status_v2::accepted,
      epose_test_journal::apply(registry, signed_record(first_expired,
          lifecycle_action_v2::register_identity, duplicate,
          crypto::null_hash), 49, 50));
}

TEST(epose_memory_soak, rewritten_snapshot_checksum_cannot_hide_member_or_qualification_changes)
{
  membership_pipeline_v2 source(cryptonote::TESTNET, genesis, parameters,
      timing, admission, committee);
  const member alice = make_member("image-alice");
  const member bob = make_member("image-bob");
  const admission_context_v2 context{
      cryptonote::TESTNET, genesis, parameters, 0, genesis};
  for (const member *value : {&alice, &bob})
    ASSERT_EQ(pipeline_status_v2::accepted,
        epose_test_journal::apply_admission(source, lease_for(*value, 1, 0, genesis), context, 1));
  const crypto::hash anchor = hash_text("image-anchor");
  ASSERT_EQ(pipeline_status_v2::accepted,
      epose_test_journal::freeze_membership(source, 1, timing.epoch_length - timing.anchor_depth, anchor));
  const membership_snapshot_v2 *frozen = source.snapshot(1);
  ASSERT_NE(nullptr, frozen);
  const receipt_context_v2 receipt_context{
      cryptonote::TESTNET, genesis, parameters};
  ASSERT_EQ(pipeline_status_v2::accepted,
      source.apply_authenticated_receipt(receipt_for(alice, bob, 1, *frozen, anchor),
          receipt_context, timing.epoch_length, anchor));
  ASSERT_EQ(pipeline_status_v2::accepted,
      source.apply_authenticated_receipt(receipt_for(bob, alice, 1, *frozen, anchor),
          receipt_context, timing.epoch_length, anchor));
  ASSERT_EQ(pipeline_status_v2::accepted,
      epose_test_journal::close_qualification(source, 1, 2 * timing.epoch_length - timing.anchor_depth - 1));
  state_image_writer_v2 writer;
  source.write_image(writer);
  const std::string canonical = writer.data();
  const std::string key(reinterpret_cast<const char *>(&alice.service), sizeof(alice.service));
  const size_t lease_key = canonical.find(key);
  ASSERT_NE(std::string::npos, lease_key);
  const size_t member_key = canonical.find(key, lease_key + key.size());
  ASSERT_NE(std::string::npos, member_key);
  const size_t qualification_key = canonical.rfind(key);
  ASSERT_GT(qualification_key, member_key);
  for (const size_t offset : {member_key, qualification_key})
  {
    state_snapshot_v2 snapshot{};
    snapshot.state_schema = 2;
    snapshot.parameter_set_hash = parameters;
    snapshot.height = timing.epoch_length;
    snapshot.block_hash = hash_text("image-block");
    snapshot.state_root = source.state_hash();
    snapshot.state_image = canonical;
    snapshot.state_image[offset] ^= 1;
    state_snapshot_v2 restored{};
    ASSERT_EQ(state_index_status_v2::accepted,
        restored.restore(snapshot.serialize(), parameters));
    membership_pipeline_v2 candidate = source;
    state_image_reader_v2 reader(restored.state_image);
    EXPECT_FALSE(candidate.read_image(reader));
    EXPECT_EQ(source.state_hash(), candidate.state_hash());
  }
}

TEST(epose_memory_soak, recomputed_inner_hashes_still_fail_the_canonical_root)
{
  const member alice = make_member("root-alice");
  const member bob = make_member("root-bob");
  const member charlie = make_member("root-charlie");
  const crypto::hash anchor = hash_text("root-anchor");
  const auto build = [&](const member &first, const member &second,
      bool include_receipts) {
    membership_pipeline_v2 pipeline(cryptonote::TESTNET, genesis, parameters,
        timing, admission, committee);
    const admission_context_v2 context{
        cryptonote::TESTNET, genesis, parameters, 0, genesis};
    for (const member *value : {&first, &second})
      EXPECT_EQ(pipeline_status_v2::accepted,
          epose_test_journal::apply_admission(pipeline, lease_for(*value, 1, 0, genesis), context, 1));
    EXPECT_EQ(pipeline_status_v2::accepted,
        epose_test_journal::freeze_membership(pipeline, 1,
            timing.epoch_length - timing.anchor_depth, anchor));
    if (include_receipts)
    {
      const membership_snapshot_v2 *frozen = pipeline.snapshot(1);
      EXPECT_NE(nullptr, frozen);
      if (frozen != nullptr)
      {
        const receipt_context_v2 receipt_context{
            cryptonote::TESTNET, genesis, parameters};
        EXPECT_EQ(pipeline_status_v2::accepted,
            pipeline.apply_authenticated_receipt(
                receipt_for(first, second, 1, *frozen, anchor),
                receipt_context, timing.epoch_length, anchor));
        EXPECT_EQ(pipeline_status_v2::accepted,
            pipeline.apply_authenticated_receipt(
                receipt_for(second, first, 1, *frozen, anchor),
                receipt_context, timing.epoch_length, anchor));
      }
    }
    EXPECT_EQ(pipeline_status_v2::accepted,
        epose_test_journal::close_qualification(pipeline, 1,
            2 * timing.epoch_length - timing.anchor_depth - 1));
    return pipeline;
  };
  const membership_pipeline_v2 canonical = build(alice, bob, true);
  for (const membership_pipeline_v2 altered :
      {build(alice, charlie, true), build(alice, bob, false)})
  {
    state_image_writer_v2 writer;
    altered.write_image(writer);
    state_snapshot_v2 snapshot{};
    snapshot.state_schema = 2;
    snapshot.parameter_set_hash = parameters;
    snapshot.height = timing.epoch_length;
    snapshot.block_hash = hash_text("root-block");
    snapshot.state_root = canonical.state_hash();
    snapshot.state_image = writer.data();
    state_snapshot_v2 restored{};
    ASSERT_EQ(state_index_status_v2::accepted,
        restored.restore(snapshot.serialize(), parameters));
    membership_pipeline_v2 candidate(cryptonote::TESTNET, genesis, parameters,
        timing, admission, committee);
    state_image_reader_v2 reader(restored.state_image);
    ASSERT_TRUE(candidate.read_image(reader));
    EXPECT_NE(restored.state_root, candidate.state_hash());
    EXPECT_EQ(snapshot.state_root, canonical.state_hash());
  }
}
