#pragma once
#include "contracts.h"

struct gen_vm_state_proof_restore : public gen_contracts_base
{
  gen_vm_state_proof_restore()
  {
    REGISTER_CALLBACK_METHOD(gen_vm_state_proof_restore, inject_restore_failure);
    REGISTER_CALLBACK_METHOD(gen_vm_state_proof_restore, check_operation_failure_and_tip_mismatch);
    REGISTER_CALLBACK_METHOD(gen_vm_state_proof_restore, check_reloaded);
  }
  bool generate(std::vector<test_event_entry>& events) const;
  bool inject_restore_failure(cryptonote::core& c, size_t, const std::vector<test_event_entry>&);
  bool check_operation_failure_and_tip_mismatch(cryptonote::core& c, size_t, const std::vector<test_event_entry>&);
  bool check_reloaded(cryptonote::core& c, size_t, const std::vector<test_event_entry>&);
};
template<> struct get_test_options<gen_vm_state_proof_restore>: public get_height_window_test_options<HF_VERSION_QWC_CONTRACTS> {};
