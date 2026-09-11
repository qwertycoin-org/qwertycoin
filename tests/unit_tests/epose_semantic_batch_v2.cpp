// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <cstring>

#include "epose/record_codec_v2.h"
#include "epose/semantic_batch_v2.h"
#include "epose/service_receipt_v2.h"

namespace
{
  using namespace qwertycoin::epose;

  crypto::hash hash_text(const char *text)
  {
    return crypto::cn_fast_hash(text, std::strlen(text));
  }

  struct keys
  {
    crypto::public_key public_key{};
    crypto::secret_key secret_key{};
  };

  keys make_keys()
  {
    keys out{};
    crypto::generate_keys(out.public_key, out.secret_key);
    return out;
  }

  struct identity
  {
    keys service = make_keys();
    keys operator_auth = make_keys();
    keys reward_view = make_keys();
    keys reward_spend = make_keys();
    identity_descriptor_v2 descriptor{};
  };

  class contexts final : public canonical_context_source_v2
  {
  public:
    crypto::hash admission = hash_text("admission-anchor");
    crypto::hash next_admission = hash_text("next-admission-anchor");
    crypto::hash round = hash_text("committee-anchor");
    mutable size_t block_hash_calls = 0;

    bool block_hash(uint64_t height, crypto::hash &hash) const override
    {
      ++block_hash_calls;
      hash = {};
      if (height == 1440)
        hash = admission;
      else if (height == 2160)
        hash = next_admission;
      return hash != crypto::null_hash;
    }

    bool round_anchor(uint64_t epoch, uint64_t round_number, crypto::hash &hash) const override
    {
      hash = {};
      if (epoch != 3 || round_number != 0)
        return false;
      hash = round;
      return true;
    }
  };

  const crypto::hash genesis = hash_text("qwc-v2-genesis");
  const crypto::hash parameters = hash_text("parameter-set");
  const epoch_timing_v2 timing{1440, 720, 60};
  const admission_policy_v2 admission_policy{admission_work_algorithm_v2::randomx, 1};
  const committee_policy_v2 committee_policy{1, 1, 1, 1, 1, {0}};

  semantic_state_v2 make_state()
  {
    return {cryptonote::TESTNET, genesis, parameters, timing, admission_policy, committee_policy};
  }

  identity make_identity(const char *endpoint)
  {
    identity out{};
    out.descriptor.identity_id = derive_identity_id_v2(
        cryptonote::TESTNET, genesis, parameters, out.operator_auth.public_key);
    out.descriptor.service_public_key = out.service.public_key;
    out.descriptor.operator_authorization_public_key = out.operator_auth.public_key;
    out.descriptor.reward_address.m_view_public_key = out.reward_view.public_key;
    out.descriptor.reward_address.m_spend_public_key = out.reward_spend.public_key;
    out.descriptor.endpoint_descriptor_hash = hash_text(endpoint);
    out.descriptor.effective_epoch = 3;
    out.descriptor.expiry_epoch = 8;
    return out;
  }

