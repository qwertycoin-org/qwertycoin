// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <cstring>
#include <map>

#include "epose/block_transition_v2.h"
#include "epose/record_codec_v2.h"

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

    bool block_hash(uint64_t height, crypto::hash &hash) const override
    {
      const auto found = blocks.find(height);
      if (found == blocks.end())
        return false;
      hash = found->second;
      return hash != crypto::null_hash;
    }

    bool round_anchor(uint64_t, uint64_t, crypto::hash &) const override
    {
      return false;
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
      uint64_t height)
  {
    cryptonote::transaction miner{};
    const crypto::hash block_hash = height == 0
        ? genesis : hash_text("block-" + std::to_string(height));
    source.blocks[height] = block_hash;
    block_apply_summary_v2 summary{};
    return state.apply_block(HF_VERSION_QWC_EPOSE, height, block_hash,
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
}

TEST(epose_block_transition_v2, exact_hf17_dispatch_starts_at_genesis)
{
  contexts source{};
  cryptonote::transaction miner{};
  const std::vector<block_transaction_context_v2> transactions{{&miner, true, nullptr}};
  block_apply_summary_v2 summary{};

  auto inherited = transition();
  EXPECT_EQ(block_transition_status_v2::inactive_protocol,
      inherited.apply_block(16, 0, genesis, transactions, source, summary));
  auto unscheduled = transition();
  EXPECT_EQ(block_transition_status_v2::inactive_protocol,
      unscheduled.apply_block(18, 0, genesis, transactions, source, summary));
  auto launch = transition();
  EXPECT_EQ(block_transition_status_v2::accepted,
      launch.apply_block(17, 0, genesis, transactions, source, summary));
  EXPECT_EQ(1u, summary.transactions);
  EXPECT_EQ(crypto::null_hash == launch.state().state_hash(), false);
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
