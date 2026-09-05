// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "service_registry.h"

#include <algorithm>
#include <cstring>

namespace
{
  bool public_key_equal(const crypto::public_key &left, const crypto::public_key &right)
  {
    return std::memcmp(&left, &right, sizeof(left)) == 0;
  }

  bool public_key_less(const crypto::public_key &left, const crypto::public_key &right)
  {
    return std::memcmp(&left, &right, sizeof(left)) < 0;
  }

  bool registration_epochs_overlap(
      const qwertycoin::epose::service_node_identity &left,
      const qwertycoin::epose::service_node_identity &right)
  {
    return left.registration_epoch < right.expiry_epoch && right.registration_epoch < left.expiry_epoch;
  }

  void set_status(qwertycoin::epose::registration_status *out, qwertycoin::epose::registration_status value)
  {
    if (out)
      *out = value;
  }

  bool expired_beyond_retention(uint64_t expiry_epoch, uint64_t current_epoch, uint64_t retention_epochs)
  {
    return current_epoch > expiry_epoch && current_epoch - expiry_epoch >= retention_epochs;
  }
}

namespace qwertycoin
{
namespace epose
{
  bool service_registry_state::apply_registration(
      const service_node_identity &identity,
      cryptonote::network_type nettype,
      const crypto::hash &previous_epoch_hash,
      uint8_t admission_leading_zero_bits,
      registration_status *status)
  {
    if (!verify_registration_signature(identity, nettype))
    {
      set_status(status, registration_status::invalid_signature);
      return false;
    }

    if (!verify_admission_proof(identity, nettype, previous_epoch_hash, admission_leading_zero_bits))
    {
      set_status(status, registration_status::invalid_admission_proof);
      return false;
    }

    for (const service_node_identity &registered : registrations_)
    {
      if (public_key_equal(registered.service_public_key, identity.service_public_key)
          && registration_epochs_overlap(registered, identity))
      {
        set_status(status, registration_status::duplicate_identity);
        return false;
      }
      if (registered.endpoint_commitment == identity.endpoint_commitment
          && registration_epochs_overlap(registered, identity))
      {
        set_status(status, registration_status::duplicate_endpoint);
        return false;
      }
    }

    registrations_.push_back(identity);
    std::sort(registrations_.begin(), registrations_.end(), [](const service_node_identity &left, const service_node_identity &right) {
      if (left.registration_epoch != right.registration_epoch)
        return left.registration_epoch < right.registration_epoch;
      if (left.expiry_epoch != right.expiry_epoch)
        return left.expiry_epoch < right.expiry_epoch;
      return public_key_less(left.service_public_key, right.service_public_key);
    });

    set_status(status, registration_status::accepted);
    return true;
  }

  std::vector<service_node_identity> service_registry_state::active_nodes(uint64_t epoch) const
  {
    std::vector<service_node_identity> active;
    for (const service_node_identity &identity : registrations_)
    {
      if (identity_active_in_epoch(identity, epoch))
        active.push_back(identity);
    }
    return active;
  }

  std::vector<service_node_identity> service_registry_state::registrations() const
  {
    return registrations_;
  }

  void service_registry_state::prune_expired(uint64_t current_epoch, uint64_t retention_epochs)
  {
    registrations_.erase(std::remove_if(registrations_.begin(), registrations_.end(), [&](const service_node_identity &identity) {
      return expired_beyond_retention(identity.expiry_epoch, current_epoch, retention_epochs);
    }), registrations_.end());
  }

  size_t service_registry_state::size() const
  {
    return registrations_.size();
  }
} // namespace epose
} // namespace qwertycoin
