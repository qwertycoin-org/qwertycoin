// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/replay_v2.h"

#include <limits>

namespace qwertycoin
{
namespace epose
{
  replay_status_v2 replay_and_verify_v2(
      block_transition_v2 &transition,
      const canonical_replay_source_v2 &source,
      uint8_t commitment_schema_version,
      const crypto::hash &parameter_set_hash,
      uint64_t first_height,
      uint64_t last_height,
      replay_summary_v2 &summary)
  {
    summary = {};
    if (!transition.valid() || commitment_schema_version == 0
        || parameter_set_hash == crypto::null_hash
        || first_height > last_height)
      return replay_status_v2::invalid_range;

    replay_summary_v2 next_summary{};
    next_summary.first_height = first_height;
    next_summary.last_height = last_height;
    for (uint64_t height = first_height;; ++height)
    {
      replay_block_v2 block{};
      if (!source.replay_block(height, block))
        return replay_status_v2::source_unavailable;
      if (block.height != height || block.block_hash == crypto::null_hash
          || block.transactions.empty() || !block.transactions.front().coinbase)
        return replay_status_v2::invalid_block;

      std::vector<block_transaction_context_v2> transactions;
      transactions.reserve(block.transactions.size());
      for (const replay_transaction_v2 &transaction : block.transactions)
      {
        transactions.push_back({
            &transaction.transaction,
            transaction.coinbase,
            transaction.has_payment_context ? &transaction.payment_context : nullptr});
      }

      block_apply_summary_v2 block_summary{};
      if (transition.apply_block(
              block.major_version, block.height, block.block_hash,
              transactions, source, block_summary)
          != block_transition_status_v2::accepted)
        return replay_status_v2::transition_failed;

      uint8_t committed_schema = 0;
      crypto::hash committed_block{};
      crypto::hash committed_state{};
      crypto::hash committed_parameters{};
      if (!source.state_commitment(
              height, committed_schema, committed_block,
              committed_state, committed_parameters))
        return replay_status_v2::commitment_missing;
      if (committed_schema != commitment_schema_version
          || committed_block != block.block_hash
          || committed_state != transition.state().state_hash()
          || committed_parameters != parameter_set_hash)
        return replay_status_v2::commitment_mismatch;

      if (next_summary.blocks == std::numeric_limits<size_t>::max()
          || next_summary.transactions
              > std::numeric_limits<size_t>::max() - block_summary.transactions)
        return replay_status_v2::invalid_range;
      ++next_summary.blocks;
      next_summary.transactions += block_summary.transactions;
      if (height == last_height)
        break;
      if (height == std::numeric_limits<uint64_t>::max())
        return replay_status_v2::invalid_range;
    }

    summary = next_summary;
    return replay_status_v2::accepted;
  }
} // namespace epose
} // namespace qwertycoin
