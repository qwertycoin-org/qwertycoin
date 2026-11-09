// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "app_shader.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "metered_processor.h"

namespace vm { namespace app
{
  namespace
  {
    constexpr char key_value_separator = '=';
    constexpr char pair_separator = ',';

    class collecting_host : public beam::bvm2::App::IHost
    {
    public:
      collecting_host(const arguments& args, const state_reader& reader, const key_deriver& deriver, const std::vector<crypto::hash>& invoked_contracts, const key_authorizer& authorizer, const key_confirmation& confirmation)
        : m_args(args), m_reader(reader), m_deriver(deriver), m_invoked_contracts(invoked_contracts), m_authorizer(authorizer), m_confirmation(confirmation)
      {
        m_open.push_back(&m_document);
      }

      void AddText(const std::string& id, const std::string& value) override
      {
        document_node& node = append(id, document_node::kind::text);
        node.text = value;
      }

      void AddNum(const std::string& id, uint64_t value) override
      {
        append(id, document_node::kind::number).number = value;
      }

      void AddBlob(const std::string& id, const beam::Blob& value) override
      {
        beam::Exc::Test(value.n <= max_blob_size);
        append(id, document_node::kind::blob).text = to_string(value);
      }

      void OpenGroup(const std::string& id) override
      {
        beam::Exc::Test(m_open.size() <= max_document_depth);
        document_node& node = append(id, document_node::kind::group);
        m_open.push_back(&node);
      }

      void CloseGroup() override
      {
        beam::Exc::Test(m_open.size() > 1);
        m_open.pop_back();
      }

      bool GetText(const std::string& id, std::string& value) override
      {
        auto it = m_args.find(id);
        if (it == m_args.end())
          return false;
        value = it->second;
        return true;
      }

      bool GetNum(const std::string& id, uint64_t& value) override
      {
        auto it = m_args.find(id);
        if (it == m_args.end() || it->second.empty())
          return false;
        char* end = nullptr;
        value = std::strtoull(it->second.c_str(), &end, 10);
        return end && *end == '\0';
      }

      bool ReadVar(const beam::Blob& key, std::string& value) override
      {
        return m_reader(to_string(key), value) && !value.empty();
      }

      bool DerivePk(const beam::bvm2::ContractID& cid, const beam::bvm2::HashValue& key_id, beam::bvm2::PubKey& public_key) override
      {
        const bool invoked = std::find(m_invoked_contracts.begin(), m_invoked_contracts.end(), cid) != m_invoked_contracts.end();
        boost::optional<crypto::public_key> approved_key;
        if (!m_deriver || (!invoked && (!m_authorizer || !m_authorizer(cid, key_id, approved_key))))
          return false;
        beam::bvm2::PubKey derived{};
        if (!m_deriver(cid, key_id, derived)
            || (!invoked && m_confirmation && !m_confirmation(cid, key_id, derived))
            || (approved_key && *approved_key != derived))
          return false;
        public_key = derived;
        m_derived_keys.push_back({derived, cid, key_id});
        return true;
      }

      void GenerateCall(const beam::bvm2::ContractID& cid, uint32_t method, const beam::Blob& args, beam::bvm2::Amount deposit) override
      {
        GenerateCallSigned(cid, method, args, deposit, nullptr, 0);
      }

      void GenerateCallSigned(const beam::bvm2::ContractID& cid, uint32_t method, const beam::Blob& args, beam::bvm2::Amount deposit, const beam::bvm2::PubKey* signers, uint32_t count) override
      {
        beam::Exc::Test(method >= method_first_public && args.n <= limits::args_size && count <= limits::signers);
        pending_call& call = next_call();
        call.kind = call_kind::invoke;
        call.contract_id = cid;
        call.method = method;
        call.args = to_string(args);
        call.deposit = deposit;
        attach_signers(call, signers, count);
      }

