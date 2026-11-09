#include "blockchain_test_access.h"
#include "chainlock_rpc.h"
#include "contracts.h"
#include "../unit_tests/vm_test_utils.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_core/blockchain.h"
#include "crypto/hash-ops.h"
#include "rpc/chainlock_rpc.h"
#include "storages/portable_storage_template_helper.h"
#include <cstdlib>
#include <fstream>
#include <sstream>

using namespace cryptonote;

bool gen_chainlock_rpc::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(start_chain(events, chain), false, "chain");
  gen_contracts_base contracts;
  transaction deploy, store;
  const std::string registry_key = "re";
  CHECK_AND_ASSERT_MES(contracts.construct_call_tx(chain, gen_contracts_base::deploy_call(), deploy, 0), false, "deploy");
  CHECK_AND_ASSERT_MES(contracts.construct_call_tx(chain, gen_contracts_base::invoke_call(vm_test::m_store, registry_key + "on"), store, 1), false, "store");
  contracts.push_kept_txs(events, {deploy, store});
  cryptonote::block locked, carrier, tip;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, fork_version, {deploy, store}, locked), false, "locked block");
  events.push_back(locked);
  DO_CALLBACK(events, "check_unlocked");
  blobdata lock;
  CHECK_AND_ASSERT_MES(make_lock_blob(events, full_lock(locked), lock), false, "lock");
  CHECK_AND_ASSERT_MES(extend(events, chain, locked, chain.miner, carrier, lock), false, "carrier");
  CHECK_AND_ASSERT_MES(extend(events, chain, carrier, chain.miner, tip), false, "tip");
  DO_CALLBACK(events, "check_locked");
  if (std::getenv("QWC_CHAINLOCK_VECTOR_PATH") != nullptr)
    DO_CALLBACK(events, "write_vector");
  return true;
}

bool gen_chainlock_rpc::check_unlocked(core& c, size_t, const std::vector<test_event_entry>&)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_rpc::check_unlocked");
  chainlock_proof proof;
  CHECK_TEST_CONDITION(c.get_blockchain_storage().get_chainlock_proof(0, proof) == chainlock_proof_result::no_lock);
  return true;
}

bool gen_chainlock_rpc::check_locked(core& c, size_t, const std::vector<test_event_entry>&)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_rpc::check_locked");
  Blockchain& chain = c.get_blockchain_storage();
  const uint64_t tip = chain.get_current_blockchain_height() - 1;
  chainlock_proof latest, requested;
  const uint64_t selections_before = chain.chainlock_quorum_selection_count();
  CHECK_TEST_CONDITION(chain.get_chainlock_proof(0, latest) == chainlock_proof_result::ok);
  CHECK_EQ(chain.chainlock_quorum_selection_count(), selections_before + 1);
  CHECK_TEST_CONDITION(chain.get_chainlock_proof(latest.height, requested) == chainlock_proof_result::ok);
  CHECK_EQ(chain.chainlock_quorum_selection_count(), selections_before + 2);
  CHECK_EQ(latest.block_id, requested.block_id);
  CHECK_EQ(latest.block_id, get_block_hash(latest.locked_block));
  CHECK_TEST_CONDITION(!latest.miner_tx_branch.empty());
  const crypto::hash miner_hash = get_transaction_hash(latest.locked_block.miner_tx);
  crypto::hash branch_root{};
  CHECK_TEST_CONDITION(crypto::tree_branch_hash(miner_hash.data,
      reinterpret_cast<const char (*)[32]>(latest.miner_tx_branch.data()),
      latest.miner_tx_branch.size(), 0, branch_root.data));
  CHECK_EQ(branch_root, get_tx_tree_hash(latest.locked_block));
  CHECK_EQ(latest.lock_carrier_height, latest.height + 1);
  qwertycoin::epose::chainlock_v1 lock;
  CHECK_TEST_CONDITION(qwertycoin::epose::parse_chainlock_v1(latest.lock_blob, lock));
  CHECK_EQ(lock.signer_count(), latest.quorum.threshold());
  chainlock_quorum_inputs inputs;
  CHECK_TEST_CONDITION(chain.get_chainlock_quorum_inputs(lock.epoch, inputs));
  CHECK_TEST_CONDITION(inputs.test_override);
  crypto::hash root{};
  CHECK_EQ(1, get_tx_vm_state_root(latest.locked_block.miner_tx.extra, root));
  CHECK_EQ(root, latest.vm_state_root);
  CHECK_TEST_CONDITION(chain.get_chainlock_proof(tip, requested) == chainlock_proof_result::height_not_locked);
  blockchain_test_access::vm_stale(chain) = true;
  const auto restore_stale = epee::misc_utils::create_scope_leave_handler([&]() { blockchain_test_access::vm_stale(chain) = false; });
  CHECK_TEST_CONDITION(chain.get_chainlock_proof(0, requested) == chainlock_proof_result::ok);

  return true;
}

