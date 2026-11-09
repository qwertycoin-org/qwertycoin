// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "gtest/gtest.h"

#include "vm/app_shader.h"
#include "wallet/app_shader_approvals.h"
#include "vm_test_utils.h"

using namespace vm_test;

namespace
{
  constexpr int32_t frame_size = 256;
  constexpr uint32_t ptr_local = 1;
  constexpr uint32_t method_entry = 2;
  constexpr uint64_t echoed_amount = 777;
  constexpr uint64_t stored_value = 1000;
  constexpr uint64_t deposit_amount = 99;
  constexpr uint32_t generated_method = 5;
  constexpr int32_t key_offset = 0;
  constexpr int32_t value_offset = 16;
  constexpr int32_t amount_offset = 40;
  constexpr int32_t read_offset = 56;
  constexpr int32_t cid_offset = 64;
  constexpr int32_t call_args_offset = 100;

  struct imports
  {
    uint32_t stack_alloc, stack_free, memset, add_text, add_num, open_group, close_group, get_num, var_read, generate_call, save_var;
  };

  imports import_all(module_builder& m)
  {
    imports i;
    i.stack_alloc = m.import("StackAlloc", {t_i32}, {t_i32});
    i.stack_free = m.import("StackFree", {t_i32}, {});
    i.memset = m.import("Memset", {t_i32, t_i32, t_i32}, {t_i32});
    i.add_text = m.import("DocAddText", {t_i32, t_i32}, {});
    i.add_num = m.import("DocAddNum", {t_i32, t_i64}, {});
    i.open_group = m.import("DocOpenGroup", {t_i32}, {});
    i.close_group = m.import("DocCloseGroup", {}, {});
    i.get_num = m.import("DocGetNum", {t_i32, t_i32}, {t_i32});
    i.var_read = m.import("VarRead", {t_i32, t_i32, t_i32, t_i32}, {t_i32});
    i.generate_call = m.import("GenerateCall", {t_i32, t_i32, t_i32, t_i32, t_i64}, {});
    i.save_var = m.import("SaveVar", {t_i32, t_i32, t_i32, t_i32, t_i32}, {t_i32});
    return i;
  }

  code& pointer_at(code& c, int32_t offset)
  {
    return c.get(ptr_local).i32(offset).op(0x6a);
  }

  code& store_string(code& c, int32_t offset, const std::string& text)
  {
    for (size_t k = 0; k <= text.size(); ++k)
    {
      c.get(ptr_local).i32(k < text.size() ? static_cast<uint8_t>(text[k]) : 0).op(0x3a).op(0);
      put_uleb(c.b, offset + k);
    }
    return c;
  }

  code& load_i64(code& c, int32_t offset)
  {
    c.get(ptr_local).op(0x29).op(3);
    put_uleb(c.b, offset);
    return c;
  }

  code& begin_frame(code& c, const imports& i)
  {
    return c.i32(frame_size).call(i.stack_alloc).set(ptr_local);
  }

  code& end_frame(code& c, const imports& i)
  {
    return c.i32(frame_size).call(i.stack_free).end();
  }

  std::string document_shader()
  {
    module_builder m;
    imports i = import_all(m);
    uint32_t mt = method_type(m);
    m.exp("Ctor", m.func(mt, code().end()));
    m.exp("Dtor", m.func(mt, code().end()));

    code c;
    begin_frame(c, i);
    store_string(c, key_offset, "title");
    store_string(c, value_offset, "hello");
    pointer_at(c, key_offset);
    pointer_at(c, value_offset).call(i.add_text);

    store_string(c, key_offset, "amount");
    pointer_at(c, key_offset);
    pointer_at(c, amount_offset).call(i.get_num).drop();
    store_string(c, key_offset, "echo");
    pointer_at(c, key_offset);
    load_i64(c, amount_offset).call(i.add_num);

    store_string(c, key_offset, "group");
    pointer_at(c, key_offset).call(i.open_group);
    store_string(c, key_offset, "inner");
    pointer_at(c, key_offset).i64(5).call(i.add_num);
    c.call(i.close_group);

    store_string(c, key_offset, "k");
    pointer_at(c, key_offset).i32(1);
    pointer_at(c, read_offset).i32(8).call(i.var_read).drop();
    store_string(c, key_offset, "var");
    pointer_at(c, key_offset);
    load_i64(c, read_offset).call(i.add_num);

    pointer_at(c, cid_offset).i32(7).i32(32).call(i.memset).drop();
    pointer_at(c, cid_offset).i32(generated_method);
    pointer_at(c, call_args_offset).i32(4).i64(deposit_amount).call(i.generate_call);
    end_frame(c, i);

    m.exp("Method_2", m.func(mt, c, {{1, t_i32}}));
    return m.build();
  }

