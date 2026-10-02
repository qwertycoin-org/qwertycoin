// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <map>

#include "epose/block_transition_v2.h"
#include "epose/record_codec_v2.h"
#include "epose/relay_pool_v2.h"

namespace
{
  using namespace qwertycoin::epose;

  crypto::hash hash_text(const std::string &text)
  {
    return crypto::cn_fast_hash(text.data(), text.size());
  }

  struct key_pair
  {
    crypto::public_key public_key{};
    crypto::secret_key secret_key{};
  };

  key_pair keys()
  {
    key_pair out{};
    crypto::generate_keys(out.public_key, out.secret_key);
    return out;
  }

  class contexts final : public canonical_context_source_v2
  {
  public:
    std::map<uint64_t, crypto::hash> blocks;
    std::map<std::pair<uint64_t, uint64_t>, crypto::hash> round_anchors;

    bool block_hash(uint64_t height, crypto::hash &hash) const override
    {
      const auto found = blocks.find(height);
      if (found == blocks.end())
        return false;
      hash = found->second;
      return hash != crypto::null_hash;
    }

    bool round_anchor(
        uint64_t epoch, uint64_t round, crypto::hash &hash) const override
    {
      const auto found = round_anchors.find({epoch, round});
      if (found == round_anchors.end())
        return false;
      hash = found->second;
      return hash != crypto::null_hash;
    }
  };

  const crypto::hash genesis = hash_text("qwc-hf17-v2-genesis");
  const crypto::hash parameters = hash_text("qwc-hf17-v2-parameters");
  const epoch_timing_v2 timing{0, 720, 60};
  const admission_policy_v2 admission_policy{admission_work_algorithm_v2::randomx, 1};
  const committee_policy_v2 committee_policy{1, 1, 1, 1, 1, {0}};

  block_transition_limits_v2 limits()
  {
    block_transition_limits_v2 out{};
    out.max_envelopes_per_transaction = 1;
    out.envelope.max_envelope_bytes = 4096;
    out.envelope.max_records = 8;
    out.envelope.max_record_payload_bytes = 2048;
    out.envelope.max_signature_verifications = 16;
    out.envelope.max_admission_verifications = 4;
    out.envelope.supported_record_versions = {0, 1, 1, 1, 1, 1};
    out.block = {16384, 32, 32, 8};
    out.max_recent_undo_blocks = 4;
    return out;
  }

  block_transition_v2 transition(const block_transition_limits_v2 &bounded = limits())
  {
    return {cryptonote::TESTNET, genesis, parameters, timing,
        admission_policy, committee_policy, bounded};
  }

  cryptonote::transaction transaction_with(
      const std::vector<envelope_record_v2> &records)
  {
    cryptonote::transaction tx{};
    if (!records.empty())
    {
      envelope_budget_v2 budget{};
      EXPECT_EQ(envelope_status_v2::accepted,
          append_transaction_envelope_v2(records, HF_VERSION_QWC_EPOSE, 1,
              limits().envelope, tx.extra, budget));
    }
    return tx;
  }

  block_transition_status_v2 apply_empty(
      block_transition_v2 &state,
      contexts &source,
      uint64_t height,
      uint8_t major_version = HF_VERSION_QWC_EPOSE)
  {
    cryptonote::transaction miner{};
    const crypto::hash block_hash = height == 0
        ? genesis : hash_text("block-" + std::to_string(height));
    source.blocks[height] = block_hash;
    block_apply_summary_v2 summary{};
    return state.apply_block(major_version, height, block_hash,
        {{&miner, true, nullptr}}, source, summary);
  }

  struct enrollment
  {
    key_pair service = keys();
    key_pair operator_key = keys();
    key_pair view = keys();
    key_pair spend = keys();
    identity_descriptor_v2 descriptor{};
  };

  enrollment make_enrollment()
  {
    enrollment out{};
    out.descriptor.identity_id = derive_identity_id_v2(
        cryptonote::TESTNET, genesis, parameters, out.operator_key.public_key);
    out.descriptor.service_public_key = out.service.public_key;
    out.descriptor.operator_authorization_public_key = out.operator_key.public_key;
    out.descriptor.reward_address.m_view_public_key = out.view.public_key;
    out.descriptor.reward_address.m_spend_public_key = out.spend.public_key;
    out.descriptor.endpoint_descriptor_hash = hash_text("endpoint");
    out.descriptor.effective_epoch = 1;
    out.descriptor.expiry_epoch = 3;
    return out;
  }

