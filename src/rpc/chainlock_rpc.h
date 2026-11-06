#pragma once

#include <algorithm>

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_core/blockchain.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "rpc/rpc_payment_costs.h"
#include "rpc/vm_dry_run_rpc.h"
#include "string_tools.h"

namespace cryptonote
{
  constexpr uint64_t RESTRICTED_VM_PROOF_MAX_DEPTH = 64;
  constexpr const char* HISTORICAL_VM_PROOF_BUSY_STATUS = "Historical VM proof busy";
  constexpr const char* RESTRICTED_VM_PROOF_DEPTH_STATUS = "Height exceeds restricted VM proof depth";
  constexpr const char* VM_STATE_REBUILDING_STATUS = "Contract state is rebuilding";
  constexpr const char* VM_PROOF_FAILED_STATUS = "Failed to build VM state proof";
  constexpr const char* CHAINLOCK_PROOF_FAILED_STATUS = "Failed to build ChainLock proof";
  constexpr const char* QUORUM_INPUTS_UNAVAILABLE_STATUS = "Quorum inputs unavailable";

  inline bool chainlock_reconstruction_allowed(bool restricted_rpc, bool has_context)
  {
    return !restricted_rpc && has_context;
  }

  inline bool chainlock_quorum_inputs_allowed(bool reconstruct, bool reconstruction_denied)
  {
    return !reconstruct || !reconstruction_denied;
  }

  inline uint64_t vm_proof_depth(boost::optional<uint64_t> requested_height, uint64_t chain_height)
  {
    return !requested_height || chain_height == 0 || *requested_height >= chain_height - 1
        ? 0 : chain_height - 1 - *requested_height;
  }

  inline double vm_proof_cost(boost::optional<uint64_t> requested_height, uint64_t chain_height)
  {
    return std::max<uint64_t>(1, vm_proof_depth(requested_height, chain_height)) * COST_PER_BLOCK;
  }

  inline bool vm_proof_outside_retention(boost::optional<uint64_t> requested_height, uint64_t chain_height)
  {
    return vm_proof_depth(requested_height, chain_height) > QWC_VM_UNDO_RETENTION_BLOCKS;
  }

  inline bool vm_proof_depth_allowed(boost::optional<uint64_t> requested_height, uint64_t chain_height, bool restricted_without_payment)
  {
    return !restricted_without_payment
        || vm_proof_depth(requested_height, chain_height) <= RESTRICTED_VM_PROOF_MAX_DEPTH;
  }

  inline const char* chainlock_rpc_status(chainlock_proof_result result)
  {
    switch (result)
    {
      case chainlock_proof_result::ok: return CORE_RPC_STATUS_OK;
      case chainlock_proof_result::no_lock: return "No lock yet";
      case chainlock_proof_result::height_not_locked: return "Height not locked";
      case chainlock_proof_result::rebuilding: return VM_STATE_REBUILDING_STATUS;
    }
    return "Invalid ChainLock result";
  }

  inline const char* vm_proof_rpc_status(vm_proof_result result)
  {
    switch (result)
    {
      case vm_proof_result::ok: return CORE_RPC_STATUS_OK;
      case vm_proof_result::busy: return HISTORICAL_VM_PROOF_BUSY_STATUS;
      case vm_proof_result::rebuilding: return VM_STATE_REBUILDING_STATUS;
      case vm_proof_result::outside_retention: return "Height outside VM undo retention";
      case vm_proof_result::unavailable: return "Height unavailable";
    }
    return "Invalid VM proof result";
  }

