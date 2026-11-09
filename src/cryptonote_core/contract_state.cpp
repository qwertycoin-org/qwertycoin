// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "contract_state.h"
#include "hdr_info_hf18.h"

#include <algorithm>
#include <limits>

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_config.h"
#include "device/device.hpp"
#include "epose/reward_v2.h"
#include "ringct/rctOps.h"
#include "vm/limits.h"

namespace cryptonote
{
  bool contract_overlay::get(const std::string &key, std::string &value) const
  {
    const auto it = m_writes.find(key);
    if (it == m_writes.end())
      return m_base.get(key, value);
    if (!it->second)
      return false;
    value = *it->second;
    return true;
  }

  void contract_overlay::apply(const vm::write_set &writes)
  {
    for (const auto &write : writes)
      m_writes[write.first] = write.second;
  }

  bool contract_call_fee_floor(const tx_extra_vm_call &call, uint64_t &fee)
  {
    fee = 0;
    if (call.gas_price != 0 && call.gas_limit > std::numeric_limits<uint64_t>::max() / call.gas_price)
      return false;
    const uint64_t gas_fee = call.gas_limit * call.gas_price;
    if (call.deposit > std::numeric_limits<uint64_t>::max() - gas_fee)
      return false;
    fee = call.deposit + gas_fee; // H-15, H-29
    return true;
  }

  bool contract_call_fee_floor(const std::vector<uint8_t> &tx_extra, uint64_t &fee)
  {
    fee = 0;
    tx_extra_vm_call call{};
    return get_tx_vm_call(tx_extra, call) != 1 || contract_call_fee_floor(call, fee);
  }

  bool split_contract_call_fee(const tx_extra_vm_call &call, uint64_t fee, contract_call_fee &split)
  {
    split = {};
    uint64_t floor = 0;
    if (!contract_call_fee_floor(call, floor) || floor > fee)
      return false;
    split.deposit = call.deposit;
    split.gas_price = call.gas_price;
    split.remainder = fee - floor;
    return true;
  }

  crypto::hash called_contract_id(const tx_extra_vm_call &call)
  {
    return call.kind == tx_extra_vm_call::kind_deploy ? vm::contract_id_for(call.code, call.args) : call.contract_id;
  }

  bool contract_signer_shape_valid(const tx_extra_vm_call &call, bool allow_unsigned)
  {
    if (call.signers.size() > vm::limits::signers)
      return false;
    if (allow_unsigned && call.signers.empty() && call.signatures.empty() && call.signer_nested.empty())
      return call.signer_contracts.size() <= vm::limits::signers
          && call.signer_methods.size() <= vm::limits::signers
          && call.signer_key_ids.size() <= vm::limits::signers;
    return call.signatures.size() == call.signers.size()
        && call.signer_contracts.size() == call.signers.size()
        && call.signer_methods.size() == call.signers.size()
        && call.signer_key_ids.size() == call.signers.size()
        && call.signer_nested.size() == call.signers.size();
  }

  vm::call make_contract_vm_call(const tx_extra_vm_call &call, uint64_t height, const vm::parent_header &parent)
  {
    vm::call request;
    request.kind = call.kind == tx_extra_vm_call::kind_deploy ? vm::call_kind::deploy : vm::call_kind::invoke;
    request.contract_id = call.contract_id;
    request.method = call.method;
    request.args = call.args;
    request.deposit = call.deposit;
    request.height = height;
    request.parent = parent;
    return request;
  }

  contract_call_verdict check_contract_call_structure(const tx_extra_vm_call &call, uint64_t fee, bool allow_unsigned)
  {
    if (call.version != 1 || call.mint.size() > 1
        || call.code.size() > vm::limits::module_size || call.args.size() > vm::limits::args_size
        || !contract_signer_shape_valid(call, allow_unsigned)
        || !contract_signers_unique(call.signers)
        || std::any_of(call.signer_nested.begin(), call.signer_nested.end(), [](uint8_t nested) { return nested > 1; })
        || (call.kind != tx_extra_vm_call::kind_deploy && call.kind != tx_extra_vm_call::kind_invoke))
      return contract_call_verdict::malformed;
    contract_call_fee split{};
    if (!split_contract_call_fee(call, fee, split))
      return contract_call_verdict::underpaid;
    if (call.gas_limit > QWC_VM_TX_GAS_CAP)
      return contract_call_verdict::over_tx_gas_cap;
    return contract_call_verdict::executed;
  }

