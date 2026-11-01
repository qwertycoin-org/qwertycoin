// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include <boost/optional/optional_io.hpp>

#include "gtest/gtest.h"

#include "crypto/hash.h"
#include "vm_test_utils.h"

using namespace vm_test;

namespace
{
  struct run
  {
    crypto::hash cid;
    crypto::hash root;
    std::vector<vm::result> results;
  };

  run scenario()
  {
    run out;
    vm::state_tree state;
    vm::result d = deploy(state, test_contract(), "seed");
    EXPECT_EQ(d.code, vm::status::ok);
    out.cid = d.contract_id;
    out.results.push_back(d);

    std::string keccak_args = "abcd" + std::string(32, '\0') + "kh";
    out.results.push_back(invoke(state, out.cid, m_store, "k1v1"));
    out.results.push_back(invoke(state, out.cid, m_copy_var, "k1xxk2"));
    out.results.push_back(invoke(state, out.cid, m_keccak, keccak_args));
    out.results.push_back(invoke(state, out.cid, m_emit_log, "lgVV"));
    out.results.push_back(invoke(state, out.cid, m_funds_lock, {}, ample_gas, 1000));
    out.results.push_back(invoke(state, out.cid, m_spin, {}, 30000));
    out.root = state.get_root();
    return out;
  }
}

TEST(vm_determinism, contract_id_matches_tagged_hash)
{
  vm::state_tree state;
  std::string wasm = test_contract();
  vm::result r = deploy(state, wasm, "seed");
  ASSERT_EQ(r.code, vm::status::ok);
  EXPECT_EQ(r.contract_id, vm::contract_id_for(wasm, "seed"));
  EXPECT_NE(r.contract_id, vm::contract_id_for(wasm, "seed2"));

  std::string compiled;
  ASSERT_TRUE(vm::compile(wasm, compiled));
  std::string stored;
  ASSERT_TRUE(state.get(vm::contract_code_key(r.contract_id), stored));
  EXPECT_EQ(stored, compiled);
}

TEST(vm_determinism, compile_is_stable)
{
  std::string a, b;
  ASSERT_TRUE(vm::compile(test_contract(), a));
  ASSERT_TRUE(vm::compile(test_contract(), b));
  EXPECT_EQ(a, b);
}

TEST(vm_determinism, repeated_scenario_is_identical)
{
  run first = scenario();
  run second = scenario();
  EXPECT_EQ(first.cid, second.cid);
  EXPECT_EQ(first.root, second.root);
  ASSERT_EQ(first.results.size(), second.results.size());
  for (size_t i = 0; i < first.results.size(); ++i)
  {
    const vm::result& a = first.results[i];
    const vm::result& b = second.results[i];
    EXPECT_EQ(a.code, b.code) << i;
    EXPECT_EQ(a.gas_used, b.gas_used) << i;
    EXPECT_EQ(a.writes, b.writes) << i;
    EXPECT_EQ(a.locked, b.locked) << i;
    ASSERT_EQ(a.logs.size(), b.logs.size()) << i;
    for (size_t j = 0; j < a.logs.size(); ++j)
    {
      EXPECT_EQ(a.logs[j].key, b.logs[j].key);
      EXPECT_EQ(a.logs[j].value, b.logs[j].value);
    }
  }
}

TEST(vm_determinism, state_effects)
{
  vm::state_tree state;
  vm::result d = deploy(state, test_contract());
  ASSERT_EQ(d.code, vm::status::ok);
  const crypto::hash cid = d.contract_id;

  vm::result store = invoke(state, cid, m_store, "k1v1");
  ASSERT_EQ(store.code, vm::status::ok);
  ASSERT_EQ(store.writes.size(), 1u);
  EXPECT_EQ(*store.writes.at(var_key(cid, 0, "k1")), "v1");

  vm::result copy = invoke(state, cid, m_copy_var, "k1xxk2");
  ASSERT_EQ(copy.code, vm::status::ok);
  std::string v;
  ASSERT_TRUE(state.get(var_key(cid, 0, "k2"), v));
  EXPECT_EQ(v, "v1");

  vm::result log = invoke(state, cid, m_emit_log, "lgVV");
  ASSERT_EQ(log.code, vm::status::ok);
  ASSERT_EQ(log.logs.size(), 1u);
  EXPECT_EQ(log.logs[0].key, var_key(cid, 0, "lg"));
  EXPECT_EQ(log.logs[0].value, "VV");
  EXPECT_TRUE(log.writes.empty());

  vm::result h = invoke(state, cid, m_keccak, "abcd" + std::string(32, '\0') + "kh");
  ASSERT_EQ(h.code, vm::status::ok);
  crypto::hash expected = crypto::cn_fast_hash("abcd", 4);
  ASSERT_TRUE(state.get(var_key(cid, 0, "kh"), v));
  EXPECT_EQ(v, std::string(reinterpret_cast<const char*>(&expected), sizeof(expected)));
}

TEST(vm_determinism, rewrite_to_base_value_is_not_a_write)
{
  vm::state_tree state;
  vm::result d = deploy(state, test_contract());
  ASSERT_EQ(d.code, vm::status::ok);
  ASSERT_EQ(invoke(state, d.contract_id, m_store, "k1v1").code, vm::status::ok);
  vm::result again = invoke(state, d.contract_id, m_store, "k1v1");
  ASSERT_EQ(again.code, vm::status::ok);
  EXPECT_TRUE(again.writes.empty());
}
