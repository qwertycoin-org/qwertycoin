// Copyright (c) 2026, The Qwertycoin Project
// SPDX-License-Identifier: BSD-3-Clause

#include "transport_policy.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>

namespace qwertycoin::qms
{
  namespace
  {
    bool fail(std::string *reason, const char *message)
    {
      if (reason)
        *reason = message;
      return false;
    }

    bool ascii_space_or_control(const unsigned char c)
    {
      return c <= 0x20 || c == 0x7f;
    }
  }

  bool strict_native_transport_ready(const std::string &proxy,
      const std::string &daemon_url, std::string *reason)
  {
    if (reason)
      reason->clear();
    if (proxy.empty())
      return fail(reason, "QMS2 requires an explicit SOCKS proxy");
    if (std::any_of(proxy.begin(), proxy.end(), [](unsigned char c) { return ascii_space_or_control(c); }))
      return fail(reason, "QMS2 proxy contains whitespace or control characters");
    if (daemon_url.empty() ||
        std::any_of(daemon_url.begin(), daemon_url.end(), [](unsigned char c) { return ascii_space_or_control(c); }))
      return fail(reason, "QMS2 daemon URL is empty or malformed");

    constexpr const char *separator = "://";
    const size_t scheme_end = daemon_url.find(separator);
    if (scheme_end == std::string::npos)
      return fail(reason, "QMS2 daemon URL must include an http or https scheme");
    std::string scheme = daemon_url.substr(0, scheme_end);
    std::transform(scheme.begin(), scheme.end(), scheme.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (scheme != "http" && scheme != "https")
      return fail(reason, "QMS2 daemon URL must use http or https over Tor");

    const size_t authority_begin = scheme_end + 3;
    const size_t authority_end = daemon_url.find_first_of("/?#", authority_begin);
    std::string authority = daemon_url.substr(authority_begin,
        authority_end == std::string::npos ? std::string::npos : authority_end - authority_begin);
    if (authority.empty() || authority.find('@') != std::string::npos || authority.front() == '[')
      return fail(reason, "QMS2 daemon URL must contain a plain Tor v3 host");

    std::string host = authority;
    const size_t colon = authority.rfind(':');
    if (colon != std::string::npos)
    {
      host = authority.substr(0, colon);
      const std::string port = authority.substr(colon + 1);
      if (port.empty() || !std::all_of(port.begin(), port.end(),
          [](unsigned char c) { return std::isdigit(c) != 0; }))
        return fail(reason, "QMS2 daemon URL has an invalid port");
      unsigned long parsed = 0;
      try { parsed = std::stoul(port); }
      catch (...) { return fail(reason, "QMS2 daemon URL has an invalid port"); }
      if (parsed == 0 || parsed > std::numeric_limits<uint16_t>::max())
        return fail(reason, "QMS2 daemon URL port is outside the valid range");
    }

    std::transform(host.begin(), host.end(), host.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    constexpr size_t onion_suffix_size = 6;
    constexpr size_t v3_label_size = 56;
    if (host.size() != v3_label_size + onion_suffix_size ||
        host.compare(v3_label_size, onion_suffix_size, ".onion") != 0)
      return fail(reason, "QMS2 requires a Tor v3 onion daemon");
    if (!std::all_of(host.begin(), host.begin() + v3_label_size, [](unsigned char c) {
          return (c >= 'a' && c <= 'z') || (c >= '2' && c <= '7');
        }))
      return fail(reason, "QMS2 daemon onion host is not valid base32");
    return true;
  }
}