  envelope_record_v2 registration_record(const identity &value)
  {
    lifecycle_record_v2 lifecycle{};
    lifecycle.next_descriptor = value.descriptor;
    EXPECT_TRUE(sign_lifecycle_record_v2(
        cryptonote::TESTNET, genesis, parameters, lifecycle,
        value.operator_auth.secret_key, value.service.secret_key));
    envelope_record_v2 record{};
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_lifecycle_record_v2(lifecycle, cryptonote::TESTNET, genesis, parameters, record));
    return record;
  }

  admission_lease_v2 admission(
      const identity &value,
      const contexts &source,
      uint64_t target_epoch = 3,
      uint64_t context_height = 1440)
  {
    admission_lease_v2 lease{};
    lease.member.service_public_key = value.descriptor.service_public_key;
    lease.member.identity_id = value.descriptor.identity_id;
    lease.member.operator_authorization_public_key = value.descriptor.operator_authorization_public_key;
    lease.member.descriptor_hash = hash_identity_descriptor_v2(
        cryptonote::TESTNET, genesis, parameters, value.descriptor);
    lease.member.reward_binding_hash = hash_reward_binding_v2(
        cryptonote::TESTNET, genesis, parameters, value.descriptor.reward_address);
    lease.member.endpoint_descriptor_hash = value.descriptor.endpoint_descriptor_hash;
    lease.member.sequence = value.descriptor.sequence;
    lease.target_epoch = target_epoch;
    lease.work_algorithm = static_cast<uint8_t>(admission_policy.algorithm);
    lease.leading_zero_bits = admission_policy.leading_zero_bits;
    lease.admission_context_height = context_height;
    lease.admission_context_hash = context_height == 2160 ? source.next_admission : source.admission;
    const admission_context_v2 context{
        cryptonote::TESTNET, genesis, parameters,
        context_height, lease.admission_context_hash};
    do
    {
      lease.work_hash = calculate_admission_work_v2(lease, context);
      ++lease.nonce;
    } while (!admission_work_meets_target_v2(lease.work_hash, admission_policy.leading_zero_bits));
    --lease.nonce;
    lease.lease_hash = calculate_admission_lease_hash_v2(lease, context);
    return lease;
  }

  envelope_record_v2 admission_record(
      const identity &value,
      const contexts &source,
      uint64_t target_epoch = 3,
      uint64_t context_height = 1440)
  {
    const admission_lease_v2 lease = admission(value, source, target_epoch, context_height);
    const admission_context_v2 context{
        cryptonote::TESTNET, genesis, parameters,
        context_height, lease.admission_context_hash};
    envelope_record_v2 record{};
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_admission_lease_record_v2(lease, context, admission_policy, record));
    return record;
  }

  envelope_record_v2 lifecycle_record(
      lifecycle_action_v2 action,
      const identity_descriptor_v2 &previous,
      const identity &next)
  {
    lifecycle_record_v2 lifecycle{};
    lifecycle.action = action;
    lifecycle.previous_descriptor_hash = hash_identity_descriptor_v2(
        cryptonote::TESTNET, genesis, parameters, previous);
    lifecycle.next_descriptor = next.descriptor;
    EXPECT_TRUE(sign_lifecycle_record_v2(
        cryptonote::TESTNET, genesis, parameters, lifecycle,
        next.operator_auth.secret_key, next.service.secret_key));
    envelope_record_v2 record{};
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_lifecycle_record_v2(lifecycle, cryptonote::TESTNET, genesis, parameters, record));
    return record;
  }

  semantic_status_v2 enroll(
      semantic_state_v2 &state, const identity &value, const contexts &source,
      semantic_apply_summary_v2 &summary)
  {
    return state.apply_transaction(
        {registration_record(value), admission_record(value, source)},
        semantic_transaction_context_v2{1500, false, nullptr}, source, summary);
  }

  service_payment_context_v2 payment_context(crypto::secret_key &transaction_secret)
  {
    service_payment_context_v2 context{};
    context.nettype = cryptonote::TESTNET;
    context.genesis_hash = genesis;
    context.parameter_set_hash = parameters;
    context.height = 2880;
    context.parent_hash = hash_text("payment-parent");
    context.payout_epoch = 4;
    context.qualification_hash = hash_text("qualification");
    context.payee_service_public_key = make_keys().public_key;
    context.reward_address.m_view_public_key = make_keys().public_key;
    context.reward_address.m_spend_public_key = make_keys().public_key;
    context.service_reward = 100;
    crypto::generate_keys(context.transaction_public_key, transaction_secret);
    context.coinbase_commitment = hash_text("normalized-coinbase");
    crypto::key_derivation derivation{};
    EXPECT_TRUE(crypto::generate_key_derivation(
        context.reward_address.m_view_public_key, transaction_secret, derivation));
    service_payment_output_v2 output{};
    output.output_index = 1;
    output.amount = context.service_reward;
    EXPECT_TRUE(crypto::derive_public_key(
        derivation, output.output_index, context.reward_address.m_spend_public_key,
        output.output_public_key));
    context.outputs.push_back(output);
    return context;
  }
}

