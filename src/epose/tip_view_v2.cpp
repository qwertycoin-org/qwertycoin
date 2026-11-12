// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/tip_view_v2.h"

#include <cassert>
#include <limits>
#include <utility>

#include "epose/service_node.h"
#include "misc_log_ex.h"

namespace
{
  thread_local unsigned reader_depth = 0;
  std::atomic<uint64_t> reader_lock_violations{0};

  void check_chain_lock_acquire()
  {
    if (reader_depth == 0)
      return;
    if (reader_lock_violations.fetch_add(1, std::memory_order_relaxed) == 0)
      MERROR("EPoSE tip view reader took the chain lock; further violations are only counted");
    assert(!"EPoSE tip view reader took the chain lock");
  }
}

namespace qwertycoin
{
namespace epose
{
  uint64_t epose_tip_view::epoch_start_height(const uint64_t value) const
  {
    if (v2)
    {
      uint64_t height = 0;
      return parameters.timing.epoch_start(value, height)
          ? height : std::numeric_limits<uint64_t>::max();
    }
    return qwertycoin::epose::epoch_start_height(value);
  }

  uint64_t epose_tip_view::epoch_end_height(const uint64_t value) const
  {
    if (v2)
    {
      uint64_t height = 0;
      return parameters.timing.epoch_end(value, height)
          ? height : std::numeric_limits<uint64_t>::max();
    }
    return qwertycoin::epose::epoch_end_height(value);
  }

  bool epose_tip_view::reward_source_epoch(
      const uint64_t at_height, uint64_t &source_epoch) const
  {
    source_epoch = 0;
    if (!v2 || !parameters.timing.valid()
        || at_height < parameters.timing.activation_height)
      return false;
    const uint64_t payout_epoch =
        (at_height - parameters.timing.activation_height)
        / parameters.timing.epoch_length;
    if (payout_epoch != 0)
      source_epoch = payout_epoch - 1;
    return true;
  }

  bool epose_tip_view::round_anchor_height(
      const uint64_t value, const uint64_t index, uint64_t &anchor_height) const
  {
    anchor_height = 0;
    if (!v2 || index >= parameters.committee.round_offsets.size())
      return false;
    if (index == 0)
      return parameters.timing.committee_anchor(value, anchor_height);
    uint64_t start = 0;
    if (!parameters.timing.epoch_start(value, start)
        || start > std::numeric_limits<uint64_t>::max()
            - parameters.committee.round_offsets[index])
      return false;
    anchor_height = start + parameters.committee.round_offsets[index];
    return true;
  }

  bool epose_tip_view::anchor_hash(
      const uint64_t anchor_height, crypto::hash &hash) const
  {
    hash = crypto::null_hash;
    const auto found = anchors.find(anchor_height);
    if (found == anchors.end())
      return false;
    hash = found->second;
    return true;
  }

  std::vector<identity_descriptor_v2> epose_tip_view::descriptors_for_epoch(
      const uint64_t value) const
  {
    if (!state)
      return {};
    if (value == epoch)
      return descriptors;
    return state->lifecycle().descriptors_for_epoch(value);
  }

  std::vector<crypto::public_key> epose_tip_view::qualified_service_nodes(
      const uint64_t value) const
  {
    if (!state)
      return {};
    if (value == epoch)
      return qualified_nodes;
    const qualification_set_v2 *stored = qualification(value);
    return stored == nullptr
        ? std::vector<crypto::public_key>{} : stored->qualified_nodes;
  }

  const membership_snapshot_v2 *epose_tip_view::snapshot(
      const uint64_t value) const
  {
    return state ? state->membership().snapshot(value) : nullptr;
  }

  const qualification_set_v2 *epose_tip_view::qualification(
      const uint64_t value) const
  {
    return state ? state->membership().qualification(value) : nullptr;
  }

  bool epose_tip_view::receipt_coverage(
      const uint64_t value, const crypto::public_key &subject_public_key,
      std::vector<size_t> &coverage) const
  {
    coverage.clear();
    if (!state)
      return false;
    coverage = state->membership().receipt_coverage(value, subject_public_key);
    return true;
  }

