// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cassert>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "vm/limits.h"
#include "vm/state_tree.h"
#include "vm/vm.h"

namespace vm_test
{
  using bytes = std::vector<uint8_t>;

  constexpr uint8_t t_i32 = 0x7f;
  constexpr uint8_t t_i64 = 0x7e;
  constexpr uint8_t t_f32 = 0x7d;
  constexpr uint8_t t_f64 = 0x7c;

  using locals = std::vector<std::pair<uint32_t, uint8_t>>;

  inline void put_uleb(bytes& out, uint64_t v)
  {
    do
    {
      uint8_t b = v & 0x7f;
      v >>= 7;
      out.push_back(v ? (b | 0x80) : b);
    } while (v);
  }

  inline void put_sleb(bytes& out, int64_t v)
  {
    for (;;)
    {
      uint8_t b = v & 0x7f;
      v >>= 7;
      bool done = (v == 0 && !(b & 0x40)) || (v == -1 && (b & 0x40));
      out.push_back(done ? b : (b | 0x80));
      if (done)
        return;
    }
  }

  inline void put_name(bytes& out, const std::string& s)
  {
    put_uleb(out, s.size());
    out.insert(out.end(), s.begin(), s.end());
  }

  inline void put_section(bytes& out, uint8_t id, const bytes& payload)
  {
    out.push_back(id);
    put_uleb(out, payload.size());
    out.insert(out.end(), payload.begin(), payload.end());
  }

  struct code
  {
    bytes b;

    code& op(uint8_t o) { b.push_back(o); return *this; }
    code& raw(const bytes& x) { b.insert(b.end(), x.begin(), x.end()); return *this; }
    code& i32(int32_t v) { op(0x41); put_sleb(b, v); return *this; }
    code& i64(int64_t v) { op(0x42); put_sleb(b, v); return *this; }
    code& get(uint32_t i) { op(0x20); put_uleb(b, i); return *this; }
    code& set(uint32_t i) { op(0x21); put_uleb(b, i); return *this; }
    code& call(uint32_t f) { op(0x10); put_uleb(b, f); return *this; }
    code& arg_at(int32_t offset) { return get(0).i32(offset).op(0x6a); }
    code& drop() { return op(0x1a); }
    code& block() { op(0x02); return op(0x40); }
    code& loop() { op(0x03); return op(0x40); }
    code& br(uint32_t depth) { op(0x0c); put_uleb(b, depth); return *this; }
    code& br_if(uint32_t depth) { op(0x0d); put_uleb(b, depth); return *this; }
    code& eqz() { return op(0x45); }
    code& unreachable() { return op(0x00); }
    code& end() { return op(0x0b); }
  };

  class module_builder
  {
  public:
    uint32_t type(const bytes& params, const bytes& results)
    {
      m_types.push_back({params, results});
      return static_cast<uint32_t>(m_types.size() - 1);
    }

    uint32_t import(const std::string& name, const bytes& params, const bytes& results, const std::string& module = "env")
    {
      assert(m_funcs.empty());
      m_imports.push_back({module, name, type(params, results)});
      return static_cast<uint32_t>(m_imports.size() - 1);
    }

    uint32_t func(uint32_t type_idx, const code& body, const locals& decl = {})
    {
      m_funcs.push_back({type_idx, decl, body.b});
      return static_cast<uint32_t>(m_imports.size() + m_funcs.size() - 1);
    }

    void exp(const std::string& name, uint32_t func_idx)
    {
      m_exports.push_back({name, func_idx});
    }

    void stack_pointer_global()
    {
      m_stack_pointer_global = true;
    }

    void indirect_table(const std::vector<uint32_t>& func_idxs)
    {
      m_table = func_idxs;
    }

    void custom(const std::string& name, size_t payload)
    {
      m_custom_name = name;
      m_custom_size = payload;
    }

