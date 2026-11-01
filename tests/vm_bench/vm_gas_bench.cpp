// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "crypto/crypto.h"
#include "vm/gas.h"
#include "vm/limits.h"
#include "vm/state_tree.h"
#include "vm/vm.h"
#include "vm_test_utils.h"

namespace
{
  using namespace vm_test;
  using steady = std::chrono::steady_clock;

  constexpr uint64_t unlimited_gas = uint64_t(1) << 60;
  constexpr uint32_t off_cid = 0;
  constexpr uint32_t off_count = 32;
  constexpr uint32_t off_key = 40;
  constexpr uint32_t off_crypto = 96;
  constexpr uint32_t off_a = 512;
  constexpr uint32_t region = 8192;
  constexpr uint32_t off_b = off_a + region;
  constexpr uint32_t off_c = off_b + region;
  constexpr uint32_t off_d = off_c + region;
  constexpr uint32_t first_method = 2;
  constexpr uint32_t callee_method = 3;
  constexpr uint32_t counter_local = 1;
  constexpr uint32_t scratch_local = 2;
  constexpr uint32_t handle_local = 3;

  struct options
  {
    std::string filter;
    unsigned repeats = 5;
    double scale = 1.0;
  };

  struct sample
  {
    std::string name;
    std::string group;
    uint64_t param = 0;
    uint32_t plain = 0;
    uint32_t ops = 1;
    uint32_t reps = 1;
    double ns = 0;
    double gas = 0;
    std::string status = "ok";
  };

  struct host_signature
  {
    const char* name;
    bytes params;
    bytes results;
  };

  const bytes five = {t_i32, t_i32, t_i32, t_i32, t_i32};

  const std::vector<host_signature>& host_signatures()
  {
    static const std::vector<host_signature> table = {
      {"Memcpy", {t_i32, t_i32, t_i32}, {t_i32}},
      {"Memset", {t_i32, t_i32, t_i32}, {t_i32}},
      {"Memcmp", {t_i32, t_i32, t_i32}, {t_i32}},
      {"Memis0", {t_i32, t_i32}, {t_i32}},
      {"Strlen", {t_i32}, {t_i32}},
      {"Strcmp", {t_i32, t_i32}, {t_i32}},
      {"StackAlloc", {t_i32}, {t_i32}},
      {"StackFree", {t_i32}, {}},
      {"Heap_Alloc", {t_i32}, {t_i32}},
      {"Heap_Free", {t_i32}, {}},
      {"LoadVar", five, {t_i32}},
      {"SaveVar", five, {t_i32}},
      {"EmitLog", five, {t_i32}},
      {"CallFar", five, {}},
      {"get_CallDepth", {}, {t_i32}},
      {"get_CallerCid", {t_i32, t_i32}, {}},
      {"FundsLock", {t_i64}, {}},
      {"FundsUnlock", {t_i64}, {}},
      {"HashWrite", {t_i32, t_i32, t_i32}, {}},
      {"HashGetValue", {t_i32, t_i32, t_i32}, {}},
      {"HashFree", {t_i32}, {}},
      {"HashClone", {t_i32}, {t_i32}},
      {"get_Height", {}, {t_i64}},
      {"get_ParentHdr", {t_i32}, {}},
      {"HashCreateSha256", {}, {t_i32}},
      {"HashCreateBlake2b", {t_i32, t_i32, t_i32}, {t_i32}},
      {"HashCreateKeccak", {t_i32}, {t_i32}},
      {"CheckSignature", {t_i32, t_i32, t_i32}, {t_i32}},
    };
    return table;
  }

  struct pad_function_set
  {
    uint32_t noop;
    uint32_t locals_i32_64;
    uint32_t locals_i32_4096;
    uint32_t locals_i64_4096;
    uint32_t noop_type;
  };

  struct environment
  {
    module_builder m;
    std::map<std::string, uint32_t> host;
    uint32_t method;
    pad_function_set pads;

    environment()
    {
      for (const host_signature& h : host_signatures())
        host[h.name] = m.import(h.name, h.params, h.results);
      m.stack_pointer_global();
      method = method_type(m);
      add_ctor_dtor(m, method);
      pads.noop_type = m.type({}, {});
      pads.noop = m.func(pads.noop_type, code().end());
      pads.locals_i32_64 = m.func(pads.noop_type, code().end(), {{64, t_i32}});
      pads.locals_i32_4096 = m.func(pads.noop_type, code().end(), {{4096, t_i32}});
      pads.locals_i64_4096 = m.func(pads.noop_type, code().end(), {{4096, t_i64}});
    }

    code& call(code& c, const char* name) { return c.call(host.at(name)); }
  };

  using emit = std::function<void(environment&, code&)>;
  using prepare_state = std::function<void(vm::state_tree&, const crypto::hash&)>;

  struct bench_case
  {
    std::string name;
    std::string group;
    uint64_t param = 0;
    uint32_t plain = 0;
    uint32_t ops = 1;
    uint32_t reps = 16;
    uint32_t n_low = 4000;
    uint32_t n_high = 24000;
    emit setup;
    emit sequence;
    prepare_state prepare;
    std::string callee;
    std::function<void(std::string&)> patch_args;
  };