  std::vector<verifier_assignment_v2> epose_tip_view::committee(
      const uint64_t value, const uint64_t index,
      const crypto::public_key &subject_public_key,
      const crypto::hash &round_anchor_hash) const
  {
    return state
        ? state->membership().committee(
              value, index, subject_public_key, round_anchor_hash)
        : std::vector<verifier_assignment_v2>{};
  }

  std::shared_ptr<const epose_tip_view> build_epose_tip_view(
      const crypto::hash &tip_hash,
      const uint64_t height,
      const bool enabled,
      const consensus_parameters_v2 *parameters,
      std::shared_ptr<const semantic_state_v2> state,
      const std::function<bool(uint64_t, crypto::hash &)> &block_hash,
      const crypto::hash *known_state_hash)
  {
    auto view = std::make_shared<epose_tip_view>();
    view->tip_hash = tip_hash;
    view->height = height;
    view->enabled = enabled;
    view->v2 = parameters != nullptr && state != nullptr;
    if (!view->v2)
    {
      view->epoch = qwertycoin::epose::epoch_for_height(height > 0 ? height - 1 : 0);
      return view;
    }

    view->parameters = *parameters;
    const epoch_timing_v2 &timing = view->parameters.timing;
    view->epoch = height > timing.activation_height
        ? (height - 1 - timing.activation_height) / timing.epoch_length
        : qwertycoin::epose::epoch_for_height(height > 0 ? height - 1 : 0);
    view->state = std::move(state);
    view->state_hash = known_state_hash != nullptr
        ? *known_state_hash : view->state->state_hash();
    view->receipt_count = view->state->membership().receipt_count();
    view->descriptors =
        view->state->lifecycle().descriptors_for_epoch(view->epoch);
    const qualification_set_v2 *qualification =
        view->state->membership().qualification(view->epoch);
    if (qualification != nullptr)
      view->qualified_nodes = qualification->qualified_nodes;

    const uint64_t first = view->epoch == 0 ? 0 : view->epoch - 1;
    const uint64_t last = view->epoch == std::numeric_limits<uint64_t>::max()
        ? view->epoch : view->epoch + 1;
    const size_t rounds = view->parameters.committee.round_offsets.size();
    for (uint64_t value = first; value <= last; ++value)
    {
      for (size_t index = 0; index < rounds; ++index)
      {
        uint64_t anchor_height = 0;
        if (!view->round_anchor_height(value, index, anchor_height)
            || anchor_height >= height)
          continue;
        crypto::hash hash{};
        if (block_hash(anchor_height, hash) && hash != crypto::null_hash)
        {
          view->anchors[anchor_height] = hash;
          if (value == view->epoch)
          {
            view->round_started = true;
            view->round = index;
          }
        }
      }
      if (value == last)
        break;
    }
    return view;
  }

  std::shared_ptr<const epose_tip_view> epose_tip_view_publisher::load() const
  {
    return std::atomic_load(&view_);
  }

  void epose_tip_view_publisher::publish(std::shared_ptr<const epose_tip_view> view)
  {
    std::atomic_store(&view_, std::move(view));
  }

  void epose_tip_view_publisher::reset()
  {
    std::atomic_store(&view_, std::shared_ptr<const epose_tip_view>{});
  }

  epose_view_reader_scope::epose_view_reader_scope()
  {
    ++reader_depth;
  }

  epose_view_reader_scope::~epose_view_reader_scope()
  {
    --reader_depth;
  }

  bool epose_view_reader_scope::active()
  {
    return reader_depth != 0;
  }

  uint64_t epose_view_reader_lock_violations()
  {
    return reader_lock_violations.load(std::memory_order_relaxed);
  }

  void checked_chain_lock_v2::lock()
  {
    check_chain_lock_acquire();
    epee::critical_section::lock();
  }

  bool checked_chain_lock_v2::tryLock()
  {
    check_chain_lock_acquire();
    return epee::critical_section::tryLock();
  }
} // namespace epose
} // namespace qwertycoin
