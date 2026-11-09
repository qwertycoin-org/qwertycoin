#pragma once
#include "height_window.h"

struct gen_contracts_base : public gen_height_window_base
{
  gen_contracts_base()
  {
    REGISTER_CALLBACK_METHOD(gen_contracts_base, check_vm_mirror);
    REGISTER_CALLBACK_METHOD(gen_contracts_base, check_stored_cd);
    REGISTER_CALLBACK_METHOD(gen_contracts_base, check_stored_xy);
    REGISTER_CALLBACK_METHOD(gen_contracts_base, check_height_unchanged);
    REGISTER_CALLBACK_METHOD(gen_contracts_base, check_template_drops_failing_call);
    REGISTER_CALLBACK_METHOD(gen_contracts_base, check_main_chain_kept);
    REGISTER_CALLBACK_METHOD(gen_contracts_base, check_pool_rejects_call);
    REGISTER_CALLBACK_METHOD(gen_contracts_base, check_pool_rejects_signature);
    REGISTER_CALLBACK_METHOD(gen_contracts_base, check_pool_rejects_signer_mismatch);
    REGISTER_CALLBACK_METHOD(gen_contracts_base, check_pool_rejects_forged_input);
    REGISTER_CALLBACK_METHOD(gen_contracts_base, check_pool_rejects_malformed_extra);
    REGISTER_CALLBACK_METHOD(gen_contracts_base, check_pool_rejects_underpaid);
    REGISTER_CALLBACK_METHOD(gen_contracts_base, check_mint_supply);
    REGISTER_CALLBACK_METHOD(gen_contracts_base, check_dry_run_and_proof);
    REGISTER_CALLBACK_METHOD(gen_contracts_base, check_zero_output_input_rejected);
    REGISTER_CALLBACK_METHOD(gen_contracts_base, check_zero_output_input_accepted);
  }

  static const std::string stored_key;
  static std::string contract_code();
  static crypto::hash contract_id();
  static cryptonote::tx_extra_vm_call deploy_call();
  static cryptonote::tx_extra_vm_call invoke_call(uint32_t method, const std::string& args);
  static const cryptonote::transaction* find_contract_tx(const std::vector<test_event_entry>& events, uint32_t method);

  bool construct_call_tx(const ring_chain& chain, const cryptonote::tx_extra_vm_call& call, cryptonote::transaction& tx, size_t spender = 0, const std::vector<crypto::public_key>& external_contract_signers = {}) const;
  bool push_kept_txs(std::vector<test_event_entry>& events, const std::vector<cryptonote::transaction>& txs) const;
  bool generate_rejected_calls(std::vector<test_event_entry>& events, const std::vector<cryptonote::tx_extra_vm_call>& calls, bool deploy_first) const;
  bool generate_rejected_coinbase_root(std::vector<test_event_entry>& events, uint8_t hf_version, const boost::optional<crypto::hash>& root) const;
  bool generate_failing_reorg(std::vector<test_event_entry>& events, bool shared_tx) const;
  bool generate_zero_output_case(std::vector<test_event_entry>& events, bool reject) const;

