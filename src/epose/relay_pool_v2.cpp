// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/relay_pool_v2.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <utility>

#include "epose/compiled_profile_v2.h"
#include "epose/record_codec_v2.h"
#include "string_tools.h"

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

  bool id_less(const crypto::hash &left, const crypto::hash &right)
  {
    return std::memcmp(&left, &right, sizeof(left)) < 0;
  }

  std::string receipt_slot(
      const qwertycoin::epose::envelope_record_v2 &record)
  {
    using namespace qwertycoin::epose;
    authenticated_service_receipt_v2 receipt{};
    if (decode_service_receipt_record_structure_v2(record, receipt)
        != record_codec_status_v2::accepted)
      return {};
    const service_challenge_v2 &challenge = receipt.challenge;
    std::string slot("QWC_EPOSE_RECEIPT_SLOT_V2");
    slot.push_back(static_cast<char>(challenge.service_kind));
    for (unsigned shift = 0; shift < 64; shift += 8)
      slot.push_back(static_cast<char>((challenge.epoch >> shift) & 0xff));
    for (unsigned shift = 0; shift < 64; shift += 8)
      slot.push_back(static_cast<char>((challenge.round >> shift) & 0xff));
    slot.append(
        reinterpret_cast<const char *>(&challenge.subject_public_key),
        sizeof(challenge.subject_public_key));
    slot.append(
        reinterpret_cast<const char *>(&challenge.verifier_public_key),
        sizeof(challenge.verifier_public_key));
    return slot;
  }
}

namespace qwertycoin
{
namespace epose
{
  bool relay_policy_v2::valid() const
  {
    return queue.valid() && mining_template.valid()
        && mining_template.max_items <= queue.max_items
        && mining_template.max_bytes <= queue.max_bytes;
  }

  bool compiled_relay_policy_v2(
      cryptonote::network_type nettype,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      relay_policy_v2 &policy)
  {
    policy = {};
    crypto::hash expected_genesis{};
    crypto::hash expected_parameters{};
    if (nettype != cryptonote::MAINNET
        || !epee::string_tools::hex_to_pod(MAINNET_FINAL_GENESIS_HASH_V2, expected_genesis)
        || !epee::string_tools::hex_to_pod(
            MAINNET_FINAL_PARAMETER_SET_HASH_V2, expected_parameters)
        || genesis_hash != expected_genesis
        || parameter_set_hash != expected_parameters)
      return false;

    policy.queue = {
        2048, 1048576,
        512, 512,
        262144, 262144};
    policy.mining_template = {
        256, 32768,
        64, 64,
        8192, 8192};
    policy.receipt_slot_dedup_height =
        QWC_EPOSE_RELAY_HARDENING_HEIGHT;
    return policy.valid();
  }

  relay_record_pool_v2::relay_record_pool_v2(
      const epoch_timing_v2 &timing,
      const envelope_limits_v2 &envelope_limits,
      const relay_queue_limits_v2 &queue_limits,
      const relay_template_limits_v2 &template_limits,
      uint64_t receipt_slot_dedup_height)
    : timing_(timing),
      envelope_limits_(envelope_limits),
      queue_limits_(queue_limits),
      template_limits_(template_limits),
      receipt_slot_dedup_height_(receipt_slot_dedup_height),
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

    prune_expired(current_height);
    const std::string slot = receipt_slot(record);
    if (current_height >= receipt_slot_dedup_height_ && !slot.empty())
    {
      const auto duplicate = std::find_if(
          records_.begin(), records_.end(), [&](const stored_record_v2 &stored) {
            return stored.receipt_slot == slot;
          });
      if (duplicate != records_.end())
        return relay_record_status_v2::idempotent_duplicate;
    }

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
    records_.push_back({id, record, deadline_height, slot});
    return relay_record_status_v2::accepted;
  }

  void relay_record_pool_v2::prune_expired(uint64_t current_height)
  {
    queue_.prune_expired(current_height);
    records_.erase(std::remove_if(records_.begin(), records_.end(), [&](const stored_record_v2 &stored) {
      return stored.deadline_height < current_height;
    }), records_.end());
    compact_receipt_slots(current_height);
  }