  std::string spin_shader()
  {
    module_builder m;
    uint32_t mt = method_type(m);
    m.exp("Ctor", m.func(mt, code().end()));
    m.exp("Dtor", m.func(mt, code().end()));
    m.exp("Method_2", m.func(mt, code().loop().br(0).end().end()));
    return m.build();
  }

  std::string signed_call_shader()
  {
    module_builder m;
    const uint32_t alloc = m.import("StackAlloc", {t_i32}, {t_i32});
    const uint32_t free = m.import("StackFree", {t_i32}, {});
    const uint32_t fill = m.import("Memset", {t_i32, t_i32, t_i32}, {t_i32});
    const uint32_t generate = m.import("GenerateCallSigned", {t_i32, t_i32, t_i32, t_i32, t_i64, t_i32, t_i32}, {});
    const uint32_t mt = method_type(m);
    add_ctor_dtor(m, mt);
    code c;
    c.i32(96).call(alloc).set(1);
    c.get(1).i32(7).i32(32).call(fill).drop();
    c.get(1).i32(32).op(0x6a).i32(9).i32(32).call(fill).drop();
    c.get(1).i32(2).get(1).i32(0).i64(0).get(1).i32(32).op(0x6a).i32(1).call(generate);
    c.i32(96).call(free).end();
    m.exp("Method_2", m.func(mt, c, {{1, t_i32}}));
    return m.build();
  }

  std::string derived_call_shader()
  {
    module_builder m;
    const uint32_t alloc = m.import("StackAlloc", {t_i32}, {t_i32});
    const uint32_t free = m.import("StackFree", {t_i32}, {});
    const uint32_t fill = m.import("Memset", {t_i32, t_i32, t_i32}, {t_i32});
    const uint32_t derive = m.import("DerivePk", {t_i32, t_i32, t_i32}, {});
    const uint32_t generate = m.import("GenerateCallSigned", {t_i32, t_i32, t_i32, t_i32, t_i64, t_i32, t_i32}, {});
    const uint32_t mt = method_type(m);
    add_ctor_dtor(m, mt);
    code c;
    c.i32(96).call(alloc).set(1);
    c.get(1).i32(7).i32(32).call(fill).drop();
    c.get(1).i32(32).op(0x6a).i32(9).i32(32).call(fill).drop();
    c.get(1).get(1).i32(32).op(0x6a).get(1).i32(64).op(0x6a).call(derive);
    c.get(1).i32(2).get(1).i32(0).i64(0).get(1).i32(64).op(0x6a).i32(1).call(generate);
    c.i32(96).call(free).end();
    m.exp("Method_2", m.func(mt, c, {{1, t_i32}}));
    return m.build();
  }

  std::string save_var_shader()
  {
    module_builder m;
    imports i = import_all(m);
    uint32_t mt = method_type(m);
    m.exp("Ctor", m.func(mt, code().end()));
    m.exp("Dtor", m.func(mt, code().end()));
    code c;
    begin_frame(c, i);
    pointer_at(c, 0).i32(1);
    pointer_at(c, 8).i32(1).i32(0).call(i.save_var).drop();
    end_frame(c, i);
    m.exp("Method_2", m.func(mt, c, {{1, t_i32}}));
    return m.build();
  }

  std::string compiled(const std::string& wasm)
  {
    std::string out;
    EXPECT_TRUE(vm::app::compile_app(wasm, out));
    return out;
  }

  bool read_stored(const std::string& key, std::string& value)
  {
    if (key != "k")
      return false;
    value.assign(8, '\0');
    for (size_t b = 0; b < 8; ++b)
      value[b] = static_cast<char>((stored_value >> (8 * b)) & 0xff);
    return true;
  }
}

