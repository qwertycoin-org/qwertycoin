// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <boost/optional.hpp>

#include "crypto/hash.h"
#include "vm.h"

namespace vm { namespace app
{
  constexpr uint64_t step_budget = 20000000;
  constexpr uint32_t max_document_nodes = 4096;
  constexpr uint32_t max_document_depth = 16;
  constexpr uint32_t max_pending_calls = 8;
  constexpr uint32_t max_argument_count = 64;
  constexpr uint32_t max_blob_size = 0x10000;

  struct document_node
  {
    enum class kind
    {
      text,
      number,
      blob,
      group
    };

    std::string id;
    kind type = kind::text;
    std::string text;
    uint64_t number = 0;
    std::vector<document_node> children;
  };

  struct pending_call
  {
    call_kind kind = call_kind::invoke;
    crypto::hash contract_id = crypto::null_hash;
    uint32_t method = 0;
    std::string args;
    std::string code;
    uint64_t deposit = 0;
    std::vector<crypto::public_key> signers;
    std::vector<crypto::hash> signer_contracts;
    std::vector<uint32_t> signer_methods;
    std::vector<crypto::hash> signer_key_ids;
  };

  struct run_result
  {
    status code = status::ok;
    uint64_t steps_used = 0;
    std::vector<document_node> document;
    std::vector<pending_call> calls;
  };

  using arguments = std::map<std::string, std::string>;
  using state_reader = std::function<bool(const std::string& key, std::string& value)>;
  using key_deriver = std::function<bool(const crypto::hash& contract_id, const crypto::hash& key_id, crypto::public_key& public_key)>;
  using key_authorizer = std::function<bool(const crypto::hash& contract_id, const crypto::hash& key_id, boost::optional<crypto::public_key>& approved_key)>;
  using key_confirmation = std::function<bool(const crypto::hash& contract_id, const crypto::hash& key_id, const crypto::public_key& public_key)>;

  bool compile_app(const std::string& wasm, std::string& compiled);
  bool parse_arguments(const std::string& text, arguments& out);
  std::string status_name(status code);
  std::string document_to_json(const std::vector<document_node>& document);

  run_result run(const std::string& compiled, uint32_t entry_method, const arguments& args, const state_reader& read_state, uint64_t budget = step_budget, const key_deriver& derive_key = {}, const std::vector<crypto::hash>& invoked_contracts = {}, const key_authorizer& authorize_key = {}, const key_confirmation& confirm_key = {});
}}