TEST(epose_semantic_batch_v2, lifecycle_and_admission_apply_atomically_in_wire_order)
{
  contexts source;
  identity subject = make_identity("subject-endpoint");
  semantic_state_v2 state = make_state();
  semantic_apply_summary_v2 summary{};
  ASSERT_EQ(semantic_status_v2::accepted, enroll(state, subject, source, summary));
  EXPECT_EQ(1u, summary.lifecycle_records);
  EXPECT_EQ(1u, summary.admission_records);
  EXPECT_EQ(2u, summary.verifications.signatures);
  EXPECT_EQ(1u, summary.verifications.randomx);
  EXPECT_NE(nullptr, state.lifecycle().descriptor_for_epoch(subject.descriptor.identity_id, 3));

  semantic_state_v2 reversed = make_state();
  const crypto::hash before = reversed.state_hash();
  EXPECT_EQ(semantic_status_v2::lifecycle_binding_mismatch,
      reversed.apply_transaction(
          {admission_record(subject, source), registration_record(subject)},
          semantic_transaction_context_v2{1500, false, nullptr}, source, summary));
  EXPECT_EQ(before, reversed.state_hash());
  EXPECT_EQ(0u, summary.lifecycle_records);
}

TEST(epose_semantic_batch_v2, later_failure_rolls_back_valid_prefix)
{
  contexts source;
  identity subject = make_identity("subject-endpoint");
  semantic_state_v2 state = make_state();
  const crypto::hash before = state.state_hash();
  semantic_apply_summary_v2 summary{9, 9, 9, 9};
  const envelope_record_v2 invalid{255, 1, "invalid"};
  EXPECT_EQ(semantic_status_v2::invalid_record,
      state.apply_transaction(
          {registration_record(subject), invalid},
          semantic_transaction_context_v2{1500, false, nullptr}, source, summary));
  EXPECT_EQ(before, state.state_hash());
  EXPECT_EQ(0u, summary.lifecycle_records);
  EXPECT_EQ(nullptr, state.lifecycle().descriptor_for_epoch(subject.descriptor.identity_id, 3));
}

TEST(epose_semantic_batch_v2, admission_must_match_authorized_historical_descriptor)
{
  contexts source;
  identity subject = make_identity("subject-endpoint");
  semantic_state_v2 state = make_state();
  admission_lease_v2 lease = admission(subject, source);
  lease.member.reward_binding_hash = hash_text("redirected-reward");
  const admission_context_v2 context{
      cryptonote::TESTNET, genesis, parameters, 1440, source.admission};
  do
  {
    lease.work_hash = calculate_admission_work_v2(lease, context);
    ++lease.nonce;
  } while (!admission_work_meets_target_v2(lease.work_hash, admission_policy.leading_zero_bits));
  --lease.nonce;
  lease.lease_hash = calculate_admission_lease_hash_v2(lease, context);
  envelope_record_v2 lease_record{};
  ASSERT_EQ(record_codec_status_v2::accepted,
      encode_admission_lease_record_v2(lease, context, admission_policy, lease_record));
  semantic_apply_summary_v2 summary{};
  EXPECT_EQ(semantic_status_v2::lifecycle_binding_mismatch,
      state.apply_transaction(
          {registration_record(subject), lease_record},
          semantic_transaction_context_v2{1500, false, nullptr}, source, summary));
  EXPECT_EQ(nullptr, state.lifecycle().descriptor_for_epoch(subject.descriptor.identity_id, 3));
  EXPECT_EQ(0u, summary.verifications.randomx);
  EXPECT_EQ(0u, source.block_hash_calls);
}