bool gen_chainlock_rpc::write_vector(core& c, size_t, const std::vector<test_event_entry>&)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_rpc::write_vector");
  Blockchain& chain = c.get_blockchain_storage();
  chainlock_proof locked_proof;
  CHECK_TEST_CONDITION(chain.get_chainlock_proof(0, locked_proof) == chainlock_proof_result::ok);
  chainlock_quorum_inputs inputs;
  CHECK_TEST_CONDITION(chain.get_chainlock_quorum_inputs(locked_proof.quorum.epoch, inputs));
  const std::string key = vm_test::var_key(gen_contracts_base::contract_id(), 0, "re");
  uint64_t proof_height = 0;
  crypto::hash root{};
  boost::optional<std::string> value;
  vm::state_proof proof;
  CHECK_TEST_CONDITION(chain.get_vm_state_proof_at(locked_proof.height, key, proof_height, root, value, proof) == vm_proof_result::ok);
  CHECK_EQ(proof_height, locked_proof.height);
  CHECK_EQ(root, locked_proof.vm_state_root);
  CHECK_TEST_CONDITION(value && *value == "on");
  const block carrier = chain.get_db().get_block_from_height(locked_proof.lock_carrier_height);
  COMMAND_RPC_GET_CHAINLOCK::response lock_response;
  COMMAND_RPC_GET_CHAINLOCK_QUORUM_INPUTS::response inputs_response;
  COMMAND_RPC_GET_VM_STATE_PROOF::response state_response;
  fill_chainlock_response(locked_proof, lock_response);
  fill_chainlock_quorum_inputs_response(inputs, 0, inputs_response);
  fill_vm_state_proof_response(proof_height, root, value, proof, state_response);
  std::string lock_json, state_json, inputs_json;
  CHECK_TEST_CONDITION(epee::serialization::store_t_to_json(lock_response, lock_json));
  CHECK_TEST_CONDITION(epee::serialization::store_t_to_json(inputs_response, inputs_json));
  CHECK_TEST_CONDITION(epee::serialization::store_t_to_json(state_response, state_json));
  const char* vector_path = std::getenv("QWC_CHAINLOCK_VECTOR_PATH");
  CHECK_TEST_CONDITION(vector_path != nullptr);
  std::ofstream output(vector_path);
  CHECK_TEST_CONDITION(output.good());
  output << "{\"network\":" << static_cast<unsigned>(FAKECHAIN)
      << ",\"genesis_hash\":\"" << epee::string_tools::pod_to_hex(inputs.context.genesis_hash)
      << "\",\"parameter_set_hash\":\"" << epee::string_tools::pod_to_hex(inputs.context.parameter_set_hash)
      << "\",\"carrier_block_id\":\"" << epee::string_tools::pod_to_hex(get_block_hash(carrier))
      << "\",\"carrier_block_blob\":\"" << epee::string_tools::buff_to_hex_nodelimer(block_to_blob(carrier))
      << "\",\"key\":\"" << epee::string_tools::buff_to_hex_nodelimer(key)
      << "\",\"get_chainlock\":" << lock_json << ",\"get_chainlock_quorum_inputs\":" << inputs_json
      << ",\"get_vm_state_proof\":" << state_json << "}\n";
  output.flush();
  return output.good();
}

