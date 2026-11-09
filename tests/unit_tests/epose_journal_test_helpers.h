#pragma once

#include <utility>

#include "epose/lifecycle_v2.h"
#include "epose/membership_v2.h"
#include "epose/semantic_batch_v2.h"

namespace epose_test_journal
{
  using namespace qwertycoin::epose;

  inline lifecycle_status_v2 apply(lifecycle_registry_v2 &registry,
      const lifecycle_record_v2 &record, uint64_t inclusion_epoch,
      uint64_t minimum_effective_epoch,
      verification_counters_v2 *counters = nullptr)
  {
    auto undo = registry.checkpoint();
    const auto status = registry.apply(record, inclusion_epoch,
        minimum_effective_epoch, counters, &undo);
    if (status != lifecycle_status_v2::accepted
        && status != lifecycle_status_v2::idempotent_duplicate)
      registry.rollback(undo);
    return status;
  }

  inline pipeline_status_v2 apply_admission(membership_pipeline_v2 &pipeline,
      const admission_lease_v2 &lease, const admission_context_v2 &context,
      uint64_t inclusion_height,
      verification_counters_v2 *counters = nullptr)
  {
    auto undo = pipeline.checkpoint();
    const auto status = pipeline.apply_admission(lease, context,
        inclusion_height, counters, &undo);
    if (status != pipeline_status_v2::accepted
        && status != pipeline_status_v2::idempotent_duplicate)
      pipeline.rollback(undo);
    return status;
  }

  template<typename State, typename... Args>
  auto freeze_membership(State &state, Args&&... args)
  {
    auto undo = state.checkpoint();
    const auto status = state.freeze_membership(
        std::forward<Args>(args)..., &undo);
    if (status != pipeline_status_v2::accepted)
      state.rollback(undo);
    return status;
  }

  template<typename State, typename... Args>
  auto close_qualification(State &state, Args&&... args)
  {
    auto undo = state.checkpoint();
    const auto status = state.close_qualification(
        std::forward<Args>(args)..., &undo);
    if (status != pipeline_status_v2::accepted)
      state.rollback(undo);
    return status;
  }

  inline bool fold_epoch(membership_pipeline_v2 &pipeline,
      uint64_t epoch, crypto::hash &digest)
  {
    auto undo = pipeline.checkpoint();
    const bool accepted = pipeline.fold_epoch(epoch, epoch + 1, digest, &undo);
    if (!accepted)
      pipeline.rollback(undo);
    return accepted;
  }

  inline std::vector<crypto::hash> fold_retired(lifecycle_registry_v2 &registry,
      uint64_t epoch)
  {
    auto undo = registry.checkpoint();
    return registry.fold_retired(epoch, &undo);
  }

  inline void activate_window(lifecycle_registry_v2 &registry,
      uint64_t retained_epochs = QWC_EPOSE_RETAINED_EPOCHS)
  {
    auto undo = registry.checkpoint();
    registry.activate_window(retained_epochs, &undo);
  }

  inline bool advance_window(semantic_state_v2 &state, uint64_t epoch)
  {
    auto undo = state.checkpoint();
    const bool accepted = state.advance_window(epoch, &undo);
    if (!accepted)
      state.rollback(undo);
    return accepted;
  }
}