  code& pointer(code& c, uint32_t offset)
  {
    return c.get(0).i32(static_cast<int32_t>(offset)).op(0x6a);
  }

  code& memarg(code& c, uint8_t opcode)
  {
    return c.op(opcode).op(0).op(0);
  }

  void put_u32(std::string& s, uint32_t at, uint32_t v)
  {
    for (int i = 0; i < 4; ++i)
      s[at + i] = static_cast<char>((v >> (8 * i)) & 0xff);
  }

  std::string base_args()
  {
    std::string args(vm::limits::args_size, 0);
    for (size_t i = 0; i < args.size(); ++i)
      args[i] = static_cast<char>((i * 31 + 7) % 251 + 1);
    for (uint32_t i = off_cid; i < off_cid + 32; ++i)
      args[i] = 0;
    for (uint32_t i = 0; i < region; ++i)
      args[off_b + i] = args[off_a + i];
    return args;
  }

  double elapsed_ns(steady::time_point from, steady::time_point to)
  {
    return std::chrono::duration<double, std::nano>(to - from).count();
  }

  vm::result run_invoke(const vm::state_tree& state, const crypto::hash& cid, const std::string& args, double& ns)
  {
    vm::call c;
    c.kind = vm::call_kind::invoke;
    c.contract_id = cid;
    c.method = first_method;
    c.args = args;
    steady::time_point t0 = steady::now();
    vm::result r = vm::execute({}, c, state, unlimited_gas);
    ns = elapsed_ns(t0, steady::now());
    return r;
  }

  std::string describe(vm::status s)
  {
    switch (s)
    {
    case vm::status::ok: return "ok";
    case vm::status::out_of_gas: return "out_of_gas";
    case vm::status::trap: return "trap";
    case vm::status::invalid_module: return "invalid_module";
    case vm::status::invalid_call: return "invalid_call";
    }
    return "unknown";
  }

  bool deploy_module(vm::state_tree& state, const std::string& wasm, crypto::hash& cid)
  {
    vm::result r = deploy(state, wasm);
    cid = r.contract_id;
    return r.code == vm::status::ok;
  }

  std::string padded_callee_module(uint32_t target_bytes)
  {
    module_builder m;
    uint32_t mt = method_type(m);
    add_ctor_dtor(m, mt);
    m.exp("Method_2", m.func(mt, code().end()));
    code empty;
    empty.end();
    m.exp("Method_3", m.func(mt, empty));
    const uint32_t pairs = 4000;
    code body;
    for (uint32_t i = 0; i < pairs; ++i)
      body.i32(1).drop();
    body.end();
    const uint32_t function_bytes = pairs * 3;
    for (uint32_t i = 0; i + function_bytes < target_bytes; i += function_bytes)
      m.func(mt, body);
    return m.build();
  }

  sample run_case(const bench_case& bc, const options& opt)
  {
    sample out;
    out.name = bc.name;
    out.group = bc.group;
    out.param = bc.param;
    out.plain = bc.plain;
    out.ops = bc.ops;
    out.reps = bc.reps;

    environment e;
    code body;
    pointer(body, off_count).op(0x28).op(0).op(0).set(counter_local);
    if (bc.setup)
      bc.setup(e, body);
    body.loop();
    for (uint32_t i = 0; i < bc.reps; ++i)
      bc.sequence(e, body);
    body.get(counter_local).i32(1).op(0x6b).op(0x22).op(counter_local).br_if(0).end().end();
    e.m.exp("Method_2", e.m.func(e.method, body, {{3, t_i32}, {1, t_i64}}));
    e.m.indirect_table({e.pads.noop});

    vm::state_tree state;
    crypto::hash cid;
    if (!deploy_module(state, e.m.build(), cid))
    {
      out.status = "deploy_failed";
      return out;
    }

    std::string args = base_args();
    if (!bc.callee.empty())
    {
      crypto::hash callee_cid;
      if (!deploy_module(state, bc.callee, callee_cid))
      {
        out.status = "callee_deploy_failed";
        return out;
      }
      std::memcpy(&args[off_cid], &callee_cid, sizeof(callee_cid));
    }
    if (bc.patch_args)
      bc.patch_args(args);
    if (bc.prepare)
      bc.prepare(state, cid);

    const uint32_t n_low = std::max<uint32_t>(4, static_cast<uint32_t>(bc.n_low * opt.scale));
    const uint32_t n_high = std::max<uint32_t>(n_low + 4, static_cast<uint32_t>(bc.n_high * opt.scale));

    auto best = [&](uint32_t n, uint64_t& gas, std::string& status) {
      put_u32(args, off_count, n);
      double best_ns = 1e300;
      for (unsigned r = 0; r < opt.repeats; ++r)
      {
        double ns = 0;
        vm::result res = run_invoke(state, cid, args, ns);
        if (res.code != vm::status::ok)
        {
          status = describe(res.code);
          return 0.0;
        }
        gas = res.gas_used;
        best_ns = std::min(best_ns, ns);
      }
      return best_ns;
    };

    uint64_t gas_low = 0, gas_high = 0;
    double ns_low = best(n_low, gas_low, out.status);
    double ns_high = best(n_high, gas_high, out.status);
    if (out.status != "ok")
      return out;

    out.ns = (ns_high - ns_low) / (n_high - n_low);
    out.gas = static_cast<double>(gas_high - gas_low) / (n_high - n_low);
    return out;
  }

