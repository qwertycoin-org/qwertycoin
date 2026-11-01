// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "crypto/hash.h"
#include "crypto/crypto.h"
#include "state_view.h"

namespace vm
{
  enum class status
  {
    ok,
    out_of_gas,
    trap,
    invalid_module,
    invalid_call
  };

  enum class call_kind
  {
    deploy,
    invoke
  };

  struct parent_header
  {
    uint64_t height = 0;
    crypto::hash hash = crypto::null_hash;
    crypto::hash prev = crypto::null_hash;
    uint64_t timestamp = 0;
  };

  using header_provider = std::function<bool(uint64_t, parent_header&)>;

  struct call
  {
    call_kind kind = call_kind::invoke;
    crypto::hash contract_id = crypto::null_hash;
    uint32_t method = 0;
    std::string args;
    uint64_t deposit = 0;
    uint64_t height = 0;
    parent_header parent;
    header_provider headers;
    bool hdr_info_active = false;
  };

  struct log_entry
  {
    std::string key;
    std::string value;
  };

  struct result
  {
    status code = status::ok;
    uint64_t gas_used = 0;
    crypto::hash contract_id = crypto::null_hash;
    write_set writes;
    std::vector<log_entry> logs;
    uint64_t locked = 0;
    uint64_t unlocked = 0;
    std::vector<crypto::public_key> required_signers;
    struct signer_origin
    {
      crypto::hash contract_id;
      uint32_t method;
      bool nested;
    };
    std::vector<signer_origin> signer_origins;
  };

  constexpr uint32_t method_ctor = 0;
  constexpr uint32_t method_dtor = 1;
  constexpr uint32_t method_first_public = 2;

  std::string contract_code_key(const crypto::hash& contract_id);
  bool compile(const std::string& wasm, std::string& compiled);
  crypto::hash contract_id_for(const std::string& wasm, const std::string& args);

  result execute(const std::string& code, const call& c, const state_view& state, uint64_t gas_limit);
}
