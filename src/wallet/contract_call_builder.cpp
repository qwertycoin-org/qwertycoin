// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "contract_call_builder.h"

#include <set>

#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_core/contract_state.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "string_tools.h"
#include "string_tools_lexical.h"

namespace tools
{
  bool can_prepare_contract_signers(const cryptonote::account_keys& keys, size_t signer_count)
  {
    return signer_count == 0 || cryptonote::can_derive_contract_key(keys);
  }

  static bool parse_contract_public_key(const std::string& value, crypto::public_key& key)
  {
    return value.size() == 64 && epee::string_tools::hex_to_pod(value, key) && crypto::check_key(key);
  }

  bool parse_contract_signature(const std::string& public_key_hex, const std::string& signature_hex, crypto::public_key& key, crypto::signature& signature)
  {
    return parse_contract_public_key(public_key_hex, key) && signature_hex.size() == 128
        && epee::string_tools::hex_to_pod(signature_hex, signature);
  }

  bool parse_contract_public_keys(const std::vector<std::string>& hex, std::vector<crypto::public_key>& keys)
  {
    std::vector<crypto::public_key> parsed;
    for (const std::string& value : hex)
    {
      crypto::public_key key{};
      if (!parse_contract_public_key(value, key))
        return false;
      parsed.push_back(key);
    }
    keys = std::move(parsed);
    return true;
  }

  bool contract_signer_requires_authorization(const crypto::hash& called_contract, uint32_t called_method,
    const crypto::hash& signer_contract, uint32_t signer_method, bool nested)
  {
    return nested || signer_contract != called_contract || signer_method != called_method;
  }

  namespace
  {
    constexpr const char* args_prefix = "args=";
    constexpr const char* deposit_prefix = "deposit=";
    constexpr const char* gas_price_prefix = "gas_price=";
    constexpr const char* gas_limit_prefix = "gas_limit=";
    constexpr const char* mint_prefix = "mint=";
    constexpr const char* key_ids_prefix = "key_ids=";
    constexpr const char* external_signers_prefix = "external_signers=";

    std::vector<std::string> split_comma_list(const std::string& value)
    {
      std::vector<std::string> parts;
      size_t start = 0;
      do
      {
        const size_t end = value.find(',', start);
        parts.push_back(value.substr(start, end - start));
        if (end == std::string::npos)
          break;
        start = end + 1;
      } while (true);
      return parts;
    }

    bool starts_with(const std::string& text, const std::string& prefix)
    {
      return text.compare(0, prefix.size(), prefix) == 0;
    }

    bool parse_count(const std::string& text, uint64_t& value)
    {
      return !text.empty() && text.find_first_not_of("0123456789") == std::string::npos && epee::string_tools::get_xtype_from_string(value, text);
    }
  }

  bool extract_contract_call_options(std::vector<std::string>& tokens, contract_call_options& options, std::string& error)
  {
    std::set<std::string> seen;
    std::vector<std::string> remaining;
    for (const std::string& token : tokens)
    {
      const char* matched = nullptr;
      for (const char* prefix : {args_prefix, deposit_prefix, gas_price_prefix, gas_limit_prefix, mint_prefix, key_ids_prefix, external_signers_prefix})
      {
        if (starts_with(token, prefix))
        {
          matched = prefix;
          break;
        }
      }
      if (matched == nullptr)
      {
        remaining.push_back(token);
        continue;
      }
      if (!seen.insert(matched).second)
      {
        error = std::string("duplicate option ") + matched;
        return false;
      }
      const std::string value = token.substr(std::string(matched).size());
      bool parsed = true;
      if (matched == args_prefix)
        options.args_hex = value;
      else if (matched == mint_prefix)
        options.mint_address = value;
      else if (matched == key_ids_prefix)
      {
        for (const std::string& part : split_comma_list(value))
        {
          crypto::hash key_id{};
          if (part.size() != 64 || !epee::string_tools::hex_to_pod(part, key_id))
          {
            parsed = false;
            break;
          }
          options.signer_key_ids.push_back(key_id);
        }
      }
      else if (matched == external_signers_prefix)
      {
        parsed = parse_contract_public_keys(split_comma_list(value), options.external_signers);
      }
      else if (matched == deposit_prefix)
        parsed = cryptonote::parse_amount(options.deposit, value);
      else if (matched == gas_price_prefix)
        parsed = parse_count(value, options.gas_price);
      else
        parsed = parse_count(value, options.gas_limit);
      if (!parsed)
      {
        error = std::string("invalid value for ") + matched;
        return false;
      }
    }
    tokens = std::move(remaining);
    return true;
  }

  contract_call_build_result build_contract_call(
    cryptonote::network_type nettype,
    bool deploy,
    const std::string& code_hex,
    const std::string& contract_id_hex,
    uint32_t method,
    const contract_call_options& options,
    cryptonote::tx_extra_vm_call& call)
  {
    call = cryptonote::tx_extra_vm_call{};
    call.contract_id = crypto::null_hash;
    call.method = method;
    call.deposit = options.deposit;
    call.gas_price = options.gas_price;
    call.gas_limit = options.gas_limit;
    call.kind = deploy ? cryptonote::tx_extra_vm_call::kind_deploy : cryptonote::tx_extra_vm_call::kind_invoke;
    call.signer_key_ids = options.signer_key_ids;
    if (!epee::string_tools::parse_hexstr_to_binbuff(code_hex, call.code)
        || !epee::string_tools::parse_hexstr_to_binbuff(options.args_hex, call.args)
        || deploy == call.code.empty()
        || (!deploy && !epee::string_tools::hex_to_pod(contract_id_hex, call.contract_id)))
      return contract_call_build_result::bad_call;

    if (!options.mint_address.empty())
    {
      cryptonote::address_parse_info info;
      cryptonote::tx_extra_vm_mint_destination destination{};
      if (!cryptonote::get_account_address_from_str(info, nettype, options.mint_address)
          || !cryptonote::make_contract_mint_destination(info.address, info.is_subaddress, destination))
        return contract_call_build_result::bad_mint_address;
      call.mint.push_back(destination);
    }
    return contract_call_build_result::ok;
  }
}