      void GenerateDeploy(const beam::Blob& code, const beam::Blob& args, beam::bvm2::Amount deposit) override
      {
        GenerateDeploySigned(code, args, deposit, nullptr, 0);
      }

      void GenerateDeploySigned(const beam::Blob& code, const beam::Blob& args, beam::bvm2::Amount deposit, const beam::bvm2::PubKey* signers, uint32_t count) override
      {
        beam::Exc::Test(code.n <= limits::module_size && args.n <= limits::args_size && count <= limits::signers);
        pending_call& call = next_call();
        call.kind = call_kind::deploy;
        call.method = method_ctor;
        call.code = to_string(code);
        call.args = to_string(args);
        call.deposit = deposit;
        call.contract_id = contract_id_for(call.code, call.args);
        attach_signers(call, signers, count);
      }

      std::vector<document_node> take_document() { return std::move(m_document.children); }
      std::vector<pending_call> take_calls() { return std::move(m_calls); }

    private:
      struct derived_key
      {
        crypto::public_key public_key;
        crypto::hash contract_id;
        crypto::hash key_id;
      };

      void attach_signers(pending_call& call, const crypto::public_key* signers, uint32_t count)
      {
        if (count)
          call.signers.assign(signers, signers + count);
        for (const crypto::public_key& key : call.signers)
        {
          auto found = std::find_if(m_derived_keys.rbegin(), m_derived_keys.rend(), [&](const derived_key& entry) { return entry.public_key == key; });
          call.signer_contracts.push_back(found == m_derived_keys.rend() ? call.contract_id : found->contract_id);
          call.signer_methods.push_back(call.method);
          call.signer_key_ids.push_back(found == m_derived_keys.rend() ? crypto::null_hash : found->key_id);
        }
      }

      document_node& append(const std::string& id, document_node::kind type)
      {
        beam::Exc::Test(++m_node_count <= max_document_nodes);
        document_node& parent = *m_open.back();
        parent.children.emplace_back();
        document_node& node = parent.children.back();
        node.id = id;
        node.type = type;
        return node;
      }

      pending_call& next_call()
      {
        beam::Exc::Test(m_calls.size() < max_pending_calls);
        m_calls.emplace_back();
        return m_calls.back();
      }

      const arguments& m_args;
      const state_reader& m_reader;
      const key_deriver& m_deriver;
      const std::vector<crypto::hash>& m_invoked_contracts;
      const key_authorizer& m_authorizer;
      const key_confirmation& m_confirmation;
      std::vector<derived_key> m_derived_keys;
      document_node m_document;
      std::vector<document_node*> m_open;
      std::vector<pending_call> m_calls;
      uint32_t m_node_count = 0;
    };

    class read_only_code_storage : public beam::bvm2::Storage::IBase
    {
    public:
      explicit read_only_code_storage(const std::string& compiled) : m_compiled(compiled) {}

      void LoadVar(const beam::Blob& key, beam::Blob& res) override
      {
        res = (key.n == sizeof(crypto::hash) && !memcmp(key.p, &m_code_id, sizeof(m_code_id))) ? to_blob(m_compiled) : beam::Blob();
      }

      uint32_t SaveVar(const beam::Blob&, const beam::Blob&) override
      {
        beam::Exc::Fail("app shader state is read-only");
        return 0;
      }

      uint32_t OnLog(const beam::Blob&, const beam::Blob&) override
      {
        beam::Exc::Fail("app shader cannot emit logs");
        return 0;
      }

      const crypto::hash& code_id() const { return m_code_id; }

    private:
      const std::string& m_compiled;
      crypto::hash m_code_id = crypto::null_hash;
    };

    class app_executor : public metered_processor
    {
    public:
      app_executor(read_only_code_storage& storage, collecting_host& host) : m_storage(storage), m_host(host) {}

      beam::bvm2::Storage::IBase& get_Storage() override { return m_storage; }

    protected:
      beam::bvm2::App::IHost* get_AppHost() override { return &m_host; }

