// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "service_node_config.h"

#include <limits>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "file_io_utils.h"
#include "string_tools.h"

namespace
{
  constexpr const char *KEY_FILE_HEADER = "QWC_EPOSE_SERVICE_NODE_KEY_V1";

  std::string secret_key_to_hex(const crypto::secret_key &secret_key)
  {
    return epee::string_tools::pod_to_hex(unwrap(unwrap(secret_key)));
  }

  bool parse_key_file(const std::string &contents, crypto::secret_key &secret_key)
  {
    std::vector<std::string> lines;
    boost::split(lines, contents, boost::is_any_of("\r\n"), boost::token_compress_on);
    if (lines.empty())
      return false;

    std::string key_hex;
    if (lines[0] == KEY_FILE_HEADER)
    {
      if (lines.size() < 2)
        return false;
      key_hex = lines[1];
    }
    else
    {
      key_hex = lines[0];
    }
    boost::trim(key_hex);
    return key_hex.size() == 64 && epee::string_tools::hex_to_pod(key_hex, unwrap(unwrap(secret_key)));
  }

  bool write_key_file(const boost::filesystem::path &path, const crypto::secret_key &secret_key, std::string &error)
  {
    boost::system::error_code ec;
    if (!path.parent_path().empty())
      boost::filesystem::create_directories(path.parent_path(), ec);
    if (ec)
    {
      error = "failed to create service-node key directory: " + ec.message();
      return false;
    }

    const std::string contents = std::string(KEY_FILE_HEADER) + "\n" + secret_key_to_hex(secret_key) + "\n";
    if (!epee::file_io_utils::save_string_to_file(path.string(), contents))
    {
      error = "failed to write service-node key file";
      return false;
    }

    boost::filesystem::permissions(path, boost::filesystem::owner_read | boost::filesystem::owner_write, ec);
    if (ec)
    {
      error = "failed to restrict service-node key file permissions: " + ec.message();
      return false;
    }
    return true;
  }
}

namespace qwertycoin
{
namespace epose
{
  bool parse_reward_address(
      const std::string &address,
      cryptonote::network_type nettype,
      cryptonote::account_public_address &reward_address,
      std::string &error)
  {
    cryptonote::address_parse_info info{};
    if (!cryptonote::get_account_address_from_str(info, nettype, address))
    {
      error = "invalid service-node reward address for selected network";
      return false;
    }
    if (info.is_subaddress)
    {
      error = "service-node reward address must be a primary address";
      return false;
    }
    if (info.has_payment_id)
    {
      error = "service-node reward address must not include a payment id";
      return false;
    }
    reward_address = info.address;
    return true;
  }

  bool parse_reward_view_secret_key(
      const std::string &key_hex,
      const cryptonote::account_public_address &reward_address,
      crypto::secret_key &reward_view_secret_key,
      std::string &error)
  {
    std::string trimmed = key_hex;
    boost::trim(trimmed);
    if (trimmed.size() != 64 || !epee::string_tools::hex_to_pod(trimmed, unwrap(unwrap(reward_view_secret_key))))
    {
      error = "invalid service-node reward view secret key";
      return false;
    }

    crypto::public_key derived_view_public_key{};
    if (!crypto::secret_key_to_public_key(reward_view_secret_key, derived_view_public_key))
    {
      error = "service-node reward view secret key is not a valid scalar";
      return false;
    }
    if (derived_view_public_key != reward_address.m_view_public_key)
    {
      error = "service-node reward view secret key does not match the reward address view public key";
      return false;
    }
    return true;
  }

  bool load_or_create_service_node_key(
      const std::string &key_path,
      crypto::public_key &service_public_key,
      crypto::secret_key &service_secret_key,
      bool &created,
      std::string &error)
  {
    created = false;
    boost::system::error_code ec;
    const boost::filesystem::path path(key_path);
    if (boost::filesystem::exists(path, ec))
    {
      std::string contents;
      if (!epee::file_io_utils::load_file_to_string(path.string(), contents, 4096))
      {
        error = "failed to read service-node key file";
        return false;
      }
      if (!parse_key_file(contents, service_secret_key))
      {
        error = "invalid service-node key file";
        return false;
      }
      if (!crypto::secret_key_to_public_key(service_secret_key, service_public_key))
      {
        error = "service-node key file does not contain a valid secret key";
        return false;
      }
      return true;
    }
    if (ec && ec.value() != boost::system::errc::no_such_file_or_directory)
    {
      error = "failed to check service-node key file: " + ec.message();
      return false;
    }

    crypto::generate_keys(service_public_key, service_secret_key);
    if (!write_key_file(path, service_secret_key, error))
      return false;
    created = true;
    return true;
  }

  bool build_service_node_registration_identity(
      const local_service_node_config &config,
      cryptonote::network_type nettype,
      uint64_t registration_epoch,
      const crypto::hash &previous_epoch_hash,
      service_node_identity &identity,
      std::string &error,
      uint8_t leading_zero_bits,
      uint64_t max_attempts)
  {
    if (!config.enabled || !config.key_loaded)
    {
      error = "local service-node mode is not ready";
      return false;
    }
    if (config.advertised_endpoint.empty())
    {
      error = "service-node advertised endpoint is required before registration";
      return false;
    }
    if (max_attempts == 0)
    {
      error = "service-node admission search limit must be greater than zero";
      return false;
    }
    if (registration_epoch > std::numeric_limits<uint64_t>::max() - EPOSE_REGISTRATION_TTL_EPOCHS)
    {
      error = "service-node registration epoch overflow";
      return false;
    }

    identity = service_node_identity{};
    identity.service_public_key = config.service_public_key;
    identity.reward_address = config.reward_address;
    identity.reward_view_secret_key = config.reward_view_secret_key;
    identity.endpoint_commitment = config.endpoint_commitment;
    identity.registration_epoch = registration_epoch;
    identity.expiry_epoch = registration_epoch + EPOSE_REGISTRATION_TTL_EPOCHS;

    for (uint64_t attempt = 0; attempt < max_attempts; ++attempt)
    {
      identity.admission_nonce = attempt;
      identity.admission_hash = calculate_admission_hash(identity, nettype, previous_epoch_hash);
      if (!admission_hash_meets_target(identity.admission_hash, leading_zero_bits))
        continue;

      sign_registration(identity, config.service_secret_key, nettype);
      if (!verify_registration_signature(identity, nettype)
          || !verify_admission_proof(identity, nettype, previous_epoch_hash, leading_zero_bits))
      {
        error = "built service-node registration failed self-verification";
        return false;
      }
      return true;
    }

    error = "failed to find service-node admission nonce within search limit";
    return false;
  }
} // namespace epose
} // namespace qwertycoin