  envelope_record_v2 lifecycle_record(const enrollment &value)
  {
    lifecycle_record_v2 lifecycle{};
    lifecycle.next_descriptor = value.descriptor;
    EXPECT_TRUE(sign_lifecycle_record_v2(
        cryptonote::TESTNET, genesis, parameters, lifecycle,
        value.operator_key.secret_key, value.service.secret_key));
    envelope_record_v2 record{};
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_lifecycle_record_v2(
            lifecycle, cryptonote::TESTNET, genesis, parameters, record));
    return record;
  }

  envelope_record_v2 admission_record(const enrollment &value)
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
    lease.target_epoch = 1;
    lease.work_algorithm = static_cast<uint8_t>(admission_policy.algorithm);
    lease.leading_zero_bits = admission_policy.leading_zero_bits;
    lease.admission_context_height = 0;
    lease.admission_context_hash = genesis;
    const admission_context_v2 context{
        cryptonote::TESTNET, genesis, parameters, 0, genesis};
    do
    {
      lease.work_hash = calculate_admission_work_v2(lease, context);
      ++lease.nonce;
    } while (!admission_work_meets_target_v2(
        lease.work_hash, admission_policy.leading_zero_bits));
    --lease.nonce;
    lease.lease_hash = calculate_admission_lease_hash_v2(lease, context);
    envelope_record_v2 record{};
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_admission_lease_record_v2(lease, context, admission_policy, record));
    return record;
  }

  const epoch_timing_v2 fast_timing{0, 18, 3};
  const committee_policy_v2 fast_committee{
      9, 6, 3, 2, 1, {0, 5, 10}, 18};

  block_transition_limits_v2 fast_limits()
  {
    block_transition_limits_v2 out = limits();
    out.block = {1048576, 512, 1024, 64};
    out.max_recent_undo_blocks = 128;
    return out;
  }

  admission_lease_v2 fast_admission(
      const enrollment &value,
      uint64_t target_epoch,
      uint64_t context_height,
      const crypto::hash &context_hash)
  {
    admission_lease_v2 lease{};
    lease.member.service_public_key = value.descriptor.service_public_key;
    lease.member.identity_id = value.descriptor.identity_id;
    lease.member.operator_authorization_public_key =
        value.descriptor.operator_authorization_public_key;
    lease.member.descriptor_hash = hash_identity_descriptor_v2(
        cryptonote::TESTNET, genesis, parameters, value.descriptor);
    lease.member.reward_binding_hash = hash_reward_binding_v2(
        cryptonote::TESTNET, genesis, parameters,
        value.descriptor.reward_address);
    lease.member.endpoint_descriptor_hash =
        value.descriptor.endpoint_descriptor_hash;
    lease.target_epoch = target_epoch;
    lease.work_algorithm = static_cast<uint8_t>(admission_policy.algorithm);
    lease.leading_zero_bits = admission_policy.leading_zero_bits;
    lease.admission_context_height = context_height;
    lease.admission_context_hash = context_hash;
    const admission_context_v2 context{
        cryptonote::TESTNET, genesis, parameters,
        context_height, context_hash};
    do
    {
      lease.work_hash = calculate_admission_work_v2(lease, context);
      ++lease.nonce;
    } while (!admission_work_meets_target_v2(
        lease.work_hash, admission_policy.leading_zero_bits));
    --lease.nonce;
    lease.lease_hash = calculate_admission_lease_hash_v2(lease, context);
    return lease;
  }

  envelope_record_v2 fast_admission_record(
      const enrollment &value,
      uint64_t target_epoch,
      uint64_t context_height,
      const crypto::hash &context_hash)
  {
    const admission_lease_v2 lease = fast_admission(
        value, target_epoch, context_height, context_hash);
    const admission_context_v2 context{
        cryptonote::TESTNET, genesis, parameters,
        context_height, context_hash};
    envelope_record_v2 record{};
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_admission_lease_record_v2(
            lease, context, admission_policy, record));
    return record;
  }

  const enrollment &find_enrollment(
      const std::vector<enrollment> &members,
      const crypto::public_key &service_key)
  {
    const auto found = std::find_if(
        members.begin(), members.end(), [&](const enrollment &member) {
          return member.service.public_key == service_key;
        });
    EXPECT_NE(members.end(), found);
    return found == members.end() ? members.front() : *found;
  }

  envelope_record_v2 fast_receipt_record(
      const semantic_state_v2 &state,
      const enrollment &subject,
      const enrollment &verifier,
      uint64_t epoch,
      uint64_t round,
      const crypto::hash &anchor,
      uint64_t nonce)
  {
    const membership_snapshot_v2 *snapshot =
        state.membership().snapshot(epoch);
    EXPECT_NE(nullptr, snapshot);
    authenticated_service_receipt_v2 receipt{};
    receipt.challenge.service_kind = fast_committee.service_kind;
    receipt.challenge.epoch = epoch;
    receipt.challenge.round = round;
    receipt.challenge.snapshot_hash = snapshot == nullptr
        ? crypto::null_hash : snapshot->snapshot_hash;
    receipt.challenge.anchor_hash = anchor;
    receipt.challenge.subject_public_key = subject.service.public_key;
    receipt.challenge.verifier_public_key = verifier.service.public_key;
    receipt.challenge.endpoint_descriptor_hash =
        subject.descriptor.endpoint_descriptor_hash;
    receipt.challenge.nonce = hash_text("receipt-" + std::to_string(nonce));
    receipt.challenge.requested_object_hash = anchor;
    receipt.response_object_hash = anchor;
    const receipt_context_v2 context{
        cryptonote::TESTNET, genesis, parameters};
    sign_subject_response_v2(receipt, subject.service.secret_key, context);
    sign_verifier_receipt_v2(receipt, verifier.service.secret_key, context);
    envelope_record_v2 record{};
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_service_receipt_record_v2(receipt, context, record));
    return record;
  }

  block_transition_status_v2 apply_record_block(
      block_transition_v2 &state,
      contexts &source,
      uint64_t height,
      const std::vector<envelope_record_v2> &records,
      block_apply_summary_v2 &summary)
  {
    std::vector<cryptonote::transaction> transactions;
    transactions.reserve(records.size());
    for (const envelope_record_v2 &record : records)
      transactions.push_back(transaction_with({record}));
    cryptonote::transaction miner{};
    std::vector<block_transaction_context_v2> block_transactions;
    block_transactions.reserve(transactions.size() + 1);
    block_transactions.push_back({&miner, true, nullptr, nullptr});
    for (const cryptonote::transaction &transaction : transactions)
      block_transactions.push_back(
          {&transaction, false, nullptr, nullptr});
    const crypto::hash block_hash =
        hash_text("fast-block-" + std::to_string(height));
    source.blocks[height] = block_hash;
    return state.apply_block(
        HF_VERSION_QWC_EPOSE, height, block_hash,
        block_transactions, source, summary);
  }
}