  std::vector<bench_case> loop_baseline()
  {
    bench_case empty;
    empty.name = "loop_overhead";
    empty.group = "baseline";
    empty.ops = 0;
    empty.reps = 0;
    empty.sequence = [](environment&, code&) {};

    bench_case pair;
    pair.name = "plain_const_drop";
    pair.group = "baseline";
    pair.ops = 2;
    pair.sequence = [](environment&, code& c) { c.i32(1).drop(); };
    return {empty, pair};
  }

  struct op_shape
  {
    const char* name;
    uint8_t opcode32;
    uint8_t opcode64;
  };

  const std::vector<op_shape> binary_ops = {
    {"add", 0x6a, 0x7c}, {"sub", 0x6b, 0x7d}, {"mul", 0x6c, 0x7e}, {"div_s", 0x6d, 0x7f},
    {"div_u", 0x6e, 0x80}, {"rem_s", 0x6f, 0x81}, {"rem_u", 0x70, 0x82}, {"and", 0x71, 0x83},
    {"or", 0x72, 0x84}, {"xor", 0x73, 0x85}, {"shl", 0x74, 0x86}, {"shr_s", 0x75, 0x87},
    {"shr_u", 0x76, 0x88}, {"rotl", 0x77, 0x89}, {"rotr", 0x78, 0x8a},
  };

  const std::vector<op_shape> compare_ops = {
    {"eq", 0x46, 0x51}, {"ne", 0x47, 0x52}, {"lt_s", 0x48, 0x53}, {"lt_u", 0x49, 0x54},
    {"gt_s", 0x4a, 0x55}, {"gt_u", 0x4b, 0x56}, {"le_s", 0x4c, 0x57}, {"le_u", 0x4d, 0x58},
    {"ge_s", 0x4e, 0x59}, {"ge_u", 0x4f, 0x5a},
  };

  struct memory_op
  {
    const char* name;
    uint8_t opcode;
    bool wide_value;
  };

  const std::vector<memory_op> load_ops = {
    {"i32.load", 0x28, false}, {"i64.load", 0x29, true}, {"i32.load8_s", 0x2c, false},
    {"i32.load8_u", 0x2d, false}, {"i32.load16_s", 0x2e, false}, {"i32.load16_u", 0x2f, false},
    {"i64.load8_s", 0x30, true}, {"i64.load8_u", 0x31, true}, {"i64.load16_s", 0x32, true},
    {"i64.load16_u", 0x33, true}, {"i64.load32_s", 0x34, true}, {"i64.load32_u", 0x35, true},
  };

  const std::vector<memory_op> store_ops = {
    {"i32.store", 0x36, false}, {"i64.store", 0x37, true}, {"i32.store8", 0x3a, false},
    {"i32.store16", 0x3b, false}, {"i64.store8", 0x3c, true}, {"i64.store16", 0x3d, true},
    {"i64.store32", 0x3e, true},
  };

  bench_case opcode_case(const std::string& name, uint32_t plain, uint32_t ops, emit sequence)
  {
    bench_case bc;
    bc.name = name;
    bc.group = "opcode";
    bc.plain = plain;
    bc.ops = ops;
    bc.sequence = std::move(sequence);
    return bc;
  }

