// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include <boost/optional.hpp>

#include "crypto/crypto.h"
#include "epose/memory_usage_v2.h"
#include "crypto/hash.h"
#include "cryptonote_config.h"
#include "epose/verification_v2.h"

struct membership_pipeline_test_access;

namespace qwertycoin
{
namespace epose
{
  class state_image_writer_v2;
  class state_image_reader_v2;
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
    bool epoch_of(uint64_t height, uint64_t &epoch) const;
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
    // Consensus ceiling for distinct effective identities in one epoch.
    size_t max_active_population = std::numeric_limits<size_t>::max();
    size_t min_compatible_committee = 5;

    bool valid() const;
  };

  // Consensus quorum for the committee that can actually be formed after
  // excluding the subject.  This is ceil(2n/3) without overflow-prone
  // multiplication and deliberately scales during the small-network
  // bootstrap phase.
  size_t required_receipts_for_committee_size_v2(size_t actual_committee_size);

  // Returns the inclusive block-height window in which a receipt for one
  // service round may be carried by a block. Round zero is anchored before
  // the service epoch. Later rounds are anchored at their configured offset,
  // so their first eligible carrier is the following block.
  bool receipt_inclusion_window_v2(
      const epoch_timing_v2 &timing,
      const committee_policy_v2 &policy,
      uint64_t epoch,
      uint64_t round,
      uint64_t &first_height,
      uint64_t &last_height);

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
    uint8_t descriptor_version = 1;
    uint8_t endpoint_transport = 1;
    uint8_t dial_capabilities = 1;
    uint8_t dns_address_families = 0;
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
    bool transport_aware = false;
    bool pruned = false;
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
    bool pruned = false;
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
    population_limit_exceeded,
    snapshot_missing,
    snapshot_already_frozen,
    wrong_boundary,
    receipt_not_prevalidated,
    subject_not_in_snapshot,
    verifier_not_in_snapshot,
    verifier_not_selected,
    receipt_slot_conflict,
    invalid_service_kind,
    qualification_already_closed,
    outside_retained_window
  };

  class membership_pipeline_v2
  {
    struct epoch_bucket;

  public:
    membership_pipeline_v2(
        cryptonote::network_type nettype,
        const crypto::hash &genesis_hash,
        const crypto::hash &parameter_set_hash,
        const epoch_timing_v2 &timing,
        const admission_policy_v2 &admission_policy,
        const committee_policy_v2 &policy);

    struct lease_change
    {
      uint64_t epoch = 0;
      size_t index = 0;
      admission_lease_v2 lease{};
      uint64_t inclusion_height = 0;
    };
    struct size_change
    {
      uint64_t epoch = 0;
      size_t size = 0;
    };
    struct undo_journal
    {
      struct bucket_change
      {
        uint64_t epoch = 0;
        std::shared_ptr<epoch_bucket> prior;
      };
      std::vector<bucket_change> buckets;
      std::vector<size_change> appended_leases;
      std::vector<size_change> appended_receipts;
      std::vector<uint64_t> created_buckets;
      size_t prior_receipt_count = 0;
      uint64_t prior_oldest_retained_epoch = 0;
      uint64_t prior_archive_tip_epoch = 0;
      uint64_t prior_next_prune_epoch = 0;
      bool prior_window_active = false;
      std::vector<lease_change> replaced_leases;
      std::vector<uint64_t> inserted_snapshots;
      std::vector<uint64_t> inserted_qualifications;
      std::vector<bucket_change> archived_buckets;
      std::vector<bucket_change> pruned_buckets;
      std::vector<uint64_t> inserted_carriers;
      std::vector<uint64_t> evicted_carriers;
    };
    state_memory_usage_v2 memory_usage() const;
    undo_journal checkpoint() const;
    void rollback(const undo_journal &undo) noexcept;

    bool valid() const;
    const epoch_timing_v2 &timing() const;
    const committee_policy_v2 &committee_policy() const;

    pipeline_status_v2 apply_admission(
        const admission_lease_v2 &lease,
        const admission_context_v2 &context,
        uint64_t inclusion_height,
        verification_counters_v2 *counters,
        undo_journal *undo);

    pipeline_status_v2 freeze_membership(
        uint64_t epoch,
        uint64_t height,
        const crypto::hash &anchor_hash,
        undo_journal *undo);

    pipeline_status_v2 freeze_membership(
        uint64_t epoch,
        uint64_t height,
        const crypto::hash &anchor_hash,
        const std::vector<frozen_member_v2> &authorized_members,
        undo_journal *undo);

    pipeline_status_v2 apply_authenticated_receipt(
        const authenticated_service_receipt_v2 &receipt,
        const receipt_context_v2 &context,
        uint64_t inclusion_height,
        const crypto::hash &canonical_round_anchor_hash,
        verification_counters_v2 *counters = nullptr);

    pipeline_status_v2 close_qualification(
        uint64_t epoch, uint64_t height, undo_journal *undo);

    const membership_snapshot_v2 *snapshot(uint64_t epoch) const;
    const qualification_set_v2 *qualification(uint64_t epoch) const;
    bool qualification_pruned(uint64_t epoch) const;
    bool reconstruct_epoch(uint64_t epoch, membership_snapshot_v2 &snapshot,
        qualification_set_v2 *qualification) const;
    bool epoch_recorded(uint64_t epoch, bool require_qualification) const;
    bool retain_carrier(uint64_t epoch, undo_journal *undo = nullptr);
    bool carrier_retained(uint64_t epoch) const;
    bool get_snapshot(uint64_t epoch, membership_snapshot_v2 &snapshot,
        bool allow_reconstruct = false) const;
    bool get_qualification(uint64_t epoch, qualification_set_v2 &qualification,
        bool allow_reconstruct = false) const;
    bool needs_pruning(uint64_t current_epoch) const;
    void prune_history(uint64_t current_epoch, undo_journal *undo = nullptr);
    pipeline_status_v2 read_epoch(uint64_t epoch) const;
    uint64_t oldest_retained_epoch() const;
    bool window_active() const;
    void activate_transport_window();
    bool fold_epoch(uint64_t epoch, uint64_t tip_epoch, crypto::hash &digest, undo_journal *undo);
    void record_receipt_size(uint64_t epoch, undo_journal *undo);
    pipeline_status_v2 epoch_transcript(uint64_t epoch, std::string &out) const;
    pipeline_status_v2 epoch_digest(uint64_t epoch, crypto::hash &digest) const;
    size_t receipt_count() const;
    size_t state_bytes() const;
    bool has_admission(const crypto::hash &identity_id, uint64_t target_epoch) const;
    bool contains_admission(const admission_lease_v2 &lease) const;
    bool has_receipt_slot(
        uint64_t epoch,
        uint64_t round,
        const crypto::public_key &subject_public_key,
        const crypto::public_key &verifier_public_key) const;
    std::vector<size_t> receipt_coverage(
        uint64_t epoch,
        const crypto::public_key &subject_public_key) const;

    std::vector<verifier_assignment_v2> committee(
        uint64_t epoch,
        uint64_t round,
        const crypto::public_key &subject_public_key,
        const crypto::hash &round_anchor_hash) const;

    crypto::hash state_hash() const;
    bool write_image(state_image_writer_v2 &writer) const;
    bool read_image(state_image_reader_v2 &reader, uint64_t tip_epoch = 0);

  private:
    friend struct ::membership_pipeline_test_access;

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

    struct epoch_bucket
    {
      std::vector<stored_lease> leases;
      boost::optional<membership_snapshot_v2> snapshot;
      std::vector<stored_receipt> receipts;
      boost::optional<qualification_set_v2> qualification;
      std::vector<crypto::hash> snapshot_lease_hashes;
      std::map<std::string, size_t> lease_by_identity;
      std::map<std::string, size_t> lease_by_service;
      std::map<std::tuple<uint64_t, std::string, std::string>, size_t> receipt_by_slot;
      std::map<std::pair<uint64_t, std::string>, size_t> receipt_coverage;
      mutable std::string lease_transcript;
      mutable std::string receipt_transcript;
      mutable bool lease_transcript_valid = false;
      mutable bool receipt_transcript_valid = false;
      mutable boost::optional<crypto::hash> digest_cache;
    };

    cryptonote::network_type nettype_;
    crypto::hash genesis_hash_{};
    crypto::hash parameter_set_hash_{};
    epoch_timing_v2 timing_{};
    admission_policy_v2 admission_policy_{};
    committee_policy_v2 policy_{};
    bool valid_ = false;
    std::map<uint64_t, std::shared_ptr<epoch_bucket>> epochs_;
    std::map<uint64_t, std::shared_ptr<epoch_bucket>> historical_epochs_;
    std::set<uint64_t> carrier_epochs_;
    size_t receipt_count_ = 0;
    uint64_t oldest_retained_epoch_ = 0;
    uint64_t archive_tip_epoch_ = 0;
    uint64_t next_prune_epoch_ = 0;
    bool window_active_ = false;
    mutable boost::optional<crypto::hash> state_hash_cache_;

    struct prune_targets
    {
      bool snapshot;
      bool qualification;
    };

    prune_targets prune_targets_of(uint64_t epoch, const epoch_bucket &selected) const;
    const epoch_bucket *bucket(uint64_t epoch) const;
    const epoch_bucket *recorded_bucket(uint64_t epoch) const;
    epoch_bucket &mutable_bucket(uint64_t epoch, undo_journal *undo);
    std::vector<const stored_lease *> sorted_leases(const epoch_bucket &selected) const;
    std::vector<const stored_receipt *> sorted_receipts(const epoch_bucket &selected) const;
    const std::string &lease_transcript(const epoch_bucket &selected) const;
    const std::string &receipt_transcript(const epoch_bucket &selected) const;
    crypto::hash snapshot_hash(const membership_snapshot_v2 &snapshot) const;
    crypto::hash qualification_hash(const qualification_set_v2 &qualification) const;

    pipeline_status_v2 freeze_membership_impl(
        uint64_t epoch,
        uint64_t height,
        const crypto::hash &anchor_hash,
        const std::vector<frozen_member_v2> *authorized_members,
        undo_journal *undo);
  };
} // namespace epose
} // namespace qwertycoin