  uint64_t contract_call_gas_limit(uint64_t gas_used)
  {
    const uint64_t used = std::min(gas_used, QWC_VM_TX_GAS_CAP);
    return std::min(QWC_VM_TX_GAS_CAP, used + (used * QWC_WALLET_GAS_MARGIN_PERCENT + 99) / 100);
  }

  bool make_contract_mint_destination(const account_public_address &address, bool is_subaddress, tx_extra_vm_mint_destination &destination)
  {
    const crypto::secret_key tx_secret = keypair::generate(hw::get_device("default")).sec;
    destination.tx_key = is_subaddress ? rct::rct2pk(rct::scalarmultKey(rct::pk2rct(address.m_spend_public_key), rct::sk2rct(tx_secret)))
                                       : rct::rct2pk(rct::scalarmultBase(rct::sk2rct(tx_secret)));
    crypto::key_derivation derivation;
    if (!crypto::generate_key_derivation(address.m_view_public_key, tx_secret, derivation))
      return false;
    crypto::derive_view_tag(derivation, QWC_MINT_DERIVATION_INDEX, destination.view_tag);
    return crypto::derive_public_key(derivation, QWC_MINT_DERIVATION_INDEX, address.m_spend_public_key, destination.key);
  }

  uint64_t contract_miner_fee(const transaction_prefix &tx, uint64_t fee, uint8_t version)
  {
    tx_extra_vm_call call{};
    if (!is_qwc_contracts_hardfork(version) || get_tx_vm_call(tx.extra, call) != 1)
      return fee;
    return fee - std::min(fee, call.deposit); // H-29
  }

  bool sum_contract_miner_fees(const std::vector<transaction> &txs, uint8_t version, uint64_t &fees)
  {
    fees = 0;
    for (const transaction &tx : txs)
    {
      const uint64_t fee = contract_miner_fee(tx, get_tx_fee(tx), version);
      if (fees > std::numeric_limits<uint64_t>::max() - fee)
        return false;
      fees += fee;
    }
    return true;
  }

  bool add_contract_mints_to_miner_tx(transaction &tx, const std::vector<contract_mint> &mints)
  {
    if (mints.empty())
      return true;
    const crypto::public_key tx_key = get_tx_pub_key_from_extra(tx);
    if (tx_key == crypto::null_pkey || mints.size() > QWC_VM_MAX_MINTS_PER_BLOCK)
      return false;
    std::vector<crypto::public_key> output_keys(tx.vout.size(), tx_key); // H-30
    for (const contract_mint &mint : mints)
    {
      tx_out out;
      set_tx_out(mint.amount, mint.destination.key, true, mint.destination.view_tag, out);
      tx.vout.push_back(out);
      output_keys.push_back(mint.destination.tx_key);
    }
    tx.invalidate_hashes();
    return add_additional_tx_pub_keys_to_extra(tx.extra, output_keys);
  }

  bool check_contract_mints_in_miner_tx(const transaction &tx, const std::vector<contract_mint> &mints)
  {
    const std::vector<crypto::public_key> output_keys = get_additional_tx_pub_keys_from_extra(tx);
    if (mints.empty())
      return output_keys.empty();
    const crypto::public_key tx_key = get_tx_pub_key_from_extra(tx);
    if (mints.size() > tx.vout.size() || output_keys.size() != tx.vout.size() || tx_key == crypto::null_pkey)
      return false;
    const size_t first_mint = tx.vout.size() - mints.size();
    if (qwertycoin::epose::coinbase_reward_output_end_v2(tx) != first_mint)
      return false;
    for (size_t i = 0; i < mints.size(); ++i)
    {
      const tx_out &out = tx.vout[first_mint + i];
      const contract_mint &mint = mints[i];
      if (out.target.type() != typeid(txout_to_tagged_key))
        return false;
      const txout_to_tagged_key &target = boost::get<txout_to_tagged_key>(out.target);
      if (out.amount != mint.amount || target.key != mint.destination.key || target.view_tag != mint.destination.view_tag
          || output_keys[first_mint + i] != mint.destination.tx_key)
        return false;
    }
    return true;
  }