  std::vector<bench_case> opcode_cases()
  {
    std::vector<bench_case> cases;
    constexpr int32_t a32 = 0x7fff1234;
    constexpr int32_t b32 = 0x1f;
    constexpr int64_t a64 = 0x0123456789abcdefLL;
    constexpr int64_t b64 = 0x1f;

    for (const op_shape& s : binary_ops)
    {
      uint8_t op32 = s.opcode32, op64 = s.opcode64;
      cases.push_back(opcode_case(std::string("i32.") + s.name, 3, 1, [=](environment&, code& c) { c.i32(a32).i32(b32).op(op32).drop(); }));
      cases.push_back(opcode_case(std::string("i64.") + s.name, 3, 1, [=](environment&, code& c) { c.i64(a64).i64(b64).op(op64).drop(); }));
    }
    for (const op_shape& s : compare_ops)
    {
      uint8_t op32 = s.opcode32, op64 = s.opcode64;
      cases.push_back(opcode_case(std::string("i32.") + s.name, 3, 1, [=](environment&, code& c) { c.i32(a32).i32(b32).op(op32).drop(); }));
      cases.push_back(opcode_case(std::string("i64.") + s.name, 3, 1, [=](environment&, code& c) { c.i64(a64).i64(b64).op(op64).drop(); }));
    }
    cases.push_back(opcode_case("i32.eqz", 2, 1, [=](environment&, code& c) { c.i32(a32).op(0x45).drop(); }));
    cases.push_back(opcode_case("i64.eqz", 2, 1, [=](environment&, code& c) { c.i64(a64).op(0x50).drop(); }));
    cases.push_back(opcode_case("i32.wrap_i64", 2, 1, [=](environment&, code& c) { c.i64(a64).op(0xa7).drop(); }));
    cases.push_back(opcode_case("i64.extend_i32_s", 2, 1, [=](environment&, code& c) { c.i32(a32).op(0xac).drop(); }));
    cases.push_back(opcode_case("i64.extend_i32_u", 2, 1, [=](environment&, code& c) { c.i32(a32).op(0xad).drop(); }));

    for (const memory_op& m : load_ops)
    {
      uint8_t opcode = m.opcode;
      cases.push_back(opcode_case(m.name, 2, 1, [=](environment&, code& c) { pointer(c, off_a); memarg(c, opcode).drop(); }));
      cases.back().plain = 5;
    }
    for (const memory_op& m : store_ops)
    {
      uint8_t opcode = m.opcode;
      bool wide = m.wide_value;
      cases.push_back(opcode_case(m.name, 5, 1, [=](environment&, code& c) {
        pointer(c, off_c);
        wide ? c.i64(a64) : c.i32(a32);
        memarg(c, opcode);
      }));
    }

    cases.push_back(opcode_case("select", 4, 1, [=](environment&, code& c) { c.i32(1).i32(2).i32(1).op(0x1b).drop(); }));
    cases.push_back(opcode_case("local.get", 1, 1, [](environment&, code& c) { c.get(scratch_local).drop(); }));
    cases.push_back(opcode_case("local.set", 1, 1, [](environment&, code& c) { c.i32(1).set(scratch_local); }));
    cases.push_back(opcode_case("local.tee", 2, 1, [](environment&, code& c) { c.i32(1).op(0x22).op(scratch_local).drop(); }));
    cases.push_back(opcode_case("i32.const", 1, 1, [](environment&, code& c) { c.i32(1).drop(); }));
    cases.push_back(opcode_case("i64.const", 1, 1, [=](environment&, code& c) { c.i64(a64).drop(); }));
    cases.push_back(opcode_case("drop", 1, 1, [](environment&, code& c) { c.i32(1).drop(); }));
    cases.push_back(opcode_case("br", 0, 1, [](environment&, code& c) { c.block().br(0).end(); }));
    cases.push_back(opcode_case("br_if", 1, 1, [](environment&, code& c) { c.block().i32(1).br_if(0).end(); }));
    cases.push_back(opcode_case("br_table", 1, 1, [](environment&, code& c) {
      c.block().block().i32(0).op(0x0e).op(1).op(0).op(1).end().end();
    }));
    cases.push_back(opcode_case("call_ret", 0, 2, [](environment& e, code& c) { c.call(e.pads.noop); }));
    cases.push_back(opcode_case("call_indirect_ret", 1, 2, [](environment& e, code& c) { c.i32(1).op(0x11).op(static_cast<uint8_t>(e.pads.noop_type)).op(0); }));
    cases.push_back(opcode_case("global.get_set", 0, 2, [](environment&, code& c) { c.op(0x23).op(0).op(0x24).op(0); }));
    return cases;
  }

  std::vector<bench_case> prolog_cases()
  {
    struct variant { const char* name; uint64_t locals; uint32_t pad_function_set::*member; };
    std::vector<bench_case> cases;
    const std::vector<std::pair<std::pair<const char*, uint64_t>, std::function<uint32_t(const pad_function_set&)>>> variants = {
      {{"call_locals_0", 0}, [](const pad_function_set& p) { return p.noop; }},
      {{"call_locals_64_i32", 64}, [](const pad_function_set& p) { return p.locals_i32_64; }},
      {{"call_locals_4096_i32", 4096}, [](const pad_function_set& p) { return p.locals_i32_4096; }},
      {{"call_locals_4096_i64", 8192}, [](const pad_function_set& p) { return p.locals_i64_4096; }},
    };
    for (const auto& v : variants)
    {
      bench_case bc;
      bc.name = v.first.first;
      bc.group = "prolog";
      bc.param = v.first.second;
      bc.ops = 2;
      bc.plain = 0;
      auto pick = v.second;
      bc.sequence = [pick](environment& e, code& c) { c.call(pick(e.pads)); };
      cases.push_back(bc);
    }
    return cases;
  }

  bench_case host_case(const std::string& name, uint64_t param, uint32_t plain, uint32_t ops, emit setup, emit sequence)
  {
    bench_case bc;
    bc.name = name;
    bc.group = "host";
    bc.param = param;
    bc.plain = plain;
    bc.ops = ops;
    bc.setup = std::move(setup);
    bc.sequence = std::move(sequence);
    return bc;
  }

  void fill_strings(environment& e, code& c, uint32_t length)
  {
    for (uint32_t base : {off_c, off_d})
    {
      pointer(c, base).i32('a').i32(static_cast<int32_t>(length));
      e.call(c, "Memset").drop();
      pointer(c, base + length).i32(0);
      memarg(c, 0x3a);
    }
  }