    private:
      read_only_code_storage& m_storage;
      collecting_host& m_host;
    };

    void write_json_string(std::ostringstream& out, const std::string& value)
    {
      out << '"';
      for (unsigned char c : value)
      {
        switch (c)
        {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
          if (c < 0x20)
          {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04x", c);
            out << buf;
          }
          else
            out << static_cast<char>(c);
        }
      }
      out << '"';
    }

    std::string to_hex(const std::string& bytes)
    {
      static const char digits[] = "0123456789abcdef";
      std::string out;
      for (unsigned char c : bytes)
      {
        out.push_back(digits[c >> 4]);
        out.push_back(digits[c & 0xf]);
      }
      return out;
    }

    void write_nodes(std::ostringstream& out, const std::vector<document_node>& nodes)
    {
      out << '{';
      for (size_t i = 0; i < nodes.size(); ++i)
      {
        if (i)
          out << ',';
        write_json_string(out, nodes[i].id);
        out << ':';
        switch (nodes[i].type)
        {
        case document_node::kind::text: write_json_string(out, nodes[i].text); break;
        case document_node::kind::number: out << nodes[i].number; break;
        case document_node::kind::blob: write_json_string(out, to_hex(nodes[i].text)); break;
        case document_node::kind::group: write_nodes(out, nodes[i].children); break;
        }
      }
      out << '}';
    }
  }

  bool compile_app(const std::string& wasm, std::string& compiled)
  {
    try
    {
      beam::ByteBuffer out;
      beam::bvm2::Processor::CompileApp(out, to_blob(wasm));
      compiled.assign(out.begin(), out.end());
      return true;
    }
    catch (const std::exception&)
    {
      return false;
    }
  }

  bool parse_arguments(const std::string& text, arguments& out)
  {
    out.clear();
    size_t pos = 0;
    while (pos < text.size())
    {
      size_t end = text.find(pair_separator, pos);
      if (end == std::string::npos)
        end = text.size();
      std::string pair = text.substr(pos, end - pos);
      size_t eq = pair.find(key_value_separator);
      if (eq == std::string::npos || eq == 0 || out.size() >= max_argument_count)
        return false;
      if (!out.emplace(pair.substr(0, eq), pair.substr(eq + 1)).second)
        return false;
      pos = end + 1;
    }
    return true;
  }

  std::string status_name(status code)
  {
    switch (code)
    {
    case status::ok: return "ok";
    case status::out_of_gas: return "step budget exhausted";
    case status::trap: return "trap";
    case status::invalid_module: return "invalid module";
    case status::invalid_call: return "invalid call";
    }
    return "unknown";
  }

  std::string document_to_json(const std::vector<document_node>& document)
  {
    std::ostringstream out;
    write_nodes(out, document);
    return out.str();
  }

  run_result run(const std::string& compiled, uint32_t entry_method, const arguments& args, const state_reader& read_state, uint64_t budget, const key_deriver& derive_key, const std::vector<crypto::hash>& invoked_contracts, const key_authorizer& authorize_key, const key_confirmation& confirm_key)
  {
    run_result res;
    collecting_host host(args, read_state, derive_key, invoked_contracts, authorize_key, confirm_key);
    read_only_code_storage storage(compiled);
    app_executor proc(storage, host);
    proc.m_Charge = budget;

    auto finish = [&](status st) {
      res.code = st;
      res.steps_used = st == status::out_of_gas ? budget : budget - proc.m_Charge;
      if (st == status::ok)
      {
        res.document = host.take_document();
        res.calls = host.take_calls();
      }
      return res;
    };

    if (entry_method < method_first_public)
      return finish(status::invalid_call);

    try
    {
      proc.start(storage.code_id(), entry_method, std::string());
      proc.run();
    }
    catch (const beam::Exc& e)
    {
      return finish(classify(e));
    }
    catch (const std::exception&)
    {
      return finish(status::trap);
    }

    return finish(status::ok);
  }
}}
