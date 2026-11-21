// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>

namespace vm { namespace limits
{
  constexpr uint32_t far_call_depth = 32;
  constexpr uint32_t stack_size = 0x10000;
  constexpr uint32_t heap_size = 0x100000;
  constexpr uint32_t var_key = 256;
  constexpr uint32_t var_value = 0x100000;
  constexpr uint32_t hash_objects = 8;
  constexpr uint32_t signers = 16;
  constexpr uint32_t module_size = 0x80000;
  constexpr uint32_t compiled_size = 0x200000;
  constexpr uint32_t args_size = 0x8000;
  constexpr uint32_t function_args = 64;
  constexpr uint32_t function_locals = 4096;
  constexpr uint32_t data_size = 0x100000;
}
}
