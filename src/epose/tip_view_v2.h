// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "syncobj.h"
#include "epose/coordinator_v2.h"
#include "epose/lifecycle_v2.h"
#include "epose/membership_v2.h"
#include "epose/semantic_batch_v2.h"

namespace qwertycoin
{
namespace epose
{
  // Read-only EPoSE state of one canonical tip. Every field derives from
  // that tip, and a view is never modified after it is published.
  //
  // Lock order: Blockchain::m_blockchain_lock, then the relay_record_pool_v2
  // mutex; never the chain lock while the pool mutex is held. A view is
  // published under the chain lock wherever the tip is final: block add,
  // each pop_block (so every reorg step), batch abort and state rebuild.
  // Readers take neither lock; relay pool diagnostics come from
  // relay_record_pool_v2::published_diagnostics().
  struct epose_tip_view
  {
    // An EPoSE-v2 coordinator is configured; otherwise the legacy epoch
    // formula applies and every v2 query is empty.
    bool v2 = false;
    // Blockchain::is_epose_enabled() at this tip.
    bool enabled = false;
    crypto::hash tip_hash{};
    // Chain height: top block height + 1.
    uint64_t height = 0;
    // Blockchain::get_epose_current_epoch() at this tip.
    uint64_t epoch = 0;
    // Latest round of `epoch` whose anchor block is at or below the tip.
    bool round_started = false;
    uint64_t round = 0;
    consensus_parameters_v2 parameters{};
    std::shared_ptr<const semantic_state_v2> state;
    crypto::hash state_hash{};
    uint64_t receipt_count = 0;
    // Current epoch.
    std::vector<identity_descriptor_v2> descriptors;
    std::vector<crypto::public_key> qualified_nodes;
    // Hashes of the committee and round anchor blocks of epochs epoch - 1,
    // epoch and epoch + 1 that are at or below the tip, by height.
    std::map<uint64_t, crypto::hash> anchors;

    uint64_t epoch_start_height(uint64_t epoch) const;
    uint64_t epoch_end_height(uint64_t epoch) const;
    bool reward_source_epoch(uint64_t height, uint64_t &epoch) const;
    bool round_anchor_height(uint64_t epoch, uint64_t round, uint64_t &height) const;
    bool anchor_hash(uint64_t height, crypto::hash &hash) const;
    std::vector<identity_descriptor_v2> descriptors_for_epoch(uint64_t epoch) const;
    std::vector<crypto::public_key> qualified_service_nodes(uint64_t epoch) const;
    const membership_snapshot_v2 *snapshot(uint64_t epoch) const;
    const qualification_set_v2 *qualification(uint64_t epoch) const;
    bool receipt_coverage(
        uint64_t epoch, const crypto::public_key &subject_public_key,
        std::vector<size_t> &coverage) const;
    std::vector<verifier_assignment_v2> committee(
        uint64_t epoch, uint64_t round,
        const crypto::public_key &subject_public_key,
        const crypto::hash &round_anchor_hash) const;
  };

  // Called under the chain lock. `parameters` and `state` are null without a
  // v2 coordinator. `block_hash` returns the canonical hash below `height`.
  // `known_state_hash`, when set, is the committed state->state_hash() of
  // this tip, which saves hashing the state again.
  std::shared_ptr<const epose_tip_view> build_epose_tip_view(
      const crypto::hash &tip_hash,
      uint64_t height,
      bool enabled,
      const consensus_parameters_v2 *parameters,
      std::shared_ptr<const semantic_state_v2> state,
      const std::function<bool(uint64_t, crypto::hash &)> &block_hash,
      const crypto::hash *known_state_hash = nullptr);

  // publish() is called under the chain lock; load() takes only the lock
  // std::atomic_load uses for the pointer.
  class epose_tip_view_publisher
  {
  public:
    std::shared_ptr<const epose_tip_view> load() const;
    void publish(std::shared_ptr<const epose_tip_view> view);
    void reset();

  private:
    std::shared_ptr<const epose_tip_view> view_;
  };

  // Marks the current thread as a lock-free tip view reader. Taking the
  // chain lock inside the scope is counted, and asserts in debug builds.
  class epose_view_reader_scope
  {
  public:
    epose_view_reader_scope();
    ~epose_view_reader_scope();
    epose_view_reader_scope(const epose_view_reader_scope &) = delete;
    epose_view_reader_scope &operator=(const epose_view_reader_scope &) = delete;

    static bool active();
  };

  // Number of chain lock acquisitions made inside an epose_view_reader_scope
  // since process start.
  uint64_t epose_view_reader_lock_violations();

  // epee::critical_section that checks no tip view reader takes it.
  class checked_chain_lock_v2 : public epee::critical_section
  {
  public:
    void lock();
    bool tryLock();
  };
} // namespace epose
} // namespace qwertycoin