bool gen_chainlock_quorum_before_epoch_start::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(start_chain(events, chain), false, "chain");
  DO_CALLBACK(events, "check_quorum");
  return true;
}

bool gen_chainlock_quorum_before_epoch_start::check_quorum(core& c, size_t, const std::vector<test_event_entry>&)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_quorum_before_epoch_start::check_quorum");
  const Blockchain& chain = c.get_blockchain_storage();
  const uint64_t chain_height = chain.get_current_blockchain_height();
  const uint64_t epoch = chain_height / lock_epoch_length + 1;
  uint64_t anchor_height = 0, epoch_start = 0;
  CHECK_TEST_CONDITION(timing().committee_anchor(epoch, anchor_height));
  CHECK_TEST_CONDITION(timing().epoch_start(epoch, epoch_start));
  CHECK_TEST_CONDITION(anchor_height < chain_height && chain_height <= epoch_start);
  qwertycoin::epose::chainlock_quorum_v1 quorum;
  CHECK_TEST_CONDITION(chain.get_chainlock_quorum(epoch, quorum));
  CHECK_TEST_CONDITION(quorum.can_lock());
  chainlock_quorum_inputs rpc_inputs;
  CHECK_TEST_CONDITION(!chain.get_chainlock_quorum_inputs(epoch, rpc_inputs));
  return true;
}

bool gen_chainlock_alt_epoch_boundary::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(start_chain(events, chain), false, "chain");
  block a0, a1, a2, b1, b2, b3, b4;
  cryptonote::account_base rival;
  rival.generate();
  CHECK_AND_ASSERT_MES(extend(events, chain, chain.tip, chain.miner, a0), false, "main 0");
  CHECK_AND_ASSERT_MES(extend(events, chain, a0, chain.miner, a1), false, "main 1");
  CHECK_AND_ASSERT_MES(extend(events, chain, a1, chain.miner, a2), false, "main 2");
  CHECK_AND_ASSERT_MES(construct_next_block(chain, a0, rival, fork_version, {}, b1), false, "alt 1");
  CHECK_AND_ASSERT_MES(construct_next_block(chain, b1, rival, fork_version, {}, b2), false, "alt 2");
  CHECK_AND_ASSERT_MES(construct_next_block(chain, b2, rival, fork_version, {}, b3), false, "alt 3");
  blobdata lock;
  CHECK_AND_ASSERT_MES(make_lock_blob(events, full_lock(b3), lock), false, "boundary lock");
  CHECK_AND_ASSERT_MES(construct_next_block(chain, b3, rival, fork_version, {}, b4, lock), false, "alt carrier");
  DO_CALLBACK(events, "check_verdict");
  for (const block &alt : {b1, b2, b3, b4})
    events.push_back(alt);
  DO_CALLBACK(events, "check_reorg");
  return true;
}

bool gen_chainlock_alt_epoch_boundary::check_verdict(core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_alt_epoch_boundary::check_verdict");
  Blockchain& chain = c.get_blockchain_storage();
  CHECK_TEST_CONDITION(ev_index + 4 < events.size());
  const block* locked = boost::get<block>(&events[ev_index + 3]);
  CHECK_TEST_CONDITION(locked != nullptr);
  CHECK_TEST_CONDITION(chain.get_current_blockchain_height() <= get_block_height(*locked));
  std::list<Blockchain::block_extended_info> candidate;
  for (size_t index = ev_index + 1; index <= ev_index + 4; ++index)
  {
    const block* alt = boost::get<block>(&events[index]);
    CHECK_TEST_CONDITION(alt != nullptr);
    candidate.push_back({*alt, get_block_height(*alt), 0, 0, 0});
  }
  CHECK_TEST_CONDITION(blockchain_test_access::alt_chainlocks_switch_verified(chain, candidate));
  return true;
}

