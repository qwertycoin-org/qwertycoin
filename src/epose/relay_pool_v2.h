// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "epose/envelope_v2.h"
#include "epose/membership_v2.h"
#include "epose/resource_policy_v2.h"
#include "epose/semantic_batch_v2.h"

namespace qwertycoin
{
namespace epose
{
  struct relay_policy_v2
  {
    relay_queue_limits_v2 queue{};
    relay_template_limits_v2 mining_template{};
    uint64_t receipt_slot_dedup_height =
        std::numeric_limits<uint64_t>::max();

    bool valid() const;
  };

  // Returns false until the same reviewed launch manifest that supplies the
  // consensus parameters also supplies all local relay/template bounds.
  bool compiled_relay_policy_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      relay_policy_v2 &policy);

  enum class relay_record_status_v2
  {
    accepted,
    // The exact authenticated wire envelope is already pending locally. It
    // is never forwarded by an intermediary or added to the queue again.
    idempotent_duplicate,
    // A different authenticated envelope occupies the same logical receipt
    // slot in the same canonical context. Keep the existing queue entry and
    // do not forward the randomized signature variant.
    receipt_slot_variant,
    invalid_configuration,
    invalid_record,
    payment_proof_forbidden,
    expired,
    full,
    conflict
  };

  enum class relay_ingress_status_v2
  {
    accepted,
    invalid_configuration,
    invalid_batch,
    expired,
    capacity_exhausted,
    context_unavailable
  };

  struct relay_record_selection_v2
  {
    crypto::hash id{};
    envelope_record_v2 record{};
  };

  struct relay_pool_diagnostics_v2
  {
    uint64_t queue_items = 0;
    uint64_t queue_bytes = 0;
    uint64_t exact_duplicates = 0;
    uint64_t slot_variants = 0;
    uint64_t expired_round_receipts = 0;
    uint64_t invalid_context_receipts = 0;
    uint64_t canonical_receipts = 0;
    uint64_t capacity_rejections = 0;
    uint64_t template_selected_receipts = 0;
    // Admission and prune cost counters. Local diagnostics only.
    uint64_t admissions = 0;
    uint64_t duplicates_short_circuited = 0;
    uint64_t verifications = 0;
    uint64_t cache_hits = 0;
    uint64_t cache_invalidations = 0;
    uint64_t prune_runs = 0;
    uint64_t prune_rechecked_receipts = 0;
    uint64_t verification_microseconds = 0;
    uint64_t pool_lock_microseconds = 0;
  };

  // Immutable canonical context captured under the chain lock. Every lookup
  // that was not captured fails closed, exactly like a missing block.
  class relay_context_map_v2 final : public canonical_context_source_v2
  {
  public:
    void add_block_hash(uint64_t height, bool available, const crypto::hash &hash);
    void add_round_anchor(
        uint64_t epoch, uint64_t round, bool available, const crypto::hash &hash);
    bool block_hash(uint64_t height, crypto::hash &hash) const override;
    bool round_anchor(uint64_t epoch, uint64_t round, crypto::hash &hash) const override;

  private:
    std::map<uint64_t, crypto::hash> block_hashes_;
    std::map<std::pair<uint64_t, uint64_t>, crypto::hash> round_anchors_;
  };

  // The canonical context lookups one admission batch can perform. They are a
  // pure function of the parsed records, so they can be resolved up front.
  struct relay_context_requests_v2
  {
    std::vector<uint64_t> block_heights;
    std::vector<std::pair<uint64_t, uint64_t>> round_anchors;

    void capture(
        const canonical_context_source_v2 &source,
        relay_context_map_v2 &captured) const;
  };

  // One canonical tip as seen by an admission batch. `state` is shared and
  // never mutated; `generation` is the pool generation it was synced to.
  struct relay_tip_snapshot_v2
  {
    uint64_t generation = 0;
    uint64_t height = 0;
    std::shared_ptr<const semantic_state_v2> state;
    relay_context_map_v2 contexts;
  };

  class relay_record_pool_v2;

  // The chain side of an admission. capture() runs without the pool lock;
  // under the chain lock it calls pool.sync_tip() and fills the snapshot.
  class relay_chain_view_v2
  {
  public:
    virtual ~relay_chain_view_v2() = default;
    virtual bool capture(
        relay_record_pool_v2 &pool,
        const relay_context_requests_v2 &requests,
        relay_tip_snapshot_v2 &snapshot) = 0;
    // Called before each record verification, with neither the chain lock
    // nor the pool lock held.
    virtual void before_verification() {}
  };

