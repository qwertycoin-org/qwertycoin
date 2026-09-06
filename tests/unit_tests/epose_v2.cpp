// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "epose/membership_v2.h"
#include "epose/lifecycle_v2.h"
#include "epose/record_codec_v2.h"
#include "epose/service_receipt_v2.h"

namespace
{
  using namespace qwertycoin::epose;

  crypto::hash hash_text(const std::string &text)
  {
    return crypto::cn_fast_hash(text.data(), text.size());
  }

  frozen_member_v2 make_member(uint64_t id)
  {
    frozen_member_v2 member{};
    crypto::secret_key secret{};
    crypto::generate_keys(member.service_public_key, secret);
    crypto::public_key operator_key{};
    crypto::generate_keys(operator_key, secret);
    member.operator_authorization_public_key = operator_key;
    member.identity_id = derive_identity_id_v2(cryptonote::TESTNET, hash_text("qwc-v2-genesis"),
        hash_text("parameter-set"), member.operator_authorization_public_key);
    member.descriptor_hash = hash_text("descriptor:" + std::to_string(id));
    member.reward_binding_hash = hash_text("reward:" + std::to_string(id));
    member.endpoint_descriptor_hash = hash_text("endpoint:" + std::to_string(id));
    member.sequence = id;
    return member;
  }

  admission_context_v2 admission_context()
  {
    return {cryptonote::TESTNET, hash_text("qwc-v2-genesis"), hash_text("parameter-set"),
        1440, hash_text("admission-context")};
  }

  admission_policy_v2 admission_policy()
  {
    return {admission_work_algorithm_v2::randomx, 1};
  }

  admission_lease_v2 make_lease_for(
      const frozen_member_v2 &member,
      uint64_t epoch,
      const admission_context_v2 &context,
      const admission_policy_v2 &policy,
      const std::string &suffix = "")
  {
    admission_lease_v2 lease{};
    lease.member = member;
    lease.target_epoch = epoch;
    lease.work_algorithm = static_cast<uint8_t>(policy.algorithm);
    lease.leading_zero_bits = policy.leading_zero_bits;
    lease.admission_context_height = context.height;
    lease.admission_context_hash = context.block_hash;
    lease.nonce = suffix.empty() ? 0 : 1000;
    do
    {
      lease.work_hash = calculate_admission_work_v2(lease, context);
      ++lease.nonce;
    } while (!admission_work_meets_target_v2(lease.work_hash, policy.leading_zero_bits));
    --lease.nonce;
    lease.lease_hash = calculate_admission_lease_hash_v2(lease, context);
    return lease;
  }

  admission_lease_v2 make_lease(const frozen_member_v2 &member, uint64_t epoch, const std::string &suffix = "")
  {
    return make_lease_for(member, epoch, admission_context(), admission_policy(), suffix);
  }

  pipeline_status_v2 apply_lease(
      membership_pipeline_v2 &pipeline,
      const admission_lease_v2 &lease,
      uint64_t inclusion_height)
  {
    return pipeline.apply_admission(lease, admission_context(), inclusion_height);
  }

  membership_pipeline_v2 make_pipeline(committee_policy_v2 policy = {2, 2, 1, 1, 1, {0}})
  {
    return membership_pipeline_v2{
        cryptonote::TESTNET,
        hash_text("qwc-v2-genesis"),
        hash_text("parameter-set"),
        epoch_timing_v2{1440, 720, 60},
        admission_policy(),
        policy};
  }

  std::vector<frozen_member_v2> admit_and_freeze(
      membership_pipeline_v2 &pipeline,
      size_t count,
      uint64_t target_epoch = 3)
  {
    std::vector<frozen_member_v2> members;
    for (size_t i = 0; i < count; ++i)
    {
      members.push_back(make_member(i + 1));
      EXPECT_EQ(pipeline_status_v2::accepted,
          apply_lease(pipeline, make_lease(members.back(), target_epoch), 2000 + i));
    }
    EXPECT_EQ(pipeline_status_v2::accepted,
        pipeline.freeze_membership(target_epoch, 2100, hash_text("committee-anchor")));
    return members;
  }

  struct keyed_member
  {
    frozen_member_v2 member{};
    crypto::secret_key secret{};
  };

