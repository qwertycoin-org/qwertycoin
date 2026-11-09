// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>

#include "cryptonote_config.h"

namespace qwertycoin
{
namespace epose
{
  constexpr uint64_t relay_hardening_height_v2(cryptonote::network_type nettype)
  {
    return nettype == cryptonote::DEVNET ? 0 : QWC_EPOSE_RELAY_HARDENING_HEIGHT;
  }
}
}
