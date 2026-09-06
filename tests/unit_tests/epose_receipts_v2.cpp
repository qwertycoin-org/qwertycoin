// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "epose/service_receipt_v2.h"

namespace
{
  using namespace qwertycoin::epose;

  struct key_pair
  {
    crypto::public_key public_key{};
    crypto::secret_key secret_key{};
  };

  crypto::hash hash_text(const std::string &text)
  {
    return crypto::cn_fast_hash(text.data(), text.size());
  }

  key_pair make_key_pair()
  {
    key_pair out;
    crypto::generate_keys(out.public_key, out.secret_key);
    return out;
  }

  receipt_context_v2 make_context()
  {
    return {cryptonote::TESTNET, hash_text("qwc-v2-genesis"), hash_text("parameter-set")};
  }

  authenticated_service_receipt_v2 make_receipt(key_pair &subject, key_pair &verifier)
  {
    authenticated_service_receipt_v2 receipt{};
    receipt.challenge.epoch = 3;
    receipt.challenge.round = 1;
    receipt.challenge.snapshot_hash = hash_text("snapshot");
    receipt.challenge.anchor_hash = hash_text("anchor");
    receipt.challenge.subject_public_key = subject.public_key;
    receipt.challenge.verifier_public_key = verifier.public_key;
    receipt.challenge.endpoint_descriptor_hash = hash_text("endpoint-descriptor");
    receipt.challenge.nonce = hash_text("fresh-nonce");
    receipt.challenge.requested_object_hash = hash_text("canonical-object");
    receipt.response_object_hash = receipt.challenge.requested_object_hash;
    sign_subject_response_v2(receipt, subject.secret_key, make_context());
    sign_verifier_receipt_v2(receipt, verifier.secret_key, make_context());
    return receipt;
  }

  void resign(authenticated_service_receipt_v2 &receipt, const key_pair &subject, const key_pair &verifier)
  {
    sign_subject_response_v2(receipt, subject.secret_key, make_context());
    sign_verifier_receipt_v2(receipt, verifier.secret_key, make_context());
  }

  frozen_member_v2 make_member(const key_pair &keys, uint64_t sequence)
  {
    frozen_member_v2 member{};
    member.service_public_key = keys.public_key;
    member.descriptor_hash = hash_text("descriptor:" + std::to_string(sequence));
    member.reward_binding_hash = hash_text("reward:" + std::to_string(sequence));
    member.endpoint_descriptor_hash = hash_text("endpoint:" + std::to_string(sequence));
    member.sequence = sequence;
    return member;
  }
}

TEST(epose_receipts_v2, valid_two_party_receipt_converts_to_prevalidated_slot)
{
  key_pair subject = make_key_pair();
  key_pair verifier = make_key_pair();
  const auto receipt = make_receipt(subject, verifier);
  ASSERT_TRUE(validate_authenticated_service_receipt_v2(receipt, make_context()));
  prevalidated_receipt_slot_v2 slot{};
  ASSERT_TRUE(make_prevalidated_receipt_slot_v2(receipt, make_context(), slot));
  EXPECT_EQ(receipt.challenge.epoch, slot.epoch);
  EXPECT_EQ(receipt.challenge.round, slot.round);
  EXPECT_EQ(receipt.challenge.subject_public_key, slot.subject_public_key);
  EXPECT_EQ(receipt.challenge.verifier_public_key, slot.verifier_public_key);
  EXPECT_NE(crypto::null_hash, slot.receipt_hash);
}

TEST(epose_receipts_v2, verifier_cannot_fabricate_missing_subject_participation)
{
  key_pair subject = make_key_pair();
  key_pair verifier = make_key_pair();
  auto receipt = make_receipt(subject, verifier);
  std::memset(&receipt.subject_signature, 0, sizeof(receipt.subject_signature));
  sign_verifier_receipt_v2(receipt, verifier.secret_key, make_context());
  EXPECT_FALSE(validate_authenticated_service_receipt_v2(receipt, make_context()));
}

TEST(epose_receipts_v2, signatures_bind_network_genesis_and_parameter_set)
{
  key_pair subject = make_key_pair();
  key_pair verifier = make_key_pair();
  const auto receipt = make_receipt(subject, verifier);
  auto context = make_context();
  context.nettype = cryptonote::STAGENET;
  EXPECT_FALSE(validate_authenticated_service_receipt_v2(receipt, context));
  context = make_context();
  context.genesis_hash = hash_text("other-genesis");
  EXPECT_FALSE(validate_authenticated_service_receipt_v2(receipt, context));
  context = make_context();
  context.parameter_set_hash = hash_text("other-parameters");
  EXPECT_FALSE(validate_authenticated_service_receipt_v2(receipt, context));
}

