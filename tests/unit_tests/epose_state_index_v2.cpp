// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include "epose/state_index_v2.h"

namespace
{
  using namespace qwertycoin::epose;

  crypto::hash hash_text(const std::string &text)
  {
    return crypto::cn_fast_hash(text.data(), text.size());
  }

  std::vector<state_checkpoint_v2> chain(size_t count, const std::string &branch = "A")
  {
    std::vector<state_checkpoint_v2> out;
    out.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
      state_checkpoint_v2 checkpoint{};
      checkpoint.height = i;
      checkpoint.parent_hash = i == 0 ? crypto::null_hash : out.back().block_hash;
      checkpoint.block_hash = hash_text(branch + "-block-" + std::to_string(i));
      checkpoint.state_hash = hash_text(branch + "-state-" + std::to_string(i));
      checkpoint.payload_hash = hash_text(branch + "-payload-" + std::to_string(i));
      out.push_back(checkpoint);
    }
    return out;
  }

  void rewrite_checksum(std::string &image)
  {
    const size_t hash_size = sizeof(crypto::hash);
    const crypto::hash checksum = crypto::cn_fast_hash(image.data(), image.size() - hash_size);
    std::memcpy(&image[image.size() - hash_size], &checksum, hash_size);
  }
}

TEST(epose_state_index_v2, connect_is_contiguous_idempotent_and_bounded)
{
  const auto history = chain(6);
  state_index_v2 index(hash_text("parameters"), 3);
  for (const auto &checkpoint : history)
    ASSERT_EQ(state_index_status_v2::accepted, index.connect(checkpoint));
  ASSERT_NE(nullptr, index.tip());
  EXPECT_EQ(5u, index.tip()->height);
  EXPECT_EQ(4u, index.retained_checkpoints());
  EXPECT_EQ(state_index_status_v2::accepted, index.connect(history.back()));
  EXPECT_EQ(4u, index.retained_checkpoints());
}

TEST(epose_state_index_v2, invalid_connect_never_mutates_committed_state)
{
  const auto history = chain(3);
  state_index_v2 index(hash_text("parameters"), 3);
  ASSERT_EQ(state_index_status_v2::accepted, index.connect(history[0]));
  const crypto::hash before = index.index_root();
  EXPECT_EQ(state_index_status_v2::height_gap, index.connect(history[2]));
  auto wrong_parent = history[1];
  wrong_parent.parent_hash = hash_text("wrong-parent");
  EXPECT_EQ(state_index_status_v2::parent_mismatch, index.connect(wrong_parent));
  auto conflict = history[0];
  conflict.state_hash = hash_text("conflict");
  EXPECT_EQ(state_index_status_v2::duplicate_conflict, index.connect(conflict));
  EXPECT_EQ(before, index.index_root());
}

TEST(epose_state_index_v2, bounded_disconnect_requires_rebuild_beyond_undo_horizon)
{
  const auto history = chain(7);
  state_index_v2 index(hash_text("parameters"), 2);
  ASSERT_EQ(state_index_status_v2::accepted, index.rebuild(history));
  EXPECT_EQ(3u, index.retained_checkpoints());
  ASSERT_EQ(state_index_status_v2::accepted, index.disconnect(history[6].block_hash));
  ASSERT_EQ(state_index_status_v2::accepted, index.disconnect(history[5].block_hash));
  EXPECT_EQ(state_index_status_v2::rebuild_required, index.disconnect(history[4].block_hash));
  ASSERT_EQ(state_index_status_v2::accepted,
      index.rebuild(std::vector<state_checkpoint_v2>(history.begin(), history.begin() + 3)));
  EXPECT_EQ(2u, index.tip()->height);
}

TEST(epose_state_index_v2, fork_a_b_a_matches_fresh_replay)
{
  auto original = chain(6);
  state_index_v2 switched(hash_text("parameters"), 4);
  ASSERT_EQ(state_index_status_v2::accepted, switched.rebuild(original));
  ASSERT_EQ(state_index_status_v2::accepted, switched.disconnect(original.back().block_hash));
  auto alternate = original.back();
  alternate.block_hash = hash_text("B-block-5");
  alternate.state_hash = hash_text("B-state-5");
  alternate.payload_hash = hash_text("B-payload-5");
  ASSERT_EQ(state_index_status_v2::accepted, switched.connect(alternate));
  ASSERT_EQ(state_index_status_v2::accepted, switched.disconnect(alternate.block_hash));
  ASSERT_EQ(state_index_status_v2::accepted, switched.connect(original.back()));

  state_index_v2 replay(hash_text("parameters"), 4);
  ASSERT_EQ(state_index_status_v2::accepted, replay.rebuild(original));
  EXPECT_EQ(replay.index_root(), switched.index_root());
}