  keyed_member make_keyed_member(uint64_t id)
  {
    keyed_member out{};
    crypto::generate_keys(out.member.service_public_key, out.secret);
    crypto::secret_key operator_secret{};
    crypto::generate_keys(out.member.operator_authorization_public_key, operator_secret);
    out.member.identity_id = derive_identity_id_v2(cryptonote::TESTNET, hash_text("qwc-v2-genesis"),
        hash_text("parameter-set"), out.member.operator_authorization_public_key);
    out.member.descriptor_hash = hash_text("descriptor:" + std::to_string(id));
    out.member.reward_binding_hash = hash_text("reward:" + std::to_string(id));
    out.member.endpoint_descriptor_hash = hash_text("endpoint:" + std::to_string(id));
    out.member.sequence = id;
    return out;
  }

  std::vector<keyed_member> admit_and_freeze_keyed(membership_pipeline_v2 &pipeline, size_t count)
  {
    std::vector<keyed_member> members;
    for (size_t i = 0; i < count; ++i)
    {
      members.push_back(make_keyed_member(i + 1));
      EXPECT_EQ(pipeline_status_v2::accepted,
          apply_lease(pipeline, make_lease(members.back().member, 3), 2000 + i));
    }
    EXPECT_EQ(pipeline_status_v2::accepted,
        pipeline.freeze_membership(3, 2100, hash_text("committee-anchor")));
    return members;
  }

  receipt_context_v2 receipt_context()
  {
    return {cryptonote::TESTNET, hash_text("qwc-v2-genesis"), hash_text("parameter-set")};
  }

  authenticated_service_receipt_v2 make_receipt(
      uint64_t epoch,
      uint64_t round,
      const keyed_member &subject,
      const keyed_member &verifier,
      const membership_snapshot_v2 &snapshot,
      uint64_t nonce,
      uint8_t service_kind = 1,
      const crypto::hash &round_anchor = crypto::null_hash)
  {
    authenticated_service_receipt_v2 receipt{};
    receipt.challenge.service_kind = service_kind;
    receipt.challenge.epoch = epoch;
    receipt.challenge.round = round;
    receipt.challenge.snapshot_hash = snapshot.snapshot_hash;
    receipt.challenge.anchor_hash = round_anchor == crypto::null_hash ? snapshot.anchor_hash : round_anchor;
    receipt.challenge.subject_public_key = subject.member.service_public_key;
    receipt.challenge.verifier_public_key = verifier.member.service_public_key;
    receipt.challenge.endpoint_descriptor_hash = subject.member.endpoint_descriptor_hash;
    receipt.challenge.nonce = hash_text("receipt:" + std::to_string(nonce));
    receipt.challenge.requested_object_hash = hash_text("object:" + std::to_string(nonce));
    receipt.response_object_hash = receipt.challenge.requested_object_hash;
    sign_subject_response_v2(receipt, subject.secret, receipt_context());
    sign_verifier_receipt_v2(receipt, verifier.secret, receipt_context());
    return receipt;
  }

  const keyed_member &find_keyed_member(
      const std::vector<keyed_member> &members,
      const crypto::public_key &public_key)
  {
    const auto found = std::find_if(members.begin(), members.end(), [&](const keyed_member &member) {
      return member.member.service_public_key == public_key;
    });
    EXPECT_NE(members.end(), found);
    return *found;
  }

  bool key_equal(const crypto::public_key &left, const crypto::public_key &right)
  {
    return std::memcmp(&left, &right, sizeof(left)) == 0;
  }
}

TEST(epose_v2, timing_boundaries_are_checked_and_match_co01)
{
  epoch_timing_v2 timing{0, 720, 60};
  ASSERT_TRUE(timing.valid());
  uint64_t value = 0;
  ASSERT_TRUE(timing.first_service_epoch(value));
  EXPECT_EQ(1u, value);
  ASSERT_TRUE(timing.first_payout_height(value));
  EXPECT_EQ(1440u, value);
  ASSERT_TRUE(timing.enrollment_cutoff(1, value));
  EXPECT_EQ(659u, value);
  ASSERT_TRUE(timing.committee_anchor(1, value));
  EXPECT_EQ(660u, value);
  ASSERT_TRUE(timing.evidence_deadline(1, value));
  EXPECT_EQ(1379u, value);

  epoch_timing_v2 unaligned{1441, 720, 60};
  EXPECT_FALSE(unaligned.valid());
  epoch_timing_v2 overflowing{1440, 720, 60};
  EXPECT_FALSE(overflowing.epoch_end(std::numeric_limits<uint64_t>::max() / 720 + 1, value));
}