    std::string build() const
    {
      bytes out = {0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};

      bytes types;
      put_uleb(types, m_types.size());
      for (const auto& t : m_types)
      {
        types.push_back(0x60);
        put_uleb(types, t.params.size());
        types.insert(types.end(), t.params.begin(), t.params.end());
        put_uleb(types, t.results.size());
        types.insert(types.end(), t.results.begin(), t.results.end());
      }
      put_section(out, 1, types);

      if (!m_imports.empty())
      {
        bytes imports;
        put_uleb(imports, m_imports.size());
        for (const auto& i : m_imports)
        {
          put_name(imports, i.module);
          put_name(imports, i.name);
          imports.push_back(0x00);
          put_uleb(imports, i.type);
        }
        put_section(out, 2, imports);
      }

      bytes funcs;
      put_uleb(funcs, m_funcs.size());
      for (const auto& f : m_funcs)
        put_uleb(funcs, f.type);
      put_section(out, 3, funcs);

      if (!m_table.empty())
      {
        bytes table;
        put_uleb(table, 1);
        table.push_back(0x70);
        table.push_back(0x00);
        put_uleb(table, m_table.size() + 1);
        put_section(out, 4, table);
      }

      if (m_stack_pointer_global)
      {
        bytes globals;
        put_uleb(globals, 1);
        globals.push_back(t_i32);
        globals.push_back(0x01);
        globals.push_back(0x41);
        put_sleb(globals, 0);
        globals.push_back(0x0b);
        put_section(out, 6, globals);
      }

      bytes exports;
      put_uleb(exports, m_exports.size());
      for (const auto& e : m_exports)
      {
        put_name(exports, e.name);
        exports.push_back(0x00);
        put_uleb(exports, e.func);
      }
      put_section(out, 7, exports);

      if (!m_table.empty())
      {
        bytes elements;
        put_uleb(elements, 1);
        put_uleb(elements, 0);
        elements.push_back(0x41);
        put_sleb(elements, 1);
        elements.push_back(0x0b);
        put_uleb(elements, m_table.size());
        for (uint32_t f : m_table)
          put_uleb(elements, f);
        put_section(out, 9, elements);
      }

      bytes bodies;
      put_uleb(bodies, m_funcs.size());
      for (const auto& f : m_funcs)
      {
        bytes body;
        put_uleb(body, f.decl.size());
        for (const auto& l : f.decl)
        {
          put_uleb(body, l.first);
          body.push_back(l.second);
        }
        body.insert(body.end(), f.code.begin(), f.code.end());
        put_uleb(bodies, body.size());
        bodies.insert(bodies.end(), body.begin(), body.end());
      }
      put_section(out, 10, bodies);

      if (m_custom_size)
      {
        bytes custom;
        put_name(custom, m_custom_name);
        custom.resize(custom.size() + m_custom_size, 0);
        put_section(out, 0, custom);
      }

      return std::string(out.begin(), out.end());
    }

  private:
    struct type_entry { bytes params; bytes results; };
    struct import_entry { std::string module; std::string name; uint32_t type; };
    struct func_entry { uint32_t type; locals decl; bytes code; };
    struct export_entry { std::string name; uint32_t func; };

    std::vector<type_entry> m_types;
    std::vector<import_entry> m_imports;
    std::vector<func_entry> m_funcs;
    std::vector<export_entry> m_exports;
    std::vector<uint32_t> m_table;
    bool m_stack_pointer_global = false;
    std::string m_custom_name;
    size_t m_custom_size = 0;
  };

  struct host
  {
    uint32_t save_var, load_var, emit_log, halt, heap_alloc, funds_lock, funds_unlock, call_far, keccak, hash_write, hash_get, get_height, add_sig;
  };

  inline host import_host(module_builder& m)
  {
    const bytes var_args = {t_i32, t_i32, t_i32, t_i32, t_i32};
    host h;
    h.save_var = m.import("SaveVar", var_args, {t_i32});
    h.load_var = m.import("LoadVar", var_args, {t_i32});
    h.emit_log = m.import("EmitLog", var_args, {t_i32});
    h.halt = m.import("Halt", {}, {});
    h.heap_alloc = m.import("Heap_Alloc", {t_i32}, {t_i32});
    h.funds_lock = m.import("FundsLock", {t_i64}, {});
    h.funds_unlock = m.import("FundsUnlock", {t_i64}, {});
    h.call_far = m.import("CallFar", var_args, {});
    h.keccak = m.import("HashCreateKeccak", {t_i32}, {t_i32});
    h.hash_write = m.import("HashWrite", {t_i32, t_i32, t_i32}, {});
    h.hash_get = m.import("HashGetValue", {t_i32, t_i32, t_i32}, {});
    h.get_height = m.import("get_Height", {}, {t_i64});
    h.add_sig = m.import("AddSig", {t_i32}, {});
    return h;
  }

  inline uint32_t method_type(module_builder& m)
  {
    return m.type({t_i32}, {});
  }

  inline void add_ctor_dtor(module_builder& m, uint32_t mt)
  {
    m.exp("Ctor", m.func(mt, code().end()));
    m.exp("Dtor", m.func(mt, code().end()));
  }

  inline std::string minimal_module(const code& method2 = code().end())
  {
    module_builder m;
    uint32_t mt = method_type(m);
    add_ctor_dtor(m, mt);
    m.exp("Method_2", m.func(mt, method2));
    return m.build();
  }

  inline std::string var_key(const crypto::hash& cid, uint8_t tag, const std::string& key)
  {
    return vm::contract_code_key(cid) + std::string(1, static_cast<char>(tag)) + key;
  }

  constexpr uint64_t ample_gas = 100000000;

  inline vm::result deploy(vm::state_tree& state, const std::string& wasm, const std::string& args = {}, uint64_t gas = ample_gas)
  {
    vm::call c;
    c.kind = vm::call_kind::deploy;
    c.args = args;
    vm::result r = vm::execute(wasm, c, state, gas);
    if (r.code == vm::status::ok)
      state.apply(r.writes);
    return r;
  }

