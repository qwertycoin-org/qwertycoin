// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/chain_state.h"

#include <algorithm>
#include <cstring>

#include "epose/service_epoch.h"

namespace
{
  crypto::hash hash_blob(const std::string &blob)
  {
    return crypto::cn_fast_hash(blob.data(), blob.size());
  }

  bool public_key_equal(const crypto::public_key &left, const crypto::public_key &right)
  {
    return std::memcmp(&left, &right, sizeof(left)) == 0;
  }

  bool same_attestation_vote(
      const qwertycoin::epose::service_attestation &left,
      const qwertycoin::epose::service_attestation &right)
  {
    return left.epoch == right.epoch
        && public_key_equal(left.verifier_public_key, right.verifier_public_key)
        && public_key_equal(left.subject_public_key, right.subject_public_key);
  }

  bool has_active_registration(
      const qwertycoin::epose::service_registry_state &registry,
      const crypto::public_key &service_public_key,
      uint64_t epoch)
  {
    const std::vector<qwertycoin::epose::service_node_identity> active = registry.active_nodes(epoch);
    return std::find_if(active.begin(), active.end(), [&](const qwertycoin::epose::service_node_identity &identity) {
      return public_key_equal(identity.service_public_key, service_public_key);
    }) != active.end();
  }

  bool is_selected_verifier(
      const qwertycoin::epose::service_registry_state &registry,
      const qwertycoin::epose::service_attestation &attestation,
      cryptonote::network_type nettype,
      const crypto::hash &epoch_seed)
  {
    const auto assignments = qwertycoin::epose::select_verifiers(
        registry.active_nodes(attestation.epoch),
        attestation.subject_public_key,
        nettype,
        attestation.epoch,
        epoch_seed,
        qwertycoin::epose::EPOSE_VERIFIER_COMMITTEE_SIZE);

    return std::find_if(assignments.begin(), assignments.end(), [&](const qwertycoin::epose::verifier_assignment &assignment) {
      return public_key_equal(assignment.verifier_public_key, attestation.verifier_public_key);
    }) != assignments.end();
  }

  bool epoch_outside_retention(uint64_t stored_epoch, uint64_t current_epoch, uint64_t retention_epochs)
  {
    return current_epoch > stored_epoch && current_epoch - stored_epoch > retention_epochs;
  }
}

namespace qwertycoin
{
namespace epose
{
  chain_state::chain_state(cryptonote::network_type nettype, uint8_t admission_leading_zero_bits)
    : nettype_(nettype),
      admission_leading_zero_bits_(admission_leading_zero_bits)
  {
  }

  bool chain_state::apply_tx_extra(
      const std::vector<uint8_t> &tx_extra,
      const crypto::hash &previous_epoch_hash,
      tx_extra_apply_summary *summary,
      uint64_t expected_epoch)
  {
    if (summary)
      *summary = {};

    std::vector<service_node_identity> registrations;
    if (!extract_registrations_from_tx_extra(tx_extra, registrations))
      return false;

    std::vector<service_attestation> attestations;
    if (!extract_attestations_from_tx_extra(tx_extra, attestations))
      return false;

    snapshot before = make_snapshot();
    tx_extra_apply_summary local_summary{};

    for (const service_node_identity &registration : registrations)
    {
      if (expected_epoch != std::numeric_limits<uint64_t>::max() && registration.registration_epoch != expected_epoch)
      {
        restore_snapshot(before);
        return false;
      }
      if (!registry_.apply_registration(registration, nettype_, previous_epoch_hash, admission_leading_zero_bits_))
      {
        restore_snapshot(before);
        return false;
      }
      ++local_summary.registrations_applied;
    }

    for (const service_attestation &attestation : attestations)
    {
      if (expected_epoch != std::numeric_limits<uint64_t>::max() && attestation.epoch != expected_epoch)
      {
        restore_snapshot(before);
        return false;
      }
      if (!verify_attestation_signature(attestation, nettype_))
      {
        restore_snapshot(before);
        return false;
      }
      if (!has_active_registration(registry_, attestation.subject_public_key, attestation.epoch)
          || !has_active_registration(registry_, attestation.verifier_public_key, attestation.epoch))
      {
        restore_snapshot(before);
        return false;
      }
      const crypto::hash epoch_seed = calculate_epoch_seed(nettype_, attestation.epoch, previous_epoch_hash);
      const crypto::hash expected_challenge = calculate_challenge_hash(
              epoch_seed,
              attestation.subject_public_key,
              attestation.verifier_public_key,
              attestation.epoch,
              0);
      const crypto::hash expected_response = calculate_response_hash(
              expected_challenge,
              attestation.observed_tip_hash,
              attestation.subject_public_key,
              attestation.verifier_public_key,
              attestation.epoch);
      if (!is_selected_verifier(registry_, attestation, nettype_, epoch_seed)
          || attestation.challenge_hash != expected_challenge
          || attestation.response_hash != expected_response)
      {
        restore_snapshot(before);
        return false;
      }
      if (std::find_if(attestations_.begin(), attestations_.end(), [&](const service_attestation &existing) { return same_attestation_vote(existing, attestation); }) != attestations_.end())
      {
        restore_snapshot(before);
        return false;
      }
      attestations_.push_back(attestation);
      ++local_summary.attestations_applied;
    }

    if (summary)
      *summary = local_summary;

    if (expected_epoch != std::numeric_limits<uint64_t>::max())
      prune_expired(expected_epoch);

    return true;
  }

