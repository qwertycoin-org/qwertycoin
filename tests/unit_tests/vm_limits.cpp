// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "gtest/gtest.h"

#include "vm_test_utils.h"

using namespace vm_test;

namespace
{
  vm::status deploy_status(const std::string& wasm)
  {
    vm::state_tree state;
    return deploy(state, wasm).code;
  }
}

TEST(vm_limits, args_size)
{
  vm::state_tree state;
  vm::result d = deploy(state, test_contract());
  ASSERT_EQ(d.code, vm::status::ok);
  EXPECT_EQ(invoke(state, d.contract_id, m_heap_small, std::string(vm::limits::args_size, 'a')).code, vm::status::ok);
  EXPECT_EQ(invoke(state, d.contract_id, m_heap_small, std::string(vm::limits::args_size + 1, 'a')).code, vm::status::invalid_call);
}

TEST(vm_limits, module_size)
{
  module_builder m;
  uint32_t mt = method_type(m);
  add_ctor_dtor(m, mt);
  m.custom("pad", vm::limits::module_size);
  EXPECT_EQ(deploy_status(m.build()), vm::status::invalid_module);

  module_builder small;
  mt = method_type(small);
  add_ctor_dtor(small, mt);
  small.custom("pad", 1024);
  EXPECT_EQ(deploy_status(small.build()), vm::status::ok);
}

TEST(vm_limits, invoke_rules)
{
  vm::state_tree state;
  std::string wasm = test_contract();
  vm::result d = deploy(state, wasm);
  ASSERT_EQ(d.code, vm::status::ok);

  EXPECT_EQ(invoke(state, d.contract_id, vm::method_ctor).code, vm::status::invalid_call);
  EXPECT_EQ(invoke(state, d.contract_id, vm::method_dtor).code, vm::status::invalid_call);
  EXPECT_EQ(invoke(state, crypto::null_hash, m_store, "k1v1").code, vm::status::invalid_call);
  EXPECT_EQ(deploy(state, wasm).code, vm::status::invalid_call);

  vm::call c;
  c.kind = vm::call_kind::invoke;
  c.contract_id = d.contract_id;
  c.method = m_heap_small;
  EXPECT_EQ(vm::execute(wasm, c, state, ample_gas).code, vm::status::invalid_call);
}

TEST(vm_limits, module_structure)
{
  EXPECT_EQ(deploy_status(std::string("\0asm\x02\0\0\0", 8)), vm::status::invalid_module);
  EXPECT_EQ(deploy_status(std::string()), vm::status::invalid_module);

  module_builder no_dtor;
  uint32_t mt = method_type(no_dtor);
  no_dtor.exp("Ctor", no_dtor.func(mt, code().end()));
  EXPECT_EQ(deploy_status(no_dtor.build()), vm::status::invalid_module);

  module_builder gap;
  mt = method_type(gap);
  add_ctor_dtor(gap, mt);
  gap.exp("Method_3", gap.func(mt, code().end()));
  EXPECT_EQ(deploy_status(gap.build()), vm::status::invalid_module);

  module_builder dup;
  mt = method_type(dup);
  add_ctor_dtor(dup, mt);
  dup.exp("Method_2", dup.func(mt, code().end()));
  dup.exp("Method_02", dup.func(mt, code().end()));
  EXPECT_EQ(deploy_status(dup.build()), vm::status::invalid_module);

  module_builder loose;
  mt = method_type(loose);
  add_ctor_dtor(loose, mt);
  loose.exp("Method_+2", loose.func(mt, code().end()));
  loose.exp("Method_2x", loose.func(mt, code().end()));
  vm::state_tree state;
  vm::result r = deploy(state, loose.build());
  ASSERT_EQ(r.code, vm::status::ok);
  EXPECT_EQ(invoke(state, r.contract_id, 2).code, vm::status::trap);
}

TEST(vm_limits, imports_must_resolve)
{
  module_builder unknown;
  unknown.import("NoSuchCall", {}, {});
  uint32_t mt = method_type(unknown);
  add_ctor_dtor(unknown, mt);
  EXPECT_EQ(deploy_status(unknown.build()), vm::status::invalid_module);

  module_builder foreign;
  foreign.import("Halt", {}, {}, "wasi");
  mt = method_type(foreign);
  add_ctor_dtor(foreign, mt);
  EXPECT_EQ(deploy_status(foreign.build()), vm::status::invalid_module);

  module_builder wrong_sig;
  wrong_sig.import("SaveVar", {t_i32, t_i32}, {t_i32});
  mt = method_type(wrong_sig);
  add_ctor_dtor(wrong_sig, mt);
  EXPECT_EQ(deploy_status(wrong_sig.build()), vm::status::invalid_module);
}

TEST(vm_limits, locals_cap)
{
  auto with_locals = [](uint32_t n) {
    module_builder m;
    uint32_t mt = method_type(m);
    add_ctor_dtor(m, mt);
    m.exp("Method_2", m.func(mt, code().end(), {{n, t_i32}}));
    return m.build();
  };
  EXPECT_EQ(deploy_status(with_locals(vm::limits::function_locals - 1)), vm::status::ok);
  EXPECT_EQ(deploy_status(with_locals(vm::limits::function_locals + 1)), vm::status::invalid_module);
}
