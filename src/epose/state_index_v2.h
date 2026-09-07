// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "crypto/crypto.h"

namespace qwertycoin
{
namespace epose
{
  constexpr uint32_t EPOSE_STATE_INDEX_SCHEMA_V2 = 1;

  struct state_checkpoint_v2
  {
    uint64_t height = 0;
    crypto::hash block_hash{};
    crypto::hash parent_hash{};
    crypto::hash state_hash{};
    crypto::hash payload_hash{};
  };

  enum class state_index_status_v2
  {
    accepted,
    invalid_configuration,
    invalid_checkpoint,
    height_gap,
    parent_mismatch,
    duplicate_conflict,
    tip_mismatch,
    rebuild_required,
    corrupt_image,
    wrong_schema,
    wrong_parameter_set,
    trailing_bytes
  };

  class state_index_v2
  {
  public:
    state_index_v2(const crypto::hash &parameter_set_hash, size_t undo_horizon);

    bool valid() const;
    state_index_status_v2 connect(const state_checkpoint_v2 &checkpoint);
    state_index_status_v2 disconnect(const crypto::hash &expected_tip_hash);
    state_index_status_v2 rebuild(const std::vector<state_checkpoint_v2> &canonical_history);

    const state_checkpoint_v2 *tip() const;
    size_t retained_checkpoints() const;
    crypto::hash index_root() const;

    std::string serialize() const;
    state_index_status_v2 restore(const std::string &image);

  private:
    crypto::hash parameter_set_hash_{};
    size_t undo_horizon_ = 0;
    std::vector<state_checkpoint_v2> checkpoints_;
  };
} // namespace epose
} // namespace qwertycoin
