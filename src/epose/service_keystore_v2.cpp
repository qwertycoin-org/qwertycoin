// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/service_keystore_v2.h"

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <system_error>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <aclapi.h>
#include <windows.h>
#endif

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

#ifndef _WIN32
  bool safe_posix_permissions(boost::filesystem::perms permissions)
  {
    const boost::filesystem::perms forbidden =
        boost::filesystem::group_read | boost::filesystem::group_write
        | boost::filesystem::group_exe | boost::filesystem::others_read
        | boost::filesystem::others_write | boost::filesystem::others_exe;
    return (permissions & forbidden) == boost::filesystem::no_perms;
  }
#else
  class windows_handle
  {
  public:
    windows_handle() = default;
    explicit windows_handle(HANDLE value) : value_(value) {}
    windows_handle(const windows_handle &) = delete;
    windows_handle &operator=(const windows_handle &) = delete;
    windows_handle(windows_handle &&other) noexcept : value_(other.release()) {}
    windows_handle &operator=(windows_handle &&other) noexcept
    {
      if (this != &other)
      {
        reset();
        value_ = other.release();
      }
      return *this;
    }
    ~windows_handle() { reset(); }

    bool valid() const { return value_ != nullptr && value_ != INVALID_HANDLE_VALUE; }
    HANDLE get() const { return value_; }
    HANDLE release()
    {
      const HANDLE value = value_;
      value_ = INVALID_HANDLE_VALUE;
      return value;
    }
    void reset(HANDLE value = INVALID_HANDLE_VALUE)
    {
      if (valid())
        CloseHandle(value_);
      value_ = value;
    }

  private:
    HANDLE value_ = INVALID_HANDLE_VALUE;
  };

  struct local_free
  {
    void operator()(void *value) const noexcept
    {
      if (value != nullptr)
        LocalFree(value);
    }
  };

  std::string windows_error(const DWORD code)
  {
    return std::error_code(static_cast<int>(code), std::system_category()).message();
  }

  bool wide_path(
      const boost::filesystem::path &path,
      std::wstring &wide,
      std::string &error)
  {
    try
    {
      wide = epee::string_tools::utf8_to_utf16(path.string());
      return true;
    }
    catch (const std::exception &exception)
    {
      error = "failed to convert v2 keystore path to UTF-16: ";
      error += exception.what();
      return false;
    }
  }

  bool current_user_sid(std::vector<uint8_t> &sid, std::string &error)
  {
    HANDLE raw_token = INVALID_HANDLE_VALUE;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw_token))
    {
      error = "failed to query current Windows account: " + windows_error(GetLastError());
      return false;
    }
    windows_handle token(raw_token);

    DWORD size = 0;
    GetTokenInformation(token.get(), TokenUser, nullptr, 0, &size);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0)
    {
      error = "failed to size current Windows account SID: " + windows_error(GetLastError());
      return false;
    }
    std::vector<uint8_t> token_user(size);
    if (!GetTokenInformation(
            token.get(), TokenUser, token_user.data(), size, &size))
    {
      error = "failed to read current Windows account SID: " + windows_error(GetLastError());
      return false;
    }
    const PSID source = reinterpret_cast<TOKEN_USER *>(token_user.data())->User.Sid;
    if (!IsValidSid(source))
    {
      error = "current Windows account SID is invalid";
      return false;
    }
    sid.resize(GetLengthSid(source));
    if (!CopySid(static_cast<DWORD>(sid.size()), sid.data(), source))
    {
      error = "failed to copy current Windows account SID: " + windows_error(GetLastError());
      return false;
    }
    return true;
  }

  bool well_known_sid(
      const WELL_KNOWN_SID_TYPE type,
      std::vector<uint8_t> &sid,
      std::string &error)
  {
    sid.resize(SECURITY_MAX_SID_SIZE);
    DWORD size = static_cast<DWORD>(sid.size());
    if (!CreateWellKnownSid(type, nullptr, sid.data(), &size))
    {
      error = "failed to create required Windows security SID: "
          + windows_error(GetLastError());
      return false;
    }
    sid.resize(size);
    return true;
  }

  struct windows_private_security
  {
    std::vector<uint8_t> user_sid;
    std::vector<uint8_t> system_sid;
    std::vector<uint8_t> administrators_sid;
    std::vector<uint8_t> acl_storage;
    SECURITY_DESCRIPTOR descriptor{};

    PACL acl() { return reinterpret_cast<PACL>(acl_storage.data()); }

    bool initialize(std::string &error)
    {
      if (!current_user_sid(user_sid, error)
          || !well_known_sid(WinLocalSystemSid, system_sid, error)
          || !well_known_sid(
              WinBuiltinAdministratorsSid, administrators_sid, error))
        return false;

      const std::array<PSID, 3> sids{{
          user_sid.data(), system_sid.data(), administrators_sid.data()}};
      DWORD acl_size = sizeof(ACL);
      for (const PSID sid : sids)
        acl_size += sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD) + GetLengthSid(sid);
      acl_storage.resize(acl_size);
      if (!InitializeAcl(acl(), acl_size, ACL_REVISION))
      {
        error = "failed to initialize private Windows keystore ACL: "
            + windows_error(GetLastError());
        return false;
      }
      for (const PSID sid : sids)
      {
        if (!AddAccessAllowedAceEx(
                acl(), ACL_REVISION, 0, FILE_ALL_ACCESS, sid))
        {
          error = "failed to build private Windows keystore ACL: "
              + windows_error(GetLastError());
          return false;
        }
      }
      if (!InitializeSecurityDescriptor(
              &descriptor, SECURITY_DESCRIPTOR_REVISION)
          || !SetSecurityDescriptorOwner(
              &descriptor, user_sid.data(), FALSE)
          || !SetSecurityDescriptorDacl(
              &descriptor, TRUE, acl(), FALSE)
          || !SetSecurityDescriptorControl(
              &descriptor, SE_DACL_PROTECTED, SE_DACL_PROTECTED))
      {
        error = "failed to build private Windows keystore security descriptor: "
            + windows_error(GetLastError());
        return false;
      }
      return true;
    }
  };

  bool regular_non_reparse_file(HANDLE file, std::string &error)
  {
    if (GetFileType(file) != FILE_TYPE_DISK)
    {
      error = "v2 keystore must be a regular non-reparse file";
      return false;
    }
    BY_HANDLE_FILE_INFORMATION information{};
    if (!GetFileInformationByHandle(file, &information))
    {
      error = "failed to inspect v2 keystore file type: "
          + windows_error(GetLastError());
      return false;
    }
    if ((information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0
        || (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
    {
      error = "v2 keystore must be a regular non-reparse file";
      return false;
    }
    if (information.nNumberOfLinks != 1)
    {
      error = "v2 keystore must not have additional hard links";
      return false;
    }
    return true;
  }

  bool owner_is_current_user(HANDLE file, std::string &error)
  {
    std::vector<uint8_t> user_sid;
    if (!current_user_sid(user_sid, error))
      return false;

    PSID owner = nullptr;
    PSECURITY_DESCRIPTOR raw_descriptor = nullptr;
    const DWORD result = GetSecurityInfo(
        file, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION,
        &owner, nullptr, nullptr, nullptr, &raw_descriptor);
    std::unique_ptr<void, local_free> descriptor(raw_descriptor);
    if (result != ERROR_SUCCESS)
    {
      error = "failed to inspect v2 keystore owner: " + windows_error(result);
      return false;
    }
    if (owner == nullptr || !IsValidSid(owner))
    {
      error = "v2 keystore owner SID is missing or invalid";
      return false;
    }
    if (!EqualSid(owner, user_sid.data()))
    {
      error = "v2 keystore owner is not the current Windows account";
      return false;
    }
    return true;
  }

  bool safe_windows_acl(HANDLE file, std::string &error)
  {
    std::vector<uint8_t> user_sid;
    std::vector<uint8_t> system_sid;
    std::vector<uint8_t> administrators_sid;
    if (!current_user_sid(user_sid, error)
        || !well_known_sid(WinLocalSystemSid, system_sid, error)
        || !well_known_sid(
            WinBuiltinAdministratorsSid, administrators_sid, error))
      return false;

    PSID owner = nullptr;
    PACL dacl = nullptr;
    PSECURITY_DESCRIPTOR raw_descriptor = nullptr;
    const DWORD result = GetSecurityInfo(
        file, SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
        &owner, nullptr, &dacl, nullptr, &raw_descriptor);
    std::unique_ptr<void, local_free> descriptor(raw_descriptor);
    if (result != ERROR_SUCCESS)
    {
      error = "failed to inspect v2 keystore ACL: " + windows_error(result);
      return false;
    }
    if (owner == nullptr || !IsValidSid(owner)
        || !EqualSid(owner, user_sid.data()))
    {
      error = "v2 keystore owner is not the current Windows account";
      return false;
    }
    if (dacl == nullptr || !IsValidAcl(dacl))
    {
      error = "v2 keystore has no reliably enforceable Windows DACL";
      return false;
    }

    SECURITY_DESCRIPTOR_CONTROL control = 0;
    DWORD revision = 0;
    if (!GetSecurityDescriptorControl(raw_descriptor, &control, &revision))
    {
      error = "failed to inspect v2 keystore ACL inheritance: "
          + windows_error(GetLastError());
      return false;
    }
    if ((control & SE_DACL_PROTECTED) == 0)
    {
      error = "v2 keystore ACL inherits permissions from its parent directory";
      return false;
    }

    ACL_SIZE_INFORMATION acl_information{};
    if (!GetAclInformation(
            dacl, &acl_information, sizeof(acl_information), AclSizeInformation))
    {
      error = "failed to enumerate v2 keystore ACL: "
          + windows_error(GetLastError());
      return false;
    }

    ACCESS_MASK user_access = 0;
    for (DWORD index = 0; index < acl_information.AceCount; ++index)
    {
      void *raw_ace = nullptr;
      if (!GetAce(dacl, index, &raw_ace) || raw_ace == nullptr)
      {
        error = "failed to read v2 keystore ACL entry: "
            + windows_error(GetLastError());
        return false;
      }
      const auto *header = static_cast<const ACE_HEADER *>(raw_ace);
      if ((header->AceFlags & INHERITED_ACE) != 0)
      {
        error = "v2 keystore ACL contains inherited permissions";
        return false;
      }
      if (header->AceType != ACCESS_ALLOWED_ACE_TYPE)
      {
        error = "v2 keystore ACL contains an unsupported access-control entry";
        return false;
      }
      const auto *ace = static_cast<const ACCESS_ALLOWED_ACE *>(raw_ace);
      const PSID sid = const_cast<DWORD *>(&ace->SidStart);
      if (!IsValidSid(sid))
      {
        error = "v2 keystore ACL contains an invalid SID";
        return false;
      }
      const bool is_user = EqualSid(sid, user_sid.data()) != FALSE;
      const bool is_system = EqualSid(sid, system_sid.data()) != FALSE;
      const bool is_administrator =
          EqualSid(sid, administrators_sid.data()) != FALSE;
      if (!is_user && !is_system && !is_administrator)
      {
        error = "v2 keystore ACL grants access outside the current account, LocalSystem, or built-in Administrators";
        return false;
      }
      if (is_user)
      {
        ACCESS_MASK mapped = ace->Mask;
        GENERIC_MAPPING mapping{
            FILE_GENERIC_READ, FILE_GENERIC_WRITE,
            FILE_GENERIC_EXECUTE, FILE_ALL_ACCESS};
        MapGenericMask(&mapped, &mapping);
        user_access |= mapped;
      }
    }

    const ACCESS_MASK required = FILE_GENERIC_READ | FILE_GENERIC_WRITE
        | READ_CONTROL | WRITE_DAC;
    if ((user_access & required) != required)
    {
      error = "v2 keystore ACL does not grant the current Windows account required read/write control";
      return false;
    }
    return true;
  }

  qwertycoin::epose::service_keystore_status_v2 load_windows_keystore_file(
      const boost::filesystem::path &path,
      std::string &contents,
      std::string &error)
  {
    std::wstring path_wide;
    if (!wide_path(path, path_wide, error))
      return qwertycoin::epose::service_keystore_status_v2::unsafe_path;
    windows_handle file(CreateFileW(
        path_wide.c_str(), GENERIC_READ | READ_CONTROL,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if (!file.valid())
    {
      error = "failed to open v2 keystore for ACL validation: "
          + windows_error(GetLastError());
      return qwertycoin::epose::service_keystore_status_v2::unsafe_permissions;
    }
    if (!regular_non_reparse_file(file.get(), error))
      return qwertycoin::epose::service_keystore_status_v2::unsafe_path;
    if (!safe_windows_acl(file.get(), error))
      return qwertycoin::epose::service_keystore_status_v2::unsafe_permissions;

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file.get(), &size) || size.QuadPart < 0 || size.QuadPart > 4096)
    {
      error = "v2 keystore size is invalid or exceeds 4096 bytes";
      return qwertycoin::epose::service_keystore_status_v2::io_error;
    }
    contents.assign(static_cast<size_t>(size.QuadPart), '\0');
    DWORD offset = 0;
    while (offset < contents.size())
    {
      DWORD read = 0;
      const DWORD remaining = static_cast<DWORD>(contents.size() - offset);
      if (!ReadFile(file.get(), &contents[offset], remaining, &read, nullptr)
          || read == 0)
      {
        error = "failed to read v2 keystore: " + windows_error(GetLastError());
        return qwertycoin::epose::service_keystore_status_v2::io_error;
      }
      offset += read;
    }
    return qwertycoin::epose::service_keystore_status_v2::loaded;
  }

  bool write_windows_keystore_file(
      const boost::filesystem::path &path,
      const std::string &contents,
      std::string &error)
  {
    std::wstring path_wide;
    if (!wide_path(path, path_wide, error))
      return false;
    windows_private_security security;
    if (!security.initialize(error))
      return false;
    SECURITY_ATTRIBUTES attributes{
        sizeof(SECURITY_ATTRIBUTES), &security.descriptor, FALSE};
    windows_handle file(CreateFileW(
        path_wide.c_str(), GENERIC_WRITE | READ_CONTROL,
        0, &attributes, CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if (!file.valid())
    {
      error = "failed to securely create temporary v2 keystore: "
          + windows_error(GetLastError());
      return false;
    }
    DWORD offset = 0;
    while (offset < contents.size())
    {
      DWORD written = 0;
      const DWORD remaining = static_cast<DWORD>(contents.size() - offset);
      if (!WriteFile(file.get(), contents.data() + offset, remaining, &written, nullptr)
          || written == 0)
      {
        error = "failed to write temporary v2 keystore: "
            + windows_error(GetLastError());
        return false;
      }
      offset += written;
    }
    if (!FlushFileBuffers(file.get()))
    {
      error = "failed to flush temporary v2 keystore: "
          + windows_error(GetLastError());
      return false;
    }
    if (!regular_non_reparse_file(file.get(), error)
        || !safe_windows_acl(file.get(), error))
      return false;
    return true;
  }

  bool publish_windows_keystore_file(
      const boost::filesystem::path &temporary,
      const boost::filesystem::path &path,
      std::string &error)
  {
    std::wstring temporary_wide;
    std::wstring path_wide;
    if (!wide_path(temporary, temporary_wide, error)
        || !wide_path(path, path_wide, error))
      return false;
    if (!MoveFileExW(
            temporary_wide.c_str(), path_wide.c_str(), MOVEFILE_WRITE_THROUGH))
    {
      error = "failed to atomically publish v2 keystore: "
          + windows_error(GetLastError());
      return false;
    }
    return true;
  }
#endif
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

      std::string contents;
#ifdef _WIN32
      const service_keystore_status_v2 secure_load =
          load_windows_keystore_file(path, contents, error);
      if (secure_load != service_keystore_status_v2::loaded)
        return secure_load;
#else
      if (!safe_posix_permissions(status.permissions()))
      {
        error = "v2 keystore permissions grant group or other access";
        return service_keystore_status_v2::unsafe_permissions;
      }
      if (!epee::file_io_utils::load_file_to_string(path.string(), contents, 4096))
      {
        error = "failed to read v2 keystore";
        return service_keystore_status_v2::io_error;
      }
#endif
      std::map<std::string, std::string> fields;
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
    ec.clear();
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
    if (ec)
    {
      keystore = {};
      error = "failed to allocate temporary v2 keystore path: " + ec.message();
      return service_keystore_status_v2::io_error;
    }
#ifdef _WIN32
    if (!write_windows_keystore_file(temporary, contents, error))
    {
      boost::filesystem::remove(temporary);
      keystore = {};
      return service_keystore_status_v2::io_error;
    }
    if (!publish_windows_keystore_file(temporary, path, error))
    {
      boost::filesystem::remove(temporary);
      keystore = {};
      return service_keystore_status_v2::io_error;
    }
    std::string verification_contents;
    const service_keystore_status_v2 verification =
        load_windows_keystore_file(path, verification_contents, error);
    if (verification != service_keystore_status_v2::loaded
        || verification_contents != contents)
    {
      boost::filesystem::remove(path);
      keystore = {};
      if (verification == service_keystore_status_v2::loaded)
        error = "new v2 keystore content changed during publication";
      return verification == service_keystore_status_v2::loaded
          ? service_keystore_status_v2::io_error : verification;
    }
#else
    if (!epee::file_io_utils::save_string_to_file(temporary.string(), contents))
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
#endif
    return service_keystore_status_v2::created;
  }

  bool repair_service_keystore_permissions_v2(
      const std::string &path_string,
      std::string &error)
  {
    error.clear();
    if (path_string.empty())
    {
      error = "v2 keystore path is empty";
      return false;
    }
    const boost::filesystem::path path(path_string);
#ifdef _WIN32
    std::wstring path_wide;
    if (!wide_path(path, path_wide, error))
      return false;
    windows_handle file(CreateFileW(
        path_wide.c_str(), READ_CONTROL | WRITE_DAC | FILE_READ_ATTRIBUTES,
        0, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if (!file.valid())
    {
      error = "failed to open v2 keystore for ACL repair: "
          + windows_error(GetLastError());
      return false;
    }
    if (!regular_non_reparse_file(file.get(), error)
        || !owner_is_current_user(file.get(), error))
      return false;

    windows_private_security security;
    if (!security.initialize(error))
      return false;
    const DWORD result = SetSecurityInfo(
        file.get(), SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
        nullptr, nullptr, security.acl(), nullptr);
    if (result != ERROR_SUCCESS)
    {
      error = "failed to repair v2 keystore ACL: " + windows_error(result);
      return false;
    }
    if (!safe_windows_acl(file.get(), error))
    {
      error = "v2 keystore ACL remained unsafe after repair: " + error;
      return false;
    }
    return true;
#else
    boost::system::error_code ec;
    const boost::filesystem::file_status status =
        boost::filesystem::symlink_status(path, ec);
    if (ec || boost::filesystem::is_symlink(status)
        || !boost::filesystem::is_regular_file(status))
    {
      error = ec ? "failed to inspect v2 keystore: " + ec.message()
                 : "v2 keystore must be a regular non-symlink file";
      return false;
    }
    boost::filesystem::permissions(
        path, boost::filesystem::owner_read | boost::filesystem::owner_write, ec);
    if (ec)
    {
      error = "failed to repair v2 keystore permissions: " + ec.message();
      return false;
    }
    const boost::filesystem::file_status repaired =
        boost::filesystem::symlink_status(path, ec);
    if (ec || !safe_posix_permissions(repaired.permissions()))
    {
      error = ec ? "failed to verify repaired v2 keystore permissions: " + ec.message()
                 : "v2 keystore permissions remained unsafe after repair";
      return false;
    }
    return true;
#endif
  }
} // namespace epose
} // namespace qwertycoin
