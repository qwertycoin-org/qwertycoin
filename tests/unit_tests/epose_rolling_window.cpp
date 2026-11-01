#include <gtest/gtest.h>

#include <map>
#include <string>

#include "epose/block_transition_v2.h"
#include "epose/record_codec_v2.h"
#include "epose/service_node.h"

namespace
{
  using namespace qwertycoin::epose;

  crypto::hash hash_text(const std::string &value)
  {
    return crypto::cn_fast_hash(value.data(), value.size());
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
      return true;
    }

    bool round_anchor(uint64_t, uint64_t, crypto::hash &) const override
    {
      return false;
    }
  };

  block_transition_v2 make_transition(size_t undo_limit = 128)
  {
    block_transition_limits_v2 limits{};
    limits.max_envelopes_per_transaction = 1;
    limits.envelope.max_envelope_bytes = 4096;
    limits.envelope.max_records = 8;
    limits.envelope.max_record_payload_bytes = 2048;
    limits.envelope.max_signature_verifications = 16;
    limits.envelope.max_admission_verifications = 4;
    limits.envelope.supported_record_versions = {0, 1, 1, 1, 1, 1};
    limits.block = {16384, 32, 32, 8};
    limits.max_recent_undo_blocks = undo_limit;
    return {cryptonote::TESTNET, hash_text("window-genesis"),
        hash_text("window-parameters"), {0, 12, 2},
        {admission_work_algorithm_v2::randomx, 1, 1},
        {1, 1, 1, 1, 1, {0}}, limits};
  }

  block_transition_status_v2 apply(
      block_transition_v2 &transition, contexts &source, uint64_t height)
  {
    cryptonote::transaction coinbase{};
    const crypto::hash block_hash = height == 0
        ? hash_text("window-genesis")
        : hash_text("window-block-" + std::to_string(height));
    source.blocks[height] = block_hash;
    block_apply_summary_v2 summary{};
    return transition.apply_block(height >= 108
            ? HF_VERSION_QWC_CONTRACTS : HF_VERSION_QWC_EPOSE,
        height, block_hash, {{&coinbase, true, nullptr}}, source, summary);
  }

  struct signing_keys
  {
    crypto::public_key public_key{};
    crypto::secret_key secret_key{};
  };

  signing_keys make_keys()
  {
    signing_keys result{};
    crypto::generate_keys(result.public_key, result.secret_key);
    return result;
  }

  std::map<uint64_t, envelope_record_v2> lifecycle_events()
  {
    const signing_keys service = make_keys();
    const signing_keys authority = make_keys();
    const signing_keys view = make_keys();
    const signing_keys spend = make_keys();
    const crypto::hash genesis = hash_text("window-genesis");
    const crypto::hash parameters = hash_text("window-parameters");
    identity_descriptor_v2 descriptor{};
    descriptor.identity_id = derive_identity_id_v2(cryptonote::TESTNET,
        genesis, parameters, authority.public_key);
    descriptor.service_public_key = service.public_key;
    descriptor.operator_authorization_public_key = authority.public_key;
    descriptor.reward_address.m_view_public_key = view.public_key;
    descriptor.reward_address.m_spend_public_key = spend.public_key;
    descriptor.endpoint_descriptor_hash = hash_text("window-endpoint");
    std::map<uint64_t, envelope_record_v2> events;
    for (uint64_t epoch = 1; epoch <= 21; ++epoch)
    {
      lifecycle_record_v2 record{};
      record.action = epoch == 1 ? lifecycle_action_v2::register_identity
          : epoch % 2 == 0 ? lifecycle_action_v2::renew_lease
          : lifecycle_action_v2::update_descriptor;
      if (epoch > 1)
        record.previous_descriptor_hash = hash_identity_descriptor_v2(
            cryptonote::TESTNET, genesis, parameters, descriptor);
      descriptor.sequence = epoch - 1;
      descriptor.effective_epoch = epoch;
      descriptor.expiry_epoch = epoch + EPOSE_REGISTRATION_TTL_EPOCHS;
      if (record.action == lifecycle_action_v2::update_descriptor)
        descriptor.endpoint_descriptor_hash = hash_text(
            "window-endpoint-" + std::to_string(epoch));
      record.next_descriptor = descriptor;
      EXPECT_TRUE(sign_lifecycle_record_v2(cryptonote::TESTNET, genesis,
          parameters, record, authority.secret_key, service.secret_key));
      envelope_record_v2 encoded{};
      EXPECT_EQ(record_codec_status_v2::accepted,
          encode_lifecycle_record_v2(record, cryptonote::TESTNET, genesis,
              parameters, encoded));
      events.emplace((epoch - 1) * 12 + 1, std::move(encoded));
    }
    return events;
  }

  std::map<uint64_t, envelope_record_v2> reregistration_events()
  {
    const signing_keys service = make_keys();
    const signing_keys authority = make_keys();
    const signing_keys view = make_keys();
    const signing_keys spend = make_keys();
    const crypto::hash genesis = hash_text("window-genesis");
    const crypto::hash parameters = hash_text("window-parameters");
    identity_descriptor_v2 descriptor{};
    descriptor.identity_id = derive_identity_id_v2(cryptonote::TESTNET,
        genesis, parameters, authority.public_key);
    descriptor.service_public_key = service.public_key;
    descriptor.operator_authorization_public_key = authority.public_key;
    descriptor.reward_address.m_view_public_key = view.public_key;
    descriptor.reward_address.m_spend_public_key = spend.public_key;
    descriptor.endpoint_descriptor_hash = hash_text("window-reregistration");
    std::map<uint64_t, envelope_record_v2> events;
    for (const uint64_t epoch : {1u, 14u})
    {
      descriptor.effective_epoch = epoch;
      descriptor.expiry_epoch = epoch + 2;
      lifecycle_record_v2 record{};
      record.action = lifecycle_action_v2::register_identity;
      record.next_descriptor = descriptor;
      EXPECT_TRUE(sign_lifecycle_record_v2(cryptonote::TESTNET, genesis,
          parameters, record, authority.secret_key, service.secret_key));
      envelope_record_v2 encoded{};
      EXPECT_EQ(record_codec_status_v2::accepted,
          encode_lifecycle_record_v2(record, cryptonote::TESTNET, genesis,
              parameters, encoded));
      events.emplace((epoch - 1) * 12 + 1, std::move(encoded));
    }
    return events;
  }

  block_transition_status_v2 apply_with_events(block_transition_v2 &transition,
      contexts &source, uint64_t height,
      const std::map<uint64_t, envelope_record_v2> &events,
      const std::string &branch = "window-block-")
  {
    cryptonote::transaction coinbase{};
    cryptonote::transaction transaction{};
    const crypto::hash block_hash = height == 0
        ? hash_text("window-genesis")
        : hash_text(branch + std::to_string(height));
    source.blocks[height] = block_hash;
    const uint8_t version = height >= 108
        ? HF_VERSION_QWC_CONTRACTS : HF_VERSION_QWC_EPOSE;
    std::vector<block_transaction_context_v2> transactions{
        {&coinbase, true, nullptr}};
    const auto found = events.find(height);
    if (found != events.end())
    {
      envelope_budget_v2 budget{};
      envelope_limits_v2 envelope{};
      envelope.max_envelope_bytes = 4096;
      envelope.max_records = 8;
      envelope.max_record_payload_bytes = 2048;
      envelope.max_signature_verifications = 16;
      envelope.max_admission_verifications = 4;
      envelope.supported_record_versions = {0, 1, 1, 1, 1, 1};
      EXPECT_EQ(envelope_status_v2::accepted,
          append_transaction_envelope_v2({found->second}, version, 1,
              envelope, transaction.extra, budget));
      transactions.push_back({&transaction, false, nullptr});
    }
    block_apply_summary_v2 summary{};
    return transition.apply_block(version, height, block_hash,
        transactions, source, summary);
  }
}

