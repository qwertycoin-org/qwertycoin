// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <cstring>
#include <string>

namespace qwertycoin
{
namespace epose
{
  class state_image_writer_v2
  {
  public:
    void byte(uint8_t value) { data_.push_back(static_cast<char>(value)); }
    void number(uint64_t value)
    {
      for (unsigned shift = 0; shift < 64; shift += 8)
        byte(static_cast<uint8_t>(value >> shift));
    }
    template <typename T> void fixed(const T &value)
    {
      data_.append(reinterpret_cast<const char *>(&value), sizeof(T));
    }
    void bytes(const std::string &value)
    {
      number(value.size());
      data_.append(value);
    }
    const std::string &data() const { return data_; }

  private:
    std::string data_;
  };

  class state_image_reader_v2
  {
  public:
    explicit state_image_reader_v2(const std::string &data) : data_(data) {}
    bool byte(uint8_t &value)
    {
      if (offset_ == data_.size())
        return false;
      value = static_cast<uint8_t>(data_[offset_++]);
      return true;
    }
    bool number(uint64_t &value)
    {
      if (remaining() < 8)
        return false;
      value = 0;
      for (unsigned shift = 0; shift < 64; shift += 8)
        value |= uint64_t{static_cast<uint8_t>(data_[offset_++])} << shift;
      return true;
    }
    template <typename T> bool fixed(T &value)
    {
      if (remaining() < sizeof(T))
        return false;
      std::memcpy(&value, data_.data() + offset_, sizeof(T));
      offset_ += sizeof(T);
      return true;
    }
    bool bytes(std::string &value)
    {
      uint64_t size = 0;
      if (!number(size) || size > remaining())
        return false;
      value.assign(data_.data() + offset_, static_cast<size_t>(size));
      offset_ += static_cast<size_t>(size);
      return true;
    }
    bool count(uint64_t &value, size_t minimum_bytes)
    {
      return number(value) && minimum_bytes != 0
          && value <= remaining() / minimum_bytes;
    }
    size_t remaining() const { return data_.size() - offset_; }
    bool finished() const { return offset_ == data_.size(); }

  private:
    const std::string &data_;
    size_t offset_ = 0;
  };
}
}
