// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "cryptonote_config.h"
#include "epose/verification_v2.h"

namespace qwertycoin
{
namespace epose
{
  struct authenticated_service_receipt_v2;
  struct receipt_context_v2;

  constexpr uint8_t EPOSE_PROTOCOL_VERSION_V2 = 2;

  struct epoch_timing_v2
  {
    uint64_t activation_height = 0;
    uint64_t epoch_length = 720;
    uint64_t anchor_depth = 60;

    bool valid() const;
    bool activation_epoch(uint64_t &epoch) const;
    bool first_service_epoch(uint64_t &epoch) const;
    bool first_payout_height(uint64_t &height) const;
    bool epoch_start(uint64_t epoch, uint64_t &height) const;
    bool epoch_end(uint64_t epoch, uint64_t &height) const;
    bool enrollment_cutoff(uint64_t epoch, uint64_t &height) const;
    bool committee_anchor(uint64_t epoch, uint64_t &height) const;
    bool evidence_deadline(uint64_t epoch, uint64_t &height) const;
  };

  struct committee_policy_v2
  {
    size_t committee_size = 0;
    size_t threshold = 0;
    uint64_t round_count = 0;
    uint64_t rounds_required = 0;
    uint8_t service_kind = 0;
    std::vector<uint64_t> round_offsets;

    bool valid() const;
  };

  // CO-02 deliberately freezes opaque commitments instead of guessing the
  // descriptor, reward-proof, or lifecycle wire formats owned by later COs.
  struct frozen_member_v2
  {
    crypto::public_key service_public_key{};
    crypto::hash identity_id{};
    crypto::public_key operator_authorization_public_key{};
    crypto::hash descriptor_hash{};
    crypto::hash reward_binding_hash{};
    crypto::hash endpoint_descriptor_hash{};
    uint64_t sequence = 0;
  };

  struct admission_lease_v2
  {
    frozen_member_v2 member{};
    uint64_t target_epoch = 0;
    uint8_t work_algorithm = 1;
    uint8_t leading_zero_bits = 0;
    uint64_t admission_context_height = 0;
    crypto::hash admission_context_hash{};
    uint64_t nonce = 0;
    crypto::hash work_hash{};
    crypto::hash lease_hash{};
  };

  enum class admission_work_algorithm_v2 : uint8_t
  {
    randomx = 1
  };

  struct admission_context_v2
  {
    cryptonote::network_type nettype = cryptonote::UNDEFINED;
    crypto::hash genesis_hash{};
    crypto::hash parameter_set_hash{};
    uint64_t height = 0;
    crypto::hash block_hash{};
  };

  struct admission_policy_v2
  {
    admission_work_algorithm_v2 algorithm = admission_work_algorithm_v2::randomx;
    uint8_t leading_zero_bits = 0;
    // EPoSE v2 fixes the work context at start(target_epoch - 1). This keeps
    // admission work independent of the later committee-selection anchor.
    uint64_t context_epoch_offset = 1;

    bool valid() const;
  };

  crypto::hash calculate_admission_work_v2(
      const admission_lease_v2 &lease,
      const admission_context_v2 &context);
  crypto::hash calculate_admission_lease_hash_v2(
      const admission_lease_v2 &lease,
      const admission_context_v2 &context);
  bool admission_work_meets_target_v2(
      const crypto::hash &work_hash,
      uint8_t leading_zero_bits);
  bool validate_admission_lease_v2(
      const admission_lease_v2 &lease,
      const admission_context_v2 &context,
      const admission_policy_v2 &policy,
      verification_counters_v2 *counters = nullptr);

  struct membership_snapshot_v2
  {
    uint64_t epoch = 0;
    uint64_t cutoff_height = 0;
    uint64_t anchor_height = 0;
    crypto::hash anchor_hash{};
    std::vector<frozen_member_v2> members;
    crypto::hash snapshot_hash{};
  };