TEST(epose_rolling_window, replay_and_reorg_across_fork_and_fold)
{
  auto live = make_transition();
  contexts source{};
  ASSERT_TRUE(live.valid());
  std::map<uint64_t, crypto::hash> hashes;
  for (uint64_t height = 0; height <= 120; ++height)
  {
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply(live, source, height)) << height;
    if (height == 107 || height == 108 || height == 119 || height == 120)
      hashes.emplace(height, live.state().state_hash());
  }

  EXPECT_EQ(1u, make_transition().state().state_commitment_schema());
  EXPECT_NE(hashes.at(107), hashes.at(108));
  EXPECT_NE(hashes.at(119), hashes.at(120));
  EXPECT_EQ(QWC_EPOSE_ROLLING_STATE_SCHEMA,
      live.state().state_commitment_schema());
  EXPECT_EQ(2u, live.state().membership().oldest_retained_epoch());
  EXPECT_EQ(pipeline_status_v2::outside_retained_window,
      live.state().membership().read_epoch(1));
  EXPECT_EQ(pipeline_status_v2::accepted,
      live.state().membership().read_epoch(2));
  std::string transcript("old");
  EXPECT_EQ(pipeline_status_v2::outside_retained_window,
      live.state().membership().epoch_transcript(1, transcript));
  EXPECT_TRUE(transcript.empty());

  ASSERT_EQ(block_transition_status_v2::accepted, live.disconnect_tip(120));
  EXPECT_EQ(hashes.at(119), live.state().state_hash());
  ASSERT_EQ(block_transition_status_v2::accepted, apply(live, source, 120));
  EXPECT_EQ(hashes.at(120), live.state().state_hash());

  for (uint64_t height = 120; height >= 108; --height)
    ASSERT_EQ(block_transition_status_v2::accepted,
        live.disconnect_tip(height));
  EXPECT_EQ(hashes.at(107), live.state().state_hash());
  for (uint64_t height = 108; height <= 120; ++height)
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply(live, source, height));
  EXPECT_EQ(hashes.at(120), live.state().state_hash());

  auto replay = make_transition();
  contexts replay_source{};
  for (uint64_t height = 0; height <= 120; ++height)
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply(replay, replay_source, height));
  EXPECT_EQ(hashes.at(120), replay.state().state_hash());
}

