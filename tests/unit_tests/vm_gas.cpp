// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "gtest/gtest.h"

#include "vm/gas.h"
#include "vm_test_utils.h"

using namespace vm_test;

namespace
{
  struct deployed
  {
    vm::state_tree state;
    crypto::hash cid;
  };

  deployed deploy_test_contract()
  {
    deployed d;
    vm::result r = deploy(d.state, test_contract());
    EXPECT_EQ(r.code, vm::status::ok);
    d.cid = r.contract_id;
    return d;
  }
}

TEST(vm_gas, opcode_cost_classes)
{
  EXPECT_EQ(vm::gas::opcode_cost(0x10), vm::gas::cycle_call);
  EXPECT_EQ(vm::gas::opcode_cost(0x28), vm::gas::cycle_mem);
  EXPECT_EQ(vm::gas::opcode_cost(0x36), vm::gas::cycle_mem);
  EXPECT_EQ(vm::gas::opcode_cost(0x6c), vm::gas::cycle_mul);
  EXPECT_EQ(vm::gas::opcode_cost(0x6d), vm::gas::cycle_div);
  EXPECT_EQ(vm::gas::opcode_cost(0x82), vm::gas::cycle_div);
  EXPECT_EQ(vm::gas::opcode_cost(0x6a), vm::gas::cycle);
}

TEST(vm_gas, host_call_cost_table)
{
  EXPECT_EQ(vm::gas::host_call_cost(vm::host_call::Memcpy), vm::gas::host_call_base);
  EXPECT_EQ(vm::gas::host_call_cost(vm::host_call::CheckSignature), vm::gas::host_call_base + vm::gas::check_signature);
  EXPECT_EQ(vm::gas::host_call_cost(vm::host_call::AddSig), vm::gas::host_call_base + vm::gas::add_signature);
  EXPECT_EQ(vm::gas::host_call_cost(vm::host_call::CallFar), vm::gas::host_call_base + vm::gas::call_far);
  EXPECT_EQ(10100u, vm::gas::host_call_cost(vm::host_call::get_HdrInfo));
  EXPECT_EQ(static_cast<uint32_t>(vm::host_call::get_HdrInfo), 0x46u);
  EXPECT_EQ(vm::gas::host_call_cost(vm::host_call::FundsLock), vm::gas::host_call_base + vm::gas::funds_lock);
  EXPECT_TRUE(vm::is_host_call(0x21));
  EXPECT_TRUE(vm::is_host_call(0x29));
  EXPECT_TRUE(vm::is_host_call(0x46));
  EXPECT_FALSE(vm::is_host_call(0x7f));
  EXPECT_STREQ(vm::host_call_name(vm::host_call::SaveVar), "SaveVar");
}

TEST(vm_gas, deploy_charges_per_byte)
{
  vm::state_tree state;
  std::string wasm = minimal_module();
  vm::result r = deploy(state, wasm);
  ASSERT_EQ(r.code, vm::status::ok);
  EXPECT_GE(r.gas_used, vm::gas::deploy_for(wasm.size()));

  vm::state_tree fresh;
  vm::result starved = deploy(fresh, wasm, {}, vm::gas::deploy_for(wasm.size()) - 1);
  EXPECT_EQ(starved.code, vm::status::out_of_gas);
  EXPECT_EQ(starved.gas_used, vm::gas::deploy_for(wasm.size()) - 1);
  EXPECT_TRUE(starved.writes.empty());
}

TEST(vm_gas, exact_limit_boundary)
{
  deployed d = deploy_test_contract();
  vm::state_tree probe = d.state;
  vm::result r = invoke(probe, d.cid, m_store, "k1v1");
  ASSERT_EQ(r.code, vm::status::ok);

  vm::state_tree exact = d.state;
  vm::result at = invoke(exact, d.cid, m_store, "k1v1", r.gas_used);
  EXPECT_EQ(at.code, vm::status::ok);
  EXPECT_EQ(at.gas_used, r.gas_used);

  vm::state_tree under = d.state;
  vm::result below = invoke(under, d.cid, m_store, "k1v1", r.gas_used - 1);
  EXPECT_EQ(below.code, vm::status::out_of_gas);
  EXPECT_EQ(below.gas_used, r.gas_used - 1);
  EXPECT_TRUE(below.writes.empty());
}

TEST(vm_gas, independent_of_limit)
{
  deployed d = deploy_test_contract();
  vm::state_tree a = d.state;
  vm::state_tree b = d.state;
  EXPECT_EQ(invoke(a, d.cid, m_store, "k1v1", 1000000).gas_used, invoke(b, d.cid, m_store, "k1v1", ample_gas).gas_used);
}

TEST(vm_gas, instructions_and_host_calls_are_metered)
{
  deployed d = deploy_test_contract();
  vm::state_tree s = d.state;
  vm::result spin_base = invoke(s, d.cid, m_heap_small);
  vm::result filler = invoke(s, d.cid, m_filler);
  vm::result height = invoke(s, d.cid, m_height);
  vm::result store = invoke(s, d.cid, m_store, "k1v1");
  ASSERT_EQ(filler.code, vm::status::ok);
  ASSERT_EQ(height.code, vm::status::ok);
  ASSERT_EQ(store.code, vm::status::ok);
  ASSERT_EQ(spin_base.code, vm::status::ok);

  EXPECT_GE(filler.gas_used, filler_pairs * 2 * vm::gas::cycle);
  EXPECT_GE(height.gas_used, vm::gas::host_call_base);
  EXPECT_GE(store.gas_used, vm::gas::host_call_base + vm::gas::save_var_for(2));
  EXPECT_GE(spin_base.gas_used, vm::gas::host_call_base + vm::gas::heap_op);
}

TEST(vm_gas, infinite_loop_runs_out)
{
  deployed d = deploy_test_contract();
  vm::result r = invoke(d.state, d.cid, m_spin, {}, 50000);
  EXPECT_EQ(r.code, vm::status::out_of_gas);
  EXPECT_EQ(r.gas_used, 50000u);
  EXPECT_TRUE(r.writes.empty());
}
