// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/relay_pool_v2.h"

#include <algorithm>
#include <cstring>

#include "epose/record_codec_v2.h"

namespace
{
  bool same_record(
      const qwertycoin::epose::envelope_record_v2 &left,
      const qwertycoin::epose::envelope_record_v2 &right)
  {
    return left.type == right.type && left.version == right.version
        && left.payload == right.payload;
  }

  crypto::hash relay_id(const std::string &canonical_envelope)
  {
    static constexpr char domain[] = "QWC_EPOSE_RELAY_RECORD_V2";
    std::string transcript(domain, sizeof(domain) - 1);
    transcript.append(canonical_envelope);
    return crypto::cn_fast_hash(transcript.data(), transcript.size());
  }
}

namespace qwertycoin
{
namespace epose
{
  relay_record_pool_v2::relay_record_pool_v2(
      const epoch_timing_v2 &timing,
      const envelope_limits_v2 &envelope_limits,
      const relay_queue_limits_v2 &queue_limits,
      const relay_template_limits_v2 &template_limits)
    : timing_(timing),
      envelope_limits_(envelope_limits),
      queue_limits_(queue_limits),
      template_limits_(template_limits),
      queue_(queue_limits)
  {
  }

  bool relay_record_pool_v2::valid() const
  {
    return timing_.valid() && envelope_limits_.valid()
        && queue_limits_.valid() && template_limits_.valid();
  }

  bool relay_record_pool_v2::describe(
      const envelope_record_v2 &record,
      relay_class_v2 &record_class,
      uint64_t &deadline_height) const
  {
    record_class = relay_class_v2::enrollment;
    deadline_height = 0;
    switch (static_cast<record_type_v2>(record.type))
    {
      case record_type_v2::identity_descriptor:
      case record_type_v2::descriptor_lifecycle:
      {
        lifecycle_record_v2 lifecycle{};
        if (decode_lifecycle_record_structure_v2(record, lifecycle)
            != record_codec_status_v2::accepted)
          return false;
        return timing_.enrollment_cutoff(
            lifecycle.next_descriptor.effective_epoch, deadline_height);
      }
      case record_type_v2::admission_lease:
      {
        admission_lease_v2 lease{};
        if (decode_admission_lease_record_structure_v2(record, lease)
            != record_codec_status_v2::accepted)
          return false;
        return timing_.enrollment_cutoff(lease.target_epoch, deadline_height);
      }
      case record_type_v2::service_receipt:
      {
        authenticated_service_receipt_v2 receipt{};
        if (decode_service_receipt_record_structure_v2(record, receipt)
            != record_codec_status_v2::accepted)
          return false;
        record_class = relay_class_v2::evidence;
        return timing_.evidence_deadline(receipt.challenge.epoch, deadline_height);
      }
      case record_type_v2::service_payment_proof:
        return false;
    }
    return false;
  }

  relay_record_status_v2 relay_record_pool_v2::enqueue(
      const envelope_record_v2 &record,
      uint64_t current_height)
  {
    if (!valid())
      return relay_record_status_v2::invalid_configuration;
    if (record.type == static_cast<uint8_t>(record_type_v2::service_payment_proof))
      return relay_record_status_v2::payment_proof_forbidden;

    relay_class_v2 record_class{};
    uint64_t deadline_height = 0;
    if (!describe(record, record_class, deadline_height))
      return relay_record_status_v2::invalid_record;

    std::string encoded;
    envelope_budget_v2 budget{};
    if (encode_envelope_v2({record}, envelope_limits_, encoded, budget)
        != envelope_status_v2::accepted)
      return relay_record_status_v2::invalid_record;
    const crypto::hash id = relay_id(encoded);
    const auto duplicate = std::find_if(records_.begin(), records_.end(), [&](const stored_record_v2 &stored) {
      return stored.id == id;
    });
    if (duplicate != records_.end())
      return same_record(duplicate->record, record)
              && duplicate->deadline_height == deadline_height
          ? relay_record_status_v2::idempotent_duplicate
          : relay_record_status_v2::conflict;

    const relay_item_v2 item{id, record_class, budget.bytes, deadline_height};
    const resource_status_v2 queue_status = queue_.enqueue(item, current_height);
    if (queue_status == resource_status_v2::relay_item_expired)
      return relay_record_status_v2::expired;
    if (queue_status == resource_status_v2::relay_queue_full)
      return relay_record_status_v2::full;
    if (queue_status != resource_status_v2::accepted)
      return queue_status == resource_status_v2::relay_item_conflict
          ? relay_record_status_v2::conflict
          : relay_record_status_v2::invalid_configuration;
    records_.push_back({id, record, deadline_height});
    return relay_record_status_v2::accepted;
  }

  void relay_record_pool_v2::prune_expired(uint64_t current_height)
  {
    queue_.prune_expired(current_height);
    records_.erase(std::remove_if(records_.begin(), records_.end(), [&](const stored_record_v2 &stored) {
      return stored.deadline_height < current_height;
    }), records_.end());
  }

  relay_record_status_v2 relay_record_pool_v2::select_for_template(
      uint64_t current_height,
      std::vector<relay_record_selection_v2> &selected) const
  {
    selected.clear();
    std::vector<relay_item_v2> items;
    const resource_status_v2 status =
        queue_.select_for_template(current_height, template_limits_, items);
    if (status != resource_status_v2::accepted)
      return relay_record_status_v2::invalid_configuration;
    std::vector<relay_record_selection_v2> next;
    next.reserve(items.size());
    for (const relay_item_v2 &item : items)
    {
      const auto found = std::find_if(records_.begin(), records_.end(), [&](const stored_record_v2 &stored) {
        return stored.id == item.id;
      });
      if (found == records_.end())
        return relay_record_status_v2::invalid_configuration;
      next.push_back({found->id, found->record});
    }
    selected.swap(next);
    return relay_record_status_v2::accepted;
  }

  void relay_record_pool_v2::erase_confirmed(const std::vector<crypto::hash> &ids)
  {
    for (const crypto::hash &id : ids)
    {
      queue_.erase(id);
      records_.erase(std::remove_if(records_.begin(), records_.end(), [&](const stored_record_v2 &stored) {
        return stored.id == id;
      }), records_.end());
    }
  }

  size_t relay_record_pool_v2::size() const { return records_.size(); }
  size_t relay_record_pool_v2::bytes() const { return queue_.bytes(); }
} // namespace epose
} // namespace qwertycoin
