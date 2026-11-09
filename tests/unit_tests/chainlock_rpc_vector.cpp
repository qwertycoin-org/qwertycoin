#include <cstring>
#include <fstream>
#include <sstream>
#include <gtest/gtest.h>
#include "common/varint.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "epose/chainlock_v1.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "storages/portable_storage_template_helper.h"
#include "string_tools.h"
#include "vm/state_tree.h"

namespace
{
  struct chainlock_vector
  {
    uint64_t network;
    std::string genesis_hash;
    std::string parameter_set_hash;
    std::string carrier_block_id;
    std::string carrier_block_blob;
    std::string key;
    cryptonote::COMMAND_RPC_GET_CHAINLOCK::response_t get_chainlock;
    cryptonote::COMMAND_RPC_GET_CHAINLOCK_QUORUM_INPUTS::response_t get_chainlock_quorum_inputs;
    cryptonote::COMMAND_RPC_GET_VM_STATE_PROOF::response_t get_vm_state_proof;

    BEGIN_KV_SERIALIZE_MAP()
      KV_SERIALIZE(network)
      KV_SERIALIZE(genesis_hash)
      KV_SERIALIZE(parameter_set_hash)
      KV_SERIALIZE(carrier_block_id)
      KV_SERIALIZE(carrier_block_blob)
      KV_SERIALIZE(key)
      KV_SERIALIZE(get_chainlock)
      KV_SERIALIZE(get_chainlock_quorum_inputs)
      KV_SERIALIZE(get_vm_state_proof)
    END_KV_SERIALIZE_MAP()
  };
}

