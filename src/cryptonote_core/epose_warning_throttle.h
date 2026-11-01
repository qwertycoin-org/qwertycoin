#pragma once

#include <atomic>
#include <cstdint>

#include "epose/service_producer_v2.h"

namespace cryptonote
{
  enum class epose_local_ingress_origin
  {
    enrollment,
    receipt,
    rpc_submit
  };

  inline bool epose_warning_once_per_epoch(
      std::atomic<uint64_t> &last_epoch, uint64_t epoch)
  {
    return last_epoch.exchange(epoch, std::memory_order_relaxed) != epoch;
  }

  inline bool epose_producer_step_failed(std::atomic<uint64_t> &count)
  {
    count.fetch_add(1, std::memory_order_relaxed);
    return false;
  }

  inline bool epose_counted_warning_once_per_epoch(
      std::atomic<uint64_t> &last_epoch, uint64_t epoch,
      std::atomic<uint64_t> &step_failed_count)
  {
    epose_producer_step_failed(step_failed_count);
    return epose_warning_once_per_epoch(last_epoch, epoch);
  }

  enum class epose_enrollment_failure_action
  {
    error,
    retry_quietly,
    retry_with_warning
  };

  inline epose_enrollment_failure_action epose_enrollment_failure_result(
      qwertycoin::epose::service_producer_status_v2 status,
      bool contracts_active, std::atomic<uint64_t> &last_epoch,
      uint64_t epoch, std::atomic<uint64_t> &step_failed_count)
  {
    if (!contracts_active
        || status != qwertycoin::epose::service_producer_status_v2::invalid_endpoint)
      return epose_enrollment_failure_action::error;
    return epose_counted_warning_once_per_epoch(
               last_epoch, epoch, step_failed_count)
        ? epose_enrollment_failure_action::retry_with_warning
        : epose_enrollment_failure_action::retry_quietly;
  }

  inline bool epose_local_ingress_rejection_warning(
      epose_local_ingress_origin origin, uint64_t epoch,
      std::atomic<uint64_t> &enrollment_epoch,
      std::atomic<uint64_t> &receipt_epoch,
      std::atomic<uint64_t> &rpc_epoch,
      std::atomic<uint64_t> &step_failed_count)
  {
    auto &last_epoch = origin == epose_local_ingress_origin::enrollment
        ? enrollment_epoch
        : origin == epose_local_ingress_origin::receipt
            ? receipt_epoch : rpc_epoch;
    return origin == epose_local_ingress_origin::rpc_submit
        ? epose_warning_once_per_epoch(last_epoch, epoch)
        : epose_counted_warning_once_per_epoch(
              last_epoch, epoch, step_failed_count);
  }
}
