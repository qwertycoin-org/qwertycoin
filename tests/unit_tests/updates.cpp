// Copyright (c) 2026, The Qwertycoin Project
//
// All rights reserved.

#include "gtest/gtest.h"

#include "common/updates.h"

namespace
{
  constexpr const char hash_a[] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  constexpr const char hash_b[] = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
}

TEST(updates, selects_exact_target_and_highest_version)
{
  const std::vector<std::string> records = {
      std::string("qwertycoin:linux-x64:2.0.2:") + hash_a,
      std::string("qwertycoin:linux-x64:2.1.0:") + hash_b,
      std::string("qwertycoin:win-x64:9.0.0:") + hash_a,
      std::string("qwertycoin-gui:linux-x64:9.0.0:") + hash_a};
  std::string version, hash;
  ASSERT_TRUE(tools::select_update_record(records, "qwertycoin", "linux-x64", version, hash));
  EXPECT_EQ(version, "2.1.0");
  EXPECT_EQ(hash, hash_b);
}

TEST(updates, ignores_placeholder_and_unrelated_records)
{
  const std::vector<std::string> records = {
      "qwc:update-metadata-not-yet-published",
      std::string("qwertycoin:win-x64:2.0.2:") + hash_a};
  std::string version = "stale", hash = "stale";
  EXPECT_FALSE(tools::select_update_record(records, "qwertycoin", "linux-x64", version, hash));
  EXPECT_TRUE(version.empty());
  EXPECT_TRUE(hash.empty());
}

TEST(updates, rejects_conflicting_hashes)
{
  const std::vector<std::string> records = {
      std::string("qwertycoin:linux-x64:2.0.2:") + hash_a,
      std::string("qwertycoin:linux-x64:2.0.2:") + hash_b};
  std::string version, hash;
  EXPECT_FALSE(tools::select_update_record(records, "qwertycoin", "linux-x64", version, hash));
  EXPECT_TRUE(version.empty());
  EXPECT_TRUE(hash.empty());
}

TEST(updates, rejects_malformed_matching_metadata)
{
  const std::vector<std::vector<std::string>> cases = {
      {"qwertycoin:linux-x64:2.0.2"},
      {"qwertycoin:linux-x64:2.0:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"},
      {"qwertycoin:linux-x64:2.0.2.0:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"},
      {"qwertycoin:linux-x64:2.0.2:0123456789ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef"},
      {"qwertycoin:linux-x64:2.0.2:xyz"},
      {"qwertycoin:linux-x64:02.0.2:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"},
      {"qwertycoin:linux-x64:9999999999.0.0:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"},
      {"qwertycoin:linux-x64:2.0.2-rc1:0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"}};
  for (const auto &records : cases)
  {
    SCOPED_TRACE(records.front());
    std::string version, hash;
    EXPECT_FALSE(tools::select_update_record(records, "qwertycoin", "linux-x64", version, hash));
    EXPECT_TRUE(version.empty());
    EXPECT_TRUE(hash.empty());
  }

  const std::vector<std::string> mixed = {
      std::string("qwertycoin:linux-x64:2.0.2:") + hash_a,
      "qwertycoin:linux-x64:2.0.3:not-a-sha256"};
  std::string version = "stale", hash = "stale";
  EXPECT_FALSE(tools::select_update_record(mixed, "qwertycoin", "linux-x64", version, hash));
  EXPECT_TRUE(version.empty());
  EXPECT_TRUE(hash.empty());
}

TEST(updates, rejects_unsupported_targets)
{
  const std::vector<std::string> records = {
      std::string("qwertycoin:source:2.0.2:") + hash_a,
      std::string("qwertycoin:mac-x64:2.0.2:") + hash_a};
  std::string version, hash;
  EXPECT_FALSE(tools::select_update_record(records, "qwertycoin", "source", version, hash));
  EXPECT_FALSE(tools::select_update_record(records, "qwertycoin", "mac-x64", version, hash));
}

TEST(updates, maps_core_release_urls)
{
  EXPECT_EQ(tools::get_update_url("qwertycoin", "cli", "linux-x64", "2.0.2", true),
      "https://github.com/qwertycoin-org/qwertycoin/releases/tag/v2.0.2");
  EXPECT_EQ(tools::get_update_url("qwertycoin", "cli", "linux-x64", "2.0.2", false),
      "https://github.com/qwertycoin-org/qwertycoin/releases/download/v2.0.2/qwertycoin-v2.0.2-linux-x86_64.tar.gz");
  EXPECT_EQ(tools::get_update_url("qwertycoin", "cli", "mac-armv8", "2.0.2", false),
      "https://github.com/qwertycoin-org/qwertycoin/releases/download/v2.0.2/qwertycoin-v2.0.2-macos-arm64.tar.gz");
  EXPECT_EQ(tools::get_update_url("qwertycoin", "cli", "win-x64", "2.0.2", false),
      "https://github.com/qwertycoin-org/qwertycoin/releases/download/v2.0.2/qwertycoin-v2.0.2-windows-x86_64.zip");
}

TEST(updates, maps_gui_release_urls)
{
  EXPECT_EQ(tools::get_update_url("qwertycoin-gui", "gui", "linux-x64", "2.0.2", false),
      "https://github.com/qwertycoin-org/qwertycoin-gui/releases/download/v2.0.2/qwertycoin-gui-v2.0.2-linux-x86_64.tar.gz");
  EXPECT_EQ(tools::get_update_url("qwertycoin-gui", "gui", "mac-armv8", "2.0.2", false),
      "https://github.com/qwertycoin-org/qwertycoin-gui/releases/download/v2.0.2/qwertycoin-gui-v2.0.2-macos-arm64.dmg");
  EXPECT_EQ(tools::get_update_url("qwertycoin-gui", "gui", "install-win-x64", "2.0.2", false),
      "https://github.com/qwertycoin-org/qwertycoin-gui/releases/download/v2.0.2/qwertycoin-gui-v2.0.2-windows-x86_64-setup.exe");
  EXPECT_EQ(tools::get_update_url("qwertycoin-gui", "gui", "win-x64", "2.0.2", false),
      "https://github.com/qwertycoin-org/qwertycoin-gui/releases/download/v2.0.2/qwertycoin-gui-v2.0.2-windows-x86_64.zip");
}

TEST(updates, rejects_unsafe_or_unknown_urls)
{
  EXPECT_TRUE(tools::get_update_url("qwertycoin", "cli", "source", "2.0.2", false).empty());
  EXPECT_TRUE(tools::get_update_url("qwertycoin", "cli", "linux-x64", "../../evil", false).empty());
  EXPECT_TRUE(tools::get_update_url("other", "cli", "linux-x64", "2.0.2", false).empty());
}