  contract_call_verdict execute_contract_tx(const transaction &tx, uint64_t fee, const contract_block_context &context, contract_overlay &state, contract_effects &effects, bool verify_signatures)
  {
    effects.required_signers.clear();
    tx_extra_vm_call call{};
    const size_t calls = get_tx_vm_call(tx.extra, call);
    if (calls == 0)
      return contract_call_verdict::absent;
    if (calls != 1)
      return contract_call_verdict::malformed;
    const contract_call_verdict structure = check_contract_call_structure(call, fee, !verify_signatures);
    if (structure != contract_call_verdict::executed)
      return structure;
    if (effects.gas_used > QWC_VM_BLOCK_GAS_LIMIT || call.gas_limit > QWC_VM_BLOCK_GAS_LIMIT - effects.gas_used)
      return contract_call_verdict::over_block_gas_limit;

    vm::call request = make_contract_vm_call(call, context.height, context.parent);
    bind_hdr_info_hf18(context, request);

    const vm::result result = vm::execute(call.code, request, state, call.gas_limit);
    if (result.code != vm::status::ok) // VM-2
      return contract_call_verdict::failed;
    if (verify_signatures)
    {
      if (call.signers != result.required_signers)
        return contract_call_verdict::signer_mismatch;
      for (size_t i = 0; i < result.required_signers.size(); ++i)
        if (call.signer_contracts[i] != result.signer_origins[i].contract_id
            || call.signer_methods[i] != result.signer_origins[i].method
            || call.signer_nested[i] != static_cast<uint8_t>(result.signer_origins[i].nested))
          return contract_call_verdict::signer_mismatch;
      crypto::hash signature_hash;
      if (!get_contract_signature_hash(tx, context.network, fee, called_contract_id(call), signature_hash))
        return contract_call_verdict::malformed;
      for (size_t i = 0; i < result.required_signers.size(); ++i)
      {
        if (!crypto::check_signature(signature_hash, result.required_signers[i], call.signatures[i].value))
          return contract_call_verdict::invalid_signature;
      }
    }
    if (result.locked != call.deposit || (result.unlocked != 0) != (call.mint.size() == 1))
      return contract_call_verdict::unbalanced_funds;
    if (result.unlocked != 0 && effects.mints.size() >= QWC_VM_MAX_MINTS_PER_BLOCK)
      return contract_call_verdict::over_block_mint_limit;
    if (effects.deposited > std::numeric_limits<uint64_t>::max() - call.deposit
        || effects.withdrawn > std::numeric_limits<uint64_t>::max() - result.unlocked)
      return contract_call_verdict::unbalanced_funds;

    state.apply(result.writes);
    effects.required_signers = result.required_signers;
    effects.gas_used += result.gas_used;
    effects.deposited += call.deposit;
    effects.withdrawn += result.unlocked;
    if (result.unlocked != 0)
      effects.mints.push_back({result.unlocked, call.mint.front()});
    return contract_call_verdict::executed;
  }

  bool contract_call_accepted(contract_call_verdict verdict)
  {
    return verdict == contract_call_verdict::absent || verdict == contract_call_verdict::executed;
  }

  bool contract_call_verdict_can_change_with_chain(contract_call_verdict verdict)
  {
    return verdict == contract_call_verdict::failed
        || verdict == contract_call_verdict::signer_mismatch
        || verdict == contract_call_verdict::over_block_gas_limit
        || verdict == contract_call_verdict::over_block_mint_limit
        || verdict == contract_call_verdict::unbalanced_funds;
  }

  contract_call_verdict contract_block_executor::execute(const transaction &tx, uint64_t fee, bool verify_signatures)
  {
    return execute_contract_tx(tx, fee, m_context, m_state, m_effects, verify_signatures);
  }

  const char *contract_call_verdict_name(contract_call_verdict verdict)
  {
    switch (verdict)
    {
    case contract_call_verdict::absent: return "absent";
    case contract_call_verdict::executed: return "executed";
    case contract_call_verdict::malformed: return "malformed";
    case contract_call_verdict::underpaid: return "underpaid";
    case contract_call_verdict::over_tx_gas_cap: return "over tx gas cap";
    case contract_call_verdict::over_block_gas_limit: return "over block gas limit";
    case contract_call_verdict::failed: return "failed";
    case contract_call_verdict::signer_mismatch: return "signer mismatch";
    case contract_call_verdict::invalid_signature: return "invalid signature";
    case contract_call_verdict::unbalanced_funds: return "unbalanced funds";
    case contract_call_verdict::over_block_mint_limit: return "over block mint limit";
    }
    return "unknown";
  }
}