  bool chain_state::apply_transaction(
      const cryptonote::transaction &tx,
      const crypto::hash &previous_epoch_hash,
      tx_extra_apply_summary *summary,
      uint64_t expected_epoch)
  {
    return apply_tx_extra(tx.extra, previous_epoch_hash, summary, expected_epoch);
  }

  bool chain_state::apply_transactions(
      const std::vector<cryptonote::transaction> &txs,
      const crypto::hash &previous_epoch_hash,
      transaction_apply_summary *summary,
      uint64_t expected_epoch)
  {
    if (summary)
      *summary = {};

    snapshot before = make_snapshot();
    transaction_apply_summary local_summary{};

    for (const cryptonote::transaction &tx : txs)
    {
      tx_extra_apply_summary tx_summary{};
      if (!apply_transaction(tx, previous_epoch_hash, &tx_summary, expected_epoch))
      {
        restore_snapshot(before);
        return false;
      }

      ++local_summary.transactions_scanned;
      local_summary.registrations_applied += tx_summary.registrations_applied;
      local_summary.attestations_applied += tx_summary.attestations_applied;
    }

    if (summary)
      *summary = local_summary;

    return true;
  }

  std::vector<crypto::public_key> chain_state::qualified_service_nodes(
      uint64_t epoch) const
  {
    return qualified_nodes(registry_.active_nodes(epoch), attestations_, epoch, nettype_);
  }

  void chain_state::prune_expired(uint64_t current_epoch, uint64_t retention_epochs)
  {
    registry_.prune_expired(current_epoch, retention_epochs);
    attestations_.erase(std::remove_if(attestations_.begin(), attestations_.end(), [&](const service_attestation &attestation) {
      return epoch_outside_retention(attestation.epoch, current_epoch, retention_epochs);
    }), attestations_.end());
  }

  chain_state::snapshot chain_state::make_snapshot() const
  {
    return {registry_, attestations_};
  }

  void chain_state::restore_snapshot(const snapshot &state)
  {
    registry_ = state.registry;
    attestations_ = state.attestations;
  }

  const service_registry_state &chain_state::registry() const
  {
    return registry_;
  }

  const std::vector<service_attestation> &chain_state::attestations() const
  {
    return attestations_;
  }

  crypto::hash chain_state::state_hash() const
  {
    std::vector<std::string> registration_blobs;
    registration_blobs.reserve(registry_.registrations().size());
    for (const service_node_identity &identity : registry_.registrations())
      registration_blobs.push_back(serialize_identity(identity));
    std::sort(registration_blobs.begin(), registration_blobs.end());

    std::vector<std::string> attestation_blobs;
    attestation_blobs.reserve(attestations_.size());
    for (const service_attestation &attestation : attestations_)
      attestation_blobs.push_back(serialize_attestation(attestation));
    std::sort(attestation_blobs.begin(), attestation_blobs.end());

    std::string blob("QWC_EPOSE_STATE_V1");
    const uint8_t nettype = static_cast<uint8_t>(nettype_);
    blob.append(reinterpret_cast<const char *>(&nettype), sizeof(nettype));
    blob.append(reinterpret_cast<const char *>(&admission_leading_zero_bits_), sizeof(admission_leading_zero_bits_));
    for (const std::string &registration_blob : registration_blobs)
      blob.append(registration_blob);
    blob.append("QWC_EPOSE_ATTESTATIONS_V1");
    for (const std::string &attestation_blob : attestation_blobs)
      blob.append(attestation_blob);
    return hash_blob(blob);
  }
} // namespace epose
} // namespace qwertycoin
