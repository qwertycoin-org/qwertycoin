// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <vector>

#include "epose/block_transition_v2.h"

namespace qwertycoin
{
namespace epose
{
  struct replay_transaction_v2
  {
    cryptonote::transaction transaction{};
    bool coinbase = false;
    bool has_payment_context = false;
    service_payment_context_v2 payment_context{};
  };

  struct replay_block_v2
  {
    uint8_t major_version = 0;
    uint64_t height = 0;
    crypto::hash block_hash{};
    std::vector<replay_transaction_v2> transactions;
  };

  class canonical_replay_source_v2 : public canonical_context_source_v2
  {
  public:
    virtual bool replay_block(uint64_t height, replay_block_v2 &block) const = 0;
    virtual bool state_commitment(
        uint64_t height,
        uint8_t &schema_version,
        crypto::hash &block_hash,
        crypto::hash &state_hash,
        crypto::hash &parameter_set_hash) const = 0;
  };

  enum class replay_status_v2
  {
    accepted,
    invalid_range,
    source_unavailable,
    invalid_block,
    transition_failed,
    commitment_missing,
    commitment_mismatch
  };

  struct replay_summary_v2
  {
    uint64_t first_height = 0;
    uint64_t last_height = 0;
    size_t blocks = 0;
    size_t transactions = 0;
  };

  // Streams canonical blocks from genesis/activation through the requested
  // tip and checks every reconstructed state against the commitment stored in
  // the same database transaction as that block. No serialized state cache is
  // trusted during recovery.
  replay_status_v2 replay_and_verify_v2(
      block_transition_v2 &transition,
      const canonical_replay_source_v2 &source,
      uint8_t commitment_schema_version,
      const crypto::hash &parameter_set_hash,
      uint64_t first_height,
      uint64_t last_height,
      replay_summary_v2 &summary);
} // namespace epose
} // namespace qwertycoin
