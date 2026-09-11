// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/service_keystore_v2.h"

#include <array>
#include <map>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include "file_io_utils.h"
#include "string_tools.h"

namespace
{
  constexpr const char *KEYSTORE_HEADER = "QWC_EPOSE_SERVICE_KEYSTORE_V2";

  template <typename T>
  void append_bytes(std::string &out, const T &value)
  {
    out.append(reinterpret_cast<const char *>(&value), sizeof(value));
  }

  std::string network_binding(cryptonote::network_type nettype)
  {
    std::string out;
    for (const uint8_t byte : cryptonote::get_config(nettype).NETWORK_ID)
      out.push_back(static_cast<char>(byte));
    return epee::string_tools::buff_to_hex_nodelimer(out);
  }

  crypto::hash checksum(
      const qwertycoin::epose::service_keystore_context_v2 &context,
      const crypto::secret_key &operator_secret,
      const crypto::secret_key &service_secret)
  {
    std::string blob("QWC_EPOSE_SERVICE_KEYSTORE_CHECKSUM_V2");
    const std::string network = network_binding(context.nettype);
    blob.append(network);
    append_bytes(blob, context.genesis_hash);
    append_bytes(blob, context.parameter_set_hash);
    append_bytes(blob, operator_secret);
    append_bytes(blob, service_secret);
    return crypto::cn_fast_hash(blob.data(), blob.size());
  }

  bool parse_lines(
      const std::string &contents,
      std::map<std::string, std::string> &fields)
  {
    fields.clear();
    std::vector<std::string> lines;
    boost::split(lines, contents, boost::is_any_of("\r\n"), boost::token_compress_on);
    if (lines.empty() || lines.front() != KEYSTORE_HEADER)
      return false;
    for (size_t i = 1; i < lines.size(); ++i)
    {
      boost::trim(lines[i]);
      if (lines[i].empty())
        continue;
      const size_t delimiter = lines[i].find('=');
      if (delimiter == std::string::npos || delimiter == 0
          || delimiter + 1 == lines[i].size())
        return false;
      const std::string key = lines[i].substr(0, delimiter);
      const std::string value = lines[i].substr(delimiter + 1);
      if (!fields.emplace(key, value).second)
        return false;
    }
    static const std::array<const char *, 6> required{{
        "network_id", "genesis_hash", "parameter_set_hash",
        "operator_secret_key", "service_secret_key", "checksum"}};
    if (fields.size() != required.size())
      return false;
    for (const char *key : required)
      if (fields.find(key) == fields.end())
        return false;
    return true;
  }

  bool derive_keys(qwertycoin::epose::service_keystore_v2 &keystore)
  {
    return crypto::secret_key_to_public_key(
               keystore.operator_secret_key, keystore.operator_public_key)
        && crypto::secret_key_to_public_key(
               keystore.service_secret_key, keystore.service_public_key);
  }

  bool safe_permissions(boost::filesystem::perms permissions)
  {
    const boost::filesystem::perms forbidden =
        boost::filesystem::group_read | boost::filesystem::group_write
        | boost::filesystem::group_exe | boost::filesystem::others_read
        | boost::filesystem::others_write | boost::filesystem::others_exe;
    return (permissions & forbidden) == boost::filesystem::no_perms;
  }
}

namespace qwertycoin
{
namespace epose
{
  bool service_keystore_context_v2::valid() const
  {
    return nettype != cryptonote::UNDEFINED
        && genesis_hash != crypto::null_hash
        && parameter_set_hash != crypto::null_hash;
  }