  inline void fill_chainlock_response(const chainlock_proof &proof, COMMAND_RPC_GET_CHAINLOCK::response &res)
  {
    res.height = proof.height;
    res.block_id = epee::string_tools::pod_to_hex(proof.block_id);
    res.block_hashing_blob = epee::string_tools::buff_to_hex_nodelimer(get_block_hashing_blob(proof.locked_block));
    res.miner_tx_blob = epee::string_tools::buff_to_hex_nodelimer(tx_to_blob(proof.locked_block.miner_tx));
    res.miner_tx_branch.clear();
    for (const crypto::hash &hash : proof.miner_tx_branch)
      res.miner_tx_branch.push_back(epee::string_tools::pod_to_hex(hash));
    res.vm_state_root = epee::string_tools::pod_to_hex(proof.vm_state_root);
    res.lock_blob = epee::string_tools::buff_to_hex_nodelimer(proof.lock_blob);
    res.lock_carrier_height = proof.lock_carrier_height;
    res.quorum_epoch = proof.quorum.epoch;
    res.quorum.clear();
    for (const crypto::public_key &member : proof.quorum.members)
      res.quorum.push_back(epee::string_tools::pod_to_hex(member));
    res.threshold = proof.quorum.threshold();
    res.status = CORE_RPC_STATUS_OK;
  }

  inline void fill_chainlock_quorum_inputs_response(const chainlock_quorum_inputs &inputs,
      uint64_t offset, COMMAND_RPC_GET_CHAINLOCK_QUORUM_INPUTS::response &res)
  {
    res.epoch = inputs.epoch;
    res.offset = offset;
    res.snapshot_total = inputs.snapshot_total;
    res.qualification_total = inputs.qualification_total;
    res.activation_height = inputs.timing.activation_height;
    res.epoch_length = inputs.timing.epoch_length;
    res.anchor_depth = inputs.timing.anchor_depth;
    res.previous_epoch_start_height = inputs.previous_epoch_start_height;
    res.previous_epoch_start_block_id = epee::string_tools::pod_to_hex(inputs.previous_epoch_start_block_id);
    res.epoch_start_height = inputs.epoch_start_height;
    res.epoch_start_block_id = epee::string_tools::pod_to_hex(inputs.epoch_start_block_id);
    res.snapshot_height = inputs.snapshot_height;
    res.snapshot_block_id = epee::string_tools::pod_to_hex(inputs.snapshot_block_id);
    res.snapshot_hash = epee::string_tools::pod_to_hex(inputs.snapshot_hash);
    res.qualification_height = inputs.qualification_height;
    res.qualification_block_id = epee::string_tools::pod_to_hex(inputs.qualification_block_id);
    res.qualification_hash = epee::string_tools::pod_to_hex(inputs.qualification_hash);
    const auto &network_id = cryptonote::get_config(inputs.context.nettype).NETWORK_ID;
    res.network_id = epee::string_tools::buff_to_hex_nodelimer(std::string(network_id.begin(), network_id.end()));
    res.genesis_hash = epee::string_tools::pod_to_hex(inputs.context.genesis_hash);
    res.parameter_set_hash = epee::string_tools::pod_to_hex(inputs.context.parameter_set_hash);
    const auto append_page = [](const std::vector<crypto::public_key> &keys, std::vector<std::string> &page) {
      page.clear();
      for (const auto &key : keys)
        page.push_back(epee::string_tools::pod_to_hex(key));
    };
    append_page(inputs.snapshot_members, res.snapshot_members);
    append_page(inputs.qualified_members, res.qualified_members);
    res.test_override = inputs.test_override;
    res.status = CORE_RPC_STATUS_OK;
  }

  inline void fill_vm_state_proof_response(uint64_t height, const crypto::hash &root,
      const boost::optional<std::string> &value, const vm::state_proof &proof,
      COMMAND_RPC_GET_VM_STATE_PROOF::response &res)
  {
    res.height = height;
    res.root = epee::string_tools::pod_to_hex(root);
    res.found = static_cast<bool>(value);
    res.value = value ? epee::string_tools::buff_to_hex_nodelimer(*value) : std::string();
    res.steps.clear();
    for (const vm::state_proof::step &step : proof.steps)
      res.steps.push_back({step.bit, epee::string_tools::pod_to_hex(step.sibling)});
    res.leaf_path = proof.leaf_path ? epee::string_tools::pod_to_hex(*proof.leaf_path) : std::string();
    res.leaf_value = epee::string_tools::pod_to_hex(proof.leaf_value);
    res.status = CORE_RPC_STATUS_OK;
  }
}
