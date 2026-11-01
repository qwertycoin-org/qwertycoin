// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "gtest/gtest.h"

#include <type_traits>

#include "vm/bvm2/wasm_interpreter.h"
#include "vm_test_utils.h"

using namespace vm_test;

namespace
{
  template <typename T, typename = void>
  struct has_reader_mode : std::false_type {};

  template <typename T>
  struct has_reader_mode<T, std::void_t<typename T::Mode>> : std::true_type {};

  uint32_t read_u32(const std::string& bytes, size_t pos)
  {
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i)
      value |= uint32_t(uint8_t(bytes[pos + i])) << (i * 8);
    return value;
  }

  void write_le(std::string& bytes, size_t pos, uint64_t value, size_t width)
  {
    for (size_t i = 0; i < width; ++i)
      bytes[pos + i] = static_cast<char>(value >> (i * 8));
  }

  crypto::hash deploy_into(vm::state_tree& state)
  {
    vm::result r = deploy(state, test_contract());
    EXPECT_EQ(r.code, vm::status::ok);
    return r.contract_id;
  }

  std::string bytecode_writer(bool call_target)
  {
    module_builder m;
    uint32_t mt = method_type(m);
    add_ctor_dtor(m, mt);
    code body;
    body.arg_at(0).op(0x28).op(2).op(0).i32(0x43).op(0x3a).op(0).op(0);
    if (call_target)
      body.i32(0).call(3);
    m.exp("Method_2", m.func(mt, body.end()));
    m.exp("Method_3", m.func(mt, code().i32(1).drop().end()));
    code indirect;
    indirect.i32(0).i32(1).op(0x11).op(0).op(0).end();
    m.exp("Method_4", m.func(mt, indirect));
    m.indirect_table({3});
    return m.build();
  }

  vm::result invoke_writer(bool call_target, bool table)
  {
    vm::state_tree state;
    std::string wasm = bytecode_writer(call_target);
    std::string compiled;
    if (!vm::compile(wasm, compiled))
    {
      vm::result failed;
      failed.code = vm::status::invalid_module;
      return failed;
    }
    uint32_t header_size = 16 + 4 * read_u32(compiled, 4);
    uint32_t target = table ? read_u32(compiled, 12) : read_u32(compiled, 16 + 4 * 3);
    uint32_t address = read_u32(compiled, 8) + target - header_size;
    vm::result deployed = deploy(state, wasm);
    if (deployed.code != vm::status::ok)
      return deployed;
    std::string args(4, '\0');
    write_le(args, 0, address, 4);
    return invoke(state, deployed.contract_id, 2, args);
  }

  std::string shift_contract(bool wide)
  {
    module_builder m;
    uint32_t save = m.import("SaveVar", {t_i32, t_i32, t_i32, t_i32, t_i32}, {t_i32});
    uint32_t mt = method_type(m);
    add_ctor_dtor(m, mt);
    uint32_t width = wide ? 8 : 4;
    uint8_t load = wide ? 0x29 : 0x28;
    uint8_t shift = wide ? 0x87 : 0x75;
    uint8_t store = wide ? 0x37 : 0x36;
    code body;
    body.arg_at(width * 2).arg_at(0).op(load).op(wide ? 3 : 2).op(0);
    body.arg_at(width).op(load).op(wide ? 3 : 2).op(0);
    body.op(shift).op(store).op(wide ? 3 : 2).op(0);
    body.arg_at(width * 3).i32(1).arg_at(width * 2).i32(width).i32(0).call(save).drop().end();
    m.exp("Method_2", m.func(mt, body));
    return m.build();
  }

  code& load_byte(code& body, int32_t base, uint32_t offset)
  {
    body.i32(base).op(0x2d).op(0);
    put_uleb(body.b, offset);
    return body;
  }

  std::string load_at(int32_t base, uint32_t offset)
  {
    code body;
    return minimal_module(load_byte(body, base, offset).drop().end());
  }

  std::string load_across_region()
  {
    module_builder m;
    uint32_t heap = m.import("Heap_Alloc", {t_i32}, {t_i32});
    uint32_t mt = method_type(m);
    add_ctor_dtor(m, mt);
    code body;
    body.i32(16).call(heap).drop();
    load_byte(body, 0x3fffffff, 1).drop().end();
    m.exp("Method_2", m.func(mt, body));
    return m.build();
  }
}

TEST(vm_reader, obsolete_modes_removed)
{
  EXPECT_FALSE(has_reader_mode<beam::Wasm::Reader>::value);
}

TEST(vm_trap, halt_traps_and_discards_writes)
{
  vm::state_tree state;
  crypto::hash cid = deploy_into(state);
  crypto::hash root = state.get_root();
  vm::result r = invoke(state, cid, m_halt);
  EXPECT_EQ(r.code, vm::status::trap);
  EXPECT_TRUE(r.writes.empty());
  EXPECT_GT(r.gas_used, 0u);
  EXPECT_LT(r.gas_used, ample_gas);
  EXPECT_EQ(state.get_root(), root);
}

TEST(vm_trap, unreachable_traps)
{
  vm::state_tree state;
  crypto::hash cid = deploy_into(state);
  EXPECT_EQ(invoke(state, cid, m_unreachable).code, vm::status::trap);
}

