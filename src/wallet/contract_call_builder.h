// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/account.h"
#include "cryptonote_basic/tx_extra.h"
#include "cryptonote_config.h"

namespace tools
{
  constexpr uint64_t default_contract_gas_price = 1; // QWC-10

  struct contract_call_options
  {
    std::string args_hex;
    uint64_t deposit = 0;
    uint64_t gas_price = default_contract_gas_price;
    uint64_t gas_limit = 0;
    std::string mint_address;
    std::vector<crypto::hash> signer_key_ids;
    std::vector<crypto::public_key> external_signers;
  };

  enum class contract_call_build_result
  {
    ok,
    bad_call,
    bad_mint_address
  };

  bool extract_contract_call_options(std::vector<std::string>& tokens, contract_call_options& options, std::string& error);
  bool parse_contract_signature(const std::string& public_key_hex, const std::string& signature_hex, crypto::public_key& key, crypto::signature& signature);
  bool parse_contract_public_keys(const std::vector<std::string>& hex, std::vector<crypto::public_key>& keys);
  bool contract_signer_requires_authorization(const crypto::hash& called_contract, uint32_t called_method,
    const crypto::hash& signer_contract, uint32_t signer_method, bool nested);
  bool can_prepare_contract_signers(const cryptonote::account_keys& keys, size_t signer_count);

  contract_call_build_result build_contract_call(
    cryptonote::network_type nettype,
    bool deploy,
    const std::string& code_hex,
    const std::string& contract_id_hex,
    uint32_t method,
    const contract_call_options& options,
    cryptonote::tx_extra_vm_call& call);
}
