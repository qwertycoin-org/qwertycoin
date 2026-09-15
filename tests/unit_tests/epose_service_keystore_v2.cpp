// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include <boost/filesystem.hpp>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <aclapi.h>
#include <windows.h>
#endif

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

  std::string contents(const boost::filesystem::path &path)
  {
    std::string value;
    EXPECT_TRUE(epee::file_io_utils::load_file_to_string(path.string(), value));
    return value;
  }

#ifdef _WIN32
  std::vector<uint8_t> token_user_sid()
  {
    HANDLE token = nullptr;
    EXPECT_NE(0, OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token));
    if (token == nullptr)
      return {};
    DWORD size = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &size);
    std::vector<uint8_t> token_user(size);
    EXPECT_NE(0, GetTokenInformation(
        token, TokenUser, token_user.data(), size, &size));
    CloseHandle(token);
    if (token_user.empty())
      return {};
    const PSID source = reinterpret_cast<TOKEN_USER *>(token_user.data())->User.Sid;
    std::vector<uint8_t> sid(GetLengthSid(source));
    EXPECT_NE(0, CopySid(static_cast<DWORD>(sid.size()), sid.data(), source));
    return sid;
  }

  std::vector<uint8_t> known_sid(WELL_KNOWN_SID_TYPE type)
  {
    std::vector<uint8_t> sid(SECURITY_MAX_SID_SIZE);
    DWORD size = static_cast<DWORD>(sid.size());
    EXPECT_NE(0, CreateWellKnownSid(type, nullptr, sid.data(), &size));
    sid.resize(size);
    return sid;
  }

  void set_test_acl(
      const boost::filesystem::path &path,
      bool grant_everyone,
      bool inherited_everyone,
      bool protected_dacl,
      bool include_system_and_administrators = true)
  {
    const std::vector<uint8_t> user = token_user_sid();
    const std::vector<uint8_t> system = known_sid(WinLocalSystemSid);
    const std::vector<uint8_t> administrators =
        known_sid(WinBuiltinAdministratorsSid);
    const std::vector<uint8_t> everyone = known_sid(WinWorldSid);
    const std::vector<PSID> sids{
        const_cast<uint8_t *>(user.data()),
        const_cast<uint8_t *>(system.data()),
        const_cast<uint8_t *>(administrators.data()),
        const_cast<uint8_t *>(everyone.data())};
    DWORD acl_size = sizeof(ACL);
    for (const PSID sid : sids)
      acl_size += sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD) + GetLengthSid(sid);
    std::vector<uint8_t> storage(acl_size);
    PACL acl = reinterpret_cast<PACL>(storage.data());
    ASSERT_NE(0, InitializeAcl(acl, acl_size, ACL_REVISION));
    ASSERT_NE(0, AddAccessAllowedAceEx(
        acl, ACL_REVISION, 0, FILE_ALL_ACCESS, sids[0]));
    if (include_system_and_administrators)
    {
      ASSERT_NE(0, AddAccessAllowedAceEx(
          acl, ACL_REVISION, 0, FILE_ALL_ACCESS, sids[1]));
      ASSERT_NE(0, AddAccessAllowedAceEx(
          acl, ACL_REVISION, 0, FILE_ALL_ACCESS, sids[2]));
    }
    if (grant_everyone)
    {
      ASSERT_NE(0, AddAccessAllowedAceEx(
          acl, ACL_REVISION, inherited_everyone ? INHERITED_ACE : 0,
          FILE_GENERIC_READ, sids[3]));
    }
    std::wstring path_wide = path.wstring();
    const SECURITY_INFORMATION inheritance = protected_dacl
        ? PROTECTED_DACL_SECURITY_INFORMATION
        : UNPROTECTED_DACL_SECURITY_INFORMATION;
    ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), SetNamedSecurityInfoW(
        &path_wide[0], SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION | inheritance,
        nullptr, nullptr, acl, nullptr));
  }

  void set_null_dacl(const boost::filesystem::path &path)
  {
    std::wstring path_wide = path.wstring();
    ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS), SetNamedSecurityInfoW(
        &path_wide[0], SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
        nullptr, nullptr, nullptr, nullptr));
  }
#endif
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
#ifndef _WIN32
  const auto permissions = boost::filesystem::status(path).permissions();
  EXPECT_EQ(boost::filesystem::no_perms,
      permissions & (boost::filesystem::group_read | boost::filesystem::group_write
          | boost::filesystem::group_exe | boost::filesystem::others_read
          | boost::filesystem::others_write | boost::filesystem::others_exe));
#endif

  const std::string original_contents = contents(path);
  for (unsigned restart = 0; restart < 3; ++restart)
  {
    service_keystore_v2 loaded{};
    ASSERT_EQ(service_keystore_status_v2::loaded,
        load_or_create_service_keystore_v2(path.string(), context(), loaded, error))
        << "restart " << restart + 1 << ": " << error;
    EXPECT_EQ(first.operator_public_key, loaded.operator_public_key);
    EXPECT_EQ(first.service_public_key, loaded.service_public_key);
    EXPECT_TRUE(original_contents == contents(path));
  }
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

#ifdef _WIN32
  set_test_acl(path, true, false, true);
#else
  boost::filesystem::permissions(path,
      boost::filesystem::owner_read | boost::filesystem::owner_write
          | boost::filesystem::group_read);
