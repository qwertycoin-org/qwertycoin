// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <map>

#include "blockchain_db/blockchain_db.h"
#include "epose/replay_v2.h"

namespace
{
  using namespace qwertycoin::epose;

  crypto::hash hash_text(const std::string &text)
  {
    return crypto::cn_fast_hash(text.data(), text.size());
  }

  const crypto::hash genesis = hash_text("qwc-hf17-v2-replay-genesis");
  const crypto::hash parameters = hash_text("qwc-hf17-v2-replay-parameters");
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
    out.max_recent_undo_blocks = 2;
    return out;
  }

  block_transition_v2 transition()
  {
    return {cryptonote::TESTNET, genesis, parameters, timing,
        admission_policy, committee_policy, limits()};
  }

  class replay_source final : public canonical_replay_source_v2
  {
  public:
    std::map<uint64_t, replay_block_v2> blocks;
    std::map<uint64_t, crypto::hash> states;
    std::map<uint64_t, crypto::hash> parameter_hashes;
    std::map<uint64_t, uint8_t> schemas;

    bool replay_block(uint64_t height, replay_block_v2 &block) const override
    {
      const auto found = blocks.find(height);
      if (found == blocks.end())
        return false;
      block = found->second;
      return true;
    }

    bool state_commitment(
        uint64_t height,
        uint8_t &schema_version,
        crypto::hash &block_hash,
        crypto::hash &state_hash,
        crypto::hash &parameter_set_hash) const override
    {
      const auto block = blocks.find(height);
      const auto state = states.find(height);
      const auto params = parameter_hashes.find(height);
      const auto schema = schemas.find(height);
      if (block == blocks.end() || state == states.end()
          || params == parameter_hashes.end() || schema == schemas.end())
        return false;
      schema_version = schema->second;
      block_hash = block->second.block_hash;
      state_hash = state->second;
      parameter_set_hash = params->second;
      return true;
    }

    bool block_hash(uint64_t height, crypto::hash &hash) const override
    {
      const auto found = blocks.find(height);
      if (found == blocks.end())
        return false;
      hash = found->second.block_hash;
      return hash != crypto::null_hash;
    }

    bool round_anchor(uint64_t, uint64_t, crypto::hash &) const override
    {
      return false;
    }
  };

  replay_source make_source(uint64_t last_height)
  {
    replay_source source{};
    auto producer = transition();
    for (uint64_t height = 0; height <= last_height; ++height)
    {
      replay_block_v2 block{};
      block.major_version = HF_VERSION_QWC_EPOSE;
      block.height = height;
      block.block_hash = height == 0
          ? genesis : hash_text("replay-block-" + std::to_string(height));
      block.transactions.resize(1);
      block.transactions.front().coinbase = true;
      source.blocks.emplace(height, block);

      const block_transaction_context_v2 transaction{
          &source.blocks.at(height).transactions.front().transaction, true, nullptr};
      block_apply_summary_v2 summary{};
      EXPECT_EQ(block_transition_status_v2::accepted,
          producer.apply_block(HF_VERSION_QWC_EPOSE, height, block.block_hash,
              {transaction}, source, summary));
      source.states.emplace(height, producer.state().state_hash());
      source.parameter_hashes.emplace(height, parameters);
      source.schemas.emplace(height, cryptonote::EPOSE_STATE_COMMITMENT_SCHEMA_V2);
    }
    return source;
  }
}

TEST(epose_replay_v2, streams_from_genesis_and_verifies_every_commitment)
{
  replay_source source = make_source(3);
  auto rebuilt = transition();
  replay_summary_v2 summary{};
  ASSERT_EQ(replay_status_v2::accepted,
      replay_and_verify_v2(rebuilt, source,
          cryptonote::EPOSE_STATE_COMMITMENT_SCHEMA_V2,
          parameters, 0, 3, summary));
  EXPECT_EQ(0u, summary.first_height);
  EXPECT_EQ(3u, summary.last_height);
  EXPECT_EQ(4u, summary.blocks);
  EXPECT_EQ(4u, summary.transactions);
  EXPECT_EQ(source.states.at(3), rebuilt.state().state_hash());
}

TEST(epose_replay_v2, rejects_missing_or_mismatched_commitments)
{
  replay_source missing = make_source(1);
  missing.states.erase(1);
  auto rebuilt_missing = transition();
  replay_summary_v2 summary{};
  EXPECT_EQ(replay_status_v2::commitment_missing,
      replay_and_verify_v2(rebuilt_missing, missing,
          cryptonote::EPOSE_STATE_COMMITMENT_SCHEMA_V2,
          parameters, 0, 1, summary));

  replay_source mismatched = make_source(1);
  mismatched.states[1] = hash_text("wrong-state");
  auto rebuilt_mismatched = transition();
  EXPECT_EQ(replay_status_v2::commitment_mismatch,
      replay_and_verify_v2(rebuilt_mismatched, mismatched,
          cryptonote::EPOSE_STATE_COMMITMENT_SCHEMA_V2,
          parameters, 0, 1, summary));

  replay_source wrong_parameters = make_source(1);
  wrong_parameters.parameter_hashes[1] = hash_text("wrong-parameters");
  auto rebuilt_parameters = transition();
  EXPECT_EQ(replay_status_v2::commitment_mismatch,
      replay_and_verify_v2(rebuilt_parameters, wrong_parameters,
          cryptonote::EPOSE_STATE_COMMITMENT_SCHEMA_V2,
          parameters, 0, 1, summary));

  replay_source wrong_schema = make_source(1);
  wrong_schema.schemas[1] = 2;
  auto rebuilt_schema = transition();
  EXPECT_EQ(replay_status_v2::commitment_mismatch,
      replay_and_verify_v2(rebuilt_schema, wrong_schema,
          cryptonote::EPOSE_STATE_COMMITMENT_SCHEMA_V2,
          parameters, 0, 1, summary));
}

TEST(epose_replay_v2, rejects_noncanonical_block_metadata_and_versions)
{
  replay_source wrong_height = make_source(0);
  wrong_height.blocks[0].height = 1;
  auto rebuilt_height = transition();
  replay_summary_v2 summary{};
  EXPECT_EQ(replay_status_v2::invalid_block,
      replay_and_verify_v2(rebuilt_height, wrong_height,
          cryptonote::EPOSE_STATE_COMMITMENT_SCHEMA_V2,
          parameters, 0, 0, summary));

  replay_source wrong_version = make_source(0);
  wrong_version.blocks[0].major_version = 18;
  auto rebuilt_version = transition();
  EXPECT_EQ(replay_status_v2::transition_failed,
      replay_and_verify_v2(rebuilt_version, wrong_version,
          cryptonote::EPOSE_STATE_COMMITMENT_SCHEMA_V2,
          parameters, 0, 0, summary));
}