  // Local policy cache for already signed, self-contained records. It is not
  // consensus state: complete blocks are always parsed and validated again.
  //
  // Lock order: Blockchain::m_blockchain_lock before the pool mutex. The pool
  // never calls into the chain; every public method takes the pool mutex.
  //
  // The chain calls note_tip_changed() whenever its tip moves and sync_tip()
  // before relying on the pool. A stored receipt keeps the context hash it
  // was verified in and is re-verified only when that hash changes.
  //
  // A receipt proven authentic is remembered with its context hash; a replay
  // under the same hash is classified skip or expired without verification.
  class relay_record_pool_v2
  {
  public:
    relay_record_pool_v2(
        const epoch_timing_v2 &timing,
        const committee_policy_v2 &committee_policy,
        const envelope_limits_v2 &envelope_limits,
        const relay_queue_limits_v2 &queue_limits,
        const relay_template_limits_v2 &template_limits,
        uint64_t receipt_slot_dedup_height =
            std::numeric_limits<uint64_t>::max());
    relay_record_pool_v2(const relay_record_pool_v2 &other);
    relay_record_pool_v2 &operator=(const relay_record_pool_v2 &other);

    bool valid() const;
    relay_record_status_v2 enqueue(
        const envelope_record_v2 &record,
        uint64_t current_height);
    void prune_expired(uint64_t current_height);
    // Unconditional incremental prune; leaves the pool synced to this view.
    void prune_unusable_receipts(
        uint64_t current_height,
        const semantic_state_v2 &canonical_state,
        const canonical_context_source_v2 &contexts);
    // Prunes if the tip changed or was marked changed since the last sync;
    // returns the pool generation. The caller holds the chain lock.
    uint64_t sync_tip(
        const crypto::hash &tip_hash,
        uint64_t current_height,
        const semantic_state_v2 &canonical_state,
        const canonical_context_source_v2 &contexts);
    // O(1); called by the chain under its lock whenever the tip moves.
    void note_tip_changed();
    void clear();
    relay_record_status_v2 select_for_template(
        uint64_t current_height,
        std::vector<relay_record_selection_v2> &selected,
        const semantic_state_v2 *canonical_state = nullptr);
    void erase_confirmed(const std::vector<crypto::hash> &ids);
    void erase_confirmed_records(
        const std::vector<envelope_record_v2> &records,
        uint64_t current_height);
    relay_pool_diagnostics_v2 diagnostics() const;
    void record_ingress_rejection(relay_record_status_v2 status);
    size_t size() const;
    size_t bytes() const;
    std::vector<crypto::hash> record_ids() const;

  private:
    friend relay_ingress_status_v2 admit_relay_envelopes_v2(
        const std::vector<std::string> &envelopes,
        const envelope_limits_v2 &envelope_limits,
        const relay_policy_v2 &policy,
        relay_chain_view_v2 &chain,
        relay_record_pool_v2 &pool,
        std::vector<std::string> &accepted);
    class scoped_lock;
    struct admission_plan_v2;

    struct stored_record_v2
    {
      crypto::hash id{};
      envelope_record_v2 record{};
      uint64_t deadline_height = 0;
      std::string receipt_slot{};
      std::string receipt_context{};
      // Verified-receipt cache entry: valid for the synced tip only.
      bool verified = false;
      uint64_t verified_epoch = 0;
      crypto::hash verified_context{};
    };

    struct id_order_v2
    {
      bool operator()(const crypto::hash &left, const crypto::hash &right) const
      {
        return std::memcmp(&left, &right, sizeof(crypto::hash)) < 0;
      }
    };

    // A receipt authenticated under `context`, the receipt cache context of
    // its epoch and round at the first inclusion height of its window.
    // Bounded by retain_until and by the queue item limit.
    struct authenticated_receipt_v2
    {
      envelope_record_v2 record{};
      uint64_t epoch = 0;
      uint64_t retain_until = 0;
      crypto::hash context{};
    };

    // Ordered undo log for one in-place batch commit. Undone in reverse.
    struct commit_journal_v2
    {
      struct operation_v2
      {
        bool inserted = false;
        bool queued = false;
        size_t record_index = 0;
        size_t queue_index = 0;
        stored_record_v2 stored{};
        relay_item_v2 item{};
      };
      std::vector<operation_v2> operations;
    };