TEST(epose_block_transition_v2, hf17_dispatch_starts_at_genesis)
{
  contexts source{};
  cryptonote::transaction miner{};
  const std::vector<block_transaction_context_v2> transactions{{&miner, true, nullptr}};
  block_apply_summary_v2 summary{};

  auto inherited = transition();
  EXPECT_EQ(block_transition_status_v2::inactive_protocol,
      inherited.apply_block(16, 0, genesis, transactions, source, summary));
  auto launch = transition();
  EXPECT_EQ(block_transition_status_v2::accepted,
      launch.apply_block(17, 0, genesis, transactions, source, summary));
  EXPECT_EQ(1u, summary.transactions);
  EXPECT_EQ(crypto::null_hash == launch.state().state_hash(), false);
}

TEST(epose_block_transition_v2, future_hardfork_continues_and_reorgs_same_state)
{
  auto state = transition();
  contexts source{};
  ASSERT_EQ(block_transition_status_v2::accepted,
      apply_empty(state, source, 0, HF_VERSION_QWC_EPOSE));
  ASSERT_EQ(block_transition_status_v2::accepted,
      apply_empty(state, source, 1, HF_VERSION_QWC_EPOSE));
  const crypto::hash at_hf17 = state.state().state_hash();

  ASSERT_EQ(block_transition_status_v2::accepted,
      apply_empty(state, source, 2, HF_VERSION_QWC_EPOSE + 1));
  const crypto::hash at_hf18 = state.state().state_hash();
  // Empty blocks do not enter the semantic commitment.  Equality here proves
  // that the scheduled future block version continued the existing state
  // instead of replacing it with a version-specific empty state.
  EXPECT_EQ(at_hf17, at_hf18);

  ASSERT_EQ(block_transition_status_v2::accepted, state.disconnect_tip(2));
  EXPECT_EQ(at_hf17, state.state().state_hash());
  ASSERT_EQ(block_transition_status_v2::accepted,
      apply_empty(state, source, 2, HF_VERSION_QWC_EPOSE + 1));
  EXPECT_EQ(at_hf18, state.state().state_hash());
}

