// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <vector>

#include "cryptonote_basic/blobdatatype.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/tx_extra.h"
#include "cryptonote_config.h"
#include "vm/vm.h"

namespace cryptonote
{
  struct contract_mint
  {
    uint64_t amount = 0;
    tx_extra_vm_mint_destination destination{};
  };

  struct contract_block_context
  {
    uint64_t height = 0;
    network_type network = MAINNET;
    vm::parent_header parent;
    vm::header_provider headers;
    bool hdr_info_active = false;
  };

  struct contract_effects
  {
    uint64_t gas_used = 0;
    uint64_t deposited = 0;
    uint64_t withdrawn = 0;
    std::vector<contract_mint> mints;
    std::vector<crypto::public_key> required_signers;
  };

  struct miner_contract_outputs
  {
    crypto::hash state_root = crypto::null_hash;
    std::vector<contract_mint> mints;
    blobdata chainlock;
  };

  enum class contract_call_verdict
  {
    absent,
    executed,
    malformed,
    underpaid,
    over_tx_gas_cap,
    over_block_gas_limit,
    failed,
    signer_mismatch,
    invalid_signature,
    unbalanced_funds,
    over_block_mint_limit
  };

  contract_call_verdict check_contract_call_structure(const tx_extra_vm_call &call, uint64_t fee, bool allow_unsigned = false);

  class contract_overlay final : public vm::state_view
  {
  public:
    explicit contract_overlay(const vm::state_view &base) : m_base(base) {}

    bool get(const std::string &key, std::string &value) const override;
    void apply(const vm::write_set &writes);
    const vm::write_set &writes() const { return m_writes; }

  private:
    const vm::state_view &m_base;
    vm::write_set m_writes;
  };

  struct contract_call_fee
  {
    uint64_t deposit = 0;
    uint64_t gas_price = 0;
    uint64_t remainder = 0;
  };

  bool contract_call_fee_floor(const tx_extra_vm_call &call, uint64_t &fee);
  bool contract_call_fee_floor(const std::vector<uint8_t> &tx_extra, uint64_t &fee);
  bool split_contract_call_fee(const tx_extra_vm_call &call, uint64_t fee, contract_call_fee &split);
  crypto::hash called_contract_id(const tx_extra_vm_call &call);
  bool contract_signer_shape_valid(const tx_extra_vm_call &call, bool allow_unsigned);
  vm::call make_contract_vm_call(const tx_extra_vm_call &call, uint64_t height, const vm::parent_header &parent);
  uint64_t contract_call_gas_limit(uint64_t gas_used);
  bool make_contract_mint_destination(const account_public_address &address, bool is_subaddress, tx_extra_vm_mint_destination &destination);
  uint64_t contract_miner_fee(const transaction_prefix &tx, uint64_t fee, uint8_t version);
  bool sum_contract_miner_fees(const std::vector<transaction> &txs, uint8_t version, uint64_t &fees);
  bool add_contract_mints_to_miner_tx(transaction &tx, const std::vector<contract_mint> &mints);
  bool check_contract_mints_in_miner_tx(const transaction &tx, const std::vector<contract_mint> &mints);
  contract_call_verdict execute_contract_tx(const transaction &tx, uint64_t fee, const contract_block_context &context, contract_overlay &state, contract_effects &effects, bool verify_signatures = true);
  bool contract_call_accepted(contract_call_verdict verdict);
  bool contract_call_verdict_can_change_with_chain(contract_call_verdict verdict);
  const char *contract_call_verdict_name(contract_call_verdict verdict);

  class contract_block_executor
  {
  public:
    contract_block_executor(const vm::state_view &base, const contract_block_context &context) : m_state(base), m_context(context) {}

    contract_call_verdict execute(const transaction &tx, uint64_t fee, bool verify_signatures = true);
    const contract_effects &effects() const { return m_effects; }
    const vm::write_set &writes() const { return m_state.writes(); }

  private:
    contract_overlay m_state;
    contract_block_context m_context;
    contract_effects m_effects;
  };
}
