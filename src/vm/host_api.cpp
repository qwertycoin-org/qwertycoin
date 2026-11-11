// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "host_api.h"

namespace vm
{
  const char* host_call_name(host_call call)
  {
    switch (call)
    {
#define VM_HOST_CALL(id, ret, name) case host_call::name: return #name;
      BVMOpsAll_Common(VM_HOST_CALL)
      BVMOpsAll_Contract(VM_HOST_CALL)
#undef VM_HOST_CALL
    }
    return nullptr;
  }

  bool is_host_call(uint32_t id)
  {
    return host_call_name(static_cast<host_call>(id)) != nullptr;
  }
}