TEST(epose_block_transition_v2, lifecycle_admission_freeze_and_close_follow_bootstrap_boundaries)
{
  auto state = transition();
  contexts source{};
  ASSERT_EQ(block_transition_status_v2::accepted, apply_empty(state, source, 0));

  const enrollment member = make_enrollment();
  cryptonote::transaction enrollment_tx = transaction_with(
      {lifecycle_record(member), admission_record(member)});
  cryptonote::transaction miner{};
  source.blocks[1] = hash_text("block-1");
  block_apply_summary_v2 summary{};
  ASSERT_EQ(block_transition_status_v2::accepted,
      state.apply_block(17, 1, source.blocks[1],
          {{&miner, true, nullptr}, {&enrollment_tx, false, nullptr}}, source, summary));
  EXPECT_EQ(1u, summary.semantic.lifecycle_records);
  EXPECT_EQ(1u, summary.semantic.admission_records);
  EXPECT_EQ(2u, summary.semantic.verifications.signatures);
  EXPECT_EQ(1u, summary.semantic.verifications.randomx);

  for (uint64_t height = 2; height <= 660; ++height)
    ASSERT_EQ(block_transition_status_v2::accepted, apply_empty(state, source, height));
  const membership_snapshot_v2 *snapshot = state.state().membership().snapshot(1);
  ASSERT_NE(nullptr, snapshot);
  ASSERT_EQ(1u, snapshot->members.size());
  EXPECT_EQ(member.service.public_key, snapshot->members.front().service_public_key);

  for (uint64_t height = 661; height <= 1379; ++height)
    ASSERT_EQ(block_transition_status_v2::accepted, apply_empty(state, source, height));
  const qualification_set_v2 *qualification = state.state().membership().qualification(1);
  ASSERT_NE(nullptr, qualification);
  EXPECT_TRUE(qualification->qualified_nodes.empty());
  EXPECT_EQ(1379u, qualification->closed_height);
}

