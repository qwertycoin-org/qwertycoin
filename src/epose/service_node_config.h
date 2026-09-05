// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>

#include "crypto/crypto.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_config.h"
#include "epose/service_node.h"

namespace qwertycoin
{
namespace epose
{
  struct local_service_node_config
  {
    bool enabled = false;
    bool key_loaded = false;
    bool key_created = false;
    std::string key_path;
    crypto::public_key service_public_key{};
    crypto::secret_key service_secret_key{};
    cryptonote::account_public_address reward_address{};
    crypto::secret_key reward_view_secret_key{};
    std::string reward_address_string;
    std::string advertised_endpoint;
    crypto::hash endpoint_commitment{};
  };

  bool parse_reward_address(
      const std::string &address,
      cryptonote::network_type nettype,
      cryptonote::account_public_address &reward_address,
      std::string &error);

  bool parse_reward_view_secret_key(
      const std::string &key_hex,
      const cryptonote::account_public_address &reward_address,
      crypto::secret_key &reward_view_secret_key,
      std::string &error);

  bool load_or_create_service_node_key(
      const std::string &key_path,
      crypto::public_key &service_public_key,
      crypto::secret_key &service_secret_key,
      bool &created,
      std::string &error);

  bool build_service_node_registration_identity(
      const local_service_node_config &config,
      cryptonote::network_type nettype,
      uint64_t registration_epoch,
      const crypto::hash &previous_epoch_hash,
      service_node_identity &identity,
      std::string &error,
      uint8_t leading_zero_bits = EPOSE_ADMISSION_LEADING_ZERO_BITS,
      uint64_t max_attempts = 1000000);
} // namespace epose
} // namespace qwertycoin
