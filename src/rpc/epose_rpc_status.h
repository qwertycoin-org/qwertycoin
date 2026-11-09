#pragma once

#include <algorithm>
#include <vector>

#include "string_tools.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "cryptonote_core/blockchain.h"
#include "epose/diagnostics_v2.h"
#include "epose/transport_committee_v2.h"

namespace cryptonote
{
  extern const char EPOSE_HISTORY_OUTSIDE_RETAINED_WINDOW[];

  inline const char *epose_qualification_rpc_status(bool pruned)
  {
    return pruned ? "Qualification pruned" : CORE_RPC_STATUS_OK;
  }

  inline const std::vector<crypto::public_key> &epose_epoch_qualified_nodes(
      const Blockchain::epose_rpc_snapshot &snapshot)
  {
    return snapshot.qualified_nodes;
  }

  inline epose_service_node_entry make_epose_service_node_entry(
      const qwertycoin::epose::identity_descriptor_v2 &identity,
      const Blockchain::epose_rpc_snapshot &snapshot,
      const std::vector<crypto::public_key> &qualified_nodes)
  {
    epose_service_node_entry entry{};
    entry.identity_id = epee::string_tools::pod_to_hex(identity.identity_id);
    entry.service_public_key = epee::string_tools::pod_to_hex(identity.service_public_key);
    entry.operator_authorization_public_key =
        epee::string_tools::pod_to_hex(identity.operator_authorization_public_key);
    entry.reward_view_public_key = epee::string_tools::pod_to_hex(identity.reward_address.m_view_public_key);
    entry.reward_spend_public_key = epee::string_tools::pod_to_hex(identity.reward_address.m_spend_public_key);
    entry.endpoint_commitment = epee::string_tools::pod_to_hex(identity.endpoint_descriptor_hash);
    entry.admission_hash.clear();
    entry.descriptor_sequence = identity.sequence;
    entry.effective_epoch = identity.effective_epoch;
    entry.registration_epoch = identity.effective_epoch;
    entry.expiry_epoch = identity.expiry_epoch;
    entry.active = identity.effective_epoch <= snapshot.epoch
        && snapshot.epoch < identity.expiry_epoch;
    entry.endpoint_transport = identity.endpoint_transport;
    entry.dial_capabilities = identity.dial_capabilities;
    entry.dns_address_families = identity.dns_address_families;
    if (snapshot.frozen_membership_available)
    {
      const auto member = std::find_if(
          snapshot.frozen_membership.members.begin(),
          snapshot.frozen_membership.members.end(),
          [&](const auto &candidate) {
            return candidate.service_public_key == identity.service_public_key;
          });
      if (member != snapshot.frozen_membership.members.end())
      {
        const size_t compatible = qwertycoin::epose::compatible_committee_size_v2(
            snapshot.frozen_membership, *member, snapshot.committee_size);
        const bool judged = qwertycoin::epose::compatible_committee_judged_v2(
            snapshot.frozen_membership, compatible, snapshot.min_compatible_committee);
        entry.rounds_judged = judged ? snapshot.round_count : 0;
        entry.rounds_unjudged = judged ? 0 : snapshot.round_count;
      }
    }
    entry.qualified = std::find(qualified_nodes.begin(), qualified_nodes.end(),
        identity.service_public_key) != qualified_nodes.end();
    return entry;
  }

  inline void set_epose_diagnostic_transport_v2(
      COMMAND_RPC_GET_EPOSE_DIAGNOSTICS::response &response,
      const qwertycoin::epose::qualification_diagnostics_v2 &qualification)
  {
    response.rounds_judged = qualification.rounds_judged;
    response.rounds_unjudged = qualification.rounds_unjudged;
    response.dial_capabilities = qualification.dial_capabilities;
    response.dns_address_families = qualification.dns_address_families;
  }

  template<typename Response>
  void epose_set_qualification_context(
      Response &response, uint64_t epoch, bool available)
  {
    response.qualification_epoch = epoch;
    response.qualification_available = available;
  }

  template<typename Response>
  bool epose_rpc_busy(bool ready, Response &response)
  {
    if (ready)
      return false;
    response.status = CORE_RPC_STATUS_BUSY;
    return true;
  }

  template<typename Response>
  bool epose_rpc_busy(const Blockchain::epose_rpc_snapshot &snapshot,
      Response &response)
  {
    return epose_rpc_busy(snapshot.ready, response);
  }

  template<typename Response>
  bool epose_service_rewards_busy(uint64_t requested_height, uint64_t resolved_height,
      uint64_t current_height, bool state_at_tip, Response &response)
  {
    return epose_rpc_busy(state_at_tip
        && (requested_height != 0 || resolved_height == current_height), response);
  }

  inline void epose_set_reward_preview(
      const qwertycoin::epose::coordinator_reward_plan_v2 &plan,
      COMMAND_RPC_GET_SERVICE_REWARDS::response &response)
  {
    response.preview_available = true;
    response.service_reward_active = plan.has_service_payee;
    if (plan.has_service_payee)
    {
      response.expected_payee_service_public_key =
          epee::string_tools::pod_to_hex(plan.expectation.payee_service_public_key);
      response.expected_reward_view_public_key =
          epee::string_tools::pod_to_hex(plan.expectation.reward_address.m_view_public_key);
      response.expected_reward_spend_public_key =
          epee::string_tools::pod_to_hex(plan.expectation.reward_address.m_spend_public_key);
    }
  }
}