TEST(epose_semantic_batch_v2, recovery_replaces_pending_admission_by_stable_identity)
{
  contexts source;
  identity original = make_identity("original-endpoint");
  semantic_state_v2 state = make_state();
  semantic_apply_summary_v2 summary{};

  ASSERT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          {registration_record(original)},
          semantic_transaction_context_v2{1500, false, nullptr}, source, summary));
  ASSERT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          {admission_record(original, source, 4, 2160)},
          semantic_transaction_context_v2{2200, false, nullptr}, source, summary));

  identity recovered = original;
  recovered.service = make_keys();
  recovered.descriptor.service_public_key = recovered.service.public_key;
  recovered.descriptor.sequence = 1;
  recovered.descriptor.effective_epoch = 4;
  ASSERT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          {lifecycle_record(
              lifecycle_action_v2::recover_service_key, original.descriptor, recovered)},
          semantic_transaction_context_v2{2201, false, nullptr}, source, summary));
  EXPECT_EQ(2u, summary.verifications.signatures);

  ASSERT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          {admission_record(recovered, source, 4, 2160)},
          semantic_transaction_context_v2{2202, false, nullptr}, source, summary));
  EXPECT_EQ(1u, summary.verifications.randomx);

  ASSERT_EQ(pipeline_status_v2::accepted,
      state.freeze_membership(4, 2820, hash_text("epoch-4-anchor")));
  const membership_snapshot_v2 *snapshot = state.membership().snapshot(4);
  ASSERT_NE(nullptr, snapshot);
  ASSERT_EQ(1u, snapshot->members.size());
  EXPECT_EQ(original.descriptor.identity_id, snapshot->members.front().identity_id);
  EXPECT_EQ(recovered.service.public_key, snapshot->members.front().service_public_key);
  EXPECT_EQ(1u, snapshot->members.front().sequence);
}

TEST(epose_semantic_batch_v2, final_descriptor_view_invalidates_superseded_bindings)
{
  contexts source;
  identity original = make_identity("original-endpoint");
  semantic_state_v2 state = make_state();
  semantic_apply_summary_v2 summary{};
  ASSERT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          {registration_record(original)},
          semantic_transaction_context_v2{1500, false, nullptr}, source, summary));
  ASSERT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          {admission_record(original, source, 4, 2160)},
          semantic_transaction_context_v2{2200, false, nullptr}, source, summary));

  identity updated = original;
  updated.descriptor.endpoint_descriptor_hash = hash_text("updated-endpoint");
  updated.descriptor.reward_address.m_view_public_key = make_keys().public_key;
  updated.descriptor.reward_address.m_spend_public_key = make_keys().public_key;
  updated.descriptor.sequence = 1;
  updated.descriptor.effective_epoch = 4;
  ASSERT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          {lifecycle_record(
              lifecycle_action_v2::update_descriptor, original.descriptor, updated)},
          semantic_transaction_context_v2{2201, false, nullptr}, source, summary));

  ASSERT_EQ(pipeline_status_v2::accepted,
      state.freeze_membership(4, 2820, hash_text("epoch-4-anchor")));
  const membership_snapshot_v2 *snapshot = state.membership().snapshot(4);
  ASSERT_NE(nullptr, snapshot);
  EXPECT_TRUE(snapshot->members.empty());
}

TEST(epose_semantic_batch_v2, deregistration_before_cutoff_invalidates_pending_admission)
{
  contexts source;
  identity subject = make_identity("subject-endpoint");
  semantic_state_v2 state = make_state();
  semantic_apply_summary_v2 summary{};
  ASSERT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          {registration_record(subject)},
          semantic_transaction_context_v2{1500, false, nullptr}, source, summary));
  ASSERT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          {admission_record(subject, source, 4, 2160)},
          semantic_transaction_context_v2{2200, false, nullptr}, source, summary));

  identity deregistered = subject;
  deregistered.descriptor.sequence = 1;
  deregistered.descriptor.effective_epoch = 4;
  deregistered.descriptor.expiry_epoch = 4;
  ASSERT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          {lifecycle_record(
              lifecycle_action_v2::deregister_identity, subject.descriptor, deregistered)},
          semantic_transaction_context_v2{2201, false, nullptr}, source, summary));

  ASSERT_EQ(pipeline_status_v2::accepted,
      state.freeze_membership(4, 2820, hash_text("epoch-4-anchor")));
  const membership_snapshot_v2 *snapshot = state.membership().snapshot(4);
  ASSERT_NE(nullptr, snapshot);
  EXPECT_TRUE(snapshot->members.empty());
}