  std::vector<bench_case> memory_host_cases()
  {
    std::vector<bench_case> cases;
    for (uint32_t size : {16u, 256u, 4096u, 8192u})
    {
      cases.push_back(host_case("Memcpy_" + std::to_string(size), size, 8, 1, {}, [=](environment& e, code& c) {
        pointer(c, off_b);
        pointer(c, off_a).i32(static_cast<int32_t>(size));
        e.call(c, "Memcpy").drop();
      }));
      cases.push_back(host_case("Memset_" + std::to_string(size), size, 6, 1, {}, [=](environment& e, code& c) {
        pointer(c, off_c).i32(0x5a).i32(static_cast<int32_t>(size));
        e.call(c, "Memset").drop();
      }));
      cases.push_back(host_case("Memcmp_" + std::to_string(size), size, 8, 1, {}, [=](environment& e, code& c) {
        pointer(c, off_a);
        pointer(c, off_b).i32(static_cast<int32_t>(size));
        e.call(c, "Memcmp").drop();
      }));
      if (size <= 4096)
        cases.push_back(host_case("Memis0_" + std::to_string(size), size, 5, 1,
          [=](environment& e, code& c) { pointer(c, off_d).i32(0).i32(static_cast<int32_t>(size)); e.call(c, "Memset").drop(); },
          [=](environment& e, code& c) { pointer(c, off_d).i32(static_cast<int32_t>(size)); e.call(c, "Memis0").drop(); }));
    }
    for (uint32_t length : {16u, 4096u})
    {
      cases.push_back(host_case("Strlen_" + std::to_string(length), length, 5, 1,
        [=](environment& e, code& c) { fill_strings(e, c, length); },
        [=](environment& e, code& c) { pointer(c, off_c); e.call(c, "Strlen").drop(); }));
      cases.push_back(host_case("Strcmp_" + std::to_string(length), length, 8, 1,
        [=](environment& e, code& c) { fill_strings(e, c, length); },
        [=](environment& e, code& c) { pointer(c, off_c); pointer(c, off_d); e.call(c, "Strcmp").drop(); }));
    }
    for (uint32_t size : {64u, 4096u})
    {
      cases.push_back(host_case("StackAlloc_Free_" + std::to_string(size), size, 3, 2, {}, [=](environment& e, code& c) {
        c.i32(static_cast<int32_t>(size));
        e.call(c, "StackAlloc").drop();
        c.i32(static_cast<int32_t>(size));
        e.call(c, "StackFree");
      }));
    }
    for (uint32_t size : {64u, 4096u, 262144u})
    {
      cases.push_back(host_case("Heap_Alloc_Free_" + std::to_string(size), size, 2, 2, {}, [=](environment& e, code& c) {
        c.i32(static_cast<int32_t>(size));
        e.call(c, "Heap_Alloc").set(scratch_local);
        c.get(scratch_local);
        e.call(c, "Heap_Free");
      }));
      cases.back().n_low = 1000;
      cases.back().n_high = size >= 262144 ? 3000 : 8000;
    }
    return cases;
  }

  void prepare_values(vm::state_tree& state, const crypto::hash& cid, uint32_t keys, uint32_t value_size)
  {
    const std::string value(value_size, 'v');
    for (uint32_t i = 1; i <= keys; ++i)
    {
      std::string key(8, 0);
      put_u32(key, 0, i);
      state.set(var_key(cid, 0, key), value);
    }
  }

  emit key_from_counter()
  {
    return [](environment&, code& c) {
      pointer(c, off_key).get(counter_local);
      memarg(c, 0x36);
    };
  }

