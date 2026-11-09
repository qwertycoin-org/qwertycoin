// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/relay_pool_v2.h"

#include <algorithm>
#include <chrono>
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

  namespace
  {
    uint64_t elapsed_microseconds(const std::chrono::steady_clock::time_point start)
    {
      return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now() - start).count());
    }

    void append_u64(std::string &blob, const uint64_t value)
    {
      for (unsigned shift = 0; shift < 64; shift += 8)
        blob.push_back(static_cast<char>((value >> shift) & 0xff));
    }

    template <typename T>
    void append_pod(std::string &blob, const T &value)
    {
      blob.append(reinterpret_cast<const char *>(&value), sizeof(value));
    }

    // Everything a pending receipt's verdict depends on besides the record
    // and the live slot check. Equal hashes at two tips give the same
    // apply_transaction verdict.
    crypto::hash receipt_cache_context(
        const semantic_state_v2 &state,
        const canonical_context_source_v2 &contexts,
        const uint64_t epoch,
        const uint64_t round,
        const uint64_t height)
    {
      const membership_pipeline_v2 &membership = state.membership();
      std::string blob("QWC_EPOSE_RELAY_RECEIPT_CACHE_V2");
      blob.push_back(state.valid() ? 1 : 0);
      blob.push_back(membership.valid() ? 1 : 0);
      append_u64(blob, static_cast<uint64_t>(state.nettype()));
      append_pod(blob, state.genesis_hash());
      append_pod(blob, state.parameter_set_hash());
      append_u64(blob, epoch);
      append_u64(blob, round);
      const membership_snapshot_v2 *frozen = membership.snapshot(epoch);
      blob.push_back(frozen != nullptr ? 1 : 0);
      if (frozen != nullptr)
      {
        append_u64(blob, frozen->epoch);
        append_u64(blob, frozen->cutoff_height);
        append_u64(blob, frozen->anchor_height);
        append_pod(blob, frozen->anchor_hash);
        append_pod(blob, frozen->snapshot_hash);
        append_u64(blob, frozen->members.size());
        for (const frozen_member_v2 &member : frozen->members)
        {
          append_pod(blob, member.service_public_key);
          append_pod(blob, member.identity_id);
          append_pod(blob, member.operator_authorization_public_key);
          append_pod(blob, member.descriptor_hash);
          append_pod(blob, member.reward_binding_hash);
          append_pod(blob, member.endpoint_descriptor_hash);
          append_u64(blob, member.sequence);
        }
      }
      crypto::hash anchor{};
      const bool has_anchor = contexts.round_anchor(epoch, round, anchor);
      blob.push_back(has_anchor ? 1 : 0);
      if (has_anchor)
        append_pod(blob, anchor);
      blob.push_back(membership.qualification(epoch) != nullptr ? 1 : 0);
      uint64_t first_inclusion = 0;
      uint64_t round_end = 0;
      const bool window = receipt_inclusion_window_v2(
          membership.timing(), membership.committee_policy(), epoch, round,
          first_inclusion, round_end);
      blob.push_back(window ? 1 : 0);
      blob.push_back(window && height >= first_inclusion ? 1 : 0);
      blob.push_back(window && height <= round_end ? 1 : 0);
      return crypto::cn_fast_hash(blob.data(), blob.size());
    }

    enum class relay_verdict_v2
    {
      pending,
      enqueue,
      skip,
      invalid_batch,
      context_unavailable,
      expired
    };

    bool terminal(const relay_verdict_v2 verdict)
    {
      return verdict == relay_verdict_v2::pending
          || verdict == relay_verdict_v2::invalid_batch
          || verdict == relay_verdict_v2::context_unavailable
          || verdict == relay_verdict_v2::expired;
    }

    struct relay_plan_entry_v2
    {
      const std::string *envelope = nullptr;
      bool parsed = false;
      envelope_record_v2 record{};
      bool has_id = false;
      crypto::hash id{};
      uint64_t epoch = 0;
      uint64_t round = 0;
      std::string slot;
      std::string slot_context;
      // Byte-identical repeat of an earlier receipt of the batch: skip or a
      // slot variant, never forwarded, so neither verified nor enqueued.
      bool repeat = false;
      // Index of an earlier byte-identical lifecycle or lease record; the
      // repeat gets its verdict. A lease repeat collapses only if no other
      // record of its identity was applied in between.
      size_t repeat_of = std::numeric_limits<size_t>::max();
      // Shares its slot with another distinct receipt of the batch.
      bool coupled = false;
      // Per attempt.
      bool hardened_receipt = false;
      bool cached = false;
      bool predicted_variant = false;
      relay_verdict_v2 verdict = relay_verdict_v2::pending;
      crypto::hash context{};
      // Set by prepare_admission: the pool holds this receipt as
      // authenticated under replay_context.
      bool replay_hint = false;
      crypto::hash replay_context{};
      // Set by verify_entry: proven authentic under authenticated_context.
      bool authenticated = false;
      crypto::hash authenticated_context{};
    };

    bool collapsed(const relay_plan_entry_v2 &entry)
    {
      return entry.repeat_of != std::numeric_limits<size_t>::max();
    }

    // The pre-enqueue half of one record's admission: hardened receipt
    // classification, semantic apply into the batch preview, then canonical
    // replay suppression.
    relay_verdict_v2 verify_entry(
        relay_plan_entry_v2 &entry,
        semantic_state_v2 &semantic,
        const semantic_state_v2 &canonical_state,
        const canonical_context_source_v2 &contexts,
        const uint64_t inclusion_height,
        semantic_state_v2::undo_journal *undo_out)
    {
      if (!entry.parsed)
        return relay_verdict_v2::invalid_batch;
      const envelope_record_v2 &record = entry.record;
      if (entry.hardened_receipt)
      {
        authenticated_service_receipt_v2 receipt{};
        if (decode_service_receipt_record_structure_v2(record, receipt)
            != record_codec_status_v2::accepted)
          return relay_verdict_v2::invalid_batch;
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
          return relay_verdict_v2::invalid_batch;
        const receipt_context_v2 receipt_context{
            canonical_state.nettype(), canonical_state.genesis_hash(),
            canonical_state.parameter_set_hash()};
        // Both classifications below validate at first_inclusion. Each status
        // they accept implies every precheck and the signature passed, which
        // depends on nothing but the record and this context hash.
        const auto proven = [&]() {
          entry.authenticated = true;
          entry.authenticated_context = receipt_cache_context(
              canonical_state, contexts, entry.epoch, entry.round, first_inclusion);
        };

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
          {
            proven();
            return relay_verdict_v2::expired;
          }
          return relay_verdict_v2::invalid_batch;
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
          {
            proven();
            return relay_verdict_v2::skip;
          }
          return relay_verdict_v2::invalid_batch;
        }
      }

      semantic_apply_summary_v2 summary{};
      const semantic_transaction_context_v2 transaction{
          inclusion_height, false, nullptr, nullptr};
      auto undo = semantic.checkpoint();
      const auto semantic_status = semantic.apply_transaction(
          {record}, transaction, contexts, summary, &undo);
      if (semantic_status == semantic_status_v2::context_unavailable)
        return relay_verdict_v2::context_unavailable;
      if (semantic_status != semantic_status_v2::accepted)
        return relay_verdict_v2::invalid_batch;

      // Authenticate before suppressing a canonical replay. A duplicate remains
      // consensus-valid, but must not reoccupy scarce carrier space.
      lifecycle_record_v2 lifecycle{};
      if (decode_lifecycle_record_structure_v2(record, lifecycle)
              == record_codec_status_v2::accepted
          && canonical_state.lifecycle().contains(lifecycle))
        return relay_verdict_v2::skip;
      admission_lease_v2 admission{};
      if (decode_admission_lease_record_structure_v2(record, admission)
              == record_codec_status_v2::accepted
          && canonical_state.membership().contains_admission(admission))
        return relay_verdict_v2::skip;

      if (entry.hardened_receipt)
        entry.context = receipt_cache_context(
            canonical_state, contexts, entry.epoch, entry.round, inclusion_height);
      if (undo_out != nullptr)
        *undo_out = std::move(undo);
      return relay_verdict_v2::enqueue;
    }

    // verify_entry's receipt classification for a remembered authenticated
    // receipt. Under an equal context hash the late path can only end
    // expired and the occupied-slot path skip; false leaves it to verify_entry.
    bool classify_authenticated_replay(
        const relay_plan_entry_v2 &entry,
        const semantic_state_v2 &preview,
        const semantic_state_v2 &canonical_state,
        const canonical_context_source_v2 &contexts,
        const uint64_t inclusion_height,
        relay_verdict_v2 &verdict)
    {
      if (!entry.parsed || !entry.hardened_receipt || !entry.replay_hint)
        return false;
      authenticated_service_receipt_v2 receipt{};
      if (decode_service_receipt_record_structure_v2(entry.record, receipt)
          != record_codec_status_v2::accepted)
        return false;
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
        return false;
      if (receipt_cache_context(canonical_state, contexts, challenge.epoch,
              challenge.round, first_inclusion) != entry.replay_context)
        return false;
      if (inclusion_height > last_inclusion)
      {
        verdict = relay_verdict_v2::expired;
        return true;
      }
      if (preview.membership().has_receipt_slot(
              challenge.epoch, challenge.round,
              challenge.subject_public_key,
              challenge.verifier_public_key))
      {
        verdict = relay_verdict_v2::skip;
        return true;
      }
      return false;
    }

    relay_ingress_status_v2 verdict_status(const relay_verdict_v2 verdict)
    {
      return verdict == relay_verdict_v2::context_unavailable
          ? relay_ingress_status_v2::context_unavailable
          : verdict == relay_verdict_v2::expired
          ? relay_ingress_status_v2::expired
          : relay_ingress_status_v2::invalid_batch;
    }

    bool batch_size_valid(
        const std::vector<std::string> &envelopes,
        const envelope_limits_v2 &envelope_limits,
        const relay_policy_v2 &policy)
    {
      size_t total_bytes = 0;
      for (const std::string &encoded : envelopes)
      {
        if (encoded.size() > envelope_limits.max_envelope_bytes
            || encoded.size() > policy.queue.max_bytes
            || total_bytes > policy.queue.max_bytes - encoded.size())
          return false;
        total_bytes += encoded.size();
      }
      return true;
    }
  }

  struct relay_record_pool_v2::admission_plan_v2
  {
    struct slot_occupant_v2
    {
      bool present = false;
      std::string context;
      crypto::hash id{};
    };

    std::vector<relay_plan_entry_v2> entries;
    // Per attempt: slot dedup at the snapshot height, and the pool's occupant
    // of every coupled slot. Commit refuses the plan if a coupled entry's
    // queue outcome differs from the one predicted from these.
    bool slot_dedup = false;
    std::map<std::string, slot_occupant_v2> occupants;
    uint64_t verifications = 0;
    uint64_t verification_microseconds = 0;
    uint64_t authenticated_replays = 0;
  };

  class relay_record_pool_v2::scoped_lock
  {
  public:
    explicit scoped_lock(const relay_record_pool_v2 &pool)
      : pool_(pool), lock_(pool.mutex_), start_(std::chrono::steady_clock::now())
    {
    }
    ~scoped_lock()
    {
      increment_saturated(
          pool_.diagnostics_.pool_lock_microseconds, elapsed_microseconds(start_));
    }
    scoped_lock(const scoped_lock &) = delete;
    scoped_lock &operator=(const scoped_lock &) = delete;

  private:
    const relay_record_pool_v2 &pool_;
    std::lock_guard<std::mutex> lock_;
    std::chrono::steady_clock::time_point start_;
  };

  void relay_context_map_v2::add_block_hash(
      const uint64_t height, const bool available, const crypto::hash &hash)
  {
    if (available)
      block_hashes_[height] = hash;
    else
      block_hashes_.erase(height);
  }

  void relay_context_map_v2::add_round_anchor(
      const uint64_t epoch, const uint64_t round, const bool available,
      const crypto::hash &hash)
  {
    if (available)
      round_anchors_[{epoch, round}] = hash;
    else
      round_anchors_.erase({epoch, round});
  }

  bool relay_context_map_v2::block_hash(uint64_t height, crypto::hash &hash) const
  {
    const auto found = block_hashes_.find(height);
    if (found == block_hashes_.end())
      return false;
    hash = found->second;
    return true;
  }

  bool relay_context_map_v2::round_anchor(
      uint64_t epoch, uint64_t round, crypto::hash &hash) const
  {
    const auto found = round_anchors_.find({epoch, round});
    if (found == round_anchors_.end())
      return false;
    hash = found->second;
    return true;
  }

  void relay_context_requests_v2::capture(
      const canonical_context_source_v2 &source,
      relay_context_map_v2 &captured) const
  {
    for (const uint64_t height : block_heights)
    {
      crypto::hash hash{};
      const bool available = source.block_hash(height, hash);
      captured.add_block_hash(height, available, hash);
    }
    for (const auto &round : round_anchors)
    {
      crypto::hash hash{};
      const bool available = source.round_anchor(round.first, round.second, hash);
      captured.add_round_anchor(round.first, round.second, available, hash);
    }
  }

  relay_record_pool_v2::relay_record_pool_v2(const relay_record_pool_v2 &other)
    : queue_(other.queue_limits_)
  {
    *this = other;
  }

  relay_record_pool_v2 &relay_record_pool_v2::operator=(const relay_record_pool_v2 &other)
  {
    if (this == &other)
      return *this;
    std::unique_lock<std::mutex> mine(mutex_, std::defer_lock);
    std::unique_lock<std::mutex> theirs(other.mutex_, std::defer_lock);
    std::lock(mine, theirs);
    timing_ = other.timing_;
    committee_policy_ = other.committee_policy_;
    envelope_limits_ = other.envelope_limits_;
    queue_limits_ = other.queue_limits_;
    template_limits_ = other.template_limits_;
    receipt_slot_dedup_height_ = other.receipt_slot_dedup_height_;
    queue_ = other.queue_;
    records_ = other.records_;
    authenticated_receipts_ = other.authenticated_receipts_;
    diagnostics_ = other.diagnostics_;
    synced_ = other.synced_;
    dirty_ = other.dirty_;
    synced_state_valid_ = other.synced_state_valid_;
    synced_tip_ = other.synced_tip_;
    synced_height_ = other.synced_height_;
    generation_ = other.generation_;
    return *this;
  }

  relay_record_status_v2 relay_record_pool_v2::enqueue(
      const envelope_record_v2 &record,
      uint64_t current_height)
  {
    scoped_lock lock(*this);
    const relay_record_status_v2 status =
        enqueue_locked(record, current_height, nullptr);
    // A directly enqueued record was never verified against the chain; the
    // next sync_tip() must look at it even if the tip did not move.
    if (status == relay_record_status_v2::accepted)
      dirty_ = true;
    return status;
  }

  relay_record_status_v2 relay_record_pool_v2::enqueue_locked(
      const envelope_record_v2 &record,
      uint64_t current_height,
      commit_journal_v2 *journal)
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

    prune_expired_locked(current_height, journal);
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
        erase_at_locked(
            static_cast<size_t>(duplicate - records_.begin()), journal);
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
    if (journal != nullptr)
    {
      commit_journal_v2::operation_v2 operation{};
      operation.inserted = true;
      operation.record_index = records_.size();
      operation.stored.id = id;
      journal->operations.push_back(std::move(operation));
    }
    records_.push_back({id, record, deadline_height, slot, context});
    return relay_record_status_v2::accepted;
  }

  void relay_record_pool_v2::erase_at_locked(
      const size_t index, commit_journal_v2 *journal)
  {
    relay_item_v2 item{};
    size_t queue_index = 0;
    const bool queued = queue_.take(records_[index].id, item, queue_index);
    if (journal != nullptr)
    {
      commit_journal_v2::operation_v2 operation{};
      operation.queued = queued;
      operation.record_index = index;
      operation.queue_index = queue_index;
      operation.stored = records_[index];
      operation.item = item;
      journal->operations.push_back(std::move(operation));
    }
    records_.erase(records_.begin() + index);
  }

  std::vector<relay_record_pool_v2::stored_record_v2>::iterator
  relay_record_pool_v2::find_record_locked(const crypto::hash &id)
  {
    return std::find_if(records_.begin(), records_.end(),
        [&](const stored_record_v2 &stored) { return stored.id == id; });
  }

  void relay_record_pool_v2::rollback_locked(const commit_journal_v2 &journal)
  {
    for (auto operation = journal.operations.rbegin();
        operation != journal.operations.rend(); ++operation)
    {
      if (operation->inserted)
      {
        queue_.erase(operation->stored.id);
        const auto found = find_record_locked(operation->stored.id);
        if (found != records_.end())
          records_.erase(found);
        continue;
      }
      records_.insert(
          records_.begin() + std::min(operation->record_index, records_.size()),
          operation->stored);
      if (operation->queued)
        queue_.restore(operation->queue_index, operation->item);
    }
  }

  void relay_record_pool_v2::prune_expired(uint64_t current_height)
  {
    scoped_lock lock(*this);
    prune_expired_locked(current_height, nullptr);
  }

  void relay_record_pool_v2::prune_expired_locked(
      const uint64_t current_height, commit_journal_v2 *journal)
  {
    const uint64_t expired_receipts = std::count_if(
        records_.begin(), records_.end(), [current_height](
            const stored_record_v2 &stored) {
          return !stored.receipt_slot.empty()
              && stored.deadline_height < current_height;
        });
    increment_saturated(
        diagnostics_.expired_round_receipts, expired_receipts);
    for (size_t index = records_.size(); index-- > 0;)
      if (records_[index].deadline_height < current_height)
        erase_at_locked(index, journal);
    compact_receipt_slots(current_height, journal);
  }

  void relay_record_pool_v2::prune_unusable_receipts(
      const uint64_t current_height,
      const semantic_state_v2 &canonical_state,
      const canonical_context_source_v2 &contexts)
  {
    scoped_lock lock(*this);
    sync_locked(crypto::null_hash, current_height, canonical_state, contexts);
  }

  uint64_t relay_record_pool_v2::sync_tip(
      const crypto::hash &tip_hash,
      const uint64_t current_height,
      const semantic_state_v2 &canonical_state,
      const canonical_context_source_v2 &contexts)
  {
    scoped_lock lock(*this);
    if (!synced_ || dirty_ || tip_hash != synced_tip_
        || current_height != synced_height_
        || canonical_state.valid() != synced_state_valid_)
      sync_locked(tip_hash, current_height, canonical_state, contexts);
    return generation_;
  }

  void relay_record_pool_v2::note_tip_changed()
  {
    scoped_lock lock(*this);
    dirty_ = true;
  }

  void relay_record_pool_v2::clear()
  {
    scoped_lock lock(*this);
    queue_ = deadline_relay_queue_v2(queue_limits_);
    records_.clear();
    authenticated_receipts_.clear();
    diagnostics_ = {};
    synced_ = false;
    dirty_ = true;
    synced_state_valid_ = false;
    synced_tip_ = crypto::null_hash;
    synced_height_ = 0;
    ++generation_;
  }

  void relay_record_pool_v2::sync_locked(
      const crypto::hash &tip_hash,
      const uint64_t current_height,
      const semantic_state_v2 &canonical_state,
      const canonical_context_source_v2 &contexts)
  {
    increment_saturated(diagnostics_.prune_runs);
    prune_expired_locked(current_height, nullptr);
    prune_authenticated_locked(current_height);
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
    erase_confirmed_locked(canonical_lifecycles);

    if (current_height < receipt_slot_dedup_height_)
    {
      // No receipt is verified below the hardening height, so no verdict
      // may be carried across it.
      for (stored_record_v2 &stored : records_)
        if (stored.verified)
        {
          stored.verified = false;
          increment_saturated(diagnostics_.cache_invalidations);
        }
    }
    else if (!records_.empty())
    {
      std::vector<crypto::hash> remove;
      std::map<std::pair<uint64_t, uint64_t>, crypto::hash> round_contexts;
      // One isolated preview for the sweep, copied only if a receipt has to
      // be verified again; each probe is journaled and undone.
      std::unique_ptr<semantic_state_v2> next;
      uint64_t verifications = 0;
      const auto start = std::chrono::steady_clock::now();
      for (stored_record_v2 &stored : records_)
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
          if (stored.verified && stored.verified_epoch == challenge.epoch)
            remember_authenticated_locked(
                stored.id, stored.record, challenge.epoch, stored.verified_context);
          remove.push_back(stored.id);
          continue;
        }
        const std::pair<uint64_t, uint64_t> round_key{challenge.epoch, challenge.round};
        auto context = round_contexts.find(round_key);
        if (context == round_contexts.end())
          context = round_contexts.emplace(round_key, receipt_cache_context(
              canonical_state, contexts, challenge.epoch, challenge.round,
              current_height)).first;
        if (stored.verified && stored.verified_epoch == challenge.epoch
            && stored.verified_context == context->second)
        {
          increment_saturated(diagnostics_.cache_hits);
          continue;
        }
        if (stored.verified)
        {
          stored.verified = false;
          increment_saturated(diagnostics_.cache_invalidations);
        }
        increment_saturated(diagnostics_.prune_rechecked_receipts);
        if (!next)
          next.reset(new semantic_state_v2(canonical_state));
        auto undo = next->checkpoint();
        semantic_apply_summary_v2 summary{};
        const semantic_transaction_context_v2 transaction{
            current_height, true, nullptr, nullptr};
        const semantic_status_v2 status = next->apply_transaction(
            {stored.record}, transaction, contexts, summary, &undo);
        next->rollback(undo);
        ++verifications;
        if (status == semantic_status_v2::accepted)
        {
          stored.verified = true;
          stored.verified_epoch = challenge.epoch;
          stored.verified_context = context->second;
        }
        else if (status != semantic_status_v2::context_unavailable)
        {
          increment_saturated(diagnostics_.invalid_context_receipts);
          remove.push_back(stored.id);
        }
      }
      if (verifications != 0)
        add_verification_time(elapsed_microseconds(start), verifications);
      erase_confirmed_locked(remove);
    }

    synced_ = true;
    dirty_ = false;
    synced_state_valid_ = canonical_state.valid();
    synced_tip_ = tip_hash;
    synced_height_ = current_height;
    ++generation_;
  }

  void relay_record_pool_v2::compact_receipt_slots(
      const uint64_t current_height, commit_journal_v2 *journal)
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
      if (remove[index])
        erase_at_locked(index, journal);
  }

  relay_record_status_v2 relay_record_pool_v2::select_for_template(
      uint64_t current_height,
      std::vector<relay_record_selection_v2> &selected,
      const semantic_state_v2 *canonical_state)
  {
    selected.clear();
    scoped_lock lock(*this);
    prune_expired_locked(current_height, nullptr);
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
    scoped_lock lock(*this);
    erase_confirmed_locked(ids);
  }

  void relay_record_pool_v2::erase_confirmed_locked(const std::vector<crypto::hash> &ids)
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
    scoped_lock lock(*this);
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
          {
            if (stored.verified)
              remember_authenticated_locked(
                  stored.id, stored.record, stored.verified_epoch,
                  stored.verified_context);
            ids.push_back(stored.id);
          }
        continue;
      }
      std::string encoded;
      envelope_budget_v2 ignored{};
      if (encode_envelope_v2({record}, envelope_limits_, encoded, ignored)
          == envelope_status_v2::accepted)
        ids.push_back(relay_id(encoded));
    }
    erase_confirmed_locked(ids);
    increment_saturated(diagnostics_.canonical_receipts, canonical_receipts);
  }

  relay_pool_diagnostics_v2 relay_record_pool_v2::diagnostics() const
  {
    scoped_lock lock(*this);
    relay_pool_diagnostics_v2 result = diagnostics_;
    result.queue_items = records_.size();
    result.queue_bytes = queue_.bytes();
    return result;
  }

  void relay_record_pool_v2::record_ingress_rejection(
      const relay_record_status_v2 status)
  {
    scoped_lock lock(*this);
    record_ingress_rejection_locked(status);
  }

  void relay_record_pool_v2::record_ingress_rejection_locked(
      const relay_record_status_v2 status)
  {
    if (status == relay_record_status_v2::expired)
      increment_saturated(diagnostics_.expired_round_receipts);
    else if (status == relay_record_status_v2::full)
      increment_saturated(diagnostics_.capacity_rejections);
  }

  void relay_record_pool_v2::restore_policy_counters(
      const relay_pool_diagnostics_v2 &backup)
  {
    // Cost counters are kept; policy counters follow the committed pool.
    diagnostics_.exact_duplicates = backup.exact_duplicates;
    diagnostics_.slot_variants = backup.slot_variants;
    diagnostics_.expired_round_receipts = backup.expired_round_receipts;
    diagnostics_.invalid_context_receipts = backup.invalid_context_receipts;
    diagnostics_.canonical_receipts = backup.canonical_receipts;
    diagnostics_.capacity_rejections = backup.capacity_rejections;
    diagnostics_.template_selected_receipts = backup.template_selected_receipts;
  }

  void relay_record_pool_v2::mark_verified_locked(
      const crypto::hash &id, const uint64_t epoch, const crypto::hash &context)
  {
    if (synced_height_ < receipt_slot_dedup_height_)
      return;
    const auto found = find_record_locked(id);
    if (found == records_.end())
      return;
    found->verified = true;
    found->verified_epoch = epoch;
    found->verified_context = context;
  }

  void relay_record_pool_v2::remember_authenticated_locked(
      const crypto::hash &id, const envelope_record_v2 &record,
      const uint64_t epoch, const crypto::hash &context)
  {
    // Late replays keep arriving after the evidence deadline of the epoch;
    // keep the proof through the next epoch's deadline.
    uint64_t retain_until = 0;
    if (queue_limits_.max_items == 0
        || epoch == std::numeric_limits<uint64_t>::max()
        || !timing_.evidence_deadline(epoch + 1, retain_until))
      return;
    authenticated_receipt_v2 &entry = authenticated_receipts_[id];
    entry.record = record;
    entry.epoch = epoch;
    entry.retain_until = retain_until;
    entry.context = context;
    while (authenticated_receipts_.size() > queue_limits_.max_items)
    {
      auto oldest = authenticated_receipts_.begin();
      for (auto it = authenticated_receipts_.begin();
          it != authenticated_receipts_.end(); ++it)
        if (it->second.retain_until < oldest->second.retain_until)
          oldest = it;
      authenticated_receipts_.erase(oldest);
    }
  }

  void relay_record_pool_v2::prune_authenticated_locked(const uint64_t current_height)
  {
    for (auto it = authenticated_receipts_.begin(); it != authenticated_receipts_.end();)
      if (it->second.retain_until < current_height)
        it = authenticated_receipts_.erase(it);
      else
        ++it;
  }

  void relay_record_pool_v2::add_verification_time(
      const uint64_t microseconds, const uint64_t verifications)
  {
    increment_saturated(diagnostics_.verifications, verifications);
    increment_saturated(diagnostics_.verification_microseconds, microseconds);
  }

  size_t relay_record_pool_v2::size() const
  {
    scoped_lock lock(*this);
    return records_.size();
  }

  size_t relay_record_pool_v2::bytes() const
  {
    scoped_lock lock(*this);
    return queue_.bytes();
  }

  std::vector<crypto::hash> relay_record_pool_v2::record_ids() const
  {
    scoped_lock lock(*this);
    std::vector<crypto::hash> ids;
    ids.reserve(records_.size());
    for (const stored_record_v2 &stored : records_)
      ids.push_back(stored.id);
    return ids;
  }

  bool relay_record_pool_v2::admit_cached_duplicates(
      const std::vector<std::string> &envelopes,
      const envelope_limits_v2 &envelope_limits,
      const relay_policy_v2 &policy,
      std::vector<std::string> &accepted)
  {
    if (envelopes.empty() || !batch_size_valid(envelopes, envelope_limits, policy))
      return false;
    // Parse outside the lock. Only an all-receipt batch can take this path.
    std::vector<crypto::hash> ids;
    std::vector<envelope_record_v2> parsed;
    std::vector<bool> forward;
    ids.reserve(envelopes.size());
    parsed.reserve(envelopes.size());
    forward.reserve(envelopes.size());
    std::map<std::string, size_t> slots;
    for (const std::string &encoded : envelopes)
    {
      std::vector<envelope_record_v2> records;
      envelope_budget_v2 ignored{};
      if (parse_envelope_v2(encoded, envelope_limits, records, ignored)
              != envelope_status_v2::accepted
          || records.size() != 1
          || records.front().type
              != static_cast<uint8_t>(record_type_v2::service_receipt))
        return false;
      const std::string slot = receipt_slot(records.front());
      if (slot.empty())
        return false;
      std::string canonical;
      envelope_budget_v2 budget{};
      if (encode_envelope_v2({records.front()}, envelope_limits_, canonical, budget)
          != envelope_status_v2::accepted)
        return false;
      const crypto::hash id = relay_id(canonical);
      const auto inserted = slots.emplace(slot, ids.size());
      if (!inserted.second)
      {
        // A repeat of the same record is classified skip, not forwarded.
        if (ids[inserted.first->second] != id
            || !same_record(parsed[inserted.first->second], records.front()))
          return false;
        forward.push_back(false);
        continue;
      }
      ids.push_back(id);
      parsed.push_back(std::move(records.front()));
      forward.push_back(true);
    }

    scoped_lock lock(*this);
    if (!synced_ || dirty_ || !synced_state_valid_
        || synced_height_ < policy.receipt_slot_dedup_height
        || synced_height_ < receipt_slot_dedup_height_)
      return false;
    for (size_t i = 0; i < ids.size(); ++i)
    {
      const auto found = find_record_locked(ids[i]);
      if (found == records_.end() || !found->verified
          || !same_record(found->record, parsed[i])
          || found->deadline_height < synced_height_)
        return false;
    }
    // Each is an idempotent duplicate of a receipt verified at this tip,
    // which v2.0.5 forwards again.
    const uint64_t count = ids.size();
    increment_saturated(diagnostics_.admissions);
    increment_saturated(diagnostics_.exact_duplicates, count);
    increment_saturated(diagnostics_.duplicates_short_circuited, count);
    increment_saturated(diagnostics_.cache_hits, count);
    accepted.clear();
    for (size_t i = 0; i < envelopes.size(); ++i)
      if (forward[i])
        accepted.push_back(envelopes[i]);
    return true;
  }

  bool relay_record_pool_v2::prepare_admission(
      admission_plan_v2 &plan,
      const relay_tip_snapshot_v2 &snapshot,
      const relay_policy_v2 &policy)
  {
    scoped_lock lock(*this);
    if (!synced_ || dirty_ || generation_ != snapshot.generation)
      return false;
    plan.slot_dedup = snapshot.height >= receipt_slot_dedup_height_;
    plan.occupants.clear();
    if (plan.slot_dedup)
      for (const relay_plan_entry_v2 &entry : plan.entries)
      {
        if (!entry.coupled || plan.occupants.count(entry.slot) != 0)
          continue;
        // enqueue_locked prunes expired records, keeps the lowest id of a
        // slot, then compares the new record with that occupant.
        admission_plan_v2::slot_occupant_v2 occupant{};
        for (const stored_record_v2 &stored : records_)
          if (stored.receipt_slot == entry.slot
              && stored.deadline_height >= snapshot.height
              && (!occupant.present || id_less(stored.id, occupant.id)))
          {
            occupant.present = true;
            occupant.context = stored.receipt_context;
            occupant.id = stored.id;
          }
        plan.occupants.emplace(entry.slot, std::move(occupant));
      }
    if (snapshot.height < policy.receipt_slot_dedup_height
        || snapshot.height < receipt_slot_dedup_height_)
      return true;
    for (relay_plan_entry_v2 &entry : plan.entries)
    {
      // A coupled entry is applied to the batch preview, so the later
      // receipt of its slot is classified against it.
      if (!entry.hardened_receipt || !entry.has_id || entry.repeat
          || entry.coupled)
        continue;
      const auto found = find_record_locked(entry.id);
      entry.cached = found != records_.end() && found->verified
          && same_record(found->record, entry.record);
    }
    for (relay_plan_entry_v2 &entry : plan.entries)
    {
      if (!entry.hardened_receipt || !entry.has_id || entry.repeat || entry.cached)
        continue;
      const auto found = authenticated_receipts_.find(entry.id);
      if (found == authenticated_receipts_.end()
          || found->second.epoch != entry.epoch
          || !same_record(found->second.record, entry.record))
        continue;
      entry.replay_hint = true;
      entry.replay_context = found->second.context;
    }
    return true;
  }

  bool relay_record_pool_v2::commit_admission(
      admission_plan_v2 &plan,
      const relay_tip_snapshot_v2 &snapshot,
      relay_ingress_status_v2 &status,
      std::vector<std::string> &accepted)
  {
    scoped_lock lock(*this);
    add_verification_time(plan.verification_microseconds, plan.verifications);
    increment_saturated(diagnostics_.duplicates_short_circuited, plan.authenticated_replays);
    increment_saturated(diagnostics_.cache_hits, plan.authenticated_replays);
    plan.verifications = 0;
    plan.verification_microseconds = 0;
    plan.authenticated_replays = 0;
    // A proof of authenticity is a pure function of the record and its
    // context hash, so it is kept whatever this commit's outcome.
    if (synced_height_ >= receipt_slot_dedup_height_)
      for (const relay_plan_entry_v2 &entry : plan.entries)
        if (entry.authenticated && entry.has_id)
          remember_authenticated_locked(
              entry.id, entry.record, entry.epoch, entry.authenticated_context);
    // Never commit a verdict computed for another tip.
    if (!synced_ || dirty_ || generation_ != snapshot.generation)
      return false;

    const uint64_t inclusion_height = snapshot.height;
    const relay_pool_diagnostics_v2 backup = diagnostics_;
    commit_journal_v2 journal;
    std::vector<bool> forward(plan.entries.size(), false);
    const auto abort = [&](const relay_ingress_status_v2 result) {
      rollback_locked(journal);
      restore_policy_counters(backup);
      increment_saturated(diagnostics_.admissions);
      status = result;
      return true;
    };
    const auto replan = [&]() {
      rollback_locked(journal);
      restore_policy_counters(backup);
      return false;
    };

    for (size_t index = 0; index < plan.entries.size(); ++index)
    {
      relay_plan_entry_v2 &entry = plan.entries[index];
      if (entry.verdict == relay_verdict_v2::skip)
        continue;
      if (terminal(entry.verdict))
        return abort(verdict_status(entry.verdict));

      if (entry.cached)
      {
        const auto found = find_record_locked(entry.id);
        if (found == records_.end() || !found->verified)
          return replan();
        increment_saturated(diagnostics_.duplicates_short_circuited);
        increment_saturated(diagnostics_.cache_hits);
      }

      const relay_record_status_v2 queue_status =
          enqueue_locked(entry.record, inclusion_height, &journal);
      if (queue_status == relay_record_status_v2::expired)
      {
        abort(relay_ingress_status_v2::expired);
        record_ingress_rejection_locked(queue_status);
        return true;
      }
      if (queue_status == relay_record_status_v2::full)
      {
        abort(relay_ingress_status_v2::capacity_exhausted);
        record_ingress_rejection_locked(queue_status);
        return true;
      }
      if (queue_status != relay_record_status_v2::accepted
          && queue_status != relay_record_status_v2::idempotent_duplicate
          && queue_status != relay_record_status_v2::receipt_slot_variant)
        return abort(relay_ingress_status_v2::invalid_batch);

      // A changed slot occupant means the later receipt of this slot was
      // verified against the wrong preview.
      if (entry.coupled
          && (queue_status == relay_record_status_v2::receipt_slot_variant)
              != entry.predicted_variant)
        return replan();
      if (queue_status == relay_record_status_v2::receipt_slot_variant)
        continue;
      if (entry.hardened_receipt && !entry.cached)
        mark_verified_locked(entry.id, entry.epoch, entry.context);
      if (queue_status == relay_record_status_v2::idempotent_duplicate)
      {
        // The duplicate is already pending locally, but it still belongs in
        // this batch's semantic preview.  Otherwise a following admission or
        // lifecycle record could lose a valid in-order dependency merely
        // because its prerequisite arrived in an earlier relay wave.
        // Preserve v2.0.5 receipt redelivery: a downstream miner may have
        // restarted. The transport limiter bounds each envelope per peer.
        if (entry.hardened_receipt)
          forward[index] = true;
        continue;
      }
      forward[index] = true;
    }

    // A newly accepted enrollment member carries its received prerequisites,
    // including canonical or pending L. All-duplicate enrollment groups stay
    // silent; receipt retries do not reopen unrelated enrollment groups.
    std::vector<envelope_record_v2> received_records;
    received_records.reserve(plan.entries.size());
    for (const relay_plan_entry_v2 &entry : plan.entries)
      received_records.push_back(entry.record);
    const auto newly_forwardable = forward;
    for (const auto &group : enrollment_template_groups_v2(received_records,
        snapshot.state->nettype(), snapshot.state->genesis_hash(),
        snapshot.state->parameter_set_hash()))
      if (std::any_of(group.begin(), group.end(), [&](size_t i) { return newly_forwardable[i]; }))
        for (auto i : group)
          forward[i] = true;
    accepted.clear();
    for (size_t i = 0; i < plan.entries.size(); ++i)
      if (forward[i])
        accepted.push_back(*plan.entries[i].envelope);
    increment_saturated(diagnostics_.admissions);
    status = relay_ingress_status_v2::accepted;
    return true;
  }

  relay_ingress_status_v2 admit_relay_envelopes_v2(
      const std::vector<std::string> &envelopes,
      const envelope_limits_v2 &envelope_limits,
      const relay_policy_v2 &policy,
      relay_chain_view_v2 &chain,
      relay_record_pool_v2 &pool,
      std::vector<std::string> &accepted)
  {
    accepted.clear();
    if (!envelope_limits.valid() || !policy.valid() || !pool.valid()
        || envelopes.size() > policy.queue.max_items)
      return relay_ingress_status_v2::invalid_configuration;

    // Exact pending receipts verified at the synced tip: no chain lock, copy
    // or verification.
    if (pool.admit_cached_duplicates(envelopes, envelope_limits, policy, accepted))
      return relay_ingress_status_v2::accepted;

    // Parse once; the chain lock is then held only to capture contexts.
    relay_record_pool_v2::admission_plan_v2 plan;
    relay_context_requests_v2 requests;
    plan.entries.resize(envelopes.size());
    // Distinct receipts of the batch, by slot.
    std::map<std::string, std::vector<size_t>> distinct_by_slot;
    // The last verified copy of each lifecycle or lease record, by id, and
    // the last verified lifecycle or lease record of each identity.
    std::map<crypto::hash, size_t, relay_record_pool_v2::id_order_v2> last_copy;
    std::map<crypto::hash, size_t, relay_record_pool_v2::id_order_v2> last_touch;
    const auto collapse = [&](relay_plan_entry_v2 &entry, const size_t index,
        const crypto::hash &identity_id, const bool lease) {
      if (!entry.has_id)
        return;
      const auto copy = last_copy.find(entry.id);
      if (copy != last_copy.end()
          && same_record(plan.entries[copy->second].record, entry.record)
          && (!lease || last_touch[identity_id] == copy->second))
      {
        entry.repeat_of = copy->second;
        return;
      }
      last_copy[entry.id] = index;
      last_touch[identity_id] = index;
    };
    for (size_t index = 0; index < envelopes.size(); ++index)
    {
      relay_plan_entry_v2 &entry = plan.entries[index];
      entry.envelope = &envelopes[index];
      std::vector<envelope_record_v2> records;
      envelope_budget_v2 ignored{};
      if (parse_envelope_v2(envelopes[index], envelope_limits, records, ignored)
              != envelope_status_v2::accepted
          || records.size() != 1)
        continue;
      entry.parsed = true;
      entry.record = std::move(records.front());
      std::string canonical;
      envelope_budget_v2 budget{};
      if (encode_envelope_v2({entry.record}, pool.envelope_limits_, canonical, budget)
          == envelope_status_v2::accepted)
      {
        entry.has_id = true;
        entry.id = relay_id(canonical);
      }
      authenticated_service_receipt_v2 receipt{};
      admission_lease_v2 lease{};
      lifecycle_record_v2 lifecycle{};
      if (entry.record.type == static_cast<uint8_t>(record_type_v2::service_receipt)
          && decode_service_receipt_record_structure_v2(entry.record, receipt)
              == record_codec_status_v2::accepted)
      {
        entry.epoch = receipt.challenge.epoch;
        entry.round = receipt.challenge.round;
        requests.round_anchors.push_back({entry.epoch, entry.round});
        entry.slot = receipt_slot(entry.record);
        entry.slot_context = receipt_context(entry.record);
        if (entry.has_id && !entry.slot.empty())
        {
          // Dedup the batch before classifying coupling: only distinct
          // receipts of one slot depend on each other's queue outcome.
          std::vector<size_t> &distinct = distinct_by_slot[entry.slot];
          entry.repeat = std::any_of(distinct.begin(), distinct.end(), [&](size_t i) {
            return plan.entries[i].id == entry.id
                && same_record(plan.entries[i].record, entry.record);
          });
          if (!entry.repeat)
            distinct.push_back(index);
        }
      }
      else if (decode_admission_lease_record_structure_v2(entry.record, lease)
          == record_codec_status_v2::accepted)
      {
        requests.block_heights.push_back(lease.admission_context_height);
        collapse(entry, index, lease.member.identity_id, true);
      }
      else if (decode_lifecycle_record_structure_v2(entry.record, lifecycle)
          == record_codec_status_v2::accepted)
        collapse(entry, index, lifecycle.next_descriptor.identity_id, false);
    }
    for (relay_plan_entry_v2 &entry : plan.entries)
      entry.coupled = !entry.repeat && !entry.slot.empty() && entry.has_id
          && distinct_by_slot[entry.slot].size() > 1;
    std::sort(requests.block_heights.begin(), requests.block_heights.end());
    requests.block_heights.erase(std::unique(
        requests.block_heights.begin(), requests.block_heights.end()),
        requests.block_heights.end());
    std::sort(requests.round_anchors.begin(), requests.round_anchors.end());
    requests.round_anchors.erase(std::unique(
        requests.round_anchors.begin(), requests.round_anchors.end()),
        requests.round_anchors.end());

    for (;;)
    {
      relay_tip_snapshot_v2 snapshot;
      if (!chain.capture(pool, requests, snapshot) || !snapshot.state
          || !snapshot.state->valid())
        return relay_ingress_status_v2::invalid_configuration;
      if (!batch_size_valid(envelopes, envelope_limits, policy))
        return relay_ingress_status_v2::invalid_batch;

      const uint64_t inclusion_height = snapshot.height;
      for (relay_plan_entry_v2 &entry : plan.entries)
      {
        entry.hardened_receipt = entry.parsed
            && inclusion_height >= policy.receipt_slot_dedup_height
            && entry.record.type
                == static_cast<uint8_t>(record_type_v2::service_receipt);
        entry.cached = false;
        entry.predicted_variant = false;
        entry.verdict = relay_verdict_v2::pending;
        entry.context = crypto::null_hash;
        entry.replay_hint = false;
        entry.replay_context = crypto::null_hash;
        entry.authenticated = false;
        entry.authenticated_context = crypto::null_hash;
      }

      if (!pool.prepare_admission(plan, snapshot, policy))
        continue;
      // Each distinct record once, against the immutable snapshot, off-lock.
      std::unique_ptr<semantic_state_v2> semantic;
      auto occupants = plan.occupants;
      const auto start = std::chrono::steady_clock::now();
      for (relay_plan_entry_v2 &entry : plan.entries)
      {
        if (entry.repeat)
        {
          entry.verdict = relay_verdict_v2::skip;
          continue;
        }
        if (collapsed(entry))
        {
          // The earlier copy was classified first; a terminal verdict there
          // already ended the loop.
          entry.verdict = plan.entries[entry.repeat_of].verdict;
          continue;
        }
        if (entry.cached)
        {
          entry.verdict = relay_verdict_v2::enqueue;
          continue;
        }
        if (classify_authenticated_replay(entry,
                semantic ? *semantic : *snapshot.state, *snapshot.state,
                snapshot.contexts, inclusion_height, entry.verdict))
        {
          ++plan.authenticated_replays;
          if (terminal(entry.verdict))
            break;
          continue;
        }
        if (!semantic)
          semantic.reset(new semantic_state_v2(*snapshot.state));
        chain.before_verification();
        semantic_state_v2::undo_journal undo{};
        entry.verdict = verify_entry(
            entry, *semantic, *snapshot.state, snapshot.contexts,
            inclusion_height, entry.coupled ? &undo : nullptr);
        ++plan.verifications;
        if (terminal(entry.verdict))
          break;
        if (!entry.coupled || !plan.slot_dedup
            || entry.verdict != relay_verdict_v2::enqueue)
          continue;
        // Mirror enqueue_locked's slot decision for the planned occupant.
        auto &occupant = occupants[entry.slot];
        if (occupant.present && occupant.context == entry.slot_context)
        {
          if (occupant.id != entry.id)
          {
            entry.predicted_variant = true;
            semantic->rollback(undo);
          }
          continue;
        }
        occupant.present = true;
        occupant.context = entry.slot_context;
        occupant.id = entry.id;
      }
      plan.verification_microseconds += elapsed_microseconds(start);

      relay_ingress_status_v2 status = relay_ingress_status_v2::accepted;
      std::vector<std::string> next_accepted;
      if (pool.commit_admission(plan, snapshot, status, next_accepted))
      {
        accepted.swap(next_accepted);
        return status;
      }
    }
  }

  namespace
  {
    // A caller-owned canonical view that does not change during the call.
    class borrowed_chain_view_v2 final : public relay_chain_view_v2
    {
    public:
      borrowed_chain_view_v2(
          uint64_t height,
          const semantic_state_v2 &state,
          const canonical_context_source_v2 &contexts)
        : height_(height), state_(state), contexts_(contexts) {}

      bool capture(
          relay_record_pool_v2 &pool,
          const relay_context_requests_v2 &requests,
          relay_tip_snapshot_v2 &snapshot) override
      {
        // No tip identity is known, so every capture re-syncs.
        pool.note_tip_changed();
        snapshot.generation =
            pool.sync_tip(crypto::null_hash, height_, state_, contexts_);
        snapshot.height = height_;
        snapshot.state = std::shared_ptr<const semantic_state_v2>(
            std::shared_ptr<const semantic_state_v2>(), &state_);
        requests.capture(contexts_, snapshot.contexts);
        return true;
      }

    private:
      uint64_t height_;
      const semantic_state_v2 &state_;
      const canonical_context_source_v2 &contexts_;
    };
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
    // The cached fast path is only sound against a tracked tip.
    pool.note_tip_changed();
    borrowed_chain_view_v2 chain(inclusion_height, canonical_state, contexts);
    return admit_relay_envelopes_v2(
        envelopes, envelope_limits, policy, chain, pool, accepted);
  }
} // namespace epose
} // namespace qwertycoin