  struct verifier_assignment_v2
  {
    crypto::public_key verifier_public_key{};
    crypto::hash selection_score{};
  };

  struct qualification_set_v2
  {
    uint64_t epoch = 0;
    uint64_t closed_height = 0;
    crypto::hash snapshot_hash{};
    std::vector<crypto::public_key> qualified_nodes;
    crypto::hash qualification_hash{};
  };

  enum class pipeline_status_v2
  {
    accepted,
    idempotent_duplicate,
    invalid_configuration,
    before_activation,
    invalid_epoch,
    too_late,
    invalid_member,
    conflicting_record,
    snapshot_missing,
    snapshot_already_frozen,
    wrong_boundary,
    receipt_not_prevalidated,
    subject_not_in_snapshot,
    verifier_not_in_snapshot,
    verifier_not_selected,
    receipt_slot_conflict,
    invalid_service_kind,
    qualification_already_closed
  };

  class membership_pipeline_v2
  {
  public:
    membership_pipeline_v2(
        cryptonote::network_type nettype,
        const crypto::hash &genesis_hash,
        const crypto::hash &parameter_set_hash,
        const epoch_timing_v2 &timing,
        const admission_policy_v2 &admission_policy,
        const committee_policy_v2 &policy);

    bool valid() const;

    pipeline_status_v2 apply_admission(
        const admission_lease_v2 &lease,
        const admission_context_v2 &context,
        uint64_t inclusion_height,
        verification_counters_v2 *counters = nullptr);

    pipeline_status_v2 freeze_membership(
        uint64_t epoch,
        uint64_t height,
        const crypto::hash &anchor_hash);

    pipeline_status_v2 freeze_membership(
        uint64_t epoch,
        uint64_t height,
        const crypto::hash &anchor_hash,
        const std::vector<frozen_member_v2> &authorized_members);

    pipeline_status_v2 apply_authenticated_receipt(
        const authenticated_service_receipt_v2 &receipt,
        const receipt_context_v2 &context,
        uint64_t inclusion_height,
        const crypto::hash &canonical_round_anchor_hash,
        verification_counters_v2 *counters = nullptr);

    pipeline_status_v2 close_qualification(uint64_t epoch, uint64_t height);

    const membership_snapshot_v2 *snapshot(uint64_t epoch) const;
    const qualification_set_v2 *qualification(uint64_t epoch) const;
    size_t receipt_count() const;

    std::vector<verifier_assignment_v2> committee(
        uint64_t epoch,
        uint64_t round,
        const crypto::public_key &subject_public_key,
        const crypto::hash &round_anchor_hash) const;

    crypto::hash state_hash() const;

  private:
    struct stored_lease
    {
      admission_lease_v2 lease;
      uint64_t inclusion_height = 0;
    };

    struct stored_receipt
    {
      uint64_t epoch = 0;
      uint64_t round = 0;
      uint8_t service_kind = 0;
      crypto::public_key subject_public_key{};
      crypto::public_key verifier_public_key{};
      crypto::hash receipt_hash{};
      uint64_t inclusion_height = 0;
    };

    cryptonote::network_type nettype_;
    crypto::hash genesis_hash_{};
    crypto::hash parameter_set_hash_{};
    epoch_timing_v2 timing_{};
    admission_policy_v2 admission_policy_{};
    committee_policy_v2 policy_{};
    bool valid_ = false;
    std::vector<stored_lease> leases_;
    std::map<uint64_t, membership_snapshot_v2> snapshots_;
    std::vector<stored_receipt> receipts_;
    std::map<uint64_t, qualification_set_v2> qualifications_;

    pipeline_status_v2 freeze_membership_impl(
        uint64_t epoch,
        uint64_t height,
        const crypto::hash &anchor_hash,
        const std::vector<frozen_member_v2> *authorized_members);
  };
} // namespace epose
} // namespace qwertycoin