TEST(epose_state_index_v2, canonical_image_roundtrip_preserves_tip_and_root)
{
  state_index_v2 original(hash_text("parameters"), 4);
  ASSERT_EQ(state_index_status_v2::accepted, original.rebuild(chain(8)));
  const std::string image = original.serialize();
  state_index_v2 restored(hash_text("parameters"), 4);
  ASSERT_EQ(state_index_status_v2::accepted, restored.restore(image));
  EXPECT_EQ(original.index_root(), restored.index_root());
  ASSERT_NE(nullptr, restored.tip());
  EXPECT_EQ(original.tip()->block_hash, restored.tip()->block_hash);
}

TEST(epose_state_index_v2, corrupt_restore_fails_atomically)
{
  state_index_v2 index(hash_text("parameters"), 4);
  ASSERT_EQ(state_index_status_v2::accepted, index.rebuild(chain(3)));
  const crypto::hash before = index.index_root();
  std::string image = index.serialize();
  image[20] ^= 1;
  EXPECT_EQ(state_index_status_v2::corrupt_image, index.restore(image));
  EXPECT_EQ(before, index.index_root());
}

TEST(epose_state_index_v2, schema_parameter_and_trailing_data_fail_closed)
{
  state_index_v2 source(hash_text("parameters"), 4);
  ASSERT_EQ(state_index_status_v2::accepted, source.rebuild(chain(2)));
  std::string image = source.serialize();

  image[4] = 2;
  rewrite_checksum(image);
  state_index_v2 target(hash_text("parameters"), 4);
  EXPECT_EQ(state_index_status_v2::wrong_schema, target.restore(image));

  target = state_index_v2(hash_text("other-parameters"), 4);
  EXPECT_EQ(state_index_status_v2::wrong_schema, target.restore(image));

  image = source.serialize();
  image.insert(image.end() - sizeof(crypto::hash), 'x');
  rewrite_checksum(image);
  target = state_index_v2(hash_text("parameters"), 4);
  EXPECT_EQ(state_index_status_v2::trailing_bytes, target.restore(image));
}

TEST(epose_state_index_v2, wrong_parameter_set_cannot_load_valid_image)
{
  state_index_v2 source(hash_text("parameters"), 4);
  ASSERT_EQ(state_index_status_v2::accepted, source.rebuild(chain(2)));
  state_index_v2 target(hash_text("other-parameters"), 4);
  EXPECT_EQ(state_index_status_v2::wrong_parameter_set, target.restore(source.serialize()));
}

TEST(epose_state_index_v2, invalid_configuration_rejects_empty_rebuild_and_restore)
{
  state_index_v2 valid(hash_text("parameters"), 4);
  ASSERT_EQ(state_index_status_v2::accepted, valid.rebuild({}));
  const std::string empty_image = valid.serialize();

  state_index_v2 null_parameters(crypto::null_hash, 4);
  EXPECT_EQ(state_index_status_v2::invalid_configuration, null_parameters.rebuild({}));
  EXPECT_EQ(state_index_status_v2::invalid_configuration, null_parameters.restore(empty_image));

  state_index_v2 zero_horizon(hash_text("parameters"), 0);
  EXPECT_EQ(state_index_status_v2::invalid_configuration, zero_horizon.rebuild({}));
  EXPECT_EQ(state_index_status_v2::invalid_configuration, zero_horizon.restore(empty_image));
}

TEST(epose_state_index_v2, canonical_image_rejects_duplicate_checkpoint_with_valid_checksum)
{
  state_index_v2 source(hash_text("parameters"), 4);
  ASSERT_EQ(state_index_status_v2::accepted, source.rebuild(chain(2)));
  std::string image = source.serialize();
  constexpr size_t header_size = 4 + 4 + sizeof(crypto::hash) + 8 + 8;
  constexpr size_t checkpoint_size = 8 + 4 * sizeof(crypto::hash);
  std::memcpy(&image[header_size + checkpoint_size], &image[header_size], checkpoint_size);
  rewrite_checksum(image);

  state_index_v2 target(hash_text("parameters"), 4);
  EXPECT_EQ(state_index_status_v2::duplicate_conflict, target.restore(image));
  EXPECT_EQ(nullptr, target.tip());
}