TEST(epose_semantic_batch_v2, invalid_admission_contexts_do_no_randomx_work)
{
  contexts source;
  identity subject = make_identity("subject-endpoint");
  semantic_state_v2 state = make_state();
  semantic_apply_summary_v2 summary{};
  ASSERT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          {registration_record(subject)},
          semantic_transaction_context_v2{1500, false, nullptr}, source, summary));

  const size_t initial_lookups = source.block_hash_calls;
  EXPECT_EQ(semantic_status_v2::invalid_admission,
      state.apply_transaction(
          {admission_record(subject, source, 3, 1439)},
          semantic_transaction_context_v2{1501, false, nullptr}, source, summary));
  EXPECT_EQ(0u, summary.verifications.randomx);
  EXPECT_EQ(initial_lookups, source.block_hash_calls);

  EXPECT_EQ(semantic_status_v2::invalid_admission,
      state.apply_transaction(
          {admission_record(subject, source)},
          semantic_transaction_context_v2{2100, false, nullptr}, source, summary));
  EXPECT_EQ(0u, summary.verifications.randomx);
  EXPECT_EQ(initial_lookups, source.block_hash_calls);
}

TEST(epose_semantic_batch_v2, effective_epoch_is_derived_from_inclusion_height_and_cutoff)
{
  contexts source;
  identity subject = make_identity("subject-endpoint");
  semantic_state_v2 state = make_state();
  const crypto::hash before = state.state_hash();
  semantic_apply_summary_v2 summary{};
  // Epoch 3 enrollment closes at 2099; block 2100 cannot make a descriptor
  // effective for epoch 3 by supplying a permissive caller-side minimum.
  EXPECT_EQ(semantic_status_v2::invalid_lifecycle,
      state.apply_transaction(
          {registration_record(subject)},
          semantic_transaction_context_v2{2100, false, nullptr}, source, summary));
  EXPECT_EQ(before, state.state_hash());
  EXPECT_EQ(nullptr, state.lifecycle().descriptor_for_epoch(subject.descriptor.identity_id, 3));
}

TEST(epose_semantic_batch_v2, authenticated_receipt_uses_canonical_round_anchor)
{
  contexts source;
  identity subject = make_identity("subject-endpoint");
  identity verifier = make_identity("verifier-endpoint");
  semantic_state_v2 state = make_state();
  semantic_apply_summary_v2 summary{};
  ASSERT_EQ(semantic_status_v2::accepted, enroll(state, subject, source, summary));
  ASSERT_EQ(semantic_status_v2::accepted, enroll(state, verifier, source, summary));
  ASSERT_EQ(pipeline_status_v2::accepted, state.freeze_membership(3, 2100, source.round));
  const membership_snapshot_v2 *snapshot = state.membership().snapshot(3);
  ASSERT_NE(nullptr, snapshot);

  authenticated_service_receipt_v2 receipt{};
  receipt.challenge.epoch = 3;
  receipt.challenge.snapshot_hash = snapshot->snapshot_hash;
  receipt.challenge.anchor_hash = source.round;
  receipt.challenge.subject_public_key = subject.service.public_key;
  receipt.challenge.verifier_public_key = verifier.service.public_key;
  receipt.challenge.endpoint_descriptor_hash = subject.descriptor.endpoint_descriptor_hash;
  receipt.challenge.nonce = hash_text("nonce");
  receipt.challenge.requested_object_hash = hash_text("object");
  receipt.response_object_hash = receipt.challenge.requested_object_hash;
  const receipt_context_v2 receipt_context{cryptonote::TESTNET, genesis, parameters};
  sign_subject_response_v2(receipt, subject.service.secret_key, receipt_context);
  sign_verifier_receipt_v2(receipt, verifier.service.secret_key, receipt_context);
  envelope_record_v2 record{};
  ASSERT_EQ(record_codec_status_v2::accepted,
      encode_service_receipt_record_v2(receipt, receipt_context, record));
  ASSERT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          {record}, semantic_transaction_context_v2{2160, false, nullptr}, source, summary));
  EXPECT_EQ(1u, summary.receipt_records);
  EXPECT_EQ(2u, summary.verifications.signatures);

  contexts wrong = source;
  wrong.round = hash_text("wrong-round-anchor");
  const crypto::hash before = state.state_hash();
  EXPECT_EQ(semantic_status_v2::invalid_receipt,
      state.apply_transaction(
          {record}, semantic_transaction_context_v2{2160, false, nullptr}, wrong, summary));
  EXPECT_EQ(before, state.state_hash());
}

