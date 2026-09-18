// Copyright (c) 2017-2022, The Monero Project
// Copyright (c) 2026, The Qwertycoin Project
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <set>

#include <boost/algorithm/string.hpp>

#include "common/dns_utils.h"
#include "common/util.h"
#include "misc_log_ex.h"
#include "updates.h"

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "updates"

namespace tools
{
  namespace
  {
    constexpr const char update_hostname[] = "updates.qwertycoin.org";
    constexpr const char unpublished_placeholder[] = "qwc:update-metadata-not-yet-published";

    bool valid_version(const std::string &version)
    {
      if (version.empty() || version.size() > 32)
        return false;

      size_t components = 1;
      size_t component_digits = 0;
      uint64_t component_value = 0;
      bool previous_was_dot = false;
      for (const unsigned char c : version)
      {
        if (c == '.')
        {
          if (previous_was_dot || component_digits == 0 || component_value > std::numeric_limits<int>::max())
            return false;
          previous_was_dot = true;
          component_digits = 0;
          component_value = 0;
          ++components;
        }
        else
        {
          if (c < '0' || c > '9')
            return false;
          if (component_digits != 0 && component_value == 0)
            return false;
          if (++component_digits > 9)
            return false;
          component_value = component_value * 10 + (c - '0');
          previous_was_dot = false;
        }
      }
      return !previous_was_dot && component_digits != 0 && component_value <= std::numeric_limits<int>::max() &&
          components == 3;
    }

    bool valid_sha256(const std::string &hash)
    {
      return hash.size() == 64 && std::all_of(hash.begin(), hash.end(), [](const unsigned char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
      });
    }

    bool supported_target(const std::string &software, const std::string &buildtag)
    {
      if (software == "qwertycoin")
        return buildtag == "linux-x64" || buildtag == "mac-armv8" || buildtag == "win-x64";
      if (software == "qwertycoin-gui")
        return buildtag == "linux-x64" || buildtag == "mac-armv8" ||
            buildtag == "install-win-x64" || buildtag == "win-x64";
      return false;
    }

    std::string asset_name(const std::string &software, const std::string &buildtag, const std::string &version)
    {
      if (software == "qwertycoin")
      {
        if (buildtag == "linux-x64")
          return "qwertycoin-v" + version + "-linux-x86_64.tar.gz";
        if (buildtag == "mac-armv8")
          return "qwertycoin-v" + version + "-macos-arm64.tar.gz";
        if (buildtag == "win-x64")
          return "qwertycoin-v" + version + "-windows-x86_64.zip";
      }
      else if (software == "qwertycoin-gui")
      {
        if (buildtag == "linux-x64")
          return "qwertycoin-gui-v" + version + "-linux-x86_64.tar.gz";
        if (buildtag == "mac-armv8")
          return "qwertycoin-gui-v" + version + "-macos-arm64.dmg";
        if (buildtag == "install-win-x64")
          return "qwertycoin-gui-v" + version + "-windows-x86_64-setup.exe";
        if (buildtag == "win-x64")
          return "qwertycoin-gui-v" + version + "-windows-x86_64.zip";
      }
      return {};
    }
  }

  bool select_update_record(const std::vector<std::string> &records, const std::string &software,
      const std::string &buildtag, std::string &version, std::string &hash)
  {
    version.clear();
    hash.clear();

    if (!supported_target(software, buildtag))
    {
      MDEBUG("No update artifact is defined for " << software << " on " << buildtag);
      return false;
    }

    std::map<std::string, std::set<std::string>> matches;
    for (const auto &record : records)
    {
      if (record == unpublished_placeholder)
        continue;

      std::vector<std::string> fields;
      boost::split(fields, record, boost::is_any_of(":"));
      if (fields.size() != 4)
      {
        if (boost::starts_with(record, software + ":" + buildtag + ":"))
        {
          MWARNING("Rejecting malformed update metadata for " << software << " on " << buildtag);
          version.clear();
          hash.clear();
          return false;
        }
        continue;
      }
      if (fields[0] != software || fields[1] != buildtag)
        continue;

      if (!valid_version(fields[2]) || !valid_sha256(fields[3]))
      {
        MWARNING("Rejecting malformed update metadata for " << software << " on " << buildtag);
        version.clear();
        hash.clear();
        return false;
      }
      matches[fields[2]].insert(fields[3]);
    }

    if (matches.empty())
      return false;

    for (const auto &match : matches)
    {
      if (match.second.size() != 1)
      {
        MWARNING("Conflicting hashes found for " << software << " " << match.first << " on " << buildtag);
        version.clear();
        hash.clear();
        return false;
      }
      if (version.empty() || tools::vercmp(version.c_str(), match.first.c_str()) < 0)
      {
        version = match.first;
        hash = *match.second.begin();
      }
    }

    MINFO("Found update metadata for " << software << " " << version << " on " << buildtag);
    return true;
  }

  bool check_updates(const std::string &software, const std::string &buildtag, std::string &version, std::string &hash)
  {
    MDEBUG("Checking updates for " << buildtag << " " << software);
    version.clear();
    hash.clear();

    if (!supported_target(software, buildtag))
      return false;

    bool dnssec_available = false;
    bool dnssec_valid = false;
    const std::vector<std::string> records = DNSResolver::instance().get_txt_record(
        update_hostname, dnssec_available, dnssec_valid);
    if (!dnssec_available || !dnssec_valid)
    {
      MWARNING("Ignoring update metadata because DNSSEC validation failed for " << update_hostname);
      return false;
    }

    return select_update_record(records, software, buildtag, version, hash);
  }

  std::string get_update_url(const std::string &software, const std::string &subdir, const std::string &buildtag, const std::string &version, bool user)
  {
    (void)subdir;
    if (!supported_target(software, buildtag) || !valid_version(version))
      return {};

    const std::string asset = asset_name(software, buildtag, version);
    if (asset.empty())
      return {};

    const std::string repository = software == "qwertycoin" ? "qwertycoin" : "qwertycoin-gui";
    const std::string release = "https://github.com/qwertycoin-org/" + repository + "/releases/";
    if (user)
      return release + "tag/v" + version;
    return release + "download/v" + version + "/" + asset;
  }
}