  inline vm::result invoke(vm::state_tree& state, const crypto::hash& cid, uint32_t method, const std::string& args = {}, uint64_t gas = ample_gas, uint64_t deposit = 0)
  {
    vm::call c;
    c.kind = vm::call_kind::invoke;
    c.contract_id = cid;
    c.method = method;
    c.args = args;
    c.deposit = deposit;
    vm::result r = vm::execute({}, c, state, gas);
    if (r.code == vm::status::ok)
      state.apply(r.writes);
    return r;
  }

  enum method : uint32_t
  {
    m_store = 2,
    m_halt,
    m_unreachable,
    m_spin,
    m_heap_oversize,
    m_heap_small,
    m_funds_lock,
    m_emit_log,
    m_copy_var,
    m_keccak,
    m_recurse,
    m_filler,
    m_height,
    m_funds_unlock,
    m_addsig,
    m_count
  };

  constexpr int64_t locked_per_call = 100;
  constexpr uint32_t filler_pairs = 10;

  inline std::string test_contract()
  {
    module_builder m;
    host h = import_host(m);
    uint32_t mt = method_type(m);
    add_ctor_dtor(m, mt);

    auto save = [&](code& c, int32_t key, int32_t val, int32_t n) -> code& {
      return c.arg_at(key).i32(2).arg_at(val).i32(n).i32(0).call(h.save_var).drop();
    };

    std::vector<std::pair<code, locals>> bodies(m_count);
    save(bodies[m_store].first, 0, 2, 2).end();
    bodies[m_halt].first.call(h.halt).end();
    bodies[m_unreachable].first.unreachable().end();
    bodies[m_spin].first.loop().br(0).end().end();
    bodies[m_heap_oversize].first.block().i32(vm::limits::heap_size + 16).call(h.heap_alloc).eqz().br_if(0).call(h.halt).end().end();
    bodies[m_heap_small].first.block().i32(64).call(h.heap_alloc).br_if(0).call(h.halt).end().end();
    bodies[m_funds_lock].first.i64(locked_per_call).call(h.funds_lock).end();
    bodies[m_emit_log].first.arg_at(0).i32(2).arg_at(2).i32(2).i32(0).call(h.emit_log).drop().end();
    bodies[m_copy_var].first.arg_at(0).i32(2).arg_at(2).i32(2).i32(0).call(h.load_var).drop();
    save(bodies[m_copy_var].first, 4, 2, 2).end();
    bodies[m_keccak].first.i32(256).call(h.keccak).set(1)
      .get(1).arg_at(0).i32(4).call(h.hash_write)
      .get(1).arg_at(4).i32(32).call(h.hash_get);
    save(bodies[m_keccak].first, 36, 4, 32).end();
    bodies[m_keccak].second = {{1, t_i32}};
    bodies[m_recurse].first.arg_at(0).i32(m_recurse).arg_at(0).i32(32).i32(0).call(h.call_far).end();
    for (uint32_t i = 0; i < filler_pairs; ++i)
      bodies[m_filler].first.i32(1).drop();
    bodies[m_filler].first.end();
    bodies[m_height].first.call(h.get_height).drop().end();
    bodies[m_funds_unlock].first.i64(locked_per_call).call(h.funds_unlock).end();
    bodies[m_addsig].first.arg_at(0).call(h.add_sig).end();

    for (uint32_t i = m_store; i < m_count; ++i)
      m.exp("Method_" + std::to_string(i), m.func(mt, bodies[i].first, bodies[i].second));

    return m.build();
  }

  inline std::string stored_signer_contract()
  {
    module_builder module;
    const uint32_t save = module.import("SaveVar", {t_i32, t_i32, t_i32, t_i32, t_i32}, {t_i32});
    const uint32_t load = module.import("LoadVar", {t_i32, t_i32, t_i32, t_i32, t_i32}, {t_i32});
    const uint32_t add_sig = module.import("AddSig", {t_i32}, {});
    const uint32_t method = method_type(module);
    add_ctor_dtor(module, method);
    code set_owner;
    set_owner.arg_at(0).i32(1).arg_at(1).i32(sizeof(crypto::public_key)).i32(0).call(save).drop().end();
    module.exp("Method_2", module.func(method, set_owner));
    code require_owner;
    require_owner.arg_at(0).i32(1).arg_at(1).i32(sizeof(crypto::public_key)).i32(0).call(load).drop();
    require_owner.arg_at(1).call(add_sig).end();
    module.exp("Method_3", module.func(method, require_owner));
    return module.build();
  }

  inline std::string stored_owner_args(const crypto::public_key& owner)
  {
    return std::string("o") + std::string(reinterpret_cast<const char*>(&owner), sizeof(owner));
  }

  inline std::string stored_owner_lookup_args()
  {
    return std::string("o") + std::string(sizeof(crypto::public_key), '\0');
  }
}