  std::vector<bench_case> storage_host_cases()
  {
    std::vector<bench_case> cases;
    for (uint32_t size : {32u, 1024u, 8192u})
    {
      bench_case warm = host_case("LoadVar_warm_" + std::to_string(size), size, 11, 1, {}, [=](environment& e, code& c) {
        pointer(c, off_key).i32(8);
        pointer(c, off_c).i32(static_cast<int32_t>(size)).i32(0);
        e.call(c, "LoadVar").drop();
      });
      warm.prepare = [=](vm::state_tree& s, const crypto::hash& cid) { prepare_values(s, cid, 1, size); };
      warm.patch_args = [](std::string& args) { put_u32(args, off_key, 1); std::fill(args.begin() + off_key + 4, args.begin() + off_key + 8, 0); };
      cases.push_back(warm);
    }
    for (uint32_t stored : {1024u, 65536u, 262144u, 1048576u})
    {
      const uint32_t keys = stored >= 262144 ? 24 : 96;
      bench_case cold = host_case("LoadVar_cold_" + std::to_string(stored) + "_read1", stored, 11, 1, {}, [=](environment& e, code& c) {
        key_from_counter()(e, c);
        pointer(c, off_key).i32(8);
        pointer(c, off_c).i32(1).i32(0);
        e.call(c, "LoadVar").drop();
      });
      cold.plain += 3;
      cold.prepare = [=](vm::state_tree& s, const crypto::hash& cid) { prepare_values(s, cid, keys, stored); };
      cold.patch_args = [](std::string& args) { std::fill(args.begin() + off_key + 4, args.begin() + off_key + 8, 0); };
      cold.reps = 1;
      cold.n_low = keys / 4;
      cold.n_high = keys;
      cases.push_back(cold);
    }
    for (uint32_t size : {32u, 1024u, 8192u, 65536u})
    {
      bench_case same = host_case("SaveVar_same_key_" + std::to_string(size), size, 11, 1, {}, [=](environment& e, code& c) {
        pointer(c, off_key).i32(8);
        pointer(c, off_a).i32(static_cast<int32_t>(size)).i32(0);
        e.call(c, "SaveVar").drop();
      });
      same.patch_args = [](std::string& args) { std::fill(args.begin() + off_key, args.begin() + off_key + 8, 0); };
      if (size > region)
      {
        same.sequence = [=](environment& e, code& c) {
          pointer(c, off_key).i32(8).get(handle_local).i32(static_cast<int32_t>(size)).i32(0);
          e.call(c, "SaveVar").drop();
        };
        same.setup = [=](environment& e, code& c) { c.i32(static_cast<int32_t>(size)); e.call(c, "Heap_Alloc").set(handle_local); };
        same.plain = 9;
        same.n_low = 500;
        same.n_high = 2500;
      }
      cases.push_back(same);

      bench_case distinct = host_case("SaveVar_distinct_keys_" + std::to_string(size), size, 14, 1, {}, [=](environment& e, code& c) {
        key_from_counter()(e, c);
        pointer(c, off_key).i32(8);
        pointer(c, off_a).i32(static_cast<int32_t>(size)).i32(0);
        e.call(c, "SaveVar").drop();
      });
      distinct.patch_args = [](std::string& args) { std::fill(args.begin() + off_key + 4, args.begin() + off_key + 8, 0); };
      distinct.reps = 1;
      distinct.n_low = size >= 8192 ? 100 : 1000;
      distinct.n_high = size >= 8192 ? 500 : 8000;
      if (size > region)
        continue;
      cases.push_back(distinct);
    }
    for (uint32_t size : {32u, 1024u})
    {
      bench_case log = host_case("EmitLog_" + std::to_string(size), size, 11, 1, {}, [=](environment& e, code& c) {
        pointer(c, off_key).i32(8);
        pointer(c, off_a).i32(static_cast<int32_t>(size)).i32(0);
        e.call(c, "EmitLog").drop();
      });
      log.n_low = 1000;
      log.n_high = 8000;
      cases.push_back(log);
    }
    return cases;
  }

  void lay_out_signature(std::string& args, bool corrupt)
  {
    crypto::public_key pub;
    crypto::secret_key sec;
    crypto::generate_keys(pub, sec);
    crypto::hash digest;
    std::memcpy(&digest, &args[off_a], sizeof(digest));
    crypto::signature sig;
    crypto::generate_signature(digest, pub, sec, sig);
    std::memcpy(&args[off_crypto], &digest, sizeof(digest));
    std::memcpy(&args[off_crypto + 32], &pub, sizeof(pub));
    std::memcpy(&args[off_crypto + 64], &sig, sizeof(sig));
    if (corrupt)
      args[off_crypto + 64] ^= 0x55;
  }

