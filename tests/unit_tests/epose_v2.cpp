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
    member.descriptor_hash = hash_text("descriptor:" + std::to_string(id));
    member.reward_binding_hash = hash_text("reward:" + std::to_string(id));
    member.endpoint_descriptor_hash = hash_text("endpoint:" + std::to_string(id));
    member.sequence = id;
    return member;
  }

  admission_lease_v2 make_lease(const frozen_member_v2 &member, uint64_t epoch, const std::string &suffix = "")
  {
    admission_lease_v2 lease{};
    lease.member = member;
    lease.target_epoch = epoch;
    lease.lease_hash = hash_text("lease:" + std::to_string(epoch) + ":" + std::to_string(member.sequence) + suffix);
    return lease;
  }

  membership_pipeline_v2 make_pipeline(committee_policy_v2 policy = {2, 2, 1, 1, 1, {0}})
  {
    return membership_pipeline_v2{
        cryptonote::TESTNET,
        hash_text("qwc-v2-genesis"),
        hash_text("parameter-set"),
        epoch_timing_v2{1440, 720, 60},
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
          pipeline.apply_prevalidated_admission(make_lease(members.back(), target_epoch), 2000 + i));
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
          pipeline.apply_prevalidated_admission(make_lease(members.back().member, 3), 2000 + i));
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
  epoch_timing_v2 timing{1440, 720, 60};
  ASSERT_TRUE(timing.valid());
  uint64_t value = 0;
  ASSERT_TRUE(timing.first_service_epoch(value));
  EXPECT_EQ(3u, value);
  ASSERT_TRUE(timing.first_payout_height(value));
  EXPECT_EQ(2880u, value);
  ASSERT_TRUE(timing.enrollment_cutoff(3, value));
  EXPECT_EQ(2099u, value);
  ASSERT_TRUE(timing.committee_anchor(3, value));
  EXPECT_EQ(2100u, value);
  ASSERT_TRUE(timing.evidence_deadline(3, value));
  EXPECT_EQ(2819u, value);

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
      committee_policy_v2{2, 2, 1, 1, 1, {0}}};
  EXPECT_FALSE(null_genesis.valid());
  EXPECT_EQ(pipeline_status_v2::invalid_configuration,
      null_genesis.apply_prevalidated_admission(make_lease(make_member(1), 3), 2000));

  membership_pipeline_v2 invalid_policy{
      cryptonote::TESTNET,
      hash_text("qwc-v2-genesis"),
      hash_text("parameter-set"),
      epoch_timing_v2{1440, 720, 60},
      committee_policy_v2{2, 0, 1, 1, 1, {0}}};
  EXPECT_FALSE(invalid_policy.valid());

  auto pipeline = make_pipeline();
  EXPECT_EQ(pipeline_status_v2::invalid_epoch,
      pipeline.apply_prevalidated_admission(make_lease(make_member(2), 2), 1500));
  EXPECT_EQ(pipeline_status_v2::invalid_epoch,
      pipeline.freeze_membership(2, 1380, hash_text("pre-service-anchor")));
}

TEST(epose_v2, admission_cutoff_is_inclusive_and_post_seed_grinding_is_rejected)
{
  auto pipeline = make_pipeline();
  const frozen_member_v2 before = make_member(1);
  const frozen_member_v2 after = make_member(2);
  EXPECT_EQ(pipeline_status_v2::accepted,
      pipeline.apply_prevalidated_admission(make_lease(before, 3), 2099));
  EXPECT_EQ(pipeline_status_v2::too_late,
      pipeline.apply_prevalidated_admission(make_lease(after, 3), 2100));
  EXPECT_EQ(pipeline_status_v2::accepted,
      pipeline.freeze_membership(3, 2100, hash_text("anchor")));
  ASSERT_NE(nullptr, pipeline.snapshot(3));
  ASSERT_EQ(1u, pipeline.snapshot(3)->members.size());
  EXPECT_TRUE(key_equal(before.service_public_key, pipeline.snapshot(3)->members.front().service_public_key));
  EXPECT_EQ(pipeline_status_v2::too_late,
      pipeline.apply_prevalidated_admission(make_lease(after, 3), 2090));
}

TEST(epose_v2, snapshot_is_canonical_across_admission_arrival_order)
{
  auto first = make_pipeline();
  auto second = make_pipeline();
  std::vector<frozen_member_v2> members{make_member(1), make_member(2), make_member(3), make_member(4)};
  for (size_t i = 0; i < members.size(); ++i)
    ASSERT_EQ(pipeline_status_v2::accepted, first.apply_prevalidated_admission(make_lease(members[i], 3), 2000 + i));
  for (size_t i = members.size(); i > 0; --i)
    ASSERT_EQ(pipeline_status_v2::accepted, second.apply_prevalidated_admission(make_lease(members[i - 1], 3), 2000 + i - 1));
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
  EXPECT_EQ(pipeline_status_v2::accepted, pipeline.apply_prevalidated_admission(lease, 2000));
  EXPECT_EQ(pipeline_status_v2::idempotent_duplicate, pipeline.apply_prevalidated_admission(lease, 2000));
  EXPECT_EQ(pipeline_status_v2::conflicting_record,
      pipeline.apply_prevalidated_admission(make_lease(member, 3, ":conflict"), 2000));
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
      pipeline.apply_prevalidated_admission(make_lease(late.member, 3), 2090));
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
        parent.apply_prevalidated_admission(make_lease(members.back(), 3), 2000 + i));
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