TEST(vm_app_shader, document_state_read_and_generated_call)
{
  vm::app::arguments args;
  args["amount"] = std::to_string(echoed_amount);
  vm::app::run_result r = vm::app::run(compiled(document_shader()), method_entry, args, read_stored);
  ASSERT_EQ(r.code, vm::status::ok);
  EXPECT_EQ(vm::app::document_to_json(r.document),
            "{\"title\":\"hello\",\"echo\":777,\"group\":{\"inner\":5},\"var\":1000}");
  ASSERT_EQ(r.calls.size(), 1u);
  EXPECT_EQ(r.calls[0].kind, vm::call_kind::invoke);
  EXPECT_EQ(r.calls[0].method, generated_method);
  EXPECT_EQ(r.calls[0].deposit, deposit_amount);
  EXPECT_EQ(r.calls[0].args.size(), 4u);
  for (size_t b = 0; b < sizeof(crypto::hash); ++b)
    EXPECT_EQ(reinterpret_cast<const uint8_t*>(&r.calls[0].contract_id)[b], 7);
  EXPECT_GT(r.steps_used, 0u);
}

TEST(vm_app_shader, generated_call_carries_signer_requests)
{
  const vm::app::run_result result = vm::app::run(compiled(signed_call_shader()), method_entry, {}, read_stored);
  ASSERT_EQ(vm::status::ok, result.code);
  ASSERT_EQ(1u, result.calls.size());
  ASSERT_EQ(1u, result.calls.front().signers.size());
  for (uint8_t byte : result.calls.front().signers.front().data)
    EXPECT_EQ(9u, byte);
}

TEST(vm_app_shader, derives_public_key_for_generated_call)
{
  crypto::public_key expected{};
  memset(&expected, 0x5a, sizeof(expected));
  const vm::app::key_deriver derive = [&](const crypto::hash& contract_id, const crypto::hash& key_id, crypto::public_key& public_key) {
    for (uint8_t byte : contract_id.data)
      EXPECT_EQ(7u, byte);
    for (uint8_t byte : key_id.data)
      EXPECT_EQ(9u, byte);
    public_key = expected;
    return true;
  };
  crypto::hash invoked_contract{};
  memset(&invoked_contract, 7, sizeof(invoked_contract));
  const std::string shader = compiled(derived_call_shader());
  const vm::app::run_result result = vm::app::run(shader, method_entry, {}, read_stored, vm::app::step_budget, derive, {invoked_contract});
  ASSERT_EQ(vm::status::ok, result.code);
  ASSERT_EQ(1u, result.calls.size());
  ASSERT_EQ(1u, result.calls.front().signers.size());
  EXPECT_EQ(expected, result.calls.front().signers.front());
  EXPECT_EQ(vm::status::trap, vm::app::run(shader, method_entry, {}, read_stored, vm::app::step_budget, derive).code);
  crypto::hash other_contract{};
  memset(&other_contract, 8, sizeof(other_contract));
  EXPECT_EQ(vm::status::trap, vm::app::run(shader, method_entry, {}, read_stored, vm::app::step_budget, derive, {other_contract}).code);
  const vm::app::key_authorizer authorize = [&](const crypto::hash& contract_id, const crypto::hash& key_id, boost::optional<crypto::public_key>& approved_key) {
    EXPECT_EQ(invoked_contract, contract_id);
    for (uint8_t byte : key_id.data)
      EXPECT_EQ(9u, byte);
    approved_key = expected;
    return true;
  };
  EXPECT_EQ(vm::status::ok, vm::app::run(shader, method_entry, {}, read_stored, vm::app::step_budget, derive, {other_contract}, authorize).code);
  const vm::app::key_authorizer wrong_key = [](const crypto::hash&, const crypto::hash&, boost::optional<crypto::public_key>& approved_key) {
    approved_key = crypto::null_pkey;
    return true;
  };
  EXPECT_EQ(vm::status::trap, vm::app::run(shader, method_entry, {}, read_stored, vm::app::step_budget, derive, {other_contract}, wrong_key).code);
  EXPECT_EQ(vm::status::trap, vm::app::run(shader, method_entry, {}, read_stored).code);
}