  std::vector<bench_case> misc_host_cases()
  {
    std::vector<bench_case> cases;
    cases.push_back(host_case("get_Height", 0, 0, 1, {}, [](environment& e, code& c) { e.call(c, "get_Height").drop(); }));
    cases.push_back(host_case("get_CallDepth", 0, 0, 1, {}, [](environment& e, code& c) { e.call(c, "get_CallDepth").drop(); }));
    cases.push_back(host_case("get_ParentHdr", 0, 3, 1, {}, [](environment& e, code& c) { pointer(c, off_c); e.call(c, "get_ParentHdr"); }));
    cases.push_back(host_case("FundsLock_Unlock", 0, 2, 2, {}, [](environment& e, code& c) {
      c.i64(0);
      e.call(c, "FundsLock");
      c.i64(0);
      e.call(c, "FundsUnlock");
    }));
    for (bool corrupt : {false, true})
    {
      bench_case sig = host_case(corrupt ? "CheckSignature_invalid" : "CheckSignature_valid", 0, 11, 1, {}, [](environment& e, code& c) {
        pointer(c, off_crypto);
        pointer(c, off_crypto + 32);
        pointer(c, off_crypto + 64);
        e.call(c, "CheckSignature").drop();
      });
      sig.patch_args = [=](std::string& args) { lay_out_signature(args, corrupt); };
      sig.reps = 4;
      sig.n_low = 300;
      sig.n_high = 1500;
      cases.push_back(sig);
    }

    struct hash_kind { const char* name; uint32_t plain; emit create; };
    const std::vector<hash_kind> kinds = {
      {"sha256", 0, [](environment& e, code& c) { e.call(c, "HashCreateSha256"); }},
      {"keccak256", 1, [](environment& e, code& c) { c.i32(256); e.call(c, "HashCreateKeccak"); }},
      {"blake2b", 5, [](environment& e, code& c) { pointer(c, off_a).i32(8).i32(32); e.call(c, "HashCreateBlake2b"); }},
    };
    for (const hash_kind& k : kinds)
    {
      emit create = k.create;
      cases.push_back(host_case(std::string("HashCreate_Free_") + k.name, 0, k.plain, 2, {}, [=](environment& e, code& c) {
        create(e, c);
        c.set(handle_local).get(handle_local);
        e.call(c, "HashFree");
      }));
      for (uint32_t size : {32u, 1024u, 8192u})
      {
        cases.push_back(host_case(std::string("HashWrite_") + k.name + "_" + std::to_string(size), size, 6, 1,
          [=](environment& e, code& c) { create(e, c); c.set(handle_local); },
          [=](environment& e, code& c) {
            c.get(handle_local);
            pointer(c, off_a).i32(static_cast<int32_t>(size));
            e.call(c, "HashWrite");
          }));
        if (std::strcmp(k.name, "keccak256") == 0)
        {
          cases.back().n_low = 4;
          cases.back().n_high = vm::limits::heap_size / (size * cases.back().reps);
        }
      }
      cases.push_back(host_case(std::string("HashClone_Read_Free_") + k.name, 32, 9, 3,
        [=](environment& e, code& c) { create(e, c); c.set(handle_local); },
        [=](environment& e, code& c) {
          c.get(handle_local);
          e.call(c, "HashClone").set(scratch_local);
          c.get(scratch_local);
          pointer(c, off_c).i32(32);
          e.call(c, "HashGetValue");
          c.get(scratch_local);
          e.call(c, "HashFree");
        }));
    }
    return cases;
  }

  std::vector<bench_case> far_call_cases()
  {
    std::vector<bench_case> cases;
    struct callee { const char* name; uint32_t bytes; };
    for (const callee& cl : {callee{"small", 0}, callee{"64k", 65536}, callee{"large_450k", 450000}})
    {
      bench_case bc = host_case(std::string("CallFar_") + cl.name, cl.bytes, 0, 1, {}, [](environment& e, code& c) {
        pointer(c, off_cid).i32(callee_method);
        pointer(c, off_cid).i32(0).i32(0);
        e.call(c, "CallFar");
      });
      bc.plain = 8;
      bc.callee = cl.bytes ? padded_callee_module(cl.bytes) : padded_callee_module(1);
      bc.reps = 4;
      bc.n_low = 300;
      bc.n_high = 1500;
      cases.push_back(bc);
    }
    return cases;
  }

  std::string straight_line_module(uint32_t target_bytes, uint32_t locals_per_function)
  {
    module_builder m;
    uint32_t mt = method_type(m);
    add_ctor_dtor(m, mt);
    m.exp("Method_2", m.func(mt, code().end()));
    const uint32_t pairs = 3000;
    code body;
    for (uint32_t i = 0; i < pairs; ++i)
      body.i32(1).drop();
    body.end();
    locals decl;
    if (locals_per_function)
      decl.push_back({locals_per_function, t_i32});
    const uint32_t function_bytes = pairs * 3 + 8;
    for (uint32_t i = 0; i + function_bytes < target_bytes; i += function_bytes)
      m.func(mt, body, decl);
    return m.build();
  }

  std::string nested_blocks_module(uint32_t depth)
  {
    module_builder m;
    uint32_t mt = method_type(m);
    add_ctor_dtor(m, mt);
    code body;
    for (uint32_t i = 0; i < depth; ++i)
      body.block();
    body.i32(1).br_if(depth - 1);
    for (uint32_t i = 0; i < depth; ++i)
      body.end();
    body.end();
    m.exp("Method_2", m.func(mt, body));
    return m.build();
  }

  template <typename F>
  double best_of(unsigned repeats, F&& f)
  {
    double best = 1e300;
    for (unsigned i = 0; i < repeats; ++i)
    {
      steady::time_point t0 = steady::now();
      f();
      best = std::min(best, elapsed_ns(t0, steady::now()));
    }
    return best;
  }

  std::vector<sample> deploy_samples(const options& opt)
  {
    std::vector<sample> out;
    auto measure = [&](const std::string& name, const std::string& wasm, uint32_t locals) {
      sample s;
      s.name = name;
      s.group = "deploy";
      s.param = wasm.size();
      s.plain = locals;
      std::string compiled;
      if (!vm::compile(wasm, compiled))
      {
        s.status = "compile_failed";
        out.push_back(s);
        return;
      }
      s.ns = best_of(opt.repeats, [&] { std::string c; vm::compile(wasm, c); });
      vm::state_tree state;
      vm::result r = deploy(state, wasm, {}, unlimited_gas);
      s.status = describe(r.code);
      s.gas = static_cast<double>(r.gas_used);
      s.reps = static_cast<uint32_t>(compiled.size());
      out.push_back(s);
    };
    for (uint32_t bytes : {1000u, 16000u, 64000u, 200000u, 500000u})
    {
      measure("compile_straight_" + std::to_string(bytes), straight_line_module(bytes, 0), 0);
      measure("compile_locals_" + std::to_string(bytes), straight_line_module(bytes, 2048), 2048);
    }
    for (uint32_t depth : {16u, 256u, 2000u})
      measure("compile_nested_" + std::to_string(depth), nested_blocks_module(depth), 0);
    return out;
  }