TEST(epose_block_transition_v2, eighteen_nodes_relay_templates_and_finalize_two_fast_epochs)
{
  block_transition_v2 chain(
      cryptonote::TESTNET, genesis, parameters, fast_timing,
      admission_policy, fast_committee, fast_limits());
  ASSERT_TRUE(chain.valid());
  contexts source{};
  ASSERT_EQ(block_transition_status_v2::accepted,
      apply_empty(chain, source, 0));

  std::vector<enrollment> members;
  members.reserve(18);
  std::vector<envelope_record_v2> enrollment_records;
  enrollment_records.reserve(36);
  for (size_t index = 0; index < 18; ++index)
  {
    members.push_back(make_enrollment());
    members.back().descriptor.endpoint_descriptor_hash =
        hash_text("fast-endpoint-" + std::to_string(index));
    enrollment_records.push_back(lifecycle_record(members.back()));
    enrollment_records.push_back(fast_admission_record(
        members.back(), 1, 0, genesis));
  }
  block_apply_summary_v2 summary{};
  ASSERT_EQ(block_transition_status_v2::accepted,
      apply_record_block(
          chain, source, 1, enrollment_records, summary));
  EXPECT_EQ(18u, summary.semantic.lifecycle_records);
  EXPECT_EQ(18u, summary.semantic.admission_records);

  for (uint64_t height = 2; height <= 17; ++height)
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply_empty(chain, source, height));
  ASSERT_NE(nullptr, chain.state().membership().snapshot(1));
  ASSERT_EQ(18u,
      chain.state().membership().snapshot(1)->members.size());

  relay_record_pool_v2 relay(
      fast_timing, fast_committee, limits().envelope,
      relay_queue_limits_v2{
          2048, 1048576, 512, 1536, 262144, 786432},
      relay_template_limits_v2{
          256, 524288, 64, 192, 65536, 458752},
      0);
  ASSERT_TRUE(relay.valid());

  const auto include_round = [&](const uint64_t epoch,
                                 const uint64_t round,
                                 const uint64_t height,
                                 const crypto::hash &anchor) {
    source.round_anchors[{epoch, round}] = anchor;
    const membership_snapshot_v2 *snapshot =
        chain.state().membership().snapshot(epoch);
    ASSERT_NE(nullptr, snapshot);
    size_t nonce = 0;
    for (const enrollment &subject : members)
    {
      const auto committee = chain.state().membership().committee(
          epoch, round, subject.service.public_key, anchor);
      ASSERT_EQ(9u, committee.size());
      for (const verifier_assignment_v2 &assignment : committee)
      {
        const enrollment &verifier = find_enrollment(
            members, assignment.verifier_public_key);
        const envelope_record_v2 record = fast_receipt_record(
            chain.state(), subject, verifier, epoch, round,
            anchor, epoch * 100000 + round * 1000 + nonce++);
        ASSERT_EQ(relay_record_status_v2::accepted,
            relay.enqueue(record, height));
        EXPECT_EQ(relay_record_status_v2::idempotent_duplicate,
            relay.enqueue(record, height));
      }
    }
    ASSERT_EQ(162u, relay.size());
    std::vector<relay_record_selection_v2> selected;
    ASSERT_EQ(relay_record_status_v2::accepted,
        relay.select_for_template(height, selected));
    ASSERT_EQ(162u, selected.size());
    std::vector<envelope_record_v2> block_records;
    block_records.reserve(selected.size());
    for (const relay_record_selection_v2 &selection : selected)
      block_records.push_back(selection.record);
    block_apply_summary_v2 round_summary{};
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply_record_block(
            chain, source, height, block_records, round_summary));
    EXPECT_EQ(162u, round_summary.semantic.receipt_records);
    relay.erase_confirmed_records(block_records, height);
    EXPECT_EQ(0u, relay.size());
    EXPECT_EQ(0u, relay.bytes());
  };

  const auto check_qualification = [&](const uint64_t epoch) {
    const qualification_set_v2 *qualification =
        chain.state().membership().qualification(epoch);
    ASSERT_NE(nullptr, qualification);
    EXPECT_EQ(18u, qualification->qualified_nodes.size());
    for (const enrollment &member : members)
    {
      const std::vector<size_t> coverage =
          chain.state().membership().receipt_coverage(
              epoch, member.service.public_key);
      ASSERT_EQ(3u, coverage.size());
      EXPECT_EQ(9u, coverage[0]);
      EXPECT_EQ(9u, coverage[1]);
      EXPECT_EQ(9u, coverage[2]);
    }
  };

  include_round(1, 0, 18,
      chain.state().membership().snapshot(1)->anchor_hash);

  // Provision epoch 2 from the real epoch-1 context block while round 0 is
  // active; this preserves the production admission and cutoff path.
  std::vector<envelope_record_v2> epoch_two_admissions;
  epoch_two_admissions.reserve(members.size());
  for (const enrollment &member : members)
    epoch_two_admissions.push_back(fast_admission_record(
        member, 2, 18, source.blocks.at(18)));
  ASSERT_EQ(block_transition_status_v2::accepted,
      apply_record_block(
          chain, source, 19, epoch_two_admissions, summary));
  for (uint64_t height = 20; height <= 23; ++height)
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply_empty(chain, source, height));
  include_round(1, 1, 24, source.blocks.at(23));
  for (uint64_t height = 25; height <= 28; ++height)
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply_empty(chain, source, height));
  include_round(1, 2, 29, source.blocks.at(28));
  for (uint64_t height = 30; height <= 32; ++height)
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply_empty(chain, source, height));
  check_qualification(1);

  for (uint64_t height = 33; height <= 35; ++height)
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply_empty(chain, source, height));
  ASSERT_NE(nullptr, chain.state().membership().snapshot(2));
  ASSERT_EQ(18u,
      chain.state().membership().snapshot(2)->members.size());
  include_round(2, 0, 36,
      chain.state().membership().snapshot(2)->anchor_hash);
  for (uint64_t height = 37; height <= 41; ++height)
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply_empty(chain, source, height));
  include_round(2, 1, 42, source.blocks.at(41));
  for (uint64_t height = 43; height <= 46; ++height)
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply_empty(chain, source, height));
  include_round(2, 2, 47, source.blocks.at(46));
  for (uint64_t height = 48; height <= 50; ++height)
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply_empty(chain, source, height));
  check_qualification(2);

  const relay_pool_diagnostics_v2 diagnostics = relay.diagnostics();
  EXPECT_EQ(972u, diagnostics.exact_duplicates);
  EXPECT_EQ(972u, diagnostics.template_selected_receipts);
  EXPECT_EQ(972u, diagnostics.canonical_receipts);
  EXPECT_EQ(0u, diagnostics.capacity_rejections);
  EXPECT_EQ(0u, diagnostics.queue_items);
}

