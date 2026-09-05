// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "epose/service_node.h"

namespace qwertycoin
{
namespace epose
{
  enum class registration_status
  {
    accepted,
    invalid_signature,
    invalid_admission_proof,
    duplicate_identity,
    duplicate_endpoint
  };

  class service_registry_state
  {
  public:
    bool apply_registration(
        const service_node_identity &identity,
        cryptonote::network_type nettype,
        const crypto::hash &previous_epoch_hash,
        uint8_t admission_leading_zero_bits = EPOSE_ADMISSION_LEADING_ZERO_BITS,
        registration_status *status = nullptr);

    std::vector<service_node_identity> active_nodes(uint64_t epoch) const;
    std::vector<service_node_identity> registrations() const;
    void prune_expired(uint64_t current_epoch, uint64_t retention_epochs = EPOSE_STATE_RETENTION_EPOCHS);
    size_t size() const;

  private:
    std::vector<service_node_identity> registrations_;
  };
} // namespace epose
} // namespace qwertycoin