  std::vector<sample> invoke_samples(const options& opt)
  {
    std::vector<sample> out;
    auto measure = [&](const std::string& name, const std::string& wasm) {
      sample s;
      s.name = name;
      s.group = "invoke";
      s.param = wasm.size();
      vm::state_tree state;
      crypto::hash cid;
      if (!deploy_module(state, wasm, cid))
      {
        s.status = "deploy_failed";
        out.push_back(s);
        return;
      }
      std::string args;
      double best = 1e300;
      for (unsigned i = 0; i < opt.repeats * 40; ++i)
      {
        double ns = 0;
        vm::result r = run_invoke(state, cid, args, ns);
        if (r.code != vm::status::ok)
        {
          s.status = describe(r.code);
          break;
        }
        s.gas = static_cast<double>(r.gas_used);
        best = std::min(best, ns);
      }
      s.ns = best;
      out.push_back(s);
    };
    measure("invoke_empty_method", minimal_module());
    measure("invoke_empty_method_450k_contract", padded_callee_module(450000));
    return out;
  }

  std::vector<sample> state_tree_samples(const options& opt)
  {
    std::vector<sample> out;
    for (uint32_t leaves : {1000u, 20000u, 200000u})
    {
      vm::state_tree tree;
      const std::string value(64, 'x');
      for (uint32_t i = 0; i < leaves; ++i)
        tree.set("leaf" + std::to_string(i), value);

      sample root;
      root.name = "state_tree_root_" + std::to_string(leaves);
      root.group = "state_tree";
      root.param = leaves;
      root.ns = best_of(std::max(1u, opt.repeats / 2), [&] { tree.get_root(); });
      out.push_back(root);

      vm::write_set writes;
      for (uint32_t i = 0; i < 1000; ++i)
        writes["leaf" + std::to_string(i * 7 % leaves)] = std::string(64, 'y');
      sample apply;
      apply.name = "state_tree_apply_1000_writes_" + std::to_string(leaves);
      apply.group = "state_tree";
      apply.param = leaves;
      apply.ns = best_of(opt.repeats, [&] { vm::state_tree copy = tree; copy.apply(writes); }) / 1000.0;
      out.push_back(apply);
    }
    return out;
  }

  void print_json(const std::vector<sample>& samples, const options& opt)
  {
    std::printf("{\n  \"scale\": %.3f,\n  \"repeats\": %u,\n  \"samples\": [\n", opt.scale, opt.repeats);
    for (size_t i = 0; i < samples.size(); ++i)
    {
      const sample& s = samples[i];
      std::printf("    {\"name\": \"%s\", \"group\": \"%s\", \"param\": %llu, \"plain\": %u, \"ops\": %u, \"reps\": %u, \"ns\": %.4f, \"gas\": %.4f, \"status\": \"%s\"}%s\n",
        s.name.c_str(), s.group.c_str(), static_cast<unsigned long long>(s.param), s.plain, s.ops, s.reps, s.ns, s.gas, s.status.c_str(),
        i + 1 == samples.size() ? "" : ",");
    }
    std::printf("  ]\n}\n");
  }
}

int main(int argc, char** argv)
{
  options opt;
  for (int i = 1; i < argc; ++i)
  {
    const std::string arg = argv[i];
    if (arg == "--filter" && i + 1 < argc)
      opt.filter = argv[++i];
    else if (arg == "--repeats" && i + 1 < argc)
      opt.repeats = static_cast<unsigned>(std::stoul(argv[++i]));
    else if (arg == "--scale" && i + 1 < argc)
      opt.scale = std::stod(argv[++i]);
    else
    {
      std::fprintf(stderr, "usage: vm_gas_bench [--filter substring] [--repeats n] [--scale factor]\n");
      return 2;
    }
  }

  std::vector<bench_case> cases = loop_baseline();
  for (auto&& group : {opcode_cases(), prolog_cases(), memory_host_cases(), storage_host_cases(), misc_host_cases(), far_call_cases()})
    cases.insert(cases.end(), group.begin(), group.end());

  std::vector<sample> samples;
  for (const bench_case& bc : cases)
  {
    if (!opt.filter.empty() && bc.name.find(opt.filter) == std::string::npos && bc.group != "baseline")
      continue;
    std::fprintf(stderr, "%s\n", bc.name.c_str());
    samples.push_back(run_case(bc, opt));
  }
  if (opt.filter.empty())
    for (auto&& group : {deploy_samples(opt), invoke_samples(opt), state_tree_samples(opt)})
      samples.insert(samples.end(), group.begin(), group.end());

  print_json(samples, opt);
  return 0;
}
