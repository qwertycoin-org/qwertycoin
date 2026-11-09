// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "gtest/gtest.h"

#include "vm_test_utils.h"

using namespace vm_test;

namespace
{
  std::string with_method(uint32_t type_idx_params_results, const code& body, const locals& decl = {})
  {
    module_builder m;
    uint32_t mt = method_type(m);
    add_ctor_dtor(m, mt);
    m.exp("Method_2", m.func(type_idx_params_results == 0 ? mt : type_idx_params_results, body, decl));
    return m.build();
  }

  bool compiles(const std::string& wasm)
  {
    std::string out;
    return vm::compile(wasm, out);
  }

  bytes f32_bits(float v)
  {
    bytes b(4);
    memcpy(b.data(), &v, 4);
    return b;
  }
}

TEST(vm_float_rejection, integer_control_compiles)
{
  EXPECT_TRUE(compiles(with_method(0, code().i32(1).i32(2).op(0x6a).drop().end())));
  EXPECT_TRUE(compiles(with_method(0, code().end(), {{1, t_i64}})));
}

TEST(vm_float_rejection, float_const)
{
  EXPECT_FALSE(compiles(with_method(0, code().op(0x43).raw(f32_bits(1.0f)).drop().end())));
  EXPECT_FALSE(compiles(with_method(0, code().op(0x44).raw(bytes(8, 0)).drop().end())));
}

TEST(vm_float_rejection, float_arithmetic_and_conversion)
{
  for (uint8_t op : {0x5b, 0x61, 0x8b, 0x92, 0x99, 0xa0, 0xa8, 0xb2, 0xbc, 0xbf})
    EXPECT_FALSE(compiles(with_method(0, code().i32(0).op(op).drop().end()))) << std::hex << int(op);
}

TEST(vm_float_rejection, float_memory_access)
{
  for (uint8_t op : {0x2a, 0x2b, 0x38, 0x39})
    EXPECT_FALSE(compiles(with_method(0, code().arg_at(0).op(op).op(0).op(0).end()))) << std::hex << int(op);
}

TEST(vm_float_rejection, float_locals)
{
  EXPECT_FALSE(compiles(with_method(0, code().end(), {{1, t_f32}})));
  EXPECT_FALSE(compiles(with_method(0, code().end(), {{1, t_f64}})));
}

namespace
{
  std::string with_helper(const bytes& params, const bytes& results, const code& body)
  {
    module_builder m;
    uint32_t mt = method_type(m);
    add_ctor_dtor(m, mt);
    m.func(m.type(params, results), body);
    m.exp("Method_2", m.func(mt, code().end()));
    return m.build();
  }
}

TEST(vm_float_rejection, float_signatures)
{
  EXPECT_TRUE(compiles(with_helper({t_i64}, {}, code().end())));
  EXPECT_FALSE(compiles(with_helper({t_f64}, {}, code().end())));
  EXPECT_FALSE(compiles(with_helper({t_f32}, {}, code().end())));

  EXPECT_TRUE(compiles(with_helper({}, {t_i64}, code().unreachable().end())));
  EXPECT_FALSE(compiles(with_helper({}, {t_f32}, code().unreachable().end())));
  EXPECT_FALSE(compiles(with_helper({}, {t_f64}, code().unreachable().end())));
}

TEST(vm_float_rejection, deploy_reports_invalid_module)
{
  vm::state_tree state;
  EXPECT_EQ(deploy(state, with_method(0, code().op(0x43).raw(f32_bits(0.5f)).drop().end())).code, vm::status::invalid_module);
  EXPECT_EQ(state.size(), 0u);
}
