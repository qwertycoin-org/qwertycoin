#pragma once

#include "cryptonote_core/blockchain.h"

struct blockchain_test_access
{
  static auto &coordinator(cryptonote::Blockchain &chain) { return chain.m_epose_v2; }
  static auto &parameters(cryptonote::Blockchain &chain) { return chain.m_epose_v2_parameters; }
  static auto &test_quorum(cryptonote::Blockchain &chain) { return chain.m_chainlock_test_quorum; }
  static auto &validation_height(cryptonote::Blockchain &chain) { return chain.m_epose_state_validation_height; }
  static auto &validation_tip(cryptonote::Blockchain &chain) { return chain.m_epose_state_validation_tip; }
  static auto &validation_checked_generation(cryptonote::Blockchain &chain) { return chain.m_epose_state_validation_checked_generation; }
  static auto &validation_generation(cryptonote::Blockchain &chain) { return chain.m_epose_state_validation_generation; }
  static auto &validation_coordinator_generation(cryptonote::Blockchain &chain) { return chain.m_epose_state_validation_coordinator_generation; }
  static auto &validation_result(cryptonote::Blockchain &chain) { return chain.m_epose_state_validation_result; }
  static auto &vm_stale(cryptonote::Blockchain &chain) { return chain.m_vm_state_stale; }
  static auto &vm_restore_hook(cryptonote::Blockchain &chain) { return chain.m_vm_restore_hook; }
  static auto &header_provider_override(cryptonote::Blockchain &chain) { return chain.m_header_provider_override; }
  static bool invalid_block(cryptonote::Blockchain &chain, const crypto::hash &id) { return chain.m_invalid_blocks.count(id) != 0; }
  static auto &vm_state(cryptonote::Blockchain &chain) { return chain.m_vm_state; }
  static auto &blockchain_lock(cryptonote::Blockchain &chain) { return chain.m_blockchain_lock; }
  static bool verify_complete_chainlock(const cryptonote::Blockchain &chain,
      const qwertycoin::epose::chainlock_v1 &lock, bool signatures)
  {
    return chain.verify_complete_chainlock(lock, signatures);
  }
  static bool alt_chainlocks_switch_verified(cryptonote::Blockchain &chain,
      const std::list<cryptonote::Blockchain::block_extended_info> &candidate)
  {
    return chain.evaluate_alt_chainlocks(candidate).verdict
        == cryptonote::Blockchain::alt_chainlock_verdict::switch_verified;
  }
  static void with_restored_vm_state(cryptonote::Blockchain &chain,
      const vm::write_set &restore, const std::function<void()> &operation)
  {
    chain.with_restored_vm_state(restore, operation);
  }
};