  service_keystore_status_v2 load_or_create_service_keystore_v2(
      const std::string &path_string,
      const service_keystore_context_v2 &context,
      service_keystore_v2 &keystore,
      std::string &error)
  {
    keystore = {};
    error.clear();
    if (!context.valid())
    {
      error = "invalid v2 keystore binding context";
      return service_keystore_status_v2::invalid_context;
    }
    if (path_string.empty())
    {
      error = "v2 keystore path is empty";
      return service_keystore_status_v2::unsafe_path;
    }

    const boost::filesystem::path path(path_string);
    boost::system::error_code ec;
    const boost::filesystem::file_status status =
        boost::filesystem::symlink_status(path, ec);
    if (ec && ec.value() != boost::system::errc::no_such_file_or_directory)
    {
      error = "failed to inspect v2 keystore: " + ec.message();
      return service_keystore_status_v2::io_error;
    }

    if (!ec && boost::filesystem::exists(status))
    {
      if (boost::filesystem::is_symlink(status)
          || !boost::filesystem::is_regular_file(status))
      {
        error = "v2 keystore must be a regular non-symlink file";
        return service_keystore_status_v2::unsafe_path;
      }
      if (!safe_permissions(status.permissions()))
      {
        error = "v2 keystore permissions grant group or other access";
        return service_keystore_status_v2::unsafe_permissions;
      }

      std::string contents;
      std::map<std::string, std::string> fields;
      if (!epee::file_io_utils::load_file_to_string(path.string(), contents, 4096))
      {
        error = "failed to read v2 keystore";
        return service_keystore_status_v2::io_error;
      }
      if (!parse_lines(contents, fields))
      {
        error = "malformed v2 keystore";
        return service_keystore_status_v2::malformed_file;
      }
      if (fields["network_id"] != network_binding(context.nettype)
          || fields["genesis_hash"] != epee::string_tools::pod_to_hex(context.genesis_hash)
          || fields["parameter_set_hash"]
              != epee::string_tools::pod_to_hex(context.parameter_set_hash))
      {
        error = "v2 keystore is bound to a different chain or parameter set";
        return service_keystore_status_v2::wrong_binding;
      }
      crypto::hash stored_checksum{};
      if (!epee::string_tools::hex_to_pod(
              fields["operator_secret_key"], unwrap(unwrap(keystore.operator_secret_key)))
          || !epee::string_tools::hex_to_pod(
              fields["service_secret_key"], unwrap(unwrap(keystore.service_secret_key)))
          || !epee::string_tools::hex_to_pod(fields["checksum"], stored_checksum)
          || stored_checksum != checksum(
                 context, keystore.operator_secret_key, keystore.service_secret_key)
          || !derive_keys(keystore))
      {
        keystore = {};
        error = "v2 keystore contains invalid or corrupted keys";
        return service_keystore_status_v2::invalid_key;
      }
      if (keystore.operator_public_key == keystore.service_public_key)
      {
        keystore = {};
        error = "v2 operator and service authorities must be distinct";
        return service_keystore_status_v2::nonseparated_authorities;
      }
      return service_keystore_status_v2::loaded;
    }

    const boost::filesystem::path parent = path.parent_path();
    if (!parent.empty())
      boost::filesystem::create_directories(parent, ec);
    if (ec)
    {
      error = "failed to create v2 keystore directory: " + ec.message();
      return service_keystore_status_v2::io_error;
    }

    crypto::generate_keys(keystore.operator_public_key, keystore.operator_secret_key);
    do
    {
      crypto::generate_keys(keystore.service_public_key, keystore.service_secret_key);
    } while (keystore.operator_public_key == keystore.service_public_key);

    const crypto::hash digest = checksum(
        context, keystore.operator_secret_key, keystore.service_secret_key);
    std::string contents(KEYSTORE_HEADER);
    contents += "\nnetwork_id=" + network_binding(context.nettype);
    contents += "\ngenesis_hash=" + epee::string_tools::pod_to_hex(context.genesis_hash);
    contents += "\nparameter_set_hash=" + epee::string_tools::pod_to_hex(context.parameter_set_hash);
    contents += "\noperator_secret_key="
        + epee::string_tools::pod_to_hex(unwrap(unwrap(keystore.operator_secret_key)));
    contents += "\nservice_secret_key="
        + epee::string_tools::pod_to_hex(unwrap(unwrap(keystore.service_secret_key)));
    contents += "\nchecksum=" + epee::string_tools::pod_to_hex(digest) + "\n";

    const boost::filesystem::path temporary =
        boost::filesystem::unique_path(path.string() + ".%%%%-%%%%.tmp", ec);
    if (ec || !epee::file_io_utils::save_string_to_file(temporary.string(), contents))
    {
      keystore = {};
      error = "failed to write temporary v2 keystore";
      return service_keystore_status_v2::io_error;
    }
    boost::filesystem::permissions(
        temporary,
        boost::filesystem::owner_read | boost::filesystem::owner_write, ec);
    if (ec)
    {
      boost::filesystem::remove(temporary);
      keystore = {};
      error = "failed to restrict temporary v2 keystore permissions: " + ec.message();
      return service_keystore_status_v2::io_error;
    }
    boost::filesystem::rename(temporary, path, ec);
    if (ec)
    {
      boost::filesystem::remove(temporary);
      keystore = {};
      error = "failed to atomically publish v2 keystore: " + ec.message();
      return service_keystore_status_v2::io_error;
    }
    return service_keystore_status_v2::created;
  }
} // namespace epose
} // namespace qwertycoin