TEST(epose_rolling_window, lifecycle_folding_matches_restart_replay_and_reorg)
{
  const auto events = lifecycle_events();
  auto live = make_transition();
  auto restarted = make_transition();
  auto replay = make_transition();
  contexts source{};
  for (uint64_t height = 0; height <= 252; ++height)
  {
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply_with_events(live, source, height, events)) << height;
    if (height == 180)
      ASSERT_TRUE(restarted.restore_state(height, live.serialize_state()));
    if (height > 180)
      ASSERT_EQ(block_transition_status_v2::accepted,
          apply_with_events(restarted, source, height, events)) << height;
  }
  const crypto::hash expected = live.state().state_hash();
  EXPECT_EQ(expected, restarted.state().state_hash());
  contexts replay_source{};
  for (uint64_t height = 0; height <= 252; ++height)
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply_with_events(replay, replay_source, height, events)) << height;
  EXPECT_EQ(expected, replay.state().state_hash());
  for (uint64_t height = 252; height > 240; --height)
    ASSERT_EQ(block_transition_status_v2::accepted, live.disconnect_tip(height));
  for (uint64_t height = 241; height <= 252; ++height)
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply_with_events(live, source, height, events));
  EXPECT_EQ(expected, live.state().state_hash());
}

TEST(epose_rolling_window, expired_identity_reregistration_matches_restart_and_replay)
{
  const auto events = reregistration_events();
  auto live = make_transition();
  auto restored = make_transition();
  auto replay = make_transition();
  contexts source{};
  for (uint64_t height = 0; height <= 169; ++height)
  {
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply_with_events(live, source, height, events)) << height;
    if (height == 145)
      ASSERT_TRUE(restored.restore_state(height, live.serialize_state()));
    if (height > 145)
      ASSERT_EQ(block_transition_status_v2::accepted,
          apply_with_events(restored, source, height, events)) << height;
  }
  contexts replay_source{};
  for (uint64_t height = 0; height <= 169; ++height)
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply_with_events(replay, replay_source, height, events)) << height;
  EXPECT_EQ(live.state().state_hash(), restored.state().state_hash());
  EXPECT_EQ(live.state().state_hash(), replay.state().state_hash());
}