TEST(vm_trap, funds_lock_bounded_by_deposit)
{
  vm::state_tree state;
  crypto::hash cid = deploy_into(state);

  vm::result ok = invoke(state, cid, m_funds_lock, {}, ample_gas, locked_per_call);
  ASSERT_EQ(ok.code, vm::status::ok);
  EXPECT_EQ(ok.locked, static_cast<uint64_t>(locked_per_call));
  EXPECT_EQ(ok.unlocked, 0u);
  std::string balance;
  ASSERT_TRUE(state.get(var_key(cid, 1, {}), balance));
  EXPECT_EQ(balance, std::string("\0\0\0\0\0\0\0\x64", 8));

  EXPECT_EQ(invoke(state, cid, m_funds_lock, {}, ample_gas, locked_per_call - 1).code, vm::status::trap);
}

TEST(vm_trap, far_call_depth_is_bounded)
{
  vm::state_tree state;
  crypto::hash cid = deploy_into(state);
  vm::result r = invoke(state, cid, m_recurse, vm::contract_code_key(cid));
  EXPECT_EQ(r.code, vm::status::trap);
  EXPECT_LT(r.gas_used, ample_gas);
}

TEST(vm_trap, method_beyond_table_traps)
{
  vm::state_tree state;
  crypto::hash cid = deploy_into(state);
  EXPECT_EQ(invoke(state, cid, m_count).code, vm::status::trap);
}

TEST(vm_trap, heap_limit_returns_null)
{
  vm::state_tree state;
  crypto::hash cid = deploy_into(state);
  EXPECT_EQ(invoke(state, cid, m_heap_oversize).code, vm::status::ok);
  EXPECT_EQ(invoke(state, cid, m_heap_small).code, vm::status::ok);
}

TEST(vm_trap, bytecode_write_traps)
{
  EXPECT_EQ(invoke_writer(false, false).code, vm::status::trap);
}

TEST(vm_trap, float_opcode_rewrite_traps_before_execution)
{
  vm::result write = invoke_writer(false, false);
  vm::result rewrite = invoke_writer(true, false);
  EXPECT_EQ(write.code, vm::status::trap);
  EXPECT_EQ(rewrite.code, vm::status::trap);
  EXPECT_EQ(rewrite.gas_used, write.gas_used);
}

TEST(vm_trap, method_table_write_traps)
{
  EXPECT_EQ(invoke_writer(false, true).code, vm::status::trap);
}

TEST(vm_trap, memory_offset_wrap_traps)
{
  for (const auto& x : {std::pair<int32_t, uint32_t>{0, 0}, {0, 1}, {-1, 1}, {-2, 2}, {-1, 0}})
  {
    vm::state_tree state;
    vm::result deployed = deploy(state, load_at(x.first, x.second));
    ASSERT_EQ(deployed.code, vm::status::ok);
    vm::status expected = x.first == 0 ? vm::status::ok : vm::status::trap;
    EXPECT_EQ(invoke(state, deployed.contract_id, 2).code, expected) << x.first << "+" << x.second;
  }
  vm::state_tree state;
  vm::result deployed = deploy(state, load_across_region());
  ASSERT_EQ(deployed.code, vm::status::ok);
  EXPECT_EQ(invoke(state, deployed.contract_id, 2).code, vm::status::trap);
}

TEST(vm_shift, arithmetic_shift_masks_count)
{
  for (bool wide : {false, true})
  {
    vm::state_tree state;
    vm::result deployed = deploy(state, shift_contract(wide));
    ASSERT_EQ(deployed.code, vm::status::ok);
    size_t width = wide ? 8 : 4;
    uint64_t mask = wide ? UINT64_MAX : UINT32_MAX;
    uint64_t high = wide ? uint64_t(1) << 63 : uint64_t(1) << 31;
    const uint32_t bits = static_cast<uint32_t>(width * 8);
    for (uint32_t count : {0u, 1u, bits - 1, bits, bits + 1, bits * 2 - 1})
    {
      for (bool negative : {false, true})
      {
        uint64_t operand = negative ? mask - 7 : 8;
        std::string args(width * 3 + 1, '\0');
        write_le(args, 0, operand, width);
        write_le(args, width, count, width);
        args[width * 3] = 'k';
        EXPECT_EQ(invoke(state, deployed.contract_id, 2, args).code, vm::status::ok);
        std::string stored;
        ASSERT_TRUE(state.get(var_key(deployed.contract_id, 0, "k"), stored));
        ASSERT_EQ(stored.size(), width);
        uint64_t actual = 0;
        for (size_t i = 0; i < width; ++i)
          actual |= uint64_t(uint8_t(stored[i])) << (i * 8);
        uint32_t shift = count % bits;
        uint64_t expected = shift == 0 ? operand : negative ? ((mask - 7) >> shift) | (mask << (bits - shift)) : uint64_t(8) >> shift;
        EXPECT_EQ(actual, expected & mask) << width << ":" << count << ":" << negative;
        EXPECT_EQ(actual & high, negative ? high : 0u);
      }
    }
  }
}

TEST(vm_trap, signed_leb_limits_compile)
{
  code body;
  body.i32(INT32_MIN).drop().i64(INT64_MIN).drop().end();
  std::string compiled;
  EXPECT_TRUE(vm::compile(minimal_module(body), compiled));
}