TEST(epose_v2, invalid_configuration_and_pre_service_epoch_fail_closed)
{
  membership_pipeline_v2 null_genesis{
      cryptonote::TESTNET,
      crypto::null_hash,
      hash_text("parameter-set"),
      epoch_timing_v2{1440, 720, 60},
      admission_policy(),
      committee_policy_v2{2, 2, 1, 1, 1, {0}}};
  EXPECT_FALSE(null_genesis.valid());
  EXPECT_EQ(pipeline_status_v2::invalid_configuration,
      apply_lease(null_genesis, make_lease(make_member(1), 3), 2000));

  membership_pipeline_v2 invalid_policy{
      cryptonote::TESTNET,
      hash_text("qwc-v2-genesis"),
      hash_text("parameter-set"),
      epoch_timing_v2{1440, 720, 60},
      admission_policy(),
      committee_policy_v2{2, 0, 1, 1, 1, {0}}};
  EXPECT_FALSE(invalid_policy.valid());

  auto selectable_context_policy = admission_policy();
  selectable_context_policy.context_epoch_offset = 2;
  membership_pipeline_v2 invalid_admission_policy{
      cryptonote::TESTNET,
      hash_text("qwc-v2-genesis"),
      hash_text("parameter-set"),
      epoch_timing_v2{1440, 720, 60},
      selectable_context_policy,
      committee_policy_v2{2, 2, 1, 1, 1, {0}}};
  EXPECT_FALSE(invalid_admission_policy.valid());

  auto pipeline = make_pipeline();
  EXPECT_EQ(pipeline_status_v2::invalid_epoch,
      apply_lease(pipeline, make_lease(make_member(2), 2), 1500));
  EXPECT_EQ(pipeline_status_v2::invalid_epoch,
      pipeline.freeze_membership(2, 1380, hash_text("pre-service-anchor")));
}

TEST(epose_v2, admission_cutoff_is_inclusive_and_post_seed_grinding_is_rejected)
{
  auto pipeline = make_pipeline();
  const frozen_member_v2 before = make_member(1);
  const frozen_member_v2 after = make_member(2);
  EXPECT_EQ(pipeline_status_v2::accepted,
      apply_lease(pipeline, make_lease(before, 3), 2099));
  EXPECT_EQ(pipeline_status_v2::too_late,
      apply_lease(pipeline, make_lease(after, 3), 2100));
  EXPECT_EQ(pipeline_status_v2::accepted,
      pipeline.freeze_membership(3, 2100, hash_text("anchor")));
  ASSERT_NE(nullptr, pipeline.snapshot(3));
  ASSERT_EQ(1u, pipeline.snapshot(3)->members.size());
  EXPECT_TRUE(key_equal(before.service_public_key, pipeline.snapshot(3)->members.front().service_public_key));
  EXPECT_EQ(pipeline_status_v2::too_late,
      apply_lease(pipeline, make_lease(after, 3), 2090));
}

