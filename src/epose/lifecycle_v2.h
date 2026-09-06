// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <vector>

#include "crypto/crypto.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_config.h"

namespace qwertycoin
{
namespace epose
{
  constexpr uint8_t EPOSE_DESCRIPTOR_VERSION_V2 = 1;

  enum class lifecycle_action_v2 : uint8_t
  {
    register_identity = 1,
    renew_lease = 2,
    update_descriptor = 3,
    deregister_identity = 4,
    recover_service_key = 5
  };

  struct identity_descriptor_v2
  {
    uint8_t version = EPOSE_DESCRIPTOR_VERSION_V2;
    crypto::hash identity_id{};
    crypto::public_key service_public_key{};
    crypto::public_key operator_authorization_public_key{};
    cryptonote::account_public_address reward_address{};
    crypto::hash endpoint_descriptor_hash{};
    uint64_t sequence = 0;
    uint64_t effective_epoch = 0;
    uint64_t expiry_epoch = 0;
  };

  struct lifecycle_record_v2
  {
    lifecycle_action_v2 action = lifecycle_action_v2::register_identity;
    crypto::hash previous_descriptor_hash{};
    identity_descriptor_v2 next_descriptor{};
    crypto::signature operator_signature{};
    crypto::signature service_signature{};
  };

  enum class lifecycle_status_v2
  {
    accepted,
    idempotent_duplicate,
    invalid_context,
    invalid_action,
    invalid_descriptor,
    nonseparated_authorities,
    identity_not_found,
    identity_already_exists,
    wrong_previous_descriptor,
    wrong_sequence,
    retroactive_change,
    overlapping_change,
    invalid_transition,
    invalid_operator_signature,
    invalid_service_signature
  };

  crypto::hash derive_identity_id_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      const crypto::public_key &operator_authorization_public_key);

  crypto::hash hash_identity_descriptor_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      const identity_descriptor_v2 &descriptor);

  crypto::hash hash_lifecycle_record_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      const lifecycle_record_v2 &record);

  bool sign_lifecycle_record_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      lifecycle_record_v2 &record,
      const crypto::secret_key &operator_authorization_secret_key,
      const crypto::secret_key &service_secret_key);

  lifecycle_status_v2 validate_lifecycle_record_authorization_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      const lifecycle_record_v2 &record);

  class lifecycle_registry_v2
  {
  public:
    lifecycle_registry_v2(
        cryptonote::network_type nettype,
        const crypto::hash &genesis_hash,
        const crypto::hash &parameter_set_hash);

    bool valid() const;

    lifecycle_status_v2 apply(
        const lifecycle_record_v2 &record,
        uint64_t inclusion_epoch,
        uint64_t minimum_effective_epoch);

    const identity_descriptor_v2 *latest(const crypto::hash &identity_id) const;
    const identity_descriptor_v2 *descriptor_for_epoch(
        const crypto::hash &identity_id,
        uint64_t epoch) const;
    crypto::hash state_hash() const;

  private:
    struct identity_history
    {
      crypto::hash identity_id{};
      std::vector<lifecycle_record_v2> records;
    };

    cryptonote::network_type nettype_;
    crypto::hash genesis_hash_{};
    crypto::hash parameter_set_hash_{};
    bool valid_ = false;
    std::vector<identity_history> histories_;
  };
} // namespace epose
} // namespace qwertycoin