TEST(vm_app_shader, approval_requires_matching_public_key_before_use)
{
  tools::app_shader_approvals approvals;
  crypto::hash contract{};
  crypto::hash key_id{};
  crypto::public_key expected{};
  crypto::public_key other{};
  contract.data[0] = 1;
  key_id.data[0] = 2;
  expected.data[0] = 3;
  other.data[0] = 4;
  std::string error;
  ASSERT_TRUE(approvals.add(contract, key_id, expected, error));
  boost::optional<crypto::public_key> offered;
  ASSERT_TRUE(approvals.preauthorize(contract, key_id, offered));
  ASSERT_EQ(expected, *offered);
  EXPECT_FALSE(approvals.confirm(contract, key_id, other, error));
  EXPECT_FALSE(approvals.all_used());
  EXPECT_NE(std::string::npos, error.find("key id"));
  EXPECT_NE(std::string::npos, error.find(epee::string_tools::pod_to_hex(key_id)));
  EXPECT_TRUE(approvals.confirm(contract, key_id, expected, error));
  EXPECT_TRUE(approvals.all_used());
}

TEST(vm_app_shader, conflicting_approval_order_is_rejected)
{
  crypto::hash contract{};
  crypto::hash key_id{};
  crypto::public_key first{};
  crypto::public_key second{};
  first.data[0] = 1;
  second.data[0] = 2;
  for (bool reverse : {false, true})
  {
    tools::app_shader_approvals approvals;
    std::string error;
    ASSERT_TRUE(approvals.add(contract, key_id, reverse ? second : first, error));
    EXPECT_FALSE(approvals.add(contract, key_id, reverse ? first : second, error));
    EXPECT_NE(std::string::npos, error.find("Duplicate approval"));
    EXPECT_NE(std::string::npos, error.find(epee::string_tools::pod_to_hex(key_id)));
  }
}

TEST(vm_app_shader, mismatching_approval_traps_without_consumption)
{
  tools::app_shader_approvals approvals;
  crypto::hash contract{};
  crypto::hash key_id{};
  crypto::public_key expected{};
  crypto::public_key other{};
  memset(&contract, 7, sizeof(contract));
  memset(&key_id, 9, sizeof(key_id));
  expected.data[0] = 1;
  other.data[0] = 2;
  std::string error;
  ASSERT_TRUE(approvals.add(contract, key_id, other, error));
  const vm::app::key_deriver derive = [&](const crypto::hash&, const crypto::hash&, crypto::public_key& public_key) {
    public_key = expected;
    return true;
  };
  const vm::app::key_authorizer authorize = [&](const crypto::hash& cid, const crypto::hash& id, boost::optional<crypto::public_key>& offered) {
    return approvals.preauthorize(cid, id, offered);
  };
  const vm::app::key_confirmation confirm = [&](const crypto::hash& cid, const crypto::hash& id, const crypto::public_key& public_key) {
    return approvals.confirm(cid, id, public_key, error);
  };
  EXPECT_EQ(vm::status::trap, vm::app::run(compiled(derived_call_shader()), method_entry, {}, read_stored,
      vm::app::step_budget, derive, {}, authorize, confirm).code);
  EXPECT_FALSE(approvals.all_used());
  EXPECT_NE(std::string::npos, error.find("key id"));
  EXPECT_EQ(error, approvals.result_error(error));
}

TEST(vm_app_shader, approval_error_precedes_unused_approval)
{
  tools::app_shader_approvals approvals;
  crypto::hash contract{};
  crypto::hash key_id{};
  crypto::public_key public_key{};
  std::string error;
  ASSERT_TRUE(approvals.add(contract, key_id, public_key, error));
  error = "Derivation failed after approval confirmation";
  EXPECT_EQ(error, approvals.result_error(error));
  EXPECT_EQ("Approval list contains an unrequested key", approvals.result_error({}));
}

TEST(vm_app_shader, unapproved_derivation_does_not_call_deriver)
{
  size_t derivations = 0;
  const vm::app::key_deriver derive = [&](const crypto::hash&, const crypto::hash&, crypto::public_key&) {
    ++derivations;
    return true;
  };
  const vm::app::run_result result = vm::app::run(compiled(derived_call_shader()), method_entry, {}, read_stored,
      vm::app::step_budget, derive);
  EXPECT_EQ(vm::status::trap, result.code);
  EXPECT_EQ(0u, derivations);
}