TEST(epose_v2, admission_recomputes_randomx_and_binds_every_canonical_context)
{
  const frozen_member_v2 member = make_member(1);
  const admission_lease_v2 original = make_lease(member, 3);
  ASSERT_TRUE(validate_admission_lease_v2(original, admission_context(), admission_policy()));

  auto pipeline = make_pipeline();
  auto changed = original;
  changed.work_hash = hash_text("claimed-prefix-only");
  EXPECT_EQ(pipeline_status_v2::invalid_member, apply_lease(pipeline, changed, 2000));
  changed = original;
  changed.lease_hash = hash_text("caller-selected-lease-hash");
  EXPECT_EQ(pipeline_status_v2::invalid_member, apply_lease(pipeline, changed, 2000));
  changed = original;
  changed.member.endpoint_descriptor_hash = hash_text("redirected-endpoint");
  EXPECT_EQ(pipeline_status_v2::invalid_member, apply_lease(pipeline, changed, 2000));
  changed = original;
  ++changed.target_epoch;
  EXPECT_EQ(pipeline_status_v2::invalid_epoch, apply_lease(pipeline, changed, 2000));

  auto one_key_member = member;
  one_key_member.operator_authorization_public_key = one_key_member.service_public_key;
  one_key_member.identity_id = derive_identity_id_v2(admission_context().nettype,
      admission_context().genesis_hash, admission_context().parameter_set_hash,
      one_key_member.operator_authorization_public_key);
  const admission_lease_v2 one_key = make_lease_for(one_key_member, 3, admission_context(), admission_policy());
  EXPECT_FALSE(validate_admission_lease_v2(one_key, admission_context(), admission_policy()));

  auto other_network = admission_context();
  other_network.nettype = cryptonote::STAGENET;
  auto other_network_member = member;
  other_network_member.identity_id = derive_identity_id_v2(other_network.nettype,
      other_network.genesis_hash, other_network.parameter_set_hash,
      other_network_member.operator_authorization_public_key);
  const admission_lease_v2 recomputed = make_lease_for(other_network_member, 3, other_network, admission_policy());
  EXPECT_TRUE(validate_admission_lease_v2(recomputed, other_network, admission_policy()));
  EXPECT_EQ(pipeline_status_v2::invalid_member,
      pipeline.apply_admission(recomputed, other_network, 2000));

  auto late_context = admission_context();
  late_context.height = 2000;
  late_context.block_hash = hash_text("same-height-context");
  const admission_lease_v2 late = make_lease_for(member, 3, late_context, admission_policy());
  EXPECT_EQ(pipeline_status_v2::invalid_member,
      pipeline.apply_admission(late, late_context, 2000));

  auto selectable_earlier_context = admission_context();
  --selectable_earlier_context.height;
  selectable_earlier_context.block_hash = hash_text("caller-selected-earlier-context");
  const admission_lease_v2 rerolled = make_lease_for(
      member, 3, selectable_earlier_context, admission_policy());
  ASSERT_TRUE(validate_admission_lease_v2(
      rerolled, selectable_earlier_context, admission_policy()));
  EXPECT_EQ(pipeline_status_v2::invalid_epoch,
      pipeline.apply_admission(rerolled, selectable_earlier_context, 2000));
}

TEST(epose_v2, admission_record_codec_is_canonical_validated_and_atomic)
{
  const admission_lease_v2 expected = make_lease(make_member(9), 3);
  envelope_record_v2 record{};
  ASSERT_EQ(record_codec_status_v2::accepted,
      encode_admission_lease_record_v2(expected, admission_context(), admission_policy(), record));
  EXPECT_EQ(static_cast<uint8_t>(record_type_v2::admission_lease), record.type);
  EXPECT_EQ(EPOSE_ADMISSION_LEASE_RECORD_VERSION_V2, record.version);
  EXPECT_EQ(EPOSE_ADMISSION_LEASE_PAYLOAD_BYTES_V2, record.payload.size());

  admission_lease_v2 actual{};
  ASSERT_EQ(record_codec_status_v2::accepted,
      decode_admission_lease_record_v2(record, admission_context(), admission_policy(), actual));
  EXPECT_EQ(expected.lease_hash, actual.lease_hash);
  EXPECT_EQ(expected.work_hash, actual.work_hash);

  auto malformed = record;
  malformed.payload.pop_back();
  actual = expected;
  EXPECT_EQ(record_codec_status_v2::wrong_size,
      decode_admission_lease_record_v2(malformed, admission_context(), admission_policy(), actual));
  EXPECT_EQ(crypto::null_hash, actual.lease_hash);

  malformed = record;
  malformed.payload[200] ^= 1;
  EXPECT_EQ(record_codec_status_v2::invalid_record,
      decode_admission_lease_record_v2(malformed, admission_context(), admission_policy(), actual));
  EXPECT_EQ(crypto::null_hash, actual.lease_hash);
}

