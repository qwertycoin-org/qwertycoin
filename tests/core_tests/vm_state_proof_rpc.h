#pragma once
#include "contracts.h"
#include "contracts_deep_reorg.h"

struct gen_vm_state_proof_rpc : public gen_contracts_base
{
  gen_vm_state_proof_rpc()
  {
    REGISTER_CALLBACK_METHOD(gen_vm_state_proof_rpc, check_proofs);
  }
  bool generate(std::vector<test_event_entry>& events) const;
  bool check_proofs(cryptonote::core& c, size_t, const std::vector<test_event_entry>&);
};
template<> struct get_test_options<gen_vm_state_proof_rpc>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};

struct gen_vm_state_proof_retention : public gen_contracts_deep_reorg_replay
{
  gen_vm_state_proof_retention()
  {
    REGISTER_CALLBACK_METHOD(gen_vm_state_proof_retention, check_retention);
  }
  bool generate(std::vector<test_event_entry>& events) const;
  bool check_retention(cryptonote::core& c, size_t, const std::vector<test_event_entry>&);
};
template<> struct get_test_options<gen_vm_state_proof_retention>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};
