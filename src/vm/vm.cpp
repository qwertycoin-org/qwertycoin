// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "vm.h"

#include "gas.h"
#include "hdr_info_binding_hf18.h"
#include "limits.h"
#include "metered_processor.h"

namespace vm
{
  namespace
  {
    class overlay_storage : public beam::bvm2::Storage::IBase
    {
    public:
      explicit overlay_storage(const state_view& state) : m_state(state) {}

      void LoadVar(const beam::Blob& key, beam::Blob& res) override
      {
        const boost::optional<std::string>& value = lookup(to_string(key));
        res = value ? to_blob(*value) : beam::Blob();
      }

      uint32_t SaveVar(const beam::Blob& key, const beam::Blob& val) override
      {
        std::string k = to_string(key);
        const boost::optional<std::string>& old = lookup(k);
        uint32_t old_size = old ? static_cast<uint32_t>(old->size()) : 0;

        boost::optional<std::string> next;
        if (val.n)
          next = to_string(val);

        if (base(k) == next)
          m_writes.erase(k);
        else
          m_writes[k] = std::move(next);

        return old_size;
      }

      uint32_t OnLog(const beam::Blob& key, const beam::Blob& val) override
      {
        m_logs.push_back({to_string(key), to_string(val)});
        return static_cast<uint32_t>(m_logs.size() - 1);
      }

      write_set m_writes;
      std::vector<log_entry> m_logs;

    private:
      const boost::optional<std::string>& lookup(const std::string& key)
      {
        auto w = m_writes.find(key);
        if (w != m_writes.end())
          return w->second;
        return base(key);
      }

      const boost::optional<std::string>& base(const std::string& key)
      {
        auto it = m_base.find(key);
        if (it == m_base.end())
        {
          std::string value;
          boost::optional<std::string> loaded;
          if (m_state.get(key, value) && !value.empty())
            loaded = std::move(value);
          it = m_base.emplace(key, std::move(loaded)).first;
        }
        return it->second;
      }

      const state_view& m_state;
      std::map<std::string, boost::optional<std::string>> m_base;
    };

    class executor : public metered_processor
    {
    public:
      explicit executor(overlay_storage& storage) : m_storage(storage) {}

      beam::bvm2::Storage::IBase& get_Storage() override { return m_storage; }

    private:
      overlay_storage& m_storage;
    };
  }

  std::string contract_code_key(const crypto::hash& contract_id)
  {
    return std::string(reinterpret_cast<const char*>(&contract_id), sizeof(contract_id));
  }

  bool compile(const std::string& wasm, std::string& compiled)
  {
    try
    {
      beam::ByteBuffer out;
      beam::bvm2::Processor::Compile(out, to_blob(wasm));
      compiled.assign(out.begin(), out.end());
      return true;
    }
    catch (const beam::Exc&)
    {
      return false;
    }
  }

  crypto::hash contract_id_for(const std::string& wasm, const std::string& args)
  {
    crypto::hash cid;
    beam::bvm2::get_Cid(cid, to_blob(wasm), to_blob(args));
    return cid;
  }

  result execute(const std::string& code, const call& c, const state_view& state, uint64_t gas_limit)
  {
    result res;
    overlay_storage storage(state);
    executor proc(storage);
    proc.m_Charge = gas_limit;

    beam::bvm2::ProcessorContract::FundsIO funds;
    funds.m_Deposit = c.deposit;
    proc.m_pFundsIO = &funds;
    proc.m_Height = c.height;
    proc.m_ParentHdr.m_Height = c.parent.height;
    proc.m_ParentHdr.m_Hash = c.parent.hash;
    proc.m_ParentHdr.m_Prev = c.parent.prev;
    proc.m_ParentHdr.m_Timestamp = c.parent.timestamp;
    bind_hdr_info_hf18(proc, c);

    auto finish = [&](status st) {
      res.code = st;
      res.gas_used = st == status::out_of_gas ? gas_limit : gas_limit - proc.m_Charge;
      if (st == status::ok)
      {
        res.writes = std::move(storage.m_writes);
        res.logs = std::move(storage.m_logs);
        res.locked = funds.m_Locked;
        res.unlocked = funds.m_Unlocked;
        res.required_signers = std::move(proc.m_vPks);
        for (const auto& origin : proc.m_vSignerOrigins)
          res.signer_origins.push_back({origin.cid, origin.method, origin.nested});
      }
      return res;
    };

    if (c.args.size() > limits::args_size)
      return finish(status::invalid_call);

    crypto::hash cid = c.contract_id;
    uint32_t method = c.method;

    try
    {
      if (c.kind == call_kind::deploy)
      {
        proc.charge(gas::deploy_for(code.size()));

        std::string compiled;
        if (!compile(code, compiled))
          return finish(status::invalid_module);

        cid = contract_id_for(code, c.args);
        beam::Blob existing;
        storage.LoadVar(beam::bvm2::ToBlob(cid), existing);
        if (existing.n)
          return finish(status::invalid_call);

        beam::Blob body = to_blob(compiled);
        proc.AddRemoveShader(cid, &body);
        method = method_ctor;
      }
      else
      {
        if (!code.empty() || method < method_first_public)
          return finish(status::invalid_call);

        beam::Blob existing;
        storage.LoadVar(beam::bvm2::ToBlob(cid), existing);
        if (!existing.n)
          return finish(status::invalid_call);
      }

      res.contract_id = cid;
      proc.start(cid, method, c.args);
      proc.run();
    }
    catch (const beam::Exc& e)
    {
      return finish(classify(e));
    }
    return finish(status::ok);
  }
}
