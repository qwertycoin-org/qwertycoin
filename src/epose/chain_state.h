// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "epose/service_node.h"
#include "epose/service_registry.h"

namespace qwertycoin
{
namespace epose
{
  struct tx_extra_apply_summary
  {
    size_t registrations_applied = 0;
    size_t attestations_applied = 0;
  };

  struct transaction_apply_summary : tx_extra_apply_summary
  {
    size_t transactions_scanned = 0;
  };

  class chain_state
  {
  public:
    struct snapshot
    {
      service_registry_state registry;
      std::vector<service_attestation> attestations;
    };

    explicit chain_state(
        cryptonote::network_type nettype,
        uint8_t admission_leading_zero_bits = EPOSE_ADMISSION_LEADING_ZERO_BITS);

    bool apply_tx_extra(
        const std::vector<uint8_t> &tx_extra,
        const crypto::hash &previous_epoch_hash,
        tx_extra_apply_summary *summary = nullptr,
        uint64_t expected_epoch = std::numeric_limits<uint64_t>::max());

    bool apply_transaction(
        const cryptonote::transaction &tx,
        const crypto::hash &previous_epoch_hash,
        tx_extra_apply_summary *summary = nullptr,
        uint64_t expected_epoch = std::numeric_limits<uint64_t>::max());

    bool apply_transactions(
        const std::vector<cryptonote::transaction> &txs,
        const crypto::hash &previous_epoch_hash,
        transaction_apply_summary *summary = nullptr,
        uint64_t expected_epoch = std::numeric_limits<uint64_t>::max());

    std::vector<crypto::public_key> qualified_service_nodes(uint64_t epoch) const;

    void prune_expired(uint64_t current_epoch, uint64_t retention_epochs = EPOSE_STATE_RETENTION_EPOCHS);

    snapshot make_snapshot() const;
    void restore_snapshot(const snapshot &state);

    const service_registry_state &registry() const;
    const std::vector<service_attestation> &attestations() const;
    crypto::hash state_hash() const;

  private:
    cryptonote::network_type nettype_;
    uint8_t admission_leading_zero_bits_;
    service_registry_state registry_;
    std::vector<service_attestation> attestations_;
  };
} // namespace epose
} // namespace qwertycoin
