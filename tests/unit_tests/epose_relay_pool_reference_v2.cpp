// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose_relay_pool_reference_v2.h"

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

  void increment_saturated(uint64_t &value, const uint64_t amount = 1)
  {
    value = amount > std::numeric_limits<uint64_t>::max() - value
        ? std::numeric_limits<uint64_t>::max() : value + amount;
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

  std::string receipt_context(
      const qwertycoin::epose::envelope_record_v2 &record)
  {
    using namespace qwertycoin::epose;
    authenticated_service_receipt_v2 receipt{};
    if (decode_service_receipt_record_structure_v2(record, receipt)
        != record_codec_status_v2::accepted)
      return {};
    std::string context("QWC_EPOSE_RECEIPT_CONTEXT_V2");
    context.append(
        reinterpret_cast<const char *>(&receipt.challenge.snapshot_hash),
        sizeof(receipt.challenge.snapshot_hash));
    context.append(
        reinterpret_cast<const char *>(&receipt.challenge.anchor_hash),
        sizeof(receipt.challenge.anchor_hash));
    return context;
  }
}

namespace qwertycoin
{
namespace epose
{
namespace reference_v2
{
  uint64_t &verification_count()
  {
    static uint64_t count = 0;
    return count;
  }

  relay_record_pool_v2::relay_record_pool_v2(
      const epoch_timing_v2 &timing,
      const committee_policy_v2 &committee_policy,
      const envelope_limits_v2 &envelope_limits,
      const relay_queue_limits_v2 &queue_limits,
      const relay_template_limits_v2 &template_limits,
      uint64_t receipt_slot_dedup_height)
    : timing_(timing),
      committee_policy_(committee_policy),
      envelope_limits_(envelope_limits),
      queue_limits_(queue_limits),
      template_limits_(template_limits),
      receipt_slot_dedup_height_(receipt_slot_dedup_height),
      queue_(queue_limits)
  {
  }

  bool relay_record_pool_v2::valid() const
  {
    return timing_.valid() && committee_policy_.valid() && envelope_limits_.valid()
        && queue_limits_.valid() && template_limits_.valid();
  }

  bool relay_record_pool_v2::describe(
      const envelope_record_v2 &record,
      const uint64_t current_height,
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
        if (current_height < receipt_slot_dedup_height_)
          return timing_.evidence_deadline(
              receipt.challenge.epoch, deadline_height);
        uint64_t first_height = 0;
        return receipt_inclusion_window_v2(
            timing_, committee_policy_, receipt.challenge.epoch,
            receipt.challenge.round, first_height, deadline_height);
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
    if (!describe(record, current_height, record_class, deadline_height))
      return relay_record_status_v2::invalid_record;

    std::string encoded;
    envelope_budget_v2 budget{};
    if (encode_envelope_v2({record}, envelope_limits_, encoded, budget)
        != envelope_status_v2::accepted)
      return relay_record_status_v2::invalid_record;
    const crypto::hash id = relay_id(encoded);

    prune_expired(current_height);
    const std::string slot = receipt_slot(record);
    const std::string context = receipt_context(record);
    if (current_height >= receipt_slot_dedup_height_ && !slot.empty())
    {
      const auto duplicate = std::find_if(
          records_.begin(), records_.end(), [&](const stored_record_v2 &stored) {
            return stored.receipt_slot == slot;
          });
      if (duplicate != records_.end())
      {
        if (duplicate->receipt_context == context)
        {
          if (duplicate->id == id)
          {
            increment_saturated(diagnostics_.exact_duplicates);
            return relay_record_status_v2::idempotent_duplicate;
          }
          increment_saturated(diagnostics_.slot_variants);
          return relay_record_status_v2::receipt_slot_variant;
        }
        // A reorg changed the snapshot or round anchor. The old local entry
        // can never be valid in the new canonical context and must not block
        // its authenticated replacement.
        queue_.erase(duplicate->id);
        records_.erase(duplicate);
      }
    }

    const auto duplicate = std::find_if(records_.begin(), records_.end(), [&](const stored_record_v2 &stored) {
      return stored.id == id;
    });
    if (duplicate != records_.end())
    {
      if (same_record(duplicate->record, record)
          && duplicate->deadline_height == deadline_height)
      {
        increment_saturated(diagnostics_.exact_duplicates);
        return relay_record_status_v2::idempotent_duplicate;
      }
      return relay_record_status_v2::conflict;
    }

    const relay_item_v2 item{id, record_class, budget.bytes, deadline_height};
    const resource_status_v2 queue_status = queue_.enqueue(item, current_height);
    if (queue_status == resource_status_v2::relay_item_expired)
    {
      if (!slot.empty())
        increment_saturated(diagnostics_.expired_round_receipts);
      return relay_record_status_v2::expired;
    }
    if (queue_status == resource_status_v2::relay_queue_full)
    {
      increment_saturated(diagnostics_.capacity_rejections);
      return relay_record_status_v2::full;
    }
    if (queue_status != resource_status_v2::accepted)
      return queue_status == resource_status_v2::relay_item_conflict
          ? relay_record_status_v2::conflict
          : relay_record_status_v2::invalid_configuration;
    records_.push_back({id, record, deadline_height, slot, context});
    return relay_record_status_v2::accepted;
  }

