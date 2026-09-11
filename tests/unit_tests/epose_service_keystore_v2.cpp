// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <cstring>

#include <boost/filesystem.hpp>

#include "epose/service_keystore_v2.h"
#include "file_io_utils.h"

namespace
{
  using namespace qwertycoin::epose;

  crypto::hash hash_text(const char *text)
  {
    return crypto::cn_fast_hash(text, std::strlen(text));
  }

  struct temporary_directory
  {
    boost::filesystem::path path = boost::filesystem::unique_path(
        boost::filesystem::temp_directory_path() / "qwc-epose-keystore-%%%%-%%%%");

    temporary_directory() { boost::filesystem::create_directories(path); }
    ~temporary_directory()
    {
      boost::system::error_code ignored;
      boost::filesystem::remove_all(path, ignored);
    }
  };

  service_keystore_context_v2 context()
  {
    return {cryptonote::MAINNET, hash_text("genesis"), hash_text("parameters")};
  }
}

TEST(epose_service_keystore_v2, creates_atomic_owner_only_distinct_bound_keys_and_reloads_them)
{
  temporary_directory directory;
  const boost::filesystem::path path = directory.path / "service.keys";
  service_keystore_v2 first{};
  std::string error;
  ASSERT_EQ(service_keystore_status_v2::created,
      load_or_create_service_keystore_v2(path.string(), context(), first, error)) << error;
  ASSERT_NE(first.operator_public_key, first.service_public_key);
  EXPECT_TRUE(boost::filesystem::is_regular_file(path));
  const auto permissions = boost::filesystem::status(path).permissions();
  EXPECT_EQ(boost::filesystem::no_perms,
      permissions & (boost::filesystem::group_read | boost::filesystem::group_write
          | boost::filesystem::group_exe | boost::filesystem::others_read
          | boost::filesystem::others_write | boost::filesystem::others_exe));

  service_keystore_v2 loaded{};
  ASSERT_EQ(service_keystore_status_v2::loaded,
      load_or_create_service_keystore_v2(path.string(), context(), loaded, error)) << error;
  EXPECT_EQ(first.operator_public_key, loaded.operator_public_key);
  EXPECT_EQ(first.operator_secret_key, loaded.operator_secret_key);
  EXPECT_EQ(first.service_public_key, loaded.service_public_key);
  EXPECT_EQ(first.service_secret_key, loaded.service_secret_key);
}

TEST(epose_service_keystore_v2, rejects_wrong_chain_permissions_symlinks_and_legacy_files)
{
  temporary_directory directory;
  const boost::filesystem::path path = directory.path / "service.keys";
  service_keystore_v2 keys{};
  std::string error;
  ASSERT_EQ(service_keystore_status_v2::created,
      load_or_create_service_keystore_v2(path.string(), context(), keys, error)) << error;

  auto wrong = context();
  wrong.genesis_hash = hash_text("other-genesis");
  EXPECT_EQ(service_keystore_status_v2::wrong_binding,
      load_or_create_service_keystore_v2(path.string(), wrong, keys, error));

  boost::filesystem::permissions(path,
      boost::filesystem::owner_read | boost::filesystem::owner_write
          | boost::filesystem::group_read);
  EXPECT_EQ(service_keystore_status_v2::unsafe_permissions,
      load_or_create_service_keystore_v2(path.string(), context(), keys, error));
  boost::filesystem::permissions(path,
      boost::filesystem::owner_read | boost::filesystem::owner_write);

  const boost::filesystem::path link = directory.path / "service-link.keys";
  boost::filesystem::create_symlink(path, link);
  EXPECT_EQ(service_keystore_status_v2::unsafe_path,
      load_or_create_service_keystore_v2(link.string(), context(), keys, error));

  const boost::filesystem::path legacy = directory.path / "legacy.keys";
  ASSERT_TRUE(epee::file_io_utils::save_string_to_file(
      legacy.string(), "QWC_EPOSE_SERVICE_NODE_KEY_V1\n00\n"));
  boost::filesystem::permissions(legacy,
      boost::filesystem::owner_read | boost::filesystem::owner_write);
  EXPECT_EQ(service_keystore_status_v2::malformed_file,
      load_or_create_service_keystore_v2(legacy.string(), context(), keys, error));
}

TEST(epose_service_keystore_v2, rejects_invalid_context_without_creating_a_file)
{
  temporary_directory directory;
  const boost::filesystem::path path = directory.path / "service.keys";
  service_keystore_v2 keys{};
  std::string error;
  service_keystore_context_v2 invalid{};
  EXPECT_EQ(service_keystore_status_v2::invalid_context,
      load_or_create_service_keystore_v2(path.string(), invalid, keys, error));
  EXPECT_FALSE(boost::filesystem::exists(path));
}