TEST(epose_block_transition_v2, later_failure_rolls_back_state_and_height)
{
  auto state = transition();
  contexts source{};
  ASSERT_EQ(block_transition_status_v2::accepted, apply_empty(state, source, 0));
  const crypto::hash before = state.state().state_hash();

  const enrollment member = make_enrollment();
  envelope_record_v2 invalid = lifecycle_record(member);
  invalid.payload.pop_back();
  cryptonote::transaction miner{};
  cryptonote::transaction tx = transaction_with({lifecycle_record(member), invalid});
  source.blocks[1] = hash_text("block-1");
  block_apply_summary_v2 summary{};
  EXPECT_EQ(block_transition_status_v2::invalid_semantics,
      state.apply_block(17, 1, source.blocks[1],
          {{&miner, true, nullptr}, {&tx, false, nullptr}}, source, summary));
  EXPECT_EQ(before, state.state().state_hash());

  EXPECT_EQ(block_transition_status_v2::accepted, apply_empty(state, source, 1));
}

TEST(epose_block_transition_v2, cumulative_budget_rejects_before_state_commit)
{
  block_transition_limits_v2 bounded = limits();
  bounded.block.max_signature_verifications = 2;
  auto state = transition(bounded);
  contexts source{};
  ASSERT_EQ(block_transition_status_v2::accepted, apply_empty(state, source, 0));
  const crypto::hash before = state.state().state_hash();

  const enrollment first = make_enrollment();
  const enrollment second = make_enrollment();
  cryptonote::transaction miner{};
  cryptonote::transaction first_tx = transaction_with({lifecycle_record(first)});
  cryptonote::transaction second_tx = transaction_with({lifecycle_record(second)});
  source.blocks[1] = hash_text("block-1");
  block_apply_summary_v2 summary{};
  EXPECT_EQ(block_transition_status_v2::resource_limit_exceeded,
      state.apply_block(17, 1, source.blocks[1],
          {{&miner, true, nullptr}, {&first_tx, false, nullptr},
              {&second_tx, false, nullptr}}, source, summary));
  EXPECT_EQ(before, state.state().state_hash());
}

TEST(epose_block_transition_v2, bounded_disconnect_restores_state_and_requires_deep_replay)
{
  block_transition_limits_v2 bounded = limits();
  bounded.max_recent_undo_blocks = 2;
  auto state = transition(bounded);
  contexts source{};
  ASSERT_EQ(block_transition_status_v2::accepted, apply_empty(state, source, 0));
  ASSERT_EQ(block_transition_status_v2::accepted, apply_empty(state, source, 1));
  const crypto::hash at_one = state.state().state_hash();
  ASSERT_EQ(block_transition_status_v2::accepted, apply_empty(state, source, 2));
  ASSERT_EQ(block_transition_status_v2::accepted, apply_empty(state, source, 3));

  EXPECT_EQ(block_transition_status_v2::accepted, state.disconnect_tip(3));
  EXPECT_EQ(block_transition_status_v2::accepted, state.disconnect_tip(2));
  EXPECT_EQ(at_one, state.state().state_hash());
  EXPECT_EQ(block_transition_status_v2::deep_replay_required, state.disconnect_tip(1));
  EXPECT_EQ(block_transition_status_v2::nonsequential_height, state.disconnect_tip(0));
}