  void relay_record_pool_v2::prune_expired(uint64_t current_height)
  {
    const uint64_t expired_receipts = std::count_if(
        records_.begin(), records_.end(), [current_height](
            const stored_record_v2 &stored) {
          return !stored.receipt_slot.empty()
              && stored.deadline_height < current_height;
        });
    increment_saturated(
        diagnostics_.expired_round_receipts, expired_receipts);
    queue_.prune_expired(current_height);
    records_.erase(std::remove_if(records_.begin(), records_.end(), [&](const stored_record_v2 &stored) {
      return stored.deadline_height < current_height;
    }), records_.end());
    compact_receipt_slots(current_height);
  }

  void relay_record_pool_v2::prune_unusable_receipts(
      const uint64_t current_height,
      const semantic_state_v2 &canonical_state,
      const canonical_context_source_v2 &contexts)
  {
    prune_expired(current_height);
    std::vector<crypto::hash> canonical_lifecycles;
    for (const auto &stored : records_)
    {
      lifecycle_record_v2 lifecycle{};
      if (decode_lifecycle_record_structure_v2(stored.record, lifecycle)
              == record_codec_status_v2::accepted
          && canonical_state.lifecycle().contains(lifecycle))
        canonical_lifecycles.push_back(stored.id);
      admission_lease_v2 admission{};
      if (decode_admission_lease_record_structure_v2(stored.record, admission)
              == record_codec_status_v2::accepted
          && canonical_state.membership().contains_admission(admission))
        canonical_lifecycles.push_back(stored.id);
    }
    erase_confirmed(canonical_lifecycles);
    if (current_height < receipt_slot_dedup_height_ || records_.empty())
      return;
    std::vector<crypto::hash> remove;
    // One isolated preview for the sweep; each probe is journaled and undone.
    semantic_state_v2 next = canonical_state;
    for (const stored_record_v2 &stored : records_)
    {
      if (stored.record.type
          != static_cast<uint8_t>(record_type_v2::service_receipt))
        continue;
      authenticated_service_receipt_v2 receipt{};
      if (decode_service_receipt_record_structure_v2(stored.record, receipt)
          != record_codec_status_v2::accepted)
      {
        remove.push_back(stored.id);
        continue;
      }
      const service_challenge_v2 &challenge = receipt.challenge;
      if (canonical_state.membership().has_receipt_slot(
              challenge.epoch, challenge.round,
              challenge.subject_public_key, challenge.verifier_public_key))
      {
        increment_saturated(diagnostics_.canonical_receipts);
        remove.push_back(stored.id);
        continue;
      }
      auto undo = next.checkpoint();
      semantic_apply_summary_v2 summary{};
      const semantic_transaction_context_v2 transaction{
          current_height, true, nullptr, nullptr};
      const semantic_status_v2 status = next.apply_transaction(
          {stored.record}, transaction, contexts, summary, &undo);
      ++verification_count();
      next.rollback(undo);
      if (status != semantic_status_v2::accepted
          && status != semantic_status_v2::context_unavailable)
      {
        increment_saturated(diagnostics_.invalid_context_receipts);
        remove.push_back(stored.id);
      }
    }
    erase_confirmed(remove);
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
      std::vector<relay_record_selection_v2> &selected,
      const semantic_state_v2 *canonical_state)
  {
    selected.clear();
    prune_expired(current_height);
    std::vector<relay_item_v2> items;
    const resource_status_v2 status =
        queue_.select_for_template(current_height,
            {queue_limits_.max_items, queue_limits_.max_bytes,
             queue_limits_.reserved_enrollment_items, queue_limits_.reserved_evidence_items,
             queue_limits_.reserved_enrollment_bytes, queue_limits_.reserved_evidence_bytes}, items);
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
    std::vector<envelope_record_v2> candidates;
    for (const auto &entry : next)
      candidates.push_back(entry.record);
    std::vector<bool> included(next.size());
    size_t bytes = 0;
    const auto add_group = [&](const std::vector<size_t> &group) {
      if (canonical_state)
        for (auto i : group)
        {
          admission_lease_v2 admission{};
          if (decode_admission_lease_record_structure_v2(next[i].record, admission)
                  == record_codec_status_v2::accepted
              && canonical_state->membership().contains_admission(admission))
            return false;
        }
      size_t group_bytes = 0, group_items = 0;
      for (auto i : group)
        if (!included[i])
        {
          group_bytes += items[i].bytes;
          ++group_items;
        }
      if (group_items > template_limits_.max_items - selected.size()
          || group_bytes > template_limits_.max_bytes - bytes)
        return false;
      for (auto i : group)
        if (!included[i])
        {
          selected.push_back(next[i]);
          included[i] = true;
        }
      bytes += group_bytes;
      return true;
    };
    // Reserve evidence as before, then prioritize complete admission groups.
    size_t evidence_items = 0, evidence_bytes = 0;
    for (size_t i = 0; i < next.size(); ++i)
      if (items[i].record_class == relay_class_v2::evidence
          && evidence_items < template_limits_.reserved_evidence_items
          && items[i].bytes <= template_limits_.reserved_evidence_bytes - evidence_bytes
          && add_group({i}))
      {
        ++evidence_items;
        evidence_bytes += items[i].bytes;
      }
    for (const auto &group : enrollment_template_groups_v2(candidates,
        canonical_state ? canonical_state->nettype() : cryptonote::MAINNET,
        canonical_state ? canonical_state->genesis_hash() : crypto::null_hash,
        canonical_state ? canonical_state->parameter_set_hash() : crypto::null_hash))
      add_group(group);
    const uint64_t receipt_count = std::count_if(
        selected.begin(), selected.end(), [](const relay_record_selection_v2 &entry) {
          return entry.record.type
              == static_cast<uint8_t>(record_type_v2::service_receipt);
        });
    increment_saturated(
        diagnostics_.template_selected_receipts, receipt_count);
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
    uint64_t canonical_receipts = 0;
    for (const envelope_record_v2 &record : records)
    {
      lifecycle_record_v2 confirmed{};
      if (decode_lifecycle_record_structure_v2(record, confirmed) == record_codec_status_v2::accepted)
      {
        const auto hash = hash_lifecycle_record_v2(cryptonote::MAINNET, {}, {}, confirmed);
        for (const auto &stored : records_)
        {
          lifecycle_record_v2 pending{};
          if (decode_lifecycle_record_structure_v2(stored.record, pending) == record_codec_status_v2::accepted
              && hash_lifecycle_record_v2(cryptonote::MAINNET, {}, {}, pending) == hash)
            ids.push_back(stored.id);
        }
        continue;
      }
      const std::string slot = receipt_slot(record);
      if (current_height >= receipt_slot_dedup_height_ && !slot.empty())
      {
        increment_saturated(canonical_receipts);
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
    increment_saturated(diagnostics_.canonical_receipts, canonical_receipts);
  }

  relay_pool_diagnostics_v2 relay_record_pool_v2::diagnostics() const
  {
    relay_pool_diagnostics_v2 result = diagnostics_;
    result.queue_items = records_.size();
    result.queue_bytes = queue_.bytes();
    return result;
  }

  void relay_record_pool_v2::record_ingress_rejection(
      const relay_record_status_v2 status)
  {
    if (status == relay_record_status_v2::expired)
      increment_saturated(diagnostics_.expired_round_receipts);
    else if (status == relay_record_status_v2::full)
      increment_saturated(diagnostics_.capacity_rejections);
  }

  size_t relay_record_pool_v2::size() const { return records_.size(); }
  size_t relay_record_pool_v2::bytes() const { return queue_.bytes(); }
  std::vector<crypto::hash> relay_record_pool_v2::record_ids() const
  {
    std::vector<crypto::hash> ids;
    for (const auto &stored : records_)
      ids.push_back(stored.id);
    return ids;
  }

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
    next_pool.prune_unusable_receipts(
        inclusion_height, canonical_state, contexts);
    semantic_state_v2 semantic = canonical_state;
    std::vector<std::string> next_accepted;
    next_accepted.reserve(envelopes.size());
    std::vector<envelope_record_v2> received_records;
    std::vector<bool> forward(envelopes.size(), false);
    for (const std::string &encoded : envelopes)
    {
      std::vector<envelope_record_v2> records;
      envelope_budget_v2 ignored{};
      if (parse_envelope_v2(encoded, envelope_limits, records, ignored)
              != envelope_status_v2::accepted
          || records.size() != 1)
        return relay_ingress_status_v2::invalid_batch;

      const size_t index = received_records.size();
      received_records.push_back(records.front());
      if (inclusion_height >= policy.receipt_slot_dedup_height
          && records.front().type
          == static_cast<uint8_t>(record_type_v2::service_receipt))
      {
        authenticated_service_receipt_v2 receipt{};
        if (decode_service_receipt_record_structure_v2(
                records.front(), receipt)
            != record_codec_status_v2::accepted)
          return relay_ingress_status_v2::invalid_batch;
        const service_challenge_v2 &challenge = receipt.challenge;
        crypto::hash canonical_anchor{};
        uint64_t first_inclusion = 0;
        uint64_t last_inclusion = 0;
        if (!contexts.round_anchor(
                challenge.epoch, challenge.round, canonical_anchor)
            || !receipt_inclusion_window_v2(
                canonical_state.membership().timing(),
                canonical_state.membership().committee_policy(),
                challenge.epoch, challenge.round,
                first_inclusion, last_inclusion))
          return relay_ingress_status_v2::invalid_batch;
        const receipt_context_v2 receipt_context{
            canonical_state.nettype(), canonical_state.genesis_hash(),
            canonical_state.parameter_set_hash()};

        // A correctly authenticated receipt that arrives after its inclusive
        // round window is an expected transport race, not peer misconduct.
        // Validate it at the first admissible carrier height so malformed
        // signatures and forged slots still fail closed, then report expiry
        // without admitting or forwarding it.
        if (inclusion_height > last_inclusion)
        {
          membership_pipeline_v2 validation = semantic.membership();
          const pipeline_status_v2 late_status =
              validation.apply_authenticated_receipt(
                  receipt, receipt_context, first_inclusion,
                  canonical_anchor);
          if (late_status == pipeline_status_v2::accepted
              || late_status == pipeline_status_v2::idempotent_duplicate
              || late_status == pipeline_status_v2::receipt_slot_conflict
              || late_status
                  == pipeline_status_v2::qualification_already_closed)
            return relay_ingress_status_v2::expired;
          return relay_ingress_status_v2::invalid_batch;
        }

        if (semantic.membership().has_receipt_slot(
                challenge.epoch, challenge.round,
                challenge.subject_public_key,
                challenge.verifier_public_key))
        {
          membership_pipeline_v2 validation = semantic.membership();
          const pipeline_status_v2 race_status =
              validation.apply_authenticated_receipt(
                  receipt, receipt_context, first_inclusion,
                  canonical_anchor);
          if (race_status == pipeline_status_v2::idempotent_duplicate
              || race_status == pipeline_status_v2::receipt_slot_conflict)
            continue;
          return relay_ingress_status_v2::invalid_batch;
        }
      }

      semantic_apply_summary_v2 summary{};
      const semantic_transaction_context_v2 transaction{
          inclusion_height, false, nullptr, nullptr};
      auto undo = semantic.checkpoint();
      const auto semantic_status = semantic.apply_transaction(
          records, transaction, contexts, summary, &undo);
      ++verification_count();
      if (semantic_status == semantic_status_v2::context_unavailable)
        return relay_ingress_status_v2::context_unavailable;
      if (semantic_status != semantic_status_v2::accepted)
        return relay_ingress_status_v2::invalid_batch;

      // Authenticate before suppressing a canonical replay. A duplicate remains
      // consensus-valid, but must not reoccupy scarce carrier space.
      lifecycle_record_v2 lifecycle{};
      if (decode_lifecycle_record_structure_v2(records.front(), lifecycle)
              == record_codec_status_v2::accepted
          && canonical_state.lifecycle().contains(lifecycle))
        continue;

      admission_lease_v2 admission{};
      if (decode_admission_lease_record_structure_v2(records.front(), admission)
              == record_codec_status_v2::accepted
          && canonical_state.membership().contains_admission(admission))
        continue;

      relay_record_pool_v2 tentative = next_pool;
      const relay_record_status_v2 queue_status =
          tentative.enqueue(records.front(), inclusion_height);
      if (queue_status == relay_record_status_v2::expired)
      {
        pool.record_ingress_rejection(queue_status);
        return relay_ingress_status_v2::expired;
      }
      if (queue_status == relay_record_status_v2::full)
      {
        pool.record_ingress_rejection(queue_status);
        return relay_ingress_status_v2::capacity_exhausted;
      }
      if (queue_status != relay_record_status_v2::accepted
          && queue_status != relay_record_status_v2::idempotent_duplicate
          && queue_status != relay_record_status_v2::receipt_slot_variant)
        return relay_ingress_status_v2::invalid_batch;

      if (queue_status == relay_record_status_v2::receipt_slot_variant)
      {
        semantic.rollback(undo);
        next_pool = std::move(tentative);
        continue;
      }
      if (queue_status == relay_record_status_v2::idempotent_duplicate)
      {
        // The duplicate is already pending locally, but it still belongs in
        // this batch's semantic preview.  Otherwise a following admission or
        // lifecycle record could lose a valid in-order dependency merely
        // because its prerequisite arrived in an earlier relay wave.
        next_pool = std::move(tentative);
        // Preserve v2.0.5 receipt redelivery: a downstream miner may have
        // restarted. The transport limiter bounds each envelope per peer.
        if (inclusion_height >= policy.receipt_slot_dedup_height
            && records.front().type
            == static_cast<uint8_t>(record_type_v2::service_receipt))
          forward[index] = true;
        continue;
      }
      next_pool = std::move(tentative);
      forward[index] = true;
    }

    // A newly accepted enrollment member carries its received prerequisites,
    // including canonical or pending L. All-duplicate enrollment groups stay
    // silent; receipt retries do not reopen unrelated enrollment groups.
    const auto newly_forwardable = forward;
    for (const auto &group : enrollment_template_groups_v2(received_records,
        canonical_state.nettype(), canonical_state.genesis_hash(),
        canonical_state.parameter_set_hash()))
      if (std::any_of(group.begin(), group.end(), [&](size_t i) { return newly_forwardable[i]; }))
        for (auto i : group)
          forward[i] = true;
    for (size_t i = 0; i < envelopes.size(); ++i)
      if (forward[i])
        next_accepted.push_back(envelopes[i]);

    pool = std::move(next_pool);
    accepted.swap(next_accepted);
    return relay_ingress_status_v2::accepted;
  }
} // namespace reference_v2
} // namespace epose
} // namespace qwertycoin