TEST(chainlock_rpc_vector, verifies_offline)
{
  const std::string source = __FILE__;
  const std::string path = source.substr(0, source.find_last_of('/')) + "/../data/chainlock_vm_state_proof.json";
  std::ifstream input(path);
  ASSERT_TRUE(input.good());
  std::ostringstream buffer;
  buffer << input.rdbuf();
  chainlock_vector data{};
  ASSERT_TRUE(epee::serialization::load_t_from_json(data, buffer.str()));
  ASSERT_EQ(data.get_chainlock.status, "OK");
  ASSERT_EQ(data.get_chainlock_quorum_inputs.status, "OK");
  ASSERT_EQ(data.get_vm_state_proof.status, "OK");
  ASSERT_EQ(data.get_chainlock.height, data.get_vm_state_proof.height);
  ASSERT_EQ(data.get_chainlock.vm_state_root, data.get_vm_state_proof.root);

  cryptonote::blobdata hashing_blob, miner_blob, carrier_blob, lock_blob, key;
  ASSERT_TRUE(epee::string_tools::parse_hexstr_to_binbuff(data.get_chainlock.block_hashing_blob, hashing_blob));
  ASSERT_TRUE(epee::string_tools::parse_hexstr_to_binbuff(data.get_chainlock.miner_tx_blob, miner_blob));
  ASSERT_TRUE(epee::string_tools::parse_hexstr_to_binbuff(data.carrier_block_blob, carrier_blob));
  ASSERT_TRUE(epee::string_tools::parse_hexstr_to_binbuff(data.get_chainlock.lock_blob, lock_blob));
  ASSERT_TRUE(epee::string_tools::parse_hexstr_to_binbuff(data.key, key));

  crypto::hash block_id{}, root{}, claimed_root{}, genesis_hash{}, parameter_set_hash{}, anchor_hash{};
  const std::string prefixed_blob = tools::get_varint_data(hashing_blob.size()) + hashing_blob;
  block_id = crypto::cn_fast_hash(prefixed_blob.data(), prefixed_blob.size());
  ASSERT_TRUE(epee::string_tools::hex_to_pod(data.get_chainlock.block_id, claimed_root));
  ASSERT_EQ(block_id, claimed_root);
  ASSERT_TRUE(epee::string_tools::hex_to_pod(data.genesis_hash, genesis_hash));
  ASSERT_TRUE(epee::string_tools::hex_to_pod(data.parameter_set_hash, parameter_set_hash));
  ASSERT_TRUE(epee::string_tools::hex_to_pod(data.get_chainlock_quorum_inputs.snapshot_block_id, anchor_hash));

  cryptonote::transaction miner_tx;
  ASSERT_TRUE(cryptonote::parse_and_validate_tx_from_blob(miner_blob, miner_tx));
  ASSERT_EQ(cryptonote::get_tx_vm_state_root(miner_tx.extra, root), 1u);
  ASSERT_EQ(epee::string_tools::pod_to_hex(root), data.get_chainlock.vm_state_root);
  const crypto::hash miner_hash = cryptonote::get_transaction_hash(miner_tx);
  std::vector<crypto::hash> branch;
  for (const std::string &hex : data.get_chainlock.miner_tx_branch)
  {
    crypto::hash sibling{};
    ASSERT_TRUE(epee::string_tools::hex_to_pod(hex, sibling));
    branch.push_back(sibling);
  }
  ASSERT_FALSE(branch.empty());
  crypto::hash merkle_root{};
  ASSERT_TRUE(crypto::tree_branch_hash(miner_hash.data, reinterpret_cast<const char (*)[32]>(branch.data()), branch.size(), 0, merkle_root.data));
  ASSERT_GE(hashing_blob.size(), sizeof(merkle_root) + 1);
  ASSERT_EQ(static_cast<uint8_t>(hashing_blob.back()), 3u);
  ASSERT_EQ(std::memcmp(merkle_root.data, hashing_blob.data() + hashing_blob.size() - sizeof(merkle_root) - 1, sizeof(merkle_root)), 0);

  qwertycoin::epose::chainlock_v1 lock;
  ASSERT_TRUE(qwertycoin::epose::parse_chainlock_v1(lock_blob, lock));
  ASSERT_EQ(lock.height, data.get_chainlock.height);
  ASSERT_EQ(lock.block_hash, block_id);
  ASSERT_EQ(lock.epoch, data.get_chainlock.quorum_epoch);
  ASSERT_GE(lock.signer_count(), data.get_chainlock.threshold);
  std::vector<crypto::public_key> snapshot;
  std::vector<crypto::public_key> qualified;
  for (const std::string &hex : data.get_chainlock_quorum_inputs.snapshot_members)
  {
    crypto::public_key key{};
    ASSERT_TRUE(epee::string_tools::hex_to_pod(hex, key));
    snapshot.push_back(key);
  }
  for (const std::string &hex : data.get_chainlock_quorum_inputs.qualified_members)
  {
    crypto::public_key key{};
    ASSERT_TRUE(epee::string_tools::hex_to_pod(hex, key));
    qualified.push_back(key);
  }
  const auto candidates = qwertycoin::epose::chainlock_candidates_v1(snapshot, qualified);
  const qwertycoin::epose::chainlock_context_v1 context{
      static_cast<cryptonote::network_type>(data.network), genesis_hash, parameter_set_hash};
  const auto quorum = qwertycoin::epose::select_chainlock_quorum_v1(lock.epoch, anchor_hash, candidates, context);
  ASSERT_EQ(data.get_chainlock_quorum_inputs.epoch, lock.epoch);
  ASSERT_NE(data.get_chainlock_quorum_inputs.epoch_length, 0u);
  ASSERT_EQ(lock.epoch, lock.height / data.get_chainlock_quorum_inputs.epoch_length);
  ASSERT_EQ(data.get_chainlock_quorum_inputs.genesis_hash, data.genesis_hash);
  ASSERT_EQ(data.get_chainlock_quorum_inputs.parameter_set_hash, data.parameter_set_hash);
  ASSERT_EQ(data.get_chainlock_quorum_inputs.epoch_start_height,
      lock.epoch * data.get_chainlock_quorum_inputs.epoch_length);
  ASSERT_EQ(data.get_chainlock_quorum_inputs.snapshot_height + data.get_chainlock_quorum_inputs.anchor_depth,
      data.get_chainlock_quorum_inputs.epoch_start_height);
  ASSERT_EQ(data.get_chainlock_quorum_inputs.qualification_height + 1,
      data.get_chainlock_quorum_inputs.snapshot_height);
  ASSERT_EQ(data.get_chainlock_quorum_inputs.previous_epoch_start_block_id.size(), 64u);
  ASSERT_EQ(data.get_chainlock_quorum_inputs.epoch_start_block_id.size(), 64u);
  ASSERT_EQ(data.get_chainlock_quorum_inputs.qualification_block_id.size(), 64u);
  ASSERT_EQ(data.get_chainlock_quorum_inputs.snapshot_total, snapshot.size());
  ASSERT_EQ(data.get_chainlock_quorum_inputs.qualification_total, qualified.size());
  ASSERT_EQ(data.get_chainlock_quorum_inputs.network_id, "515743324d41494e3230323652303102");
  ASSERT_EQ(data.get_chainlock_quorum_inputs.test_override, data.network == cryptonote::FAKECHAIN);
  ASSERT_EQ(quorum.threshold(), data.get_chainlock.threshold);
  ASSERT_EQ(quorum.members.size(), data.get_chainlock.quorum.size());
  for (size_t index = 0; index < quorum.members.size(); ++index)
    ASSERT_EQ(epee::string_tools::pod_to_hex(quorum.members[index]), data.get_chainlock.quorum[index]);
  ASSERT_TRUE(qwertycoin::epose::chainlock_complete_v1(lock, quorum));
  ASSERT_TRUE(qwertycoin::epose::verify_chainlock_v1(lock, quorum, context, true));

  cryptonote::block carrier;
  ASSERT_TRUE(cryptonote::parse_and_validate_block_from_blob(carrier_blob, carrier));
  ASSERT_EQ(cryptonote::get_block_height(carrier), data.get_chainlock.lock_carrier_height);
  ASSERT_EQ(epee::string_tools::pod_to_hex(cryptonote::get_block_hash(carrier)), data.carrier_block_id);
  cryptonote::blobdata carried_lock;
  ASSERT_EQ(cryptonote::get_tx_chainlock(carrier.miner_tx.extra, carried_lock), 1u);
  ASSERT_EQ(carried_lock, lock_blob);

  vm::state_proof proof;
  for (const cryptonote::vm_state_proof_step &wire : data.get_vm_state_proof.steps)
  {
    vm::state_proof::step step{wire.bit, crypto::null_hash};
    ASSERT_TRUE(epee::string_tools::hex_to_pod(wire.sibling, step.sibling));
    proof.steps.push_back(step);
  }
  if (!data.get_vm_state_proof.leaf_path.empty())
  {
    crypto::hash leaf_path{};
    ASSERT_TRUE(epee::string_tools::hex_to_pod(data.get_vm_state_proof.leaf_path, leaf_path));
    proof.leaf_path = leaf_path;
  }
  ASSERT_TRUE(epee::string_tools::hex_to_pod(data.get_vm_state_proof.leaf_value, proof.leaf_value));
  cryptonote::blobdata value;
  ASSERT_TRUE(epee::string_tools::parse_hexstr_to_binbuff(data.get_vm_state_proof.value, value));
  ASSERT_TRUE(vm::state_tree::verify_proof(root, key, data.get_vm_state_proof.found ? boost::optional<std::string>(value) : boost::none, proof));
}
