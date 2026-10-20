// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <cstddef>

namespace qwertycoin
{
namespace epose
{
  // Diagnostic payload accounting, not serialized or committed. Vector capacity
  // is included; map/deque allocator overhead and hash temporaries are excluded.
  struct state_memory_usage_v2
  {
    size_t identities = 0;
    size_t lifecycle_records = 0;
    size_t leases = 0;
    size_t receipts = 0;
    size_t snapshots = 0;
    size_t qualifications = 0;
    size_t vector_payload_bytes = 0;
    size_t map_value_bytes = 0;
  };
} // namespace epose
} // namespace qwertycoin
