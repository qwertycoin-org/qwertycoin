// Copyright (c) 2026, The Qwertycoin Project
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>

namespace qwertycoin::qms
{
  /**
   * Proves the fail-closed native QMS2 transport configuration without
   * opening a socket or starting Tor. Native network operations require an
   * explicit SOCKS proxy and a syntactically valid Tor v3 daemon target.
   */
  bool strict_native_transport_ready(const std::string &proxy,
      const std::string &daemon_url, std::string *reason = nullptr);
}