  void relay_record_pool_v2::compact_receipt_slots(uint64_t current_height)
  {
    if (current_height < receipt_slot_dedup_height_ || records_.size() < 2)
      return;
    std::map<std::string, size_t> kept;
    std::vector<bool> remove(records_.size(), false);
    for (size_t index = 0; index < records_.size(); ++index)
    {
      if (records_[index].receipt_slot.empty())
        continue;
      const auto inserted = kept.emplace(records_[index].receipt_slot, index);
      if (inserted.second)
        continue;
      const size_t previous = inserted.first->second;
      if (id_less(records_[index].id, records_[previous].id))
      {
        remove[previous] = true;
        inserted.first->second = index;
      }
      else
        remove[index] = true;
    }
    for (size_t index = records_.size(); index-- > 0;)
    {
      if (!remove[index])
        continue;
      queue_.erase(records_[index].id);
      records_.erase(records_.begin() + index);
    }
  }

  relay_record_status_v2 relay_record_pool_v2::select_for_template(
      uint64_t current_height,
      std::vector<relay_record_selection_v2> &selected)
  {
    selected.clear();
    prune_expired(current_height);
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

  void relay_record_pool_v2::erase_confirmed_records(
      const std::vector<envelope_record_v2> &records,
      uint64_t current_height)
  {
    std::vector<crypto::hash> ids;
    for (const envelope_record_v2 &record : records)
    {
      const std::string slot = receipt_slot(record);
      if (current_height >= receipt_slot_dedup_height_ && !slot.empty())
      {
        for (const stored_record_v2 &stored : records_)
          if (stored.receipt_slot == slot)
            ids.push_back(stored.id);
        continue;
      }
      std::string encoded;
      envelope_budget_v2 ignored{};
      if (encode_envelope_v2({record}, envelope_limits_, encoded, ignored)
          == envelope_status_v2::accepted)
        ids.push_back(relay_id(encoded));
    }
    erase_confirmed(ids);
  }

  size_t relay_record_pool_v2::size() const { return records_.size(); }
  size_t relay_record_pool_v2::bytes() const { return queue_.bytes(); }

  relay_ingress_status_v2 admit_relay_envelopes_v2(
      const std::vector<std::string> &envelopes,
      uint64_t inclusion_height,
      const envelope_limits_v2 &envelope_limits,
      const relay_policy_v2 &policy,
      const semantic_state_v2 &canonical_state,
      const canonical_context_source_v2 &contexts,
      relay_record_pool_v2 &pool,
      std::vector<std::string> &accepted)
  {
    accepted.clear();
    if (!envelope_limits.valid() || !policy.valid()
        || !canonical_state.valid() || !pool.valid()
        || envelopes.size() > policy.queue.max_items)
      return relay_ingress_status_v2::invalid_configuration;

    size_t total_bytes = 0;
    for (const std::string &encoded : envelopes)
    {
      if (encoded.size() > envelope_limits.max_envelope_bytes
          || encoded.size() > policy.queue.max_bytes
          || total_bytes > policy.queue.max_bytes - encoded.size())
        return relay_ingress_status_v2::invalid_batch;
      total_bytes += encoded.size();
    }

    relay_record_pool_v2 next_pool = pool;
    next_pool.prune_expired(inclusion_height);
    semantic_state_v2 semantic = canonical_state;
    std::vector<std::string> next_accepted;
    next_accepted.reserve(envelopes.size());
    for (const std::string &encoded : envelopes)
    {
      std::vector<envelope_record_v2> records;
      envelope_budget_v2 ignored{};
      if (parse_envelope_v2(encoded, envelope_limits, records, ignored)
              != envelope_status_v2::accepted
          || records.size() != 1)
        return relay_ingress_status_v2::invalid_batch;

      relay_record_pool_v2 tentative = next_pool;
      const relay_record_status_v2 queue_status =
          tentative.enqueue(records.front(), inclusion_height);
      if (queue_status == relay_record_status_v2::expired)
        return relay_ingress_status_v2::expired;
      if (queue_status == relay_record_status_v2::full)
        return relay_ingress_status_v2::capacity_exhausted;
      if (queue_status != relay_record_status_v2::accepted
          && queue_status != relay_record_status_v2::idempotent_duplicate)
        return relay_ingress_status_v2::invalid_batch;

      semantic_apply_summary_v2 summary{};
      const semantic_transaction_context_v2 transaction{
          inclusion_height, false, nullptr, nullptr};
      if (semantic.apply_transaction(
              records, transaction, contexts, summary)
          != semantic_status_v2::accepted)
        return relay_ingress_status_v2::invalid_batch;

      if (queue_status == relay_record_status_v2::idempotent_duplicate)
        continue;
      next_pool = std::move(tentative);
      next_accepted.push_back(encoded);
    }

    pool = std::move(next_pool);
    accepted.swap(next_accepted);
    return relay_ingress_status_v2::accepted;
  }
} // namespace epose
} // namespace qwertycoin
