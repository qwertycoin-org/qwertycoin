// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "include_base_utils.h"
#include "vm/state_tree.h"
#include "vm/vm.h"
#include "fuzzer.h"

namespace
{
  constexpr uint64_t fuzz_gas = 1000000;
}

BEGIN_INIT_SIMPLE_FUZZER()
END_INIT_SIMPLE_FUZZER()

BEGIN_SIMPLE_FUZZER()
  const std::string wasm(reinterpret_cast<const char*>(buf), len);
  std::string compiled;
  vm::state_tree state;
  vm::call deploy;
  deploy.kind = vm::call_kind::deploy;
  deploy.deposit = len;
  vm::result created;
  if (vm::compile(wasm, compiled))
    created = vm::execute(wasm, deploy, state, fuzz_gas);
  if (created.code == vm::status::ok && created.contract_id != crypto::null_hash)
  {
    state.apply(created.writes);
    vm::call invoke;
    invoke.contract_id = created.contract_id;
    invoke.method = vm::method_first_public;
    invoke.args = wasm.substr(0, std::min<size_t>(len, 64));
    invoke.deposit = len;
    vm::execute(std::string(), invoke, state, fuzz_gas);
  }
END_SIMPLE_FUZZER()