  bool check_vm_mirror(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_stored(cryptonote::core& c, const std::string& value);
  bool check_stored_cd(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_stored_xy(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_height_unchanged(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_main_chain_kept(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_template_drops_failing_call(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_pool_rejects_call(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_pool_rejects_signature(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_pool_rejects_signer_mismatch(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_pool_rejects_forged_input(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_pool_rejects_malformed_extra(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_pool_rejects_underpaid(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_mint_supply(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_dry_run_and_proof(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_zero_output_input_rejected(cryptonote::core& c, size_t ev_index,
      const std::vector<test_event_entry>& events);
  bool check_zero_output_input_accepted(cryptonote::core& c, size_t ev_index,
      const std::vector<test_event_entry>& events);
};

#define CONTRACTS_CASE(NAME, VERSION)                                                        \
  struct NAME : public gen_contracts_base { bool generate(std::vector<test_event_entry>& events) const; }; \
  template<> struct get_test_options<NAME>: public get_height_window_test_options<VERSION> {};

CONTRACTS_CASE(gen_contracts_deploy_and_invoke, HF_VERSION_QWC_CONTRACTS)
CONTRACTS_CASE(gen_contracts_zero_output_call, HF_VERSION_QWC_CONTRACTS)
CONTRACTS_CASE(gen_contracts_zero_output_rejected, HF_VERSION_QWC_CONTRACTS)
struct gen_contracts_zero_output_before_fork_rejected : public gen_contracts_base {
  bool generate(std::vector<test_event_entry>& events) const;
};
template<> struct get_test_options<gen_contracts_zero_output_before_fork_rejected> {
  const std::pair<uint8_t, uint64_t> hard_forks[5] = {
    {1, 0}, {2, 1}, {HF_VERSION_QWC_EPOSE, gen_height_window_base::mined_height},
    {HF_VERSION_QWC_CONTRACTS, gen_height_window_base::mined_height + 1}, {0, 0}
  };
  const cryptonote::test_options test_options = {hard_forks, 0};
};
CONTRACTS_CASE(gen_contracts_failing_call_rejected, HF_VERSION_QWC_CONTRACTS)
CONTRACTS_CASE(gen_contracts_underpaid_rejected, HF_VERSION_QWC_CONTRACTS)
CONTRACTS_CASE(gen_contracts_tx_gas_cap_rejected, HF_VERSION_QWC_CONTRACTS)
CONTRACTS_CASE(gen_contracts_root_mismatch_rejected, HF_VERSION_QWC_CONTRACTS)
CONTRACTS_CASE(gen_contracts_root_missing_rejected, HF_VERSION_QWC_CONTRACTS)
CONTRACTS_CASE(gen_contracts_root_before_fork_rejected, HF_VERSION_QWC_EPOSE)
CONTRACTS_CASE(gen_contracts_call_before_fork_rejected, HF_VERSION_QWC_EPOSE)
CONTRACTS_CASE(gen_contracts_reorg_undo, HF_VERSION_QWC_CONTRACTS)
CONTRACTS_CASE(gen_contracts_template_drops_failing_call, HF_VERSION_QWC_CONTRACTS)
CONTRACTS_CASE(gen_contracts_failing_alt_chain_keeps_main, HF_VERSION_QWC_CONTRACTS)
CONTRACTS_CASE(gen_contracts_failing_reorg_shared_tx, HF_VERSION_QWC_CONTRACTS)
CONTRACTS_CASE(gen_contracts_pool_rules, HF_VERSION_QWC_CONTRACTS)
CONTRACTS_CASE(gen_contracts_mint_supply_invariant, HF_VERSION_QWC_CONTRACTS)
CONTRACTS_CASE(gen_contracts_addsig_validation, HF_VERSION_QWC_CONTRACTS)
struct gen_contracts_external_addsig : public gen_contracts_base
{
  gen_contracts_external_addsig();
  bool generate(std::vector<test_event_entry>& events) const;
  bool check_mined_replay(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
};
template<> struct get_test_options<gen_contracts_external_addsig> : get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};
struct gen_contracts_stored_signer_changes : public gen_contracts_base
{
  gen_contracts_stored_signer_changes();
  bool generate(std::vector<test_event_entry>& events) const;
  bool check_original_call(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_changed_call(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
};
template<> struct get_test_options<gen_contracts_stored_signer_changes> : get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};
CONTRACTS_CASE(gen_contracts_forged_input_skips_vm, HF_VERSION_QWC_CONTRACTS)

struct gen_contracts_fork_boundary : public gen_contracts_base
{
  gen_contracts_fork_boundary();
  bool generate(std::vector<test_event_entry>& events) const;
  bool check_before_boundary(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_boundary(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
};
template<> struct get_test_options<gen_contracts_fork_boundary> {
  const std::pair<uint8_t, uint64_t> hard_forks[5] = {
    {1, 0}, {2, 1}, {HF_VERSION_QWC_CONTRACTS - 1, gen_height_window_base::mined_height - 1},
    {HF_VERSION_QWC_CONTRACTS, gen_height_window_base::mined_height}, {0, 0}
  };
  const cryptonote::test_options test_options = {hard_forks, 0};
};

struct gen_contracts_block_rules : public gen_contracts_base
{
  gen_contracts_block_rules();
  bool generate(std::vector<test_event_entry>& events) const;
  bool check_preverified_block_rejected(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
  bool check_supplement_block_accepted(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events);
};
template<> struct get_test_options<gen_contracts_block_rules> : get_test_options<gen_contracts_fork_boundary> {};