    bool describe(
        const envelope_record_v2 &record,
        uint64_t current_height,
        relay_class_v2 &record_class,
        uint64_t &deadline_height) const;
    relay_record_status_v2 enqueue_locked(
        const envelope_record_v2 &record,
        uint64_t current_height,
        commit_journal_v2 *journal);
    void prune_expired_locked(uint64_t current_height, commit_journal_v2 *journal);
    void compact_receipt_slots(uint64_t current_height, commit_journal_v2 *journal);
    void erase_at_locked(size_t index, commit_journal_v2 *journal);
    void erase_confirmed_locked(const std::vector<crypto::hash> &ids);
    void sync_locked(
        const crypto::hash &tip_hash,
        uint64_t current_height,
        const semantic_state_v2 &canonical_state,
        const canonical_context_source_v2 &contexts);
    std::vector<stored_record_v2>::iterator find_record_locked(const crypto::hash &id);
    void rollback_locked(const commit_journal_v2 &journal);
    void restore_policy_counters(const relay_pool_diagnostics_v2 &backup);
    void record_ingress_rejection_locked(relay_record_status_v2 status);
    void mark_verified_locked(
        const crypto::hash &id, uint64_t epoch, const crypto::hash &context);
    void remember_authenticated_locked(
        const crypto::hash &id, const envelope_record_v2 &record,
        uint64_t epoch, const crypto::hash &context);
    void prune_authenticated_locked(uint64_t current_height);
    bool admit_cached_duplicates(
        const std::vector<std::string> &envelopes,
        const envelope_limits_v2 &envelope_limits,
        const relay_policy_v2 &policy,
        std::vector<std::string> &accepted);
    bool prepare_admission(
        admission_plan_v2 &plan,
        const relay_tip_snapshot_v2 &snapshot,
        const relay_policy_v2 &policy);
    bool commit_admission(
        admission_plan_v2 &plan,
        const relay_tip_snapshot_v2 &snapshot,
        relay_ingress_status_v2 &status,
        std::vector<std::string> &accepted);
    void add_verification_time(uint64_t microseconds, uint64_t verifications);

    mutable std::mutex mutex_;

    epoch_timing_v2 timing_{};
    committee_policy_v2 committee_policy_{};
    envelope_limits_v2 envelope_limits_{};
    relay_queue_limits_v2 queue_limits_{};
    relay_template_limits_v2 template_limits_{};
    uint64_t receipt_slot_dedup_height_ =
        std::numeric_limits<uint64_t>::max();
    deadline_relay_queue_v2 queue_;
    std::vector<stored_record_v2> records_;
    std::map<crypto::hash, authenticated_receipt_v2, id_order_v2>
        authenticated_receipts_;
    mutable relay_pool_diagnostics_v2 diagnostics_{};
    bool synced_ = false;
    bool dirty_ = true;
    bool synced_state_valid_ = false;
    crypto::hash synced_tip_{};
    uint64_t synced_height_ = 0;
    uint64_t generation_ = 0;
  };

  // Validates one-record canonical envelopes against the current canonical
  // semantic state and commits the entire local relay batch atomically. Before
  // the configured hardening height only exact complete-byte duplicates are
  // idempotent. At and after the boundary, service receipts are additionally
  // deduplicated by their consensus slot so randomized re-signatures cannot
  // exhaust the bounded queue. Malformed or unauthenticated records cannot
  // enter the pool. `accepted` is the forwardable output: it also includes
  // bounded receipt retries and received enrollment prerequisites when another
  // member of their group is newly queued. All-duplicate enrollment is silent.
  //
  // Exact pending receipts verified at the synced tip are answered under the
  // pool lock alone. Other batches are verified off-lock against the
  // chain.capture() snapshot and committed only at its generation, else
  // captured and verified again.
  relay_ingress_status_v2 admit_relay_envelopes_v2(
      const std::vector<std::string> &envelopes,
      const envelope_limits_v2 &envelope_limits,
      const relay_policy_v2 &policy,
      relay_chain_view_v2 &chain,
      relay_record_pool_v2 &pool,
      std::vector<std::string> &accepted);

  // Convenience form over a caller-owned canonical view (tests and tools).
  // Every call re-syncs the pool to that view before admitting.
  relay_ingress_status_v2 admit_relay_envelopes_v2(
      const std::vector<std::string> &envelopes,
      uint64_t inclusion_height,
      const envelope_limits_v2 &envelope_limits,
      const relay_policy_v2 &policy,
      const semantic_state_v2 &canonical_state,
      const canonical_context_source_v2 &contexts,
      relay_record_pool_v2 &pool,
      std::vector<std::string> &accepted);
} // namespace epose
} // namespace qwertycoin
