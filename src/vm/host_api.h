// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>

#include "bvm2/bvm2_opcodes.h"

namespace vm
{
  enum class host_call : uint32_t
  {
#define VM_HOST_CALL(id, ret, name) name = id,
    BVMOpsAll_Common(VM_HOST_CALL)
    BVMOpsAll_Contract(VM_HOST_CALL)
#undef VM_HOST_CALL
  };

  const char* host_call_name(host_call call);
  bool is_host_call(uint32_t id);
}