bool gen_chainlock_alt_epoch_boundary::check_reorg(core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_alt_epoch_boundary::check_reorg");
  const Blockchain& chain = c.get_blockchain_storage();
  CHECK_TEST_CONDITION(ev_index >= 4);
  const block* locked = boost::get<block>(&events[ev_index - 2]);
  const block* tip = boost::get<block>(&events[ev_index - 1]);
  CHECK_TEST_CONDITION(locked != nullptr && tip != nullptr);
  CHECK_EQ(chain.get_tail_id(), get_block_hash(*tip));
  uint64_t locked_height = 0;
  CHECK_TEST_CONDITION(chain.get_highest_main_chainlock(locked_height));
  CHECK_EQ(locked_height, get_block_height(*locked));
  return true;
}

bool gen_chainlock_rpc_late_carrier::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(start_chain(events, chain), false, "chain");
  block locked, prev, next;
  CHECK_AND_ASSERT_MES(extend(events, chain, chain.tip, chain.miner, locked), false, "locked");
  blobdata lock;
  CHECK_AND_ASSERT_MES(make_lock_blob(events, full_lock(locked), lock), false, "lock");
  prev = locked;
  for (uint64_t count = 1; count < 700; ++count)
  {
    CHECK_AND_ASSERT_MES(extend(events, chain, prev, chain.miner, next), false, "intermediate");
    prev = next;
  }
  CHECK_AND_ASSERT_MES(extend(events, chain, prev, chain.miner, next, lock), false, "late carrier");
  DO_CALLBACK(events, "check_proof");
  return true;
}

bool gen_chainlock_rpc_late_carrier::check_proof(core& c, size_t, const std::vector<test_event_entry>&)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_rpc_late_carrier::check_proof");
  const Blockchain& chain = c.get_blockchain_storage();
  const uint64_t tip = chain.get_current_blockchain_height() - 1;
  chainlock_proof proof;
  const uint64_t selections_before = chain.chainlock_quorum_selection_count();
  CHECK_TEST_CONDITION(chain.get_chainlock_proof(tip - 700, proof) == chainlock_proof_result::ok);
  CHECK_EQ(proof.height, tip - 700);
  CHECK_EQ(proof.lock_carrier_height, tip);
  CHECK_EQ(chain.chainlock_quorum_selection_count(), selections_before + 1);
  return true;
}

bool gen_chainlock_rpc_pages::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(start_chain(events, chain), false, "chain");
  DO_CALLBACK(events, "check_page");
  return true;
}

bool gen_chainlock_rpc_pages::check_page(core& c, size_t, const std::vector<test_event_entry>&)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_rpc_pages::check_page");
  Blockchain& chain = c.get_blockchain_storage();
  std::vector<crypto::public_key> keys(10000);
  for (size_t index = 0; index < keys.size(); ++index)
  {
    keys[index].data[0] = static_cast<char>(index & 0xff);
    keys[index].data[1] = static_cast<char>(index >> 8);
  }
  CHECK_TEST_CONDITION(chain.set_chainlock_test_quorum(timing(), keys));
  chainlock_quorum_inputs inputs;
  const uint64_t epoch = (chain.get_current_blockchain_height() - 1) / lock_epoch_length;
  CHECK_TEST_CONDITION(chain.get_chainlock_quorum_inputs(epoch, inputs, 256, 256));
  CHECK_EQ(inputs.snapshot_total, 10000u);
  CHECK_EQ(inputs.qualification_total, 10000u);
  CHECK_EQ(inputs.snapshot_members.size(), 256u);
  CHECK_EQ(inputs.qualified_members.size(), 256u);
  CHECK_EQ(inputs.snapshot_members.front(), keys[256]);
  CHECK_EQ(inputs.snapshot_members.back(), keys[511]);
  CHECK_EQ(inputs.qualified_members.front(), keys[256]);
  CHECK_EQ(inputs.qualified_members.back(), keys[511]);
  return true;
}