#endif
  EXPECT_EQ(service_keystore_status_v2::unsafe_permissions,
      load_or_create_service_keystore_v2(path.string(), context(), keys, error));
#ifdef _WIN32
  set_test_acl(path, false, false, true);
#else
  boost::filesystem::permissions(path,
      boost::filesystem::owner_read | boost::filesystem::owner_write);
#endif

  const boost::filesystem::path link = directory.path / "service-link.keys";
#ifdef _WIN32
  ASSERT_NE(0, CreateSymbolicLinkW(
      link.wstring().c_str(), path.wstring().c_str(),
      SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)) << GetLastError();
#else
  boost::filesystem::create_symlink(path, link);
#endif
  EXPECT_EQ(service_keystore_status_v2::unsafe_path,
      load_or_create_service_keystore_v2(link.string(), context(), keys, error));

#ifdef _WIN32
  const boost::filesystem::path hard_link = directory.path / "service-hard-link.keys";
  ASSERT_NE(0, CreateHardLinkW(
      hard_link.wstring().c_str(), path.wstring().c_str(), nullptr)) << GetLastError();
  EXPECT_EQ(service_keystore_status_v2::unsafe_path,
      load_or_create_service_keystore_v2(hard_link.string(), context(), keys, error));
  ASSERT_TRUE(boost::filesystem::remove(hard_link));
#endif

  const boost::filesystem::path legacy = directory.path / "legacy.keys";
  ASSERT_TRUE(epee::file_io_utils::save_string_to_file(
      legacy.string(), "QWC_EPOSE_SERVICE_NODE_KEY_V1\n00\n"));
#ifdef _WIN32
  set_test_acl(legacy, false, false, true);
#else
  boost::filesystem::permissions(legacy,
      boost::filesystem::owner_read | boost::filesystem::owner_write);
#endif
  EXPECT_EQ(service_keystore_status_v2::malformed_file,
      load_or_create_service_keystore_v2(legacy.string(), context(), keys, error));
}

#ifdef _WIN32
TEST(epose_service_keystore_v2, rejects_inherited_and_null_dacls_and_repairs_only_acl)
{
  temporary_directory directory;
  const boost::filesystem::path path = directory.path / "service.keys";
  service_keystore_v2 original{};
  std::string error;
  ASSERT_EQ(service_keystore_status_v2::created,
      load_or_create_service_keystore_v2(path.string(), context(), original, error)) << error;
  const std::string original_contents = contents(path);

  set_test_acl(path, false, false, true, false);
  service_keystore_v2 user_only_loaded{};
  ASSERT_EQ(service_keystore_status_v2::loaded,
      load_or_create_service_keystore_v2(
          path.string(), context(), user_only_loaded, error)) << error;
  EXPECT_EQ(original.operator_public_key, user_only_loaded.operator_public_key);
  EXPECT_EQ(original.service_public_key, user_only_loaded.service_public_key);

  set_test_acl(path, true, true, false);
  service_keystore_v2 loaded{};
  EXPECT_EQ(service_keystore_status_v2::unsafe_permissions,
      load_or_create_service_keystore_v2(path.string(), context(), loaded, error));
  EXPECT_NE(std::string::npos, error.find("inherits"));
  ASSERT_TRUE(repair_service_keystore_permissions_v2(path.string(), error)) << error;
  EXPECT_TRUE(original_contents == contents(path));
  ASSERT_EQ(service_keystore_status_v2::loaded,
      load_or_create_service_keystore_v2(path.string(), context(), loaded, error)) << error;
  EXPECT_EQ(original.operator_public_key, loaded.operator_public_key);
  EXPECT_EQ(original.service_public_key, loaded.service_public_key);

  set_null_dacl(path);
  EXPECT_EQ(service_keystore_status_v2::unsafe_permissions,
      load_or_create_service_keystore_v2(path.string(), context(), loaded, error));
  EXPECT_NE(std::string::npos, error.find("DACL"));
  ASSERT_TRUE(repair_service_keystore_permissions_v2(path.string(), error)) << error;
  EXPECT_TRUE(original_contents == contents(path));
}
#else
TEST(epose_service_keystore_v2, repairs_only_posix_mode_and_preserves_keys)
{
  temporary_directory directory;
  const boost::filesystem::path path = directory.path / "service.keys";
  service_keystore_v2 original{};
  std::string error;
  ASSERT_EQ(service_keystore_status_v2::created,
      load_or_create_service_keystore_v2(path.string(), context(), original, error)) << error;
  const std::string original_contents = contents(path);
  boost::filesystem::permissions(path,
      boost::filesystem::owner_read | boost::filesystem::owner_write
          | boost::filesystem::group_read);
  ASSERT_TRUE(repair_service_keystore_permissions_v2(path.string(), error)) << error;
  EXPECT_TRUE(original_contents == contents(path));
  service_keystore_v2 loaded{};
  ASSERT_EQ(service_keystore_status_v2::loaded,
      load_or_create_service_keystore_v2(path.string(), context(), loaded, error)) << error;
  EXPECT_EQ(original.operator_public_key, loaded.operator_public_key);
  EXPECT_EQ(original.service_public_key, loaded.service_public_key);
}
#endif

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