TEST(epose_semantic_batch_v2, receipt_duplicates_are_authenticated_before_success)
{
  contexts source;
  identity subject = make_identity("subject-endpoint");
  identity verifier = make_identity("verifier-endpoint");
  semantic_state_v2 state = make_state();
  semantic_apply_summary_v2 summary{};
  ASSERT_EQ(semantic_status_v2::accepted, enroll(state, subject, source, summary));
  ASSERT_EQ(semantic_status_v2::accepted, enroll(state, verifier, source, summary));
  ASSERT_EQ(pipeline_status_v2::accepted, state.freeze_membership(3, 2100, source.round));

  const membership_snapshot_v2 *snapshot = state.membership().snapshot(3);
  ASSERT_NE(nullptr, snapshot);
  authenticated_service_receipt_v2 receipt{};
  receipt.challenge.epoch = 3;
  receipt.challenge.snapshot_hash = snapshot->snapshot_hash;
  receipt.challenge.anchor_hash = source.round;
  receipt.challenge.subject_public_key = subject.service.public_key;
  receipt.challenge.verifier_public_key = verifier.service.public_key;
  receipt.challenge.endpoint_descriptor_hash = subject.descriptor.endpoint_descriptor_hash;
  receipt.challenge.nonce = hash_text("duplicate-nonce");
  receipt.challenge.requested_object_hash = hash_text("duplicate-object");
  receipt.response_object_hash = receipt.challenge.requested_object_hash;
  const receipt_context_v2 receipt_context{cryptonote::TESTNET, genesis, parameters};
  sign_subject_response_v2(receipt, subject.service.secret_key, receipt_context);
  sign_verifier_receipt_v2(receipt, verifier.service.secret_key, receipt_context);
  envelope_record_v2 valid{};
  ASSERT_EQ(record_codec_status_v2::accepted,
      encode_service_receipt_record_v2(receipt, receipt_context, valid));

  envelope_record_v2 bad_verifier = valid;
  ASSERT_FALSE(bad_verifier.payload.empty());
  bad_verifier.payload.back() ^= 1;
  authenticated_service_receipt_v2 decoded_bad{};
  ASSERT_EQ(record_codec_status_v2::accepted,
      decode_service_receipt_record_structure_v2(bad_verifier, decoded_bad));
  EXPECT_FALSE(validate_authenticated_service_receipt_v2(decoded_bad, receipt_context));

  ASSERT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          {valid}, semantic_transaction_context_v2{2160, false, nullptr}, source, summary));
  EXPECT_EQ(2u, summary.verifications.signatures);
  const crypto::hash accepted_state = state.state_hash();
  EXPECT_EQ(semantic_status_v2::invalid_receipt,
      state.apply_transaction(
          {bad_verifier}, semantic_transaction_context_v2{2160, false, nullptr}, source, summary));
  EXPECT_EQ(2u, summary.verifications.signatures);
  EXPECT_EQ(accepted_state, state.state_hash());

  semantic_state_v2 batch = make_state();
  ASSERT_EQ(semantic_status_v2::accepted, enroll(batch, subject, source, summary));
  ASSERT_EQ(semantic_status_v2::accepted, enroll(batch, verifier, source, summary));
  ASSERT_EQ(pipeline_status_v2::accepted, batch.freeze_membership(3, 2100, source.round));
  const crypto::hash before_batch = batch.state_hash();
  EXPECT_EQ(semantic_status_v2::invalid_receipt,
      batch.apply_transaction(
          {valid, bad_verifier}, semantic_transaction_context_v2{2160, false, nullptr}, source, summary));
  EXPECT_EQ(4u, summary.verifications.signatures);
  EXPECT_EQ(before_batch, batch.state_hash());

  ASSERT_EQ(semantic_status_v2::accepted,
      batch.apply_transaction(
          {valid, valid}, semantic_transaction_context_v2{2160, false, nullptr}, source, summary));
  EXPECT_EQ(2u, summary.receipt_records);
  EXPECT_EQ(4u, summary.verifications.signatures);
  EXPECT_EQ(accepted_state, batch.state_hash());

  envelope_record_v2 bad_subject = valid;
  bad_subject.payload[bad_subject.payload.size() - sizeof(crypto::signature) * 2] ^= 1;
  EXPECT_EQ(semantic_status_v2::invalid_receipt,
      batch.apply_transaction(
          {bad_subject}, semantic_transaction_context_v2{2160, false, nullptr}, source, summary));
  EXPECT_EQ(accepted_state, batch.state_hash());
}