TEST(epose_v2, snapshot_is_canonical_across_admission_arrival_order)
{
  auto first = make_pipeline();
  auto second = make_pipeline();
  std::vector<frozen_member_v2> members{make_member(1), make_member(2), make_member(3), make_member(4)};
  for (size_t i = 0; i < members.size(); ++i)
    ASSERT_EQ(pipeline_status_v2::accepted, apply_lease(first, make_lease(members[i], 3), 2000 + i));
  for (size_t i = members.size(); i > 0; --i)
    ASSERT_EQ(pipeline_status_v2::accepted, apply_lease(second, make_lease(members[i - 1], 3), 2000 + i - 1));
  const crypto::hash anchor = hash_text("same-anchor");
  ASSERT_EQ(pipeline_status_v2::accepted, first.freeze_membership(3, 2100, anchor));
  ASSERT_EQ(pipeline_status_v2::accepted, second.freeze_membership(3, 2100, anchor));
  EXPECT_EQ(first.snapshot(3)->snapshot_hash, second.snapshot(3)->snapshot_hash);
  EXPECT_EQ(first.state_hash(), second.state_hash());
}

TEST(epose_v2, duplicate_admission_is_idempotent_but_conflict_is_rejected)
{
  auto pipeline = make_pipeline();
  const frozen_member_v2 member = make_member(1);
  const admission_lease_v2 lease = make_lease(member, 3);
  EXPECT_EQ(pipeline_status_v2::accepted, apply_lease(pipeline, lease, 2000));
  EXPECT_EQ(pipeline_status_v2::idempotent_duplicate, apply_lease(pipeline, lease, 2000));
  EXPECT_EQ(pipeline_status_v2::conflicting_record,
      apply_lease(pipeline, make_lease(member, 3, ":conflict"), 2000));
}

TEST(epose_v2, admission_reserves_service_key_for_one_identity_per_target_epoch)
{
  auto pipeline = make_pipeline({1, 1, 1, 1, 1, {0}});
  keyed_member first = make_keyed_member(1);
  keyed_member second = make_keyed_member(2);
  second.member.service_public_key = first.member.service_public_key;

  const admission_lease_v2 first_lease = make_lease(first.member, 3, "first");
  const admission_lease_v2 second_lease = make_lease(second.member, 3, "second");
  const keyed_member verifier = make_keyed_member(3);
  ASSERT_EQ(pipeline_status_v2::accepted, apply_lease(pipeline, first_lease, 2000));
  EXPECT_EQ(pipeline_status_v2::conflicting_record, apply_lease(pipeline, second_lease, 2001));
  ASSERT_EQ(pipeline_status_v2::accepted,
      apply_lease(pipeline, make_lease(verifier.member, 3), 2002));

  auto reverse = make_pipeline({1, 1, 1, 1, 1, {0}});
  ASSERT_EQ(pipeline_status_v2::accepted, apply_lease(reverse, second_lease, 2000));
  EXPECT_EQ(pipeline_status_v2::conflicting_record, apply_lease(reverse, first_lease, 2001));

  ASSERT_EQ(pipeline_status_v2::accepted,
      pipeline.freeze_membership(3, 2100, hash_text("committee-anchor")));
  const membership_snapshot_v2 *snapshot = pipeline.snapshot(3);
  ASSERT_NE(nullptr, snapshot);
  ASSERT_EQ(2u, snapshot->members.size());
  const auto first_member = std::find_if(
      snapshot->members.begin(), snapshot->members.end(), [&](const frozen_member_v2 &member) {
        return member.identity_id == first.member.identity_id;
      });
  ASSERT_NE(snapshot->members.end(), first_member);
  EXPECT_EQ(first.member.service_public_key, first_member->service_public_key);
  EXPECT_NE(snapshot->members[0].service_public_key, snapshot->members[1].service_public_key);

  const auto selected = pipeline.committee(
      3, 0, first.member.service_public_key, snapshot->anchor_hash);
  ASSERT_EQ(1u, selected.size());
  ASSERT_EQ(verifier.member.service_public_key, selected.front().verifier_public_key);
  ASSERT_EQ(pipeline_status_v2::accepted,
      pipeline.apply_authenticated_receipt(
          make_receipt(3, 0, first, verifier, *snapshot, 1), receipt_context(), 2200,
          snapshot->anchor_hash));
  ASSERT_EQ(pipeline_status_v2::accepted, pipeline.close_qualification(3, 2819));
  const qualification_set_v2 *qualification = pipeline.qualification(3);
  ASSERT_NE(nullptr, qualification);
  ASSERT_EQ(1u, qualification->qualified_nodes.size());
  EXPECT_EQ(first.member.service_public_key, qualification->qualified_nodes.front());
}

