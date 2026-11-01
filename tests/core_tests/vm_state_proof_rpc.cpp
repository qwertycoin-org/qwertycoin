#include "vm_state_proof_rpc.h"
#include "../unit_tests/vm_test_utils.h"
#include "cryptonote_core/blockchain.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "rpc/chainlock_rpc.h"

using namespace cryptonote;

bool gen_vm_state_proof_rpc::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "chain");
  transaction deploy, store;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, deploy_call(), deploy, 0), false, "deploy");
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, invoke_call(vm_test::m_store, stored_key + "cd"), store, 1), false, "store");
  push_kept_txs(events, {deploy, store});
  block written, tip;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, fork_version, {deploy, store}, written), false, "written");
  events.push_back(written);
  CHECK_AND_ASSERT_MES(construct_next_block(chain, written, chain.miner, fork_version, {}, tip), false, "tip");
  events.push_back(tip);
  DO_CALLBACK(events, "check_proofs");
  return true;
}

bool gen_vm_state_proof_rpc::check_proofs(core& c, size_t, const std::vector<test_event_entry>&)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_vm_state_proof_rpc::check_proofs");
  const Blockchain& chain = c.get_blockchain_storage();
  const std::string key = vm_test::var_key(contract_id(), 0, stored_key);
  const uint64_t tip = chain.get_current_blockchain_height() - 1;
  uint64_t height = 0;
  crypto::hash root{};
  boost::optional<std::string> value;
  vm::state_proof proof;
  CHECK_TEST_CONDITION(chain.get_vm_state_proof_at(boost::none, key, height, root, value, proof) == vm_proof_result::ok);
  CHECK_EQ(height, tip);
  const crypto::hash tip_root = root;
  const boost::optional<std::string> tip_value = value;
  const vm::state_proof tip_proof = proof;
  CHECK_TEST_CONDITION(chain.get_vm_state_proof_at(tip, key, height, root, value, proof) == vm_proof_result::ok);
  CHECK_EQ(root, tip_root);
  CHECK_TEST_CONDITION(value == tip_value);
  CHECK_EQ(proof.steps.size(), tip_proof.steps.size());
  for (size_t index = 0; index < proof.steps.size(); ++index)
  {
    CHECK_EQ(proof.steps[index].bit, tip_proof.steps[index].bit);
    CHECK_EQ(proof.steps[index].sibling, tip_proof.steps[index].sibling);
  }
  CHECK_TEST_CONDITION(proof.leaf_path == tip_proof.leaf_path);
  CHECK_EQ(proof.leaf_value, tip_proof.leaf_value);
  CHECK_TEST_CONDITION(value && *value == "cd");
  CHECK_TEST_CONDITION(vm::state_tree::verify_proof(root, key, value, proof));
  CHECK_TEST_CONDITION(chain.get_vm_state_proof_at(0, key, height, root, value, proof) == vm_proof_result::ok);
  CHECK_EQ(height, 0);
  CHECK_TEST_CONDITION(!value);
  CHECK_TEST_CONDITION(vm::state_tree::verify_proof(root, key, value, proof));
  const uint64_t copies_before = vm::state_tree::copy_count();
  CHECK_TEST_CONDITION(vm_proof_depth_allowed(tip - 1, tip + 1, true));
  CHECK_TEST_CONDITION(chain.get_vm_state_proof_at(tip - 1, key, height, root, value, proof) == vm_proof_result::ok);
  CHECK_EQ(vm::state_tree::copy_count(), copies_before);
  CHECK_EQ(height, tip - 1);
  crypto::hash claimed_root{};
  CHECK_EQ(1, get_tx_vm_state_root(chain.get_db().get_block_from_height(height).miner_tx.extra, claimed_root));
  CHECK_EQ(root, claimed_root);
  CHECK_TEST_CONDITION(vm::state_tree::verify_proof(root, key, value, proof));
  CHECK_TEST_CONDITION(chain.get_vm_state_proof_at(tip + 1, key, height, root, value, proof) == vm_proof_result::unavailable);
  return true;
}

bool gen_vm_state_proof_retention::generate(std::vector<test_event_entry>& events) const
{
  CHECK_AND_ASSERT_MES(gen_contracts_deep_reorg_replay::generate(events), false, "deep chain");
  DO_CALLBACK(events, "check_retention");
  return true;
}

bool gen_vm_state_proof_retention::check_retention(core& c, size_t, const std::vector<test_event_entry>&)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_vm_state_proof_retention::check_retention");
  const Blockchain& chain = c.get_blockchain_storage();
  const uint64_t tip = chain.get_current_blockchain_height() - 1;
  const std::string key = vm_test::var_key(contract_id(), 0, stored_key);
  uint64_t height = 0;
  crypto::hash root{};
  boost::optional<std::string> value;
  vm::state_proof proof;
  const uint64_t copies_before = vm::state_tree::copy_count();
  CHECK_TEST_CONDITION(!vm_proof_depth_allowed(tip - RESTRICTED_VM_PROOF_MAX_DEPTH - 1, tip + 1, true));
  CHECK_TEST_CONDITION(chain.get_vm_state_proof_at(tip - QWC_VM_UNDO_RETENTION_BLOCKS, key, height, root, value, proof) == vm_proof_result::ok);
  CHECK_EQ(vm::state_tree::copy_count(), copies_before);
  CHECK_TEST_CONDITION(chain.get_vm_state_proof_at(tip - QWC_VM_UNDO_RETENTION_BLOCKS - 1, key, height, root, value, proof) == vm_proof_result::outside_retention);
  return true;
}
