// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <vector>

#include "epose/envelope_v2.h"
#include "epose/lifecycle_v2.h"
#include "epose/membership_v2.h"
#include "epose/reward_v2.h"

namespace qwertycoin
{
namespace epose
{
  class canonical_context_source_v2
  {
  public:
    virtual ~canonical_context_source_v2() = default;
    virtual bool block_hash(uint64_t height, crypto::hash &hash) const = 0;
    virtual bool round_anchor(uint64_t epoch, uint64_t round, crypto::hash &hash) const = 0;
  };

  struct semantic_transaction_context_v2
  {
    uint64_t inclusion_height = 0;
    bool coinbase = false;
    const service_payment_context_v2 *payment = nullptr;
    const validated_service_payment_v2 *validated_payment = nullptr;
  };

  struct semantic_apply_summary_v2
  {
    size_t lifecycle_records = 0;
    size_t admission_records = 0;
    size_t receipt_records = 0;
    size_t payment_proof_records = 0;
    verification_counters_v2 verifications{};
  };

  enum class semantic_status_v2
  {
    accepted,
    invalid_configuration,
    before_activation,
    context_unavailable,
    invalid_record,
    invalid_lifecycle,
    lifecycle_binding_mismatch,
    invalid_admission,
    invalid_receipt,
    invalid_payment_proof,
    payment_proof_wrong_carrier,
    duplicate_payment_proof,
    arithmetic_overflow
  };

  class semantic_state_v2
  {
  public:
    semantic_state_v2(
        cryptonote::network_type nettype,
        const crypto::hash &genesis_hash,
        const crypto::hash &parameter_set_hash,
        const epoch_timing_v2 &timing,
        const admission_policy_v2 &admission_policy,
        const committee_policy_v2 &committee_policy);

    bool valid() const;
    semantic_status_v2 apply_transaction(
        const std::vector<envelope_record_v2> &records,
        const semantic_transaction_context_v2 &transaction,
        const canonical_context_source_v2 &contexts,
        semantic_apply_summary_v2 &summary);
    pipeline_status_v2 freeze_membership(
        uint64_t epoch, uint64_t height, const crypto::hash &anchor_hash);
    pipeline_status_v2 close_qualification(uint64_t epoch, uint64_t height);

    const lifecycle_registry_v2 &lifecycle() const;
    const membership_pipeline_v2 &membership() const;
    crypto::hash state_hash() const;

  private:
    cryptonote::network_type nettype_;
    crypto::hash genesis_hash_{};
    crypto::hash parameter_set_hash_{};
    epoch_timing_v2 timing_{};
    admission_policy_v2 admission_policy_{};
    lifecycle_registry_v2 lifecycle_;
    membership_pipeline_v2 membership_;
    bool valid_ = false;
  };
} // namespace epose
} // namespace qwertycoin