TEST(epose_rolling_window, restarted_hundred_block_reorg_restores_once)
{
  const auto events = lifecycle_events();
  auto canonical = make_transition();
  contexts source{};
  std::string boundary_image;
  for (uint64_t height = 0; height <= 252; ++height)
  {
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply_with_events(canonical, source, height, events));
    if (height == 144)
      boundary_image = canonical.serialize_state();
  }
  ASSERT_FALSE(boundary_image.empty());
  auto restarted = make_transition();
  ASSERT_TRUE(restarted.restore_state(252, canonical.serialize_state()));
  EXPECT_EQ(block_transition_status_v2::deep_replay_required,
      restarted.disconnect_tip(252));
  size_t restores_during_reorg = 0;
  ASSERT_TRUE(restarted.restore_state(144, boundary_image));
  ++restores_during_reorg;
  for (uint64_t height = 145; height <= 152; ++height)
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply_with_events(restarted, source, height, events));
  for (uint64_t height = 153; height <= 252; ++height)
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply_with_events(restarted, source, height, events, "alternate-block-"));

  auto fresh = make_transition();
  contexts fresh_source{};
  for (uint64_t height = 0; height <= 252; ++height)
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply_with_events(fresh, fresh_source, height, events,
            height > 152 ? "alternate-block-" : "window-block-"));
  EXPECT_EQ(1u, restores_during_reorg);
  EXPECT_EQ(fresh.state().state_hash(), restarted.state().state_hash());
}

TEST(epose_rolling_window, full_undo_horizon_restores_folded_parent_states)
{
  const auto events = lifecycle_events();
  auto live = make_transition(2160);
  contexts source{};
  const crypto::hash initial = live.state().state_hash();
  std::vector<crypto::hash> parent_hashes;
  parent_hashes.reserve(2160);
  for (uint64_t height = 0; height < 2160; ++height)
  {
    parent_hashes.push_back(live.state().state_hash());
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply_with_events(live, source, height, events)) << height;
  }
  ASSERT_EQ(2160u, live.recent_undo_count());
  for (uint64_t height = 2160; height > 0; --height)
  {
    ASSERT_EQ(block_transition_status_v2::accepted,
        live.disconnect_tip(height - 1)) << height;
    EXPECT_EQ(parent_hashes[height - 1], live.state().state_hash()) << height;
  }
  EXPECT_EQ(initial, live.state().state_hash());
}

TEST(epose_rolling_window, rejected_record_after_fold_restores_parent_image)
{
  const auto events = lifecycle_events();
  auto live = make_transition();
  contexts source{};
  for (uint64_t height = 0; height < 120; ++height)
    ASSERT_EQ(block_transition_status_v2::accepted,
        apply_with_events(live, source, height, events)) << height;
  const crypto::hash parent_hash = live.state().state_hash();
  const std::string parent_image = live.serialize_state();
  const std::map<uint64_t, envelope_record_v2> rejected{{120, events.at(1)}};
  EXPECT_EQ(block_transition_status_v2::invalid_semantics,
      apply_with_events(live, source, 120, rejected));
  EXPECT_EQ(parent_hash, live.state().state_hash());
  EXPECT_EQ(parent_image, live.serialize_state());
  ASSERT_EQ(block_transition_status_v2::accepted,
      apply_with_events(live, source, 120, events));
  ASSERT_EQ(block_transition_status_v2::accepted, live.disconnect_tip(120));
  EXPECT_EQ(parent_hash, live.state().state_hash());
}
