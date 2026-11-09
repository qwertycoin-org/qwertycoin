// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <cstring>
#include <string>

#include "crypto/hash.h"
#include "cryptonote_config.h"

namespace qwertycoin
{
namespace epose
{
namespace transcript
{
  template <typename T>
  bool bytes_equal(const T &left, const T &right)
  {
    return std::memcmp(&left, &right, sizeof(T)) == 0;
  }

  template <typename T>
  bool bytes_less(const T &left, const T &right)
  {
    return std::memcmp(&left, &right, sizeof(T)) < 0;
  }

  template <typename T>
  void append_bytes(std::string &out, const T &value)
  {
    out.append(reinterpret_cast<const char *>(&value), sizeof(T));
  }

  inline void append_u8(std::string &out, uint8_t value)
  {
    out.push_back(static_cast<char>(value));
  }

  inline void append_u16_le(std::string &out, uint16_t value)
  {
    out.push_back(static_cast<char>(value & 0xff));
    out.push_back(static_cast<char>((value >> 8) & 0xff));
  }

  inline void append_u32_le(std::string &out, uint32_t value)
  {
    for (unsigned shift = 0; shift < 32; shift += 8)
      out.push_back(static_cast<char>((value >> shift) & 0xff));
  }

  inline void append_u64_le(std::string &out, uint64_t value)
  {
    for (unsigned shift = 0; shift < 64; shift += 8)
      out.push_back(static_cast<char>((value >> shift) & 0xff));
  }

  inline void append_network(std::string &out, cryptonote::network_type nettype)
  {
    const auto &network_id = cryptonote::get_config(nettype).NETWORK_ID;
    for (const auto byte : network_id)
      append_u8(out, byte);
  }

  inline crypto::hash hash_blob(const std::string &blob)
  {
    return crypto::cn_fast_hash(blob.data(), blob.size());
  }
}
}
}
