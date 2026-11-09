#include "blockchain_test_access.h"
#include "vm_state_proof_restore.h"
#include "../unit_tests/vm_test_utils.h"
#include "cryptonote_core/blockchain.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include <new>

using namespace cryptonote;

bool gen_vm_state_proof_restore::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "chain");
  transaction deploy, store;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, deploy_call(), deploy, 0), false, "deploy");
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, invoke_call(vm_test::m_store, stored_key + "cd"), store, 1), false, "store");
  push_kept_txs(events, {deploy, store});
  block before, tip, next;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, fork_version, {deploy}, before), false, "before");
  events.push_back(before);
  CHECK_AND_ASSERT_MES(construct_next_block(chain, before, chain.miner, fork_version, {store}, tip), false, "tip");
  events.push_back(tip);
  DO_CALLBACK(events, "inject_restore_failure");
  CHECK_AND_ASSERT_MES(construct_next_block(chain, tip, chain.miner, fork_version, {}, next), false, "next");
  events.push_back(next);
  DO_CALLBACK(events, "check_reloaded");
  DO_CALLBACK(events, "check_operation_failure_and_tip_mismatch");
  block rebuilt;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, next, chain.miner, fork_version, {}, rebuilt), false, "rebuilt");
  events.push_back(rebuilt);
  DO_CALLBACK(events, "check_reloaded");
  return true;
}

bool gen_vm_state_proof_restore::inject_restore_failure(core& c, size_t, const std::vector<test_event_entry>&)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_vm_state_proof_restore::inject_restore_failure");
  Blockchain& chain = c.get_blockchain_storage();
  const uint64_t tip = chain.get_current_blockchain_height() - 1;
  uint64_t height = 0;
  crypto::hash root{};
  boost::optional<std::string> value;
  vm::state_proof proof;
  blockchain_test_access::vm_restore_hook(chain) = []() { throw std::bad_alloc(); };
  bool threw = false;
  try
  {
    chain.get_vm_state_proof_at(tip - 1, vm_test::var_key(contract_id(), 0, stored_key), height, root, value, proof);
  }
  catch (const std::bad_alloc&)
  {
    threw = true;
  }
  blockchain_test_access::vm_restore_hook(chain) = {};
  CHECK_TEST_CONDITION(threw);
  CHECK_TEST_CONDITION(blockchain_test_access::vm_stale(chain));
  crypto::hash tip_commitment{};
  CHECK_EQ(1, get_tx_vm_state_root(chain.get_db().get_block_from_height(tip).miner_tx.extra, tip_commitment));
  CHECK_TEST_CONDITION(chain.get_vm_state_root() != tip_commitment);
  return true;
}

bool gen_vm_state_proof_restore::check_reloaded(core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_vm_state_proof_restore::check_reloaded");
  Blockchain& chain = c.get_blockchain_storage();
  CHECK_TEST_CONDITION(!blockchain_test_access::vm_stale(chain));
  CHECK_TEST_CONDITION(ev_index > 0);
  const block* next = boost::get<block>(&events[ev_index - 1]);
  CHECK_TEST_CONDITION(next != nullptr);
  CHECK_EQ(chain.get_tail_id(), get_block_hash(*next));
  crypto::hash committed{};
  CHECK_EQ(1, get_tx_vm_state_root(chain.get_db().get_block_from_height(chain.get_current_blockchain_height() - 1).miner_tx.extra, committed));
  CHECK_EQ(chain.get_vm_state_root(), committed);
  return true;
}

bool gen_vm_state_proof_restore::check_operation_failure_and_tip_mismatch(core& c, size_t, const std::vector<test_event_entry>&)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_vm_state_proof_restore::check_operation_failure_and_tip_mismatch");
  Blockchain& chain = c.get_blockchain_storage();
  const uint64_t tip = chain.get_current_blockchain_height() - 1;
  crypto::hash committed{};
  CHECK_EQ(1, get_tx_vm_state_root(chain.get_db().get_block_from_height(tip).miner_tx.extra, committed));
  bool threw = false;
  try
  {
    blockchain_test_access::with_restored_vm_state(chain, {}, []() { throw std::bad_alloc(); });
  }
  catch (const std::bad_alloc&)
  {
    threw = true;
  }
  CHECK_TEST_CONDITION(threw);
  CHECK_TEST_CONDITION(!blockchain_test_access::vm_stale(chain));
  CHECK_EQ(chain.get_vm_state_root(), committed);
  const std::string key = vm_test::var_key(contract_id(), 0, stored_key);
  blockchain_test_access::vm_state(chain).set(key, "mismatch");
  uint64_t height = 0;
  crypto::hash root{};
  boost::optional<std::string> value;
  vm::state_proof proof;
  CHECK_TEST_CONDITION(chain.get_vm_state_proof_at(tip - 1, key, height, root, value, proof) == vm_proof_result::unavailable);
  CHECK_TEST_CONDITION(!blockchain_test_access::vm_stale(chain));
  CHECK_TEST_CONDITION(chain.get_vm_state_proof_at(boost::none, key, height, root, value, proof) == vm_proof_result::rebuilding);
  CHECK_TEST_CONDITION(blockchain_test_access::vm_stale(chain));
  return true;
}