TEST(epose_semantic_batch_v2, service_keys_are_unique_across_active_identities)
{
  contexts source;
  identity first = make_identity("first-endpoint");
  identity second = make_identity("second-endpoint");
  second.service = first.service;
  second.descriptor.service_public_key = first.service.public_key;

  semantic_apply_summary_v2 summary{};
  semantic_state_v2 state = make_state();
  ASSERT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          {registration_record(first)},
          semantic_transaction_context_v2{1500, false, nullptr}, source, summary));
  const crypto::hash after_first = state.state_hash();
  EXPECT_EQ(semantic_status_v2::invalid_lifecycle,
      state.apply_transaction(
          {registration_record(second)},
          semantic_transaction_context_v2{1500, false, nullptr}, source, summary));
  EXPECT_EQ(after_first, state.state_hash());

  semantic_state_v2 reverse = make_state();
  ASSERT_EQ(semantic_status_v2::accepted,
      reverse.apply_transaction(
          {registration_record(second)},
          semantic_transaction_context_v2{1500, false, nullptr}, source, summary));
  EXPECT_EQ(semantic_status_v2::invalid_lifecycle,
      reverse.apply_transaction(
          {registration_record(first)},
          semantic_transaction_context_v2{1500, false, nullptr}, source, summary));

  semantic_state_v2 mixed = make_state();
  const crypto::hash before_mixed = mixed.state_hash();
  EXPECT_EQ(semantic_status_v2::invalid_lifecycle,
      mixed.apply_transaction(
          {registration_record(first), registration_record(second)},
          semantic_transaction_context_v2{1500, false, nullptr}, source, summary));
  EXPECT_EQ(before_mixed, mixed.state_hash());
  EXPECT_EQ(nullptr, mixed.lifecycle().descriptor_for_epoch(first.descriptor.identity_id, 3));
}

TEST(epose_semantic_batch_v2, recovery_cannot_claim_another_active_identity_service_key)
{
  contexts source;
  identity first = make_identity("first-endpoint");
  identity second = make_identity("second-endpoint");
  semantic_state_v2 state = make_state();
  semantic_apply_summary_v2 summary{};
  ASSERT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          {registration_record(first), registration_record(second)},
          semantic_transaction_context_v2{1500, false, nullptr}, source, summary));
  ASSERT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          {admission_record(first, source, 4, 2160), admission_record(second, source, 4, 2160)},
          semantic_transaction_context_v2{2200, false, nullptr}, source, summary));

  identity recovered = second;
  recovered.service = first.service;
  recovered.descriptor.service_public_key = first.service.public_key;
  recovered.descriptor.sequence = 1;
  recovered.descriptor.effective_epoch = 4;
  const crypto::hash before = state.state_hash();
  EXPECT_EQ(semantic_status_v2::invalid_lifecycle,
      state.apply_transaction(
          {lifecycle_record(
              lifecycle_action_v2::recover_service_key, second.descriptor, recovered)},
          semantic_transaction_context_v2{2201, false, nullptr}, source, summary));
  EXPECT_EQ(before, state.state_hash());

  ASSERT_EQ(pipeline_status_v2::accepted,
      state.freeze_membership(4, 2820, hash_text("epoch-4-anchor")));
  const membership_snapshot_v2 *snapshot = state.membership().snapshot(4);
  ASSERT_NE(nullptr, snapshot);
  ASSERT_EQ(2u, snapshot->members.size());
  EXPECT_NE(snapshot->members[0].identity_id, snapshot->members[1].identity_id);
  EXPECT_NE(snapshot->members[0].service_public_key, snapshot->members[1].service_public_key);
  for (const frozen_member_v2 &member : snapshot->members)
  {
    const auto committee = state.membership().committee(
        4, 0, member.service_public_key, snapshot->anchor_hash);
    ASSERT_EQ(1u, committee.size());
    EXPECT_NE(member.service_public_key, committee.front().verifier_public_key);
  }
}