TEST(epose_v2, receipt_membership_and_committee_use_only_frozen_snapshot)
{
  auto pipeline = make_pipeline();
  const auto members = admit_and_freeze_keyed(pipeline, 4);
  const auto selected = pipeline.committee(
      3, 0, members[0].member.service_public_key, pipeline.snapshot(3)->anchor_hash);
  ASSERT_EQ(2u, selected.size());
  const auto &verifier = find_keyed_member(members, selected[0].verifier_public_key);

  const keyed_member late = make_keyed_member(99);
  EXPECT_EQ(pipeline_status_v2::too_late,
      apply_lease(pipeline, make_lease(late.member, 3), 2090));
  EXPECT_EQ(pipeline_status_v2::verifier_not_in_snapshot,
      pipeline.apply_authenticated_receipt(
          make_receipt(3, 0, members[0], late, *pipeline.snapshot(3), 1), receipt_context(), 2200,
          pipeline.snapshot(3)->anchor_hash));
  auto unsigned_receipt = make_receipt(3, 0, members[0], verifier, *pipeline.snapshot(3), 2);
  reinterpret_cast<unsigned char *>(&unsigned_receipt.subject_signature)[0] ^= 1;
  EXPECT_EQ(pipeline_status_v2::receipt_not_prevalidated,
      pipeline.apply_authenticated_receipt(
          unsigned_receipt, receipt_context(), 2200, pipeline.snapshot(3)->anchor_hash));
  auto wrong_snapshot = make_receipt(3, 0, members[0], verifier, *pipeline.snapshot(3), 2);
  wrong_snapshot.challenge.snapshot_hash = hash_text("other-snapshot");
  sign_subject_response_v2(wrong_snapshot, members[0].secret, receipt_context());
  sign_verifier_receipt_v2(wrong_snapshot, verifier.secret, receipt_context());
  EXPECT_EQ(pipeline_status_v2::receipt_not_prevalidated,
      pipeline.apply_authenticated_receipt(
          wrong_snapshot, receipt_context(), 2200, pipeline.snapshot(3)->anchor_hash));
  EXPECT_EQ(pipeline_status_v2::accepted,
      pipeline.apply_authenticated_receipt(
          make_receipt(3, 0, members[0], verifier, *pipeline.snapshot(3), 3), receipt_context(), 2200,
          pipeline.snapshot(3)->anchor_hash));
}

TEST(epose_v2, evidence_deadline_is_inclusive_and_late_receipt_is_rejected)
{
  auto pipeline = make_pipeline();
  const auto members = admit_and_freeze_keyed(pipeline, 4);
  const auto selected = pipeline.committee(
      3, 0, members[0].member.service_public_key, pipeline.snapshot(3)->anchor_hash);
  ASSERT_EQ(2u, selected.size());
  const auto &first = find_keyed_member(members, selected[0].verifier_public_key);
  const auto &second = find_keyed_member(members, selected[1].verifier_public_key);
  EXPECT_EQ(pipeline_status_v2::accepted,
      pipeline.apply_authenticated_receipt(
          make_receipt(3, 0, members[0], first, *pipeline.snapshot(3), 1), receipt_context(), 2819,
          pipeline.snapshot(3)->anchor_hash));
  EXPECT_EQ(pipeline_status_v2::too_late,
      pipeline.apply_authenticated_receipt(
          make_receipt(3, 0, members[0], second, *pipeline.snapshot(3), 2), receipt_context(), 2820,
          pipeline.snapshot(3)->anchor_hash));
}

TEST(epose_v2, later_rounds_have_distinct_windows_and_fresh_canonical_anchors)
{
  auto pipeline = make_pipeline({2, 2, 3, 2, 1, {0, 200, 400}});
  const auto members = admit_and_freeze_keyed(pipeline, 4);
  const crypto::hash round_anchor = hash_text("round-one-anchor");
  const auto selected = pipeline.committee(
      3, 1, members[0].member.service_public_key, round_anchor);
  ASSERT_EQ(2u, selected.size());
  const auto &verifier = find_keyed_member(members, selected[0].verifier_public_key);
  const auto receipt = make_receipt(
      3, 1, members[0], verifier, *pipeline.snapshot(3), 1, 1, round_anchor);

  EXPECT_EQ(pipeline_status_v2::invalid_epoch,
      pipeline.apply_authenticated_receipt(receipt, receipt_context(), 2359, round_anchor));
  EXPECT_EQ(pipeline_status_v2::receipt_not_prevalidated,
      pipeline.apply_authenticated_receipt(receipt, receipt_context(), 2360, hash_text("wrong-anchor")));
  EXPECT_EQ(pipeline_status_v2::accepted,
      pipeline.apply_authenticated_receipt(receipt, receipt_context(), 2360, round_anchor));
  EXPECT_EQ(pipeline_status_v2::too_late,
      pipeline.apply_authenticated_receipt(
          make_receipt(3, 1, members[0], verifier, *pipeline.snapshot(3), 2, 1, round_anchor),
          receipt_context(), 2560, round_anchor));
}