TEST(vm_app_shader, rejected_authorization_precedes_derivation)
{
  size_t derivations = 0;
  const vm::app::key_deriver derive = [&](const crypto::hash&, const crypto::hash&, crypto::public_key&) {
    ++derivations;
    return true;
  };
  const vm::app::key_authorizer reject = [](const crypto::hash&, const crypto::hash&, boost::optional<crypto::public_key>&) {
    return false;
  };
  const vm::app::run_result result = vm::app::run(compiled(derived_call_shader()), method_entry, {}, read_stored,
      vm::app::step_budget, derive, {}, reject);
  EXPECT_EQ(vm::status::trap, result.code);
  EXPECT_EQ(0u, derivations);
}

TEST(vm_app_shader, run_is_deterministic)
{
  vm::app::arguments args;
  args["amount"] = "5";
  std::string code_bytes = compiled(document_shader());
  vm::app::run_result a = vm::app::run(code_bytes, method_entry, args, read_stored);
  vm::app::run_result b = vm::app::run(code_bytes, method_entry, args, read_stored);
  EXPECT_EQ(vm::app::document_to_json(a.document), vm::app::document_to_json(b.document));
  EXPECT_EQ(a.steps_used, b.steps_used);
}

TEST(vm_app_shader, app_imports_are_rejected_by_contract_compile)
{
  std::string wasm = document_shader();
  std::string out;
  EXPECT_FALSE(vm::compile(wasm, out));
  EXPECT_TRUE(vm::app::compile_app(wasm, out));
}

TEST(vm_app_shader, contract_imports_are_rejected_by_app_compile)
{
  module_builder m;
  uint32_t funds = m.import("FundsLock", {t_i64}, {});
  uint32_t mt = method_type(m);
  m.exp("Ctor", m.func(mt, code().end()));
  m.exp("Dtor", m.func(mt, code().end()));
  m.exp("Method_2", m.func(mt, code().i64(1).call(funds).end()));
  std::string wasm = m.build();
  std::string out;
  EXPECT_TRUE(vm::compile(wasm, out));
  EXPECT_FALSE(vm::app::compile_app(wasm, out));
}

TEST(vm_app_shader, step_budget_is_enforced)
{
  vm::app::run_result r = vm::app::run(compiled(spin_shader()), method_entry, {}, read_stored, 10000);
  EXPECT_EQ(r.code, vm::status::out_of_gas);
  EXPECT_TRUE(r.document.empty());
  EXPECT_TRUE(r.calls.empty());
}

TEST(vm_app_shader, state_writes_trap)
{
  vm::app::run_result r = vm::app::run(compiled(save_var_shader()), method_entry, {}, read_stored);
  EXPECT_EQ(r.code, vm::status::trap);
}

TEST(vm_app_shader, constructor_entry_is_rejected)
{
  vm::app::run_result r = vm::app::run(compiled(spin_shader()), vm::method_ctor, {}, read_stored);
  EXPECT_EQ(r.code, vm::status::invalid_call);
}

TEST(vm_app_shader, parse_arguments)
{
  vm::app::arguments out;
  EXPECT_TRUE(vm::app::parse_arguments("a=1,b=two,c=", out));
  EXPECT_EQ(out.size(), 3u);
  EXPECT_EQ(out["b"], "two");
  EXPECT_TRUE(vm::app::parse_arguments("", out));
  EXPECT_TRUE(out.empty());
  EXPECT_FALSE(vm::app::parse_arguments("novalue", out));
  EXPECT_FALSE(vm::app::parse_arguments("=x", out));
  EXPECT_FALSE(vm::app::parse_arguments("a=1,a=2", out));
}

TEST(vm_app_shader, json_escapes_text_and_hexes_blobs)
{
  vm::app::document_node text;
  text.id = "q\"";
  text.text = "line\n\\";
  vm::app::document_node blob;
  blob.id = "b";
  blob.type = vm::app::document_node::kind::blob;
  blob.text = std::string("\x01\xab", 2);
  EXPECT_EQ(vm::app::document_to_json({text, blob}), "{\"q\\\"\":\"line\\n\\\\\",\"b\":\"01ab\"}");
}
