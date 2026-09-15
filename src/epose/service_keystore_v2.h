// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>

#include "crypto/crypto.h"
#include "cryptonote_config.h"

namespace qwertycoin
{
namespace epose
{
  struct service_keystore_context_v2
  {
    cryptonote::network_type nettype = cryptonote::UNDEFINED;
    crypto::hash genesis_hash{};
    crypto::hash parameter_set_hash{};

    bool valid() const;
  };

  struct service_keystore_v2
  {
    crypto::public_key operator_public_key{};
    crypto::secret_key operator_secret_key{};
    crypto::public_key service_public_key{};
    crypto::secret_key service_secret_key{};
  };

  enum class service_keystore_status_v2
  {
    loaded,
    created,
    invalid_context,
    unsafe_path,
    unsafe_permissions,
    io_error,
    malformed_file,
    wrong_binding,
    invalid_key,
    nonseparated_authorities
  };

  // The v2 keystore is deliberately incompatible with the legacy v1 service
  // key file. It stores separate operator and online-service authorities and
  // binds them to the exact network, genesis and compiled parameter set.
  // Existing files must be regular, non-redirected files accessible only by
  // their owner (plus LocalSystem and built-in Administrators on Windows).
  // New files are written atomically with mode 0600 on POSIX or an equivalent
  // protected Windows DACL created before any key material is written.
  service_keystore_status_v2 load_or_create_service_keystore_v2(
      const std::string &path,
      const service_keystore_context_v2 &context,
      service_keystore_v2 &keystore,
      std::string &error);

  // Restrict only the ACL/mode of one existing regular keystore file. The
  // file contents, parent directory permissions and key material are never
  // modified. Loading still performs all format, binding and key checks.
  bool repair_service_keystore_permissions_v2(
      const std::string &path,
      std::string &error);
} // namespace epose
} // namespace qwertycoin
