// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <limits>

namespace qwertycoin
{
namespace epose
{
  inline bool checked_add(uint64_t left, uint64_t right, uint64_t &out)
  {
    if (left > std::numeric_limits<uint64_t>::max() - right)
      return false;
    out = left + right;
    return true;
  }
}
}