TEST(epose_v2, qualification_closes_once_and_uses_fixed_threshold)
{
  auto pipeline = make_pipeline({2, 2, 1, 1, 1, {0}});
  const auto members = admit_and_freeze_keyed(pipeline, 4);
  const auto selected = pipeline.committee(
      3, 0, members[0].member.service_public_key, pipeline.snapshot(3)->anchor_hash);
  ASSERT_EQ(2u, selected.size());
  for (size_t i = 0; i < selected.size(); ++i)
  {
    const auto &verifier = find_keyed_member(members, selected[i].verifier_public_key);
    ASSERT_EQ(pipeline_status_v2::accepted,
        pipeline.apply_authenticated_receipt(
            make_receipt(3, 0, members[0], verifier, *pipeline.snapshot(3), i), receipt_context(), 2200 + i,
            pipeline.snapshot(3)->anchor_hash));
  }
  EXPECT_EQ(pipeline_status_v2::wrong_boundary, pipeline.close_qualification(3, 2818));
  EXPECT_EQ(pipeline_status_v2::accepted, pipeline.close_qualification(3, 2819));
  ASSERT_NE(nullptr, pipeline.qualification(3));
  ASSERT_EQ(1u, pipeline.qualification(3)->qualified_nodes.size());
  EXPECT_TRUE(key_equal(members[0].member.service_public_key, pipeline.qualification(3)->qualified_nodes.front()));
  EXPECT_EQ(pipeline_status_v2::qualification_already_closed, pipeline.close_qualification(3, 2819));
}

TEST(epose_v2, small_network_does_not_shrink_committee_or_threshold)
{
  auto pipeline = make_pipeline({3, 2, 1, 1, 1, {0}});
  const auto members = admit_and_freeze(pipeline, 3);
  EXPECT_TRUE(pipeline.committee(
      3, 0, members[0].service_public_key, pipeline.snapshot(3)->anchor_hash).empty());
  EXPECT_EQ(pipeline_status_v2::accepted, pipeline.close_qualification(3, 2819));
  ASSERT_NE(nullptr, pipeline.qualification(3));
  EXPECT_TRUE(pipeline.qualification(3)->qualified_nodes.empty());
}

TEST(epose_v2, snapshot_anchor_changes_committee_context_and_state_hash)
{
  auto parent = make_pipeline();
  std::vector<frozen_member_v2> members;
  for (size_t i = 0; i < 6; ++i)
  {
    members.push_back(make_member(i + 1));
    ASSERT_EQ(pipeline_status_v2::accepted,
        apply_lease(parent, make_lease(members.back(), 3), 2000 + i));
  }
  auto branch_a = parent;
  auto branch_b = parent;
  ASSERT_EQ(pipeline_status_v2::accepted, branch_a.freeze_membership(3, 2100, hash_text("anchor-a")));
  ASSERT_EQ(pipeline_status_v2::accepted, branch_b.freeze_membership(3, 2100, hash_text("anchor-b")));
  EXPECT_NE(branch_a.snapshot(3)->snapshot_hash, branch_b.snapshot(3)->snapshot_hash);
  EXPECT_NE(branch_a.state_hash(), branch_b.state_hash());

  auto branch_a_replayed = parent;
  ASSERT_EQ(pipeline_status_v2::accepted, branch_a_replayed.freeze_membership(3, 2100, hash_text("anchor-a")));
  EXPECT_EQ(branch_a.snapshot(3)->snapshot_hash, branch_a_replayed.snapshot(3)->snapshot_hash);
  EXPECT_EQ(branch_a.state_hash(), branch_a_replayed.state_hash());
}