TEST(epose_receipts_v2, subject_signature_binds_every_challenge_dimension)
{
  key_pair subject = make_key_pair();
  key_pair verifier = make_key_pair();
  const auto original = make_receipt(subject, verifier);

  auto changed = original;
  ++changed.challenge.epoch;
  EXPECT_FALSE(validate_authenticated_service_receipt_v2(changed, make_context()));
  changed = original;
  ++changed.challenge.round;
  EXPECT_FALSE(validate_authenticated_service_receipt_v2(changed, make_context()));
  changed = original;
  changed.challenge.anchor_hash = hash_text("other-anchor");
  EXPECT_FALSE(validate_authenticated_service_receipt_v2(changed, make_context()));
  changed = original;
  changed.challenge.snapshot_hash = hash_text("other-snapshot");
  EXPECT_FALSE(validate_authenticated_service_receipt_v2(changed, make_context()));
  changed = original;
  changed.challenge.endpoint_descriptor_hash = hash_text("other-endpoint");
  EXPECT_FALSE(validate_authenticated_service_receipt_v2(changed, make_context()));
  changed = original;
  changed.challenge.nonce = hash_text("replayed-nonce-in-other-slot");
  EXPECT_FALSE(validate_authenticated_service_receipt_v2(changed, make_context()));
}

TEST(epose_receipts_v2, wrong_object_is_rejected_even_when_both_parties_sign)
{
  key_pair subject = make_key_pair();
  key_pair verifier = make_key_pair();
  auto receipt = make_receipt(subject, verifier);
  receipt.response_object_hash = hash_text("wrong-object");
  resign(receipt, subject, verifier);
  EXPECT_FALSE(validate_authenticated_service_receipt_v2(receipt, make_context()));
}

TEST(epose_receipts_v2, role_swaps_self_votes_and_unknown_service_kind_fail_closed)
{
  key_pair subject = make_key_pair();
  key_pair verifier = make_key_pair();
  const auto original = make_receipt(subject, verifier);

  auto changed = original;
  changed.challenge.subject_public_key = verifier.public_key;
  changed.challenge.verifier_public_key = subject.public_key;
  EXPECT_FALSE(validate_authenticated_service_receipt_v2(changed, make_context()));
  changed = original;
  changed.challenge.verifier_public_key = subject.public_key;
  resign(changed, subject, subject);
  EXPECT_FALSE(validate_authenticated_service_receipt_v2(changed, make_context()));
  changed = original;
  changed.challenge.service_kind = 255;
  resign(changed, subject, verifier);
  EXPECT_FALSE(validate_authenticated_service_receipt_v2(changed, make_context()));
}

TEST(epose_receipts_v2, null_context_and_challenge_commitments_fail_closed)
{
  key_pair subject = make_key_pair();
  key_pair verifier = make_key_pair();
  const auto original = make_receipt(subject, verifier);
  auto context = make_context();
  context.genesis_hash = crypto::null_hash;
  EXPECT_FALSE(validate_authenticated_service_receipt_v2(original, context));

  auto changed = original;
  changed.challenge.nonce = crypto::null_hash;
  resign(changed, subject, verifier);
  EXPECT_FALSE(validate_authenticated_service_receipt_v2(changed, make_context()));
  changed = original;
  changed.challenge.requested_object_hash = crypto::null_hash;
  changed.response_object_hash = crypto::null_hash;
  resign(changed, subject, verifier);
  EXPECT_FALSE(validate_authenticated_service_receipt_v2(changed, make_context()));
}

TEST(epose_receipts_v2, authenticated_receipt_is_accepted_by_frozen_membership_pipeline)
{
  std::vector<key_pair> keys{make_key_pair(), make_key_pair(), make_key_pair(), make_key_pair()};
  membership_pipeline_v2 pipeline{
      cryptonote::TESTNET,
      make_context().genesis_hash,
      make_context().parameter_set_hash,
      epoch_timing_v2{1440, 720, 60},
      committee_policy_v2{2, 2, 1, 1, static_cast<uint8_t>(service_kind_v2::canonical_object)}};
  std::vector<frozen_member_v2> members;
  for (size_t index = 0; index < keys.size(); ++index)
  {
    members.push_back(make_member(keys[index], index + 1));
    admission_lease_v2 lease{};
    lease.member = members.back();
    lease.target_epoch = 3;
    lease.lease_hash = hash_text("lease:" + std::to_string(index));
    ASSERT_EQ(pipeline_status_v2::accepted, pipeline.apply_prevalidated_admission(lease, 2000 + index));
  }
  const crypto::hash anchor = hash_text("anchor");
  ASSERT_EQ(pipeline_status_v2::accepted, pipeline.freeze_membership(3, 2100, anchor));
  const auto selected = pipeline.committee(3, 0, keys[0].public_key);
  ASSERT_EQ(2u, selected.size());
  const auto verifier = std::find_if(keys.begin(), keys.end(), [&](const key_pair &candidate) {
    return candidate.public_key == selected.front().verifier_public_key;
  });
  ASSERT_NE(keys.end(), verifier);

  auto receipt = make_receipt(keys[0], *verifier);
  receipt.challenge.round = 0;
  receipt.challenge.snapshot_hash = pipeline.snapshot(3)->snapshot_hash;
  receipt.challenge.anchor_hash = anchor;
  receipt.challenge.endpoint_descriptor_hash = members[0].endpoint_descriptor_hash;
  resign(receipt, keys[0], *verifier);
  prevalidated_receipt_slot_v2 slot{};
  ASSERT_TRUE(make_prevalidated_receipt_slot_v2(receipt, make_context(), slot));
  EXPECT_EQ(pipeline_status_v2::accepted, pipeline.apply_prevalidated_receipt(slot, 2200, true));
}