TEST(epose_semantic_batch_v2, admission_key_collision_rolls_back_valid_lifecycle_prefix)
{
  contexts source;
  identity first = make_identity("first-endpoint");
  semantic_state_v2 state = make_state();
  semantic_apply_summary_v2 summary{};
  ASSERT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          {registration_record(first)},
          semantic_transaction_context_v2{1500, false, nullptr}, source, summary));
  ASSERT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          {admission_record(first, source, 4, 2160)},
          semantic_transaction_context_v2{2200, false, nullptr}, source, summary));

  identity deregistered = first;
  deregistered.descriptor.sequence = 1;
  deregistered.descriptor.effective_epoch = 4;
  deregistered.descriptor.expiry_epoch = 4;
  ASSERT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          {lifecycle_record(
              lifecycle_action_v2::deregister_identity, first.descriptor, deregistered)},
          semantic_transaction_context_v2{2201, false, nullptr}, source, summary));

  identity successor = make_identity("successor-endpoint");
  successor.service = first.service;
  successor.descriptor.service_public_key = first.service.public_key;
  successor.descriptor.effective_epoch = 4;
  const crypto::hash before = state.state_hash();
  EXPECT_EQ(semantic_status_v2::invalid_admission,
      state.apply_transaction(
          {registration_record(successor), admission_record(successor, source, 4, 2160)},
          semantic_transaction_context_v2{2202, false, nullptr}, source, summary));
  EXPECT_EQ(before, state.state_hash());
  EXPECT_EQ(nullptr,
      state.lifecycle().descriptor_for_epoch(successor.descriptor.identity_id, 4));
}

TEST(epose_semantic_batch_v2, payment_proof_is_coinbase_only)
{
  contexts source;
  semantic_state_v2 state = make_state();
  semantic_apply_summary_v2 summary{};
  const envelope_record_v2 proof{
      static_cast<uint8_t>(record_type_v2::service_payment_proof),
      EPOSE_PAYMENT_PROOF_RECORD_VERSION_V2,
      std::string(EPOSE_PAYMENT_PROOF_PAYLOAD_BYTES_V2, '\0')};
  EXPECT_EQ(semantic_status_v2::payment_proof_wrong_carrier,
      state.apply_transaction(
          {proof}, semantic_transaction_context_v2{2880, false, nullptr}, source, summary));
  EXPECT_EQ(0u, summary.payment_proof_records);
}

TEST(epose_semantic_batch_v2, coinbase_accepts_one_context_bound_payment_proof_only)
{
  contexts source;
  semantic_state_v2 state = make_state();
  crypto::secret_key transaction_secret{};
  const service_payment_context_v2 payment = payment_context(transaction_secret);
  scoped_payment_proof_v2 proof{};
  ASSERT_EQ(reward_status_v2::accepted,
      generate_scoped_payment_proof_v2(payment, transaction_secret, proof));
  envelope_record_v2 record{};
  ASSERT_EQ(record_codec_status_v2::accepted,
      encode_payment_proof_record_v2(proof, payment, record));
  semantic_apply_summary_v2 summary{};
  ASSERT_EQ(semantic_status_v2::accepted,
      state.apply_transaction(
          {record}, semantic_transaction_context_v2{2880, true, &payment}, source, summary));
  EXPECT_EQ(1u, summary.payment_proof_records);

  const crypto::hash before = state.state_hash();
  EXPECT_EQ(semantic_status_v2::duplicate_payment_proof,
      state.apply_transaction(
          {record, record}, semantic_transaction_context_v2{2880, true, &payment}, source, summary));
  EXPECT_EQ(before, state.state_hash());
  EXPECT_EQ(0u, summary.payment_proof_records);
}
