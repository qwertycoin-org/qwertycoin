// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>

#include "host_api.h"

namespace vm { namespace gas
{
  constexpr uint64_t cycle = 20;
  constexpr uint64_t cycle_mul = 30;
  constexpr uint64_t cycle_div = 50;
  constexpr uint64_t cycle_mem = 30;
  constexpr uint64_t cycle_call = 50;
  constexpr uint64_t local_word = 2;

  constexpr uint64_t host_call_base = 100;
  constexpr uint64_t mem_op_per_byte = 1;
  constexpr uint64_t heap_op = 1000;
  constexpr uint64_t load_var = 2000;
  constexpr uint64_t load_var_per_byte = 1;
  constexpr uint64_t save_var = 5000;
  constexpr uint64_t save_var_per_byte = 2;
  constexpr uint64_t log = 1000;
  constexpr uint64_t log_per_byte = 1;
  constexpr uint64_t call_far = 10000;
  constexpr uint64_t hdr_info = 10000;
  constexpr uint64_t funds_lock = 5000;
  constexpr uint64_t hash_op = 1000;
  constexpr uint64_t hash_write = 100;
  constexpr uint64_t hash_write_per_byte = 5;
  constexpr uint64_t check_signature = 100000;
  constexpr uint64_t add_signature = check_signature;
  constexpr uint64_t deploy = 100000;
  constexpr uint64_t deploy_per_byte = 12;

  constexpr uint64_t mem_op(uint32_t n) { return mem_op_per_byte * n; }
  constexpr uint64_t load_var_for(uint32_t n) { return load_var + load_var_per_byte * n; }
  constexpr uint64_t save_var_for(uint32_t n) { return save_var + save_var_per_byte * n; }
  constexpr uint64_t log_for(uint32_t n) { return log + log_per_byte * n; }
  constexpr uint64_t hash_write_for(uint32_t n) { return hash_write + hash_write_per_byte * n; }
  constexpr uint64_t hash_read_for(uint32_t n) { return hash_op + hash_write_per_byte * n; }
  constexpr uint64_t deploy_for(uint64_t n) { return deploy + deploy_per_byte * n; }

  constexpr uint64_t opcode_cost(uint8_t op)
  {
    if (op == 0x10 || op == 0x11)
      return cycle_call;
    if (op >= 0x28 && op <= 0x3e)
      return cycle_mem;
    if (op == 0x6c || op == 0x7e)
      return cycle_mul;
    if ((op >= 0x6d && op <= 0x70) || (op >= 0x7f && op <= 0x82))
      return cycle_div;
    return cycle;
  }

  constexpr uint64_t host_call_cost(host_call call)
  {
    switch (call)
    {
    case host_call::CheckSignature: return host_call_base + check_signature;
    case host_call::AddSig: return host_call_base + add_signature;
    case host_call::CallFar: return host_call_base + call_far;
    case host_call::get_HdrInfo: return host_call_base + hdr_info;
    case host_call::FundsLock:
    case host_call::FundsUnlock: return host_call_base + funds_lock;
    default: return host_call_base;
    }
  }
}
}
