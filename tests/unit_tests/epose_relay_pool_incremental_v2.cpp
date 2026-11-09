// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

// The incremental relay pool against the full-prune reference
// (epose_relay_pool_reference_v2).

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <future>
#include <limits>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "epose/record_codec_v2.h"
#include "epose/relay_pool_v2.h"
#include "epose/semantic_batch_v2.h"
#include "epose/service_receipt_v2.h"
#include "epose_relay_pool_reference_v2.h"

namespace
{
  using namespace qwertycoin::epose;
  namespace reference = qwertycoin::epose::reference_v2;

  crypto::hash hash_text(const std::string &text)
  {
    return crypto::cn_fast_hash(text.data(), text.size());
  }

  struct keys
  {
    crypto::public_key public_key{};
    crypto::secret_key secret_key{};
  };

  keys make_keys()
  {
    keys out{};
    crypto::generate_keys(out.public_key, out.secret_key);
    return out;
  }

  struct identity
  {
    keys service = make_keys();
    keys operator_auth = make_keys();
    keys reward_view = make_keys();
    keys reward_spend = make_keys();
    identity_descriptor_v2 descriptor{};
  };

  class contexts final : public canonical_context_source_v2
  {
  public:
    crypto::hash admission = hash_text("admission-anchor");
    crypto::hash next_admission = hash_text("next-admission-anchor");
    crypto::hash round = hash_text("committee-anchor");
    // Epoch 3, round 1: a block hash after the epoch start, so a reorg can
    // replace it without touching the frozen snapshot. Null: unavailable.
    crypto::hash round1{};

    bool block_hash(uint64_t height, crypto::hash &hash) const override
    {
      hash = {};
      if (height == 1440)
        hash = admission;
      else if (height == 2160)
        hash = next_admission;
      return hash != crypto::null_hash;
    }

    bool round_anchor(uint64_t epoch, uint64_t round_number, crypto::hash &hash) const override
    {
      hash = {};
      if (epoch != 3 || round_number > 1)
        return false;
      hash = round_number == 0 ? round : round1;
      return hash != crypto::null_hash;
    }
  };

  const crypto::hash genesis = hash_text("qwc-v2-genesis");
  const crypto::hash parameters = hash_text("parameter-set");
  const epoch_timing_v2 timing{1440, 720, 60};
  const admission_policy_v2 admission_policy{admission_work_algorithm_v2::randomx, 1};
  const committee_policy_v2 small_committee{1, 1, 1, 1, 1, {0}};

  // Epoch 3, round 0: inclusion window [2160, 2819], evidence deadline 2819.
  constexpr uint64_t window_first = 2160;
  constexpr uint64_t evidence_deadline = 2819;

  committee_policy_v2 wide_committee()
  {
    return {8, required_receipts_for_committee_size_v2(8), 1, 1, 1, {0}};
  }

  semantic_state_v2 make_state(const committee_policy_v2 &committee)
  {
    return {cryptonote::TESTNET, genesis, parameters, timing, admission_policy, committee};
  }

  envelope_limits_v2 relay_envelope_limits()
  {
    envelope_limits_v2 limits{};
    limits.max_envelope_bytes = 2048;
    limits.max_records = 4;
    limits.max_record_payload_bytes = 512;
    limits.max_signature_verifications = 8;
    limits.max_admission_verifications = 4;
    limits.supported_record_versions = {0, 1, 1, 1, 1, 1};
    return limits;
  }

  relay_policy_v2 relay_policy(uint64_t dedup_height = 0)
  {
    return {
        relay_queue_limits_v2{1024, 1 << 20, 64, 896, 1 << 16, 917504},
        relay_template_limits_v2{32, 32768, 8, 24, 8192, 24576},
        dedup_height};
  }

  relay_record_pool_v2 new_pool(
      const committee_policy_v2 &committee, uint64_t dedup_height = 0)
  {
    const relay_policy_v2 policy = relay_policy(dedup_height);
    return {timing, committee, relay_envelope_limits(),
        policy.queue, policy.mining_template,
        policy.receipt_slot_dedup_height};
  }

  reference::relay_record_pool_v2 old_pool(
      const committee_policy_v2 &committee, uint64_t dedup_height = 0)
  {
    const relay_policy_v2 policy = relay_policy(dedup_height);
    return {timing, committee, relay_envelope_limits(),
        policy.queue, policy.mining_template,
        policy.receipt_slot_dedup_height};
  }

  identity make_identity(const std::string &endpoint, uint64_t effective_epoch = 3)
  {
    identity out{};
    out.descriptor.identity_id = derive_identity_id_v2(
        cryptonote::TESTNET, genesis, parameters, out.operator_auth.public_key);
    out.descriptor.service_public_key = out.service.public_key;
    out.descriptor.operator_authorization_public_key = out.operator_auth.public_key;
    out.descriptor.reward_address.m_view_public_key = out.reward_view.public_key;
    out.descriptor.reward_address.m_spend_public_key = out.reward_spend.public_key;
    out.descriptor.endpoint_descriptor_hash = hash_text(endpoint);
    out.descriptor.effective_epoch = effective_epoch;
    out.descriptor.expiry_epoch = 8;
    return out;
  }

  envelope_record_v2 registration_record(const identity &value)
  {
    lifecycle_record_v2 lifecycle{};
    lifecycle.next_descriptor = value.descriptor;
    EXPECT_TRUE(sign_lifecycle_record_v2(
        cryptonote::TESTNET, genesis, parameters, lifecycle,
        value.operator_auth.secret_key, value.service.secret_key));
    envelope_record_v2 record{};
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_lifecycle_record_v2(lifecycle, cryptonote::TESTNET, genesis, parameters, record));
    return record;
  }

  envelope_record_v2 admission_record(
      const identity &value,
      const contexts &source,
      uint64_t target_epoch = 3,
      uint64_t context_height = 1440)
  {
    admission_lease_v2 lease{};
    lease.member.service_public_key = value.descriptor.service_public_key;
    lease.member.identity_id = value.descriptor.identity_id;
    lease.member.operator_authorization_public_key = value.descriptor.operator_authorization_public_key;
    lease.member.descriptor_hash = hash_identity_descriptor_v2(
        cryptonote::TESTNET, genesis, parameters, value.descriptor);
    lease.member.reward_binding_hash = hash_reward_binding_v2(
        cryptonote::TESTNET, genesis, parameters, value.descriptor.reward_address);
    lease.member.endpoint_descriptor_hash = value.descriptor.endpoint_descriptor_hash;
    lease.member.sequence = value.descriptor.sequence;
    lease.target_epoch = target_epoch;
    lease.work_algorithm = static_cast<uint8_t>(admission_policy.algorithm);
    lease.leading_zero_bits = admission_policy.leading_zero_bits;
    lease.admission_context_height = context_height;
    lease.admission_context_hash = context_height == 2160 ? source.next_admission : source.admission;
    const admission_context_v2 context{
        cryptonote::TESTNET, genesis, parameters,
        context_height, lease.admission_context_hash};
    do
    {
      lease.work_hash = calculate_admission_work_v2(lease, context);
      ++lease.nonce;
    } while (!admission_work_meets_target_v2(lease.work_hash, admission_policy.leading_zero_bits));
    --lease.nonce;
    lease.lease_hash = calculate_admission_lease_hash_v2(lease, context);
    envelope_record_v2 record{};
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_admission_lease_record_v2(lease, context, admission_policy, record));
    return record;
  }

  envelope_record_v2 receipt_record(
      const semantic_state_v2 &state,
      const identity &subject,
      const crypto::public_key &verifier_public_key,
      const crypto::secret_key &verifier_secret_key,
      const crypto::hash &anchor,
      const std::string &nonce_text,
      uint64_t round = 0)
  {
    const membership_snapshot_v2 *snapshot = state.membership().snapshot(3);
    EXPECT_NE(nullptr, snapshot);
    authenticated_service_receipt_v2 receipt{};
    receipt.challenge.epoch = 3;
    receipt.challenge.round = round;
    receipt.challenge.snapshot_hash = snapshot == nullptr
        ? crypto::null_hash : snapshot->snapshot_hash;
    receipt.challenge.anchor_hash = anchor;
    receipt.challenge.subject_public_key = subject.service.public_key;
    receipt.challenge.verifier_public_key = verifier_public_key;
    receipt.challenge.endpoint_descriptor_hash =
        subject.descriptor.endpoint_descriptor_hash;
    receipt.challenge.nonce = hash_text(nonce_text);
    receipt.challenge.requested_object_hash = anchor;
    receipt.response_object_hash = anchor;
    const receipt_context_v2 receipt_context{
        cryptonote::TESTNET, genesis, parameters};
    sign_subject_response_v2(receipt, subject.service.secret_key, receipt_context);
    sign_verifier_receipt_v2(receipt, verifier_secret_key, receipt_context);
    envelope_record_v2 record{};
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_service_receipt_record_v2(receipt, receipt_context, record));
    return record;
  }

  std::string envelope(const envelope_record_v2 &record)
  {
    std::string encoded;
    envelope_budget_v2 ignored{};
    EXPECT_EQ(envelope_status_v2::accepted,
        encode_envelope_v2({record}, relay_envelope_limits(), encoded, ignored));
    return encoded;
  }

  envelope_record_v2 record_of(const std::string &encoded)
  {
    std::vector<envelope_record_v2> records;
    envelope_budget_v2 ignored{};
    EXPECT_EQ(envelope_status_v2::accepted,
        parse_envelope_v2(encoded, relay_envelope_limits(), records, ignored));
    EXPECT_EQ(1u, records.size());
    return records.empty() ? envelope_record_v2{} : records.front();
  }

  // One enrolled, frozen epoch-3 membership on one canonical branch.
  struct network
  {
    contexts source;
    committee_policy_v2 committee;
    std::vector<identity> members;
    semantic_state_v2 state;

    network(size_t count, const committee_policy_v2 &policy,
        const std::string &round_anchor, const std::vector<identity> *reuse = nullptr)
      : committee(policy), state(make_state(policy))
    {
      source.round = hash_text(round_anchor);
      if (reuse != nullptr)
        members = *reuse;
      else
        for (size_t i = 0; i < count; ++i)
          members.push_back(make_identity("member-" + std::to_string(i)));
      for (const identity &member : members)
      {
        semantic_apply_summary_v2 summary{};
        EXPECT_EQ(semantic_status_v2::accepted, state.apply_transaction(
            {registration_record(member), admission_record(member, source)},
            semantic_transaction_context_v2{1500, false, nullptr}, source, summary));
      }
      EXPECT_EQ(pipeline_status_v2::accepted,
          state.freeze_membership(3, 2100, source.round));
    }

    const identity &by_key(const crypto::public_key &key) const
    {
      for (const identity &member : members)
        if (member.service.public_key == key)
          return member;
      ADD_FAILURE() << "unknown verifier";
      return members.front();
    }

    // The k-th canonical verifier of subject in epoch 3, round 0.
    const identity &verifier(size_t subject, size_t k = 0) const
    {
      const auto assigned = state.membership().committee(
          3, 0, members[subject].service.public_key, source.round);
      EXPECT_LT(k, assigned.size());
      return by_key(assigned[std::min(k, assigned.size() - 1)].verifier_public_key);
    }

    envelope_record_v2 receipt(size_t subject, size_t k, const std::string &nonce) const
    {
      const identity &signer = verifier(subject, k);
      return receipt_record(state, members[subject], signer.service.public_key,
          signer.service.secret_key, source.round, nonce);
    }
  };

  // The chain side as Blockchain implements it: tip-guarded sync under the
  // (here implicit) chain lock, one shared immutable state per tip.
  class test_chain final : public relay_chain_view_v2
  {
  public:
    std::shared_ptr<const semantic_state_v2> state;
    const canonical_context_source_v2 *source = nullptr;
    uint64_t height = 0;
    crypto::hash tip{};
    size_t captures = 0;
    // Simulates a block arriving between capture and commit, once.
    std::function<void(relay_record_pool_v2 &)> after_capture;
    // Runs before every off-lock record verification.
    std::function<void()> on_verify;

    void before_verification() override
    {
      if (on_verify)
        on_verify();
    }

    bool capture(
        relay_record_pool_v2 &pool,
        const relay_context_requests_v2 &requests,
        relay_tip_snapshot_v2 &snapshot) override
    {
      ++captures;
      snapshot.generation = pool.sync_tip(tip, height, *state, *source);
      snapshot.height = height;
      snapshot.state = state;
      requests.capture(*source, snapshot.contexts);
      if (after_capture)
      {
        auto once = std::move(after_capture);
        after_capture = nullptr;
        once(pool);
      }
      return true;
    }

    void move_to(std::shared_ptr<const semantic_state_v2> next_state,
        const canonical_context_source_v2 &next_source, uint64_t next_height,
        const std::string &tip_name, relay_record_pool_v2 &pool)
    {
      state = std::move(next_state);
      source = &next_source;
      height = next_height;
      tip = hash_text(tip_name);
      pool.note_tip_changed();
    }
  };

  relay_ingress_status_v2 admit_new(
      const std::vector<std::string> &envelopes, test_chain &chain,
      relay_record_pool_v2 &pool, std::vector<std::string> &accepted,
      uint64_t dedup_height = 0)
  {
    return admit_relay_envelopes_v2(envelopes, relay_envelope_limits(),
        relay_policy(dedup_height), chain, pool, accepted);
  }

  relay_ingress_status_v2 admit_old(
      const std::vector<std::string> &envelopes, const test_chain &chain,
      reference::relay_record_pool_v2 &pool, std::vector<std::string> &accepted,
      uint64_t dedup_height = 0)
  {
    return reference::admit_relay_envelopes_v2(envelopes, chain.height,
        relay_envelope_limits(), relay_policy(dedup_height), *chain.state,
        *chain.source, pool, accepted);
  }

  std::vector<crypto::hash> selection_ids(
      const std::vector<relay_record_selection_v2> &selected)
  {
    std::vector<crypto::hash> ids;
    for (const auto &entry : selected)
      ids.push_back(entry.id);
    return ids;
  }

  // Every receipt the template would carry must be accepted by the chain.
  bool selection_valid(
      const std::vector<relay_record_selection_v2> &selected,
      const semantic_state_v2 &state,
      const canonical_context_source_v2 &source,
      uint64_t height)
  {
    for (const auto &entry : selected)
    {
      if (entry.record.type != static_cast<uint8_t>(record_type_v2::service_receipt))
        continue;
      semantic_state_v2 probe = state;
      semantic_apply_summary_v2 summary{};
      if (probe.apply_transaction({entry.record},
              semantic_transaction_context_v2{height, true, nullptr, nullptr},
              source, summary) != semantic_status_v2::accepted)
        return false;
    }
    return true;
  }

  void expect_selection_valid(
      const std::vector<relay_record_selection_v2> &selected,
      const semantic_state_v2 &state,
      const canonical_context_source_v2 &source,
      uint64_t height)
  {
    EXPECT_TRUE(selection_valid(selected, state, source, height));
  }

  uint64_t verifications(const relay_record_pool_v2 &pool)
  {
    return pool.diagnostics().verifications;
  }
}

// ---------------------------------------------------------------------------
// Property test: random admit / block add / pop_block / reorg sequences.

namespace
{
  struct tip_entry
  {
    std::shared_ptr<const semantic_state_v2> state;
    const network *branch = nullptr;
    uint64_t height = 0;
    std::string name;
  };

  struct property_stats
  {
    size_t forwarded = 0;
    size_t max_pool = 0;
    size_t below = 0;
    size_t above = 0;
    size_t crossings = 0;
    size_t below_selected = 0;

    void add(const property_stats &other)
    {
      forwarded += other.forwarded;
      max_pool = std::max(max_pool, other.max_pool);
      below += other.below;
      above += other.above;
      crossings += other.crossings;
      below_selected += other.below_selected;
    }
  };

  void run_property_sequence(uint32_t seed, uint64_t start_height, size_t steps,
      uint64_t dedup_height = 0, property_stats *stats = nullptr)
  {
    std::mt19937 rng(seed);
    const network a(5, small_committee, "anchor-a");
    const network b(5, small_committee, "anchor-b", &a.members);
    ASSERT_NE(a.state.membership().snapshot(3)->snapshot_hash,
        b.state.membership().snapshot(3)->snapshot_hash);

    // Candidate envelopes: receipts and same-slot variants for both
    // branches, plus one next-epoch enrollment group.
    std::vector<std::string> candidates;
    for (const network *branch : {&a, &b})
      for (size_t subject = 0; subject < branch->members.size(); ++subject)
        for (const char *nonce : {"first", "variant"})
          candidates.push_back(envelope(branch->receipt(subject, 0,
              std::string(nonce) + (branch == &a ? "-a-" : "-b-")
                  + std::to_string(subject))));
    const identity late = make_identity("late-member", 4);
    candidates.push_back(envelope(registration_record(late)));
    candidates.push_back(envelope(admission_record(late, a.source, 4, 2160)));

    std::vector<tip_entry> chain_tips;
    size_t tip_counter = 0;
    const auto base = [&](const network &branch) {
      return tip_entry{std::make_shared<const semantic_state_v2>(branch.state),
          &branch, start_height, "base-" + std::to_string(tip_counter++)};
    };
    chain_tips.push_back(base(a));

    relay_record_pool_v2 pool = new_pool(small_committee, dedup_height);
    reference::relay_record_pool_v2 old = old_pool(small_committee, dedup_height);
    test_chain chain;
    const auto point_chain = [&]() {
      const tip_entry &tip = chain_tips.back();
      chain.move_to(tip.state, tip.branch->source, tip.height, tip.name, pool);
    };
    point_chain();

    const auto add_block = [&]() {
      const tip_entry &tip = chain_tips.back();
      semantic_state_v2 next = *tip.state;
      std::vector<envelope_record_v2> confirmed;
      const size_t count = rng() % 4;
      for (size_t i = 0; i < count; ++i)
      {
        const envelope_record_v2 record = record_of(candidates[rng() % candidates.size()]);
        semantic_apply_summary_v2 summary{};
        auto undo = next.checkpoint();
        if (next.apply_transaction({record},
                semantic_transaction_context_v2{tip.height, false, nullptr, nullptr},
                tip.branch->source, summary, &undo)
            == semantic_status_v2::accepted)
          confirmed.push_back(record);
        else
          next.rollback(undo);
      }
      // A membership change: the epoch's qualification closes at its
      // evidence deadline, invalidating every pending receipt of epoch 3.
      if (tip.height + 1 == evidence_deadline)
      {
        EXPECT_EQ(pipeline_status_v2::accepted,
            next.close_qualification(3, evidence_deadline));
      }
      pool.erase_confirmed_records(confirmed, tip.height);
      old.erase_confirmed_records(confirmed, tip.height);
      chain_tips.push_back(tip_entry{std::make_shared<const semantic_state_v2>(std::move(next)),
          tip.branch, tip.height + 1, "block-" + std::to_string(tip_counter++)});
      pool.note_tip_changed();
    };

    size_t forwarded = 0;
    size_t max_pool = 0;
    size_t below = 0;
    size_t above = 0;
    size_t crossings = 0;
    size_t below_selected = 0;
    bool was_hardened = start_height >= dedup_height;
    for (size_t step = 0; step < steps; ++step)
    {
      const uint32_t action = rng() % 10;
      if (action < 5)
      {
        std::vector<std::string> batch;
        const size_t count = 1 + rng() % 3;
        for (size_t i = 0; i < count; ++i)
          batch.push_back(candidates[rng() % candidates.size()]);
        std::vector<std::string> accepted_new;
        std::vector<std::string> accepted_old;
        const relay_ingress_status_v2 status_old =
            admit_old(batch, chain, old, accepted_old, dedup_height);
        const relay_ingress_status_v2 status_new =
            admit_new(batch, chain, pool, accepted_new, dedup_height);
        EXPECT_EQ(static_cast<int>(status_old), static_cast<int>(status_new))
            << "seed " << seed << " step " << step;
        EXPECT_EQ(accepted_old, accepted_new) << "seed " << seed << " step " << step;
        forwarded += accepted_new.size();
      }
      else if (action < 7)
      {
        if (chain_tips.back().height < evidence_deadline + 2)
          add_block();
        point_chain();
      }
      else if (action < 9)
      {
        if (chain_tips.size() > 1)
        {
          chain_tips.pop_back();
          pool.note_tip_changed();
        }
        point_chain();
      }
      else
      {
        // Reorg to the other branch: unwind, then extend past the old tip.
        const uint64_t target = chain_tips.back().height + 1;
        const network &other = chain_tips.back().branch == &a ? b : a;
        while (chain_tips.size() > 1)
        {
          chain_tips.pop_back();
          pool.note_tip_changed();
        }
        chain_tips.back() = base(other);
        pool.note_tip_changed();
        while (chain_tips.back().height < target)
          add_block();
        point_chain();
      }

      // After every step the incremental pool equals the old full prune.
      reference::relay_record_pool_v2 pruned = old;
      pruned.prune_unusable_receipts(chain.height, *chain.state, *chain.source);
      old = pruned;
      pool.sync_tip(chain.tip, chain.height, *chain.state, *chain.source);
      ASSERT_EQ(old.record_ids(), pool.record_ids()) << "seed " << seed << " step " << step;
      ASSERT_EQ(old.bytes(), pool.bytes());
      max_pool = std::max(max_pool, pool.size());
      const bool hardened = chain.height >= dedup_height;
      ++(hardened ? above : below);
      crossings += hardened != was_hardened ? 1 : 0;
      was_hardened = hardened;

      std::vector<relay_record_selection_v2> selected_old;
      std::vector<relay_record_selection_v2> selected_new;
      reference::relay_record_pool_v2 old_template = old;
      relay_record_pool_v2 new_template = pool;
      ASSERT_EQ(old_template.select_for_template(chain.height, selected_old, chain.state.get()),
          new_template.select_for_template(chain.height, selected_new, chain.state.get()));
      ASSERT_EQ(selection_ids(selected_old), selection_ids(selected_new))
          << "seed " << seed << " step " << step;
      // Below the hardening height a template may carry a receipt the chain
      // rejects; the Blockchain re-apply drops it.
      if (hardened)
        expect_selection_valid(selected_new, *chain.state, *chain.source, chain.height);
      else
        below_selected += selected_new.size();
    }
    const relay_pool_diagnostics_v2 diagnostics = pool.diagnostics();
    EXPECT_GT(diagnostics.prune_runs, 0u);
    EXPECT_GT(diagnostics.admissions, 0u);
    if (stats == nullptr)
    {
      EXPECT_GT(forwarded, 0u);
      EXPECT_GT(max_pool, 1u);
    }
    else
      stats->add(property_stats{forwarded, max_pool, below, above, crossings, below_selected});
    std::cout << "[relay-fuzz] seed=" << seed << " start=" << start_height
              << " dedup=" << dedup_height << " below=" << below
              << " above=" << above << " crossings=" << crossings
              << " below_selected=" << below_selected
              << " forwarded=" << forwarded << " max_pool=" << max_pool
              << " prune_runs=" << diagnostics.prune_runs
              << " cache_invalidations=" << diagnostics.cache_invalidations << std::endl;
  }
}

TEST(epose_relay_pool_incremental_v2, random_sequences_match_full_prune_reference)
{
  for (uint32_t seed = 1; seed <= 3; ++seed)
    run_property_sequence(seed, window_first, 150);
  // Across qualification close and expiry.
  for (uint32_t seed = 11; seed <= 13; ++seed)
    run_property_sequence(seed, evidence_deadline - 3, 150);
}

// The sequences must run on both sides of the hardening height and cross it
// repeatedly, so slot dedup and the fast path switch on and off.
static void expect_hardening_crossed(const property_stats &stats)
{
  EXPECT_GT(stats.forwarded, 0u);
  EXPECT_GT(stats.max_pool, 1u);
  EXPECT_GT(stats.below, 0u);
  EXPECT_GT(stats.above, 0u);
  EXPECT_GT(stats.crossings, 5u);
}

TEST(epose_relay_pool_incremental_v2, random_sequences_across_hardening_height_match_reference)
{
  property_stats stats;
  for (uint32_t seed = 21; seed <= 24; ++seed)
    run_property_sequence(seed, window_first, 150, window_first + 2, &stats);
  expect_hardening_crossed(stats);
}

TEST(epose_relay_pool_incremental_v2, random_sequences_across_hardening_and_qualification_match_reference)
{
  property_stats stats;
  for (uint32_t seed = 31; seed <= 34; ++seed)
    run_property_sequence(seed, evidence_deadline - 3, 150, evidence_deadline - 1, &stats);
  expect_hardening_crossed(stats);
}

// ---------------------------------------------------------------------------

TEST(epose_relay_pool_incremental_v2, select_for_template_matches_old_path)
{
  const network net(6, small_committee, "anchor-select");
  relay_record_pool_v2 pool = new_pool(small_committee);
  reference::relay_record_pool_v2 old = old_pool(small_committee);
  test_chain chain;
  chain.move_to(std::make_shared<const semantic_state_v2>(net.state), net.source,
      window_first + 1, "select-tip", pool);

  const identity late = make_identity("select-late", 4);
  std::vector<std::vector<std::string>> batches;
  for (size_t subject = 0; subject < net.members.size(); ++subject)
    batches.push_back({envelope(net.receipt(subject, 0, "select-" + std::to_string(subject)))});
  batches.push_back({envelope(registration_record(late)),
      envelope(admission_record(late, net.source, 4, 2160))});
  for (const std::vector<std::string> &item : batches)
  {
    std::vector<std::string> accepted_new;
    std::vector<std::string> accepted_old;
    ASSERT_EQ(static_cast<int>(admit_old(item, chain, old, accepted_old)),
        static_cast<int>(admit_new(item, chain, pool, accepted_new)));
    ASSERT_EQ(item, accepted_new);
    ASSERT_EQ(accepted_old, accepted_new);
  }
  ASSERT_EQ(old.record_ids(), pool.record_ids());
  ASSERT_EQ(8u, pool.size());

  for (const uint64_t height : {window_first, window_first + 1, evidence_deadline,
           evidence_deadline + 1})
    for (const bool with_state : {false, true})
    {
      reference::relay_record_pool_v2 old_copy = old;
      relay_record_pool_v2 new_copy = pool;
      old_copy.prune_unusable_receipts(height, net.state, net.source);
      new_copy.sync_tip(hash_text("select-" + std::to_string(height)), height,
          net.state, net.source);
      std::vector<relay_record_selection_v2> selected_old;
      std::vector<relay_record_selection_v2> selected_new;
      const semantic_state_v2 *state = with_state ? &net.state : nullptr;
      ASSERT_EQ(old_copy.select_for_template(height, selected_old, state),
          new_copy.select_for_template(height, selected_new, state));
      EXPECT_EQ(selection_ids(selected_old), selection_ids(selected_new)) << height;
      ASSERT_EQ(selected_old.size(), selected_new.size());
      for (size_t i = 0; i < selected_old.size(); ++i)
        EXPECT_EQ(selected_old[i].record.payload, selected_new[i].record.payload);
      if (height <= evidence_deadline)
      {
        EXPECT_FALSE(selected_new.empty());
      }
    }
}

// ---------------------------------------------------------------------------
// Duplicates.

TEST(epose_relay_pool_incremental_v2, duplicate_receipt_short_circuits_without_chain_or_verification)
{
  const network net(4, small_committee, "anchor-dup");
  relay_record_pool_v2 pool = new_pool(small_committee);
  reference::relay_record_pool_v2 old = old_pool(small_committee);
  test_chain chain;
  chain.move_to(std::make_shared<const semantic_state_v2>(net.state), net.source,
      window_first, "dup-tip", pool);
  const std::string first = envelope(net.receipt(0, 0, "dup-first"));

  std::vector<std::string> accepted;
  std::vector<std::string> accepted_old;
  ASSERT_EQ(relay_ingress_status_v2::accepted, admit_new({first}, chain, pool, accepted));
  ASSERT_EQ(relay_ingress_status_v2::accepted, admit_old({first}, chain, old, accepted_old));
  ASSERT_EQ(std::vector<std::string>{first}, accepted);
  const size_t captures = chain.captures;
  const uint64_t verified = verifications(pool);

  ASSERT_EQ(relay_ingress_status_v2::accepted, admit_new({first}, chain, pool, accepted));
  ASSERT_EQ(relay_ingress_status_v2::accepted, admit_old({first}, chain, old, accepted_old));
  // Same status and forward set as the old path (v2.0.5 receipt retry), but
  // no chain capture, no copy and no verification.
  EXPECT_EQ(accepted_old, accepted);
  EXPECT_EQ(captures, chain.captures);
  EXPECT_EQ(verified, verifications(pool));
  EXPECT_EQ(1u, pool.size());
  const relay_pool_diagnostics_v2 diagnostics = pool.diagnostics();
  EXPECT_EQ(1u, diagnostics.duplicates_short_circuited);
  EXPECT_EQ(1u, diagnostics.exact_duplicates);
  EXPECT_EQ(1u, diagnostics.cache_hits);
}

TEST(epose_relay_pool_incremental_v2, duplicate_enrollment_and_pre_hardening_receipts_are_not_rerelayed)
{
  const network net(4, small_committee, "anchor-dup-silent");
  const identity late = make_identity("dup-late", 4);
  const std::string registration = envelope(registration_record(late));
  const std::string lease = envelope(admission_record(late, net.source, 4, 2160));
  const std::string receipt = envelope(net.receipt(1, 0, "pre-hardening"));
  const uint64_t never = std::numeric_limits<uint64_t>::max();

  relay_record_pool_v2 pool = new_pool(small_committee, never);
  reference::relay_record_pool_v2 old = old_pool(small_committee, never);
  test_chain chain;
  chain.move_to(std::make_shared<const semantic_state_v2>(net.state), net.source,
      window_first + 1, "silent-tip", pool);

  for (const std::vector<std::string> &batch : std::vector<std::vector<std::string>>{
           {registration, lease}, {receipt}})
  {
    std::vector<std::string> accepted;
    std::vector<std::string> accepted_old;
    ASSERT_EQ(relay_ingress_status_v2::accepted, admit_new(batch, chain, pool, accepted, never));
    ASSERT_EQ(relay_ingress_status_v2::accepted, admit_old(batch, chain, old, accepted_old, never));
    EXPECT_EQ(batch, accepted);
    const size_t size = pool.size();

    ASSERT_EQ(relay_ingress_status_v2::accepted, admit_new(batch, chain, pool, accepted, never));
    ASSERT_EQ(relay_ingress_status_v2::accepted, admit_old(batch, chain, old, accepted_old, never));
    EXPECT_TRUE(accepted.empty());
    EXPECT_EQ(accepted_old, accepted);
    EXPECT_EQ(size, pool.size());
  }
  EXPECT_EQ(old.record_ids(), pool.record_ids());
}

TEST(epose_relay_pool_incremental_v2, accepted_unforwarded_record_stays_accepted)
{
  const network net(4, small_committee, "anchor-unforwarded");
  relay_record_pool_v2 pool = new_pool(small_committee);
  test_chain chain;
  chain.move_to(std::make_shared<const semantic_state_v2>(net.state), net.source,
      window_first, "unforwarded-tip", pool);
  const envelope_record_v2 local = net.receipt(2, 0, "local");
  const std::string encoded = envelope(local);

  // Locally produced, queued, never forwarded.
  ASSERT_EQ(relay_record_status_v2::accepted, pool.enqueue(local, window_first));
  ASSERT_EQ(1u, pool.size());

  // The same record arriving from a peer is still accepted, not rejected.
  std::vector<std::string> accepted;
  ASSERT_EQ(relay_ingress_status_v2::accepted, admit_new({encoded}, chain, pool, accepted));
  EXPECT_EQ(1u, pool.size());

  // A failing batch that carries it rolls back without removing it.
  const keys outsider = make_keys();
  const std::string foreign = envelope(receipt_record(net.state, net.members[0],
      outsider.public_key, outsider.secret_key, net.source.round, "forged"));
  EXPECT_EQ(relay_ingress_status_v2::invalid_batch,
      admit_new({encoded, foreign}, chain, pool, accepted));
  EXPECT_TRUE(accepted.empty());
  EXPECT_EQ(1u, pool.size());

  // Repeated syncs at the same tip keep it, and it reaches the template.
  pool.sync_tip(chain.tip, chain.height, *chain.state, *chain.source);
  pool.sync_tip(chain.tip, chain.height, *chain.state, *chain.source);
  std::vector<relay_record_selection_v2> selected;
  ASSERT_EQ(relay_record_status_v2::accepted,
      pool.select_for_template(window_first, selected, chain.state.get()));
  ASSERT_EQ(1u, selected.size());
  EXPECT_EQ(local.payload, selected.front().record.payload);
}

// ---------------------------------------------------------------------------
// Stale verified-receipt cache.

TEST(epose_relay_pool_incremental_v2, cache_survives_unrelated_tip_and_drops_on_membership_change)
{
  const network net(4, small_committee, "anchor-membership");
  relay_record_pool_v2 pool = new_pool(small_committee);
  test_chain chain;
  const uint64_t height = evidence_deadline;
  chain.move_to(std::make_shared<const semantic_state_v2>(net.state), net.source,
      height, "membership-tip-1", pool);
  std::vector<std::string> accepted;
  ASSERT_EQ(relay_ingress_status_v2::accepted, admit_new(
      {envelope(net.receipt(0, 0, "cached"))}, chain, pool, accepted));
  ASSERT_EQ(1u, pool.size());

  // Tip moves, the receipt's committee context does not: cache hit.
  chain.move_to(std::make_shared<const semantic_state_v2>(net.state), net.source,
      height, "membership-tip-2", pool);
  const relay_pool_diagnostics_v2 before = pool.diagnostics();
  pool.sync_tip(chain.tip, chain.height, *chain.state, *chain.source);
  relay_pool_diagnostics_v2 after = pool.diagnostics();
  EXPECT_EQ(before.prune_runs + 1, after.prune_runs);
  EXPECT_EQ(before.prune_rechecked_receipts, after.prune_rechecked_receipts);
  EXPECT_EQ(before.cache_hits + 1, after.cache_hits);
  EXPECT_EQ(1u, pool.size());

  // Same tip again: no prune at all.
  pool.sync_tip(chain.tip, chain.height, *chain.state, *chain.source);
  EXPECT_EQ(after.prune_runs, pool.diagnostics().prune_runs);

  // Membership change: the epoch's qualification closes.
  semantic_state_v2 closed = net.state;
  ASSERT_EQ(pipeline_status_v2::accepted, closed.close_qualification(3, evidence_deadline));
  chain.move_to(std::make_shared<const semantic_state_v2>(closed), net.source,
      height, "membership-tip-3", pool);
  pool.sync_tip(chain.tip, chain.height, *chain.state, *chain.source);
  const relay_pool_diagnostics_v2 dropped = pool.diagnostics();
  EXPECT_EQ(after.cache_invalidations + 1, dropped.cache_invalidations);
  EXPECT_EQ(after.prune_rechecked_receipts + 1, dropped.prune_rechecked_receipts);
  EXPECT_EQ(after.invalid_context_receipts + 1, dropped.invalid_context_receipts);
  EXPECT_EQ(0u, pool.size());
  std::vector<relay_record_selection_v2> selected;
  ASSERT_EQ(relay_record_status_v2::accepted,
      pool.select_for_template(height, selected, chain.state.get()));
  EXPECT_TRUE(selected.empty());
}

TEST(epose_relay_pool_incremental_v2, reorg_to_another_committee_reverifies_and_drops)
{
  const network a(4, small_committee, "anchor-reorg-a");
  const network b(4, small_committee, "anchor-reorg-b", &a.members);
  relay_record_pool_v2 pool = new_pool(small_committee);
  test_chain chain;
  chain.move_to(std::make_shared<const semantic_state_v2>(a.state), a.source,
      window_first, "reorg-a", pool);
  std::vector<std::string> accepted;
  for (size_t subject = 0; subject < 3; ++subject)
    ASSERT_EQ(relay_ingress_status_v2::accepted, admit_new(
        {envelope(a.receipt(subject, 0, "reorg-" + std::to_string(subject)))},
        chain, pool, accepted));
  ASSERT_EQ(3u, pool.size());

  // pop_block + add_block onto branch b: a different snapshot and anchor.
  chain.move_to(std::make_shared<const semantic_state_v2>(b.state), b.source,
      window_first, "reorg-b", pool);
  std::vector<relay_record_selection_v2> selected;
  pool.sync_tip(chain.tip, chain.height, *chain.state, *chain.source);
  ASSERT_EQ(relay_record_status_v2::accepted,
      pool.select_for_template(window_first, selected, chain.state.get()));
  EXPECT_TRUE(selected.empty());
  EXPECT_EQ(0u, pool.size());
  EXPECT_EQ(3u, pool.diagnostics().cache_invalidations);

  // The replacement for the same slot is admitted and selected.
  const envelope_record_v2 replacement = b.receipt(0, 0, "replacement");
  ASSERT_EQ(relay_ingress_status_v2::accepted, admit_new(
      {envelope(replacement)}, chain, pool, accepted));
  ASSERT_EQ(relay_record_status_v2::accepted,
      pool.select_for_template(window_first, selected, chain.state.get()));
  ASSERT_EQ(1u, selected.size());
  EXPECT_EQ(replacement.payload, selected.front().record.payload);
  expect_selection_valid(selected, *chain.state, *chain.source, window_first);
}

TEST(epose_relay_pool_incremental_v2, superseded_tip_verification_is_not_committed)
{
  const network a(4, small_committee, "anchor-race-a");
  const network b(4, small_committee, "anchor-race-b", &a.members);
  relay_record_pool_v2 pool = new_pool(small_committee);
  test_chain chain;
  chain.move_to(std::make_shared<const semantic_state_v2>(a.state), a.source,
      window_first, "race-a", pool);
  const auto state_b = std::make_shared<const semantic_state_v2>(b.state);
  // A reorg lands after the snapshot was taken and before the commit.
  chain.after_capture = [&](relay_record_pool_v2 &target) {
    chain.state = state_b;
    chain.source = &b.source;
    chain.tip = hash_text("race-b");
    target.note_tip_changed();
  };
  std::vector<std::string> accepted;
  EXPECT_EQ(relay_ingress_status_v2::invalid_batch, admit_new(
      {envelope(a.receipt(1, 0, "race"))}, chain, pool, accepted));
  EXPECT_EQ(2u, chain.captures);
  EXPECT_TRUE(accepted.empty());
  EXPECT_EQ(0u, pool.size());
}

// ---------------------------------------------------------------------------
// Microbenchmark: ~200 receipts pending, 1000 single-envelope admissions of
// which 80% are exact duplicates (round start), old algorithm vs new.

TEST(epose_relay_pool_incremental_v2, benchmark_round_start_duplicates)
{
  const committee_policy_v2 committee = wide_committee();
  const network net(51, committee, "anchor-bench");
  std::vector<std::string> pending;
  std::vector<std::string> fresh;
  for (size_t subject = 0; subject < net.members.size() && fresh.size() < 200; ++subject)
    for (size_t k = 0; k < 8 && fresh.size() < 200; ++k)
    {
      std::string encoded = envelope(net.receipt(subject, k, "bench"));
      if (pending.size() < 200)
        pending.push_back(std::move(encoded));
      else
        fresh.push_back(std::move(encoded));
    }
  ASSERT_EQ(200u, pending.size());
  ASSERT_EQ(200u, fresh.size());
  std::vector<std::string> workload;
  for (size_t i = 0; i < 800; ++i)
    workload.push_back(pending[i % pending.size()]);
  workload.insert(workload.end(), fresh.begin(), fresh.end());
  std::mt19937 rng(2026);
  std::shuffle(workload.begin(), workload.end(), rng);

  relay_record_pool_v2 pool = new_pool(committee);
  reference::relay_record_pool_v2 old = old_pool(committee);
  test_chain chain;
  chain.move_to(std::make_shared<const semantic_state_v2>(net.state), net.source,
      window_first, "bench-tip", pool);
  std::vector<std::string> accepted;
  for (const std::string &item : pending)
  {
    ASSERT_EQ(relay_ingress_status_v2::accepted, admit_new({item}, chain, pool, accepted));
    ASSERT_EQ(relay_ingress_status_v2::accepted, admit_old({item}, chain, old, accepted));
  }
  ASSERT_EQ(200u, pool.size());

  std::vector<relay_ingress_status_v2> statuses_old;
  std::vector<std::vector<std::string>> forwarded_old;
  reference::verification_count() = 0;
  auto start = std::chrono::steady_clock::now();
  for (const std::string &item : workload)
  {
    statuses_old.push_back(admit_old({item}, chain, old, accepted));
    forwarded_old.push_back(accepted);
  }
  const auto old_us = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - start).count();
  const uint64_t old_verifications = reference::verification_count();

  const relay_pool_diagnostics_v2 before = pool.diagnostics();
  std::vector<relay_ingress_status_v2> statuses_new;
  std::vector<std::vector<std::string>> forwarded_new;
  start = std::chrono::steady_clock::now();
  for (const std::string &item : workload)
  {
    statuses_new.push_back(admit_new({item}, chain, pool, accepted));
    forwarded_new.push_back(accepted);
  }
  const auto new_us = std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now() - start).count();
  const relay_pool_diagnostics_v2 after = pool.diagnostics();
  const uint64_t new_verifications = after.verifications - before.verifications;

  EXPECT_EQ(statuses_old, statuses_new);
  EXPECT_EQ(forwarded_old, forwarded_new);
  EXPECT_EQ(400u, pool.size());
  EXPECT_EQ(old.record_ids(), pool.record_ids());
  EXPECT_EQ(800u, after.duplicates_short_circuited - before.duplicates_short_circuited);
  EXPECT_LT(new_verifications, old_verifications);

  std::cout << "[relay-bench] envelopes=1000 duplicates=800 pending_start=200"
            << " old_verifications=" << old_verifications
            << " old_wall_us=" << old_us
            << " new_verifications=" << new_verifications
            << " new_wall_us=" << new_us
            << " new_short_circuited="
            << after.duplicates_short_circuited - before.duplicates_short_circuited
            << " new_cache_hits=" << after.cache_hits - before.cache_hits
            << " new_verification_us="
            << after.verification_microseconds - before.verification_microseconds
            << " new_pool_lock_us="
            << after.pool_lock_microseconds - before.pool_lock_microseconds
            << std::endl;
}

// ---------------------------------------------------------------------------
// Repeated envelopes and same-slot variants: dedup the batch before coupling,
// verify each distinct record once, outside the pool lock.

namespace
{
  struct lock_probe
  {
    size_t calls = 0;
    size_t lock_held = 0;
    // Kept alive past the hook: a probe blocked on a held lock must not
    // block the verifier that holds it.
    std::vector<std::future<size_t>> probes;

    void attach(test_chain &chain, const relay_record_pool_v2 &pool)
    {
      chain.on_verify = [this, &pool]() {
        ++calls;
        probes.push_back(std::async(std::launch::async, [&pool]() { return pool.size(); }));
        if (probes.back().wait_for(std::chrono::seconds(5)) != std::future_status::ready)
          ++lock_held;
      };
    }
  };

  struct paired_pools
  {
    relay_record_pool_v2 pool;
    reference::relay_record_pool_v2 old;
    test_chain chain;

    explicit paired_pools(const committee_policy_v2 &committee)
      : pool(new_pool(committee)), old(old_pool(committee)) {}

    relay_ingress_status_v2 admit(
        const std::vector<std::string> &batch, std::vector<std::string> &accepted)
    {
      std::vector<std::string> accepted_old;
      const relay_ingress_status_v2 status_old = admit_old(batch, chain, old, accepted_old);
      const relay_ingress_status_v2 status = admit_new(batch, chain, pool, accepted);
      EXPECT_EQ(static_cast<int>(status_old), static_cast<int>(status));
      EXPECT_EQ(accepted_old, accepted);
      EXPECT_EQ(old.record_ids(), pool.record_ids());
      return status;
    }
  };
}

TEST(epose_relay_pool_incremental_v2, repeated_envelopes_verify_once_per_distinct_receipt_off_lock)
{
  const committee_policy_v2 committee = wide_committee();
  const network net(9, committee, "anchor-repeat");
  paired_pools pools(committee);
  pools.chain.move_to(std::make_shared<const semantic_state_v2>(net.state), net.source,
      window_first + 1, "repeat-tip", pools.pool);
  lock_probe probe;
  probe.attach(pools.chain, pools.pool);

  const std::string r1 = envelope(net.receipt(0, 0, "repeat-1"));
  const std::string r2 = envelope(net.receipt(0, 1, "repeat-2"));
  const std::string r3 = envelope(net.receipt(0, 2, "repeat-3"));
  std::vector<std::string> accepted;
  for (const std::string &item : {r1, r2, r3})
    ASSERT_EQ(relay_ingress_status_v2::accepted, pools.admit({item}, accepted));
  ASSERT_EQ(3u, pools.pool.size());

  // Cached receipts, one of them repeated: no capture, no verification.
  uint64_t verified = verifications(pools.pool);
  const size_t captures = pools.chain.captures;
  probe.calls = 0;
  ASSERT_EQ(relay_ingress_status_v2::accepted, pools.admit({r1, r1, r2, r1, r3}, accepted));
  EXPECT_EQ((std::vector<std::string>{r1, r2, r3}), accepted);
  EXPECT_EQ(verified, verifications(pools.pool));
  EXPECT_EQ(captures, pools.chain.captures);
  EXPECT_EQ(0u, probe.calls);

  // A fresh receipt sends the batch down the slow path: the repeats are
  // still collapsed and the cached receipts still are not verified.
  const std::string n1 = envelope(net.receipt(1, 0, "repeat-new-1"));
  ASSERT_EQ(relay_ingress_status_v2::accepted,
      pools.admit({r1, r1, n1, r2, n1, r1}, accepted));
  EXPECT_EQ((std::vector<std::string>{r1, n1, r2}), accepted);
  EXPECT_EQ(verified + 1, verifications(pools.pool));
  EXPECT_EQ(1u, probe.calls);

  // Fresh receipts, repeated: one verification per distinct receipt.
  verified = verifications(pools.pool);
  probe.calls = 0;
  const std::string n2 = envelope(net.receipt(1, 1, "repeat-new-2"));
  const std::string n3 = envelope(net.receipt(1, 2, "repeat-new-3"));
  ASSERT_EQ(relay_ingress_status_v2::accepted,
      pools.admit({n2, n2, n3, n2, n3, n2}, accepted));
  EXPECT_EQ((std::vector<std::string>{n2, n3}), accepted);
  EXPECT_EQ(verified + 2, verifications(pools.pool));
  EXPECT_EQ(2u, probe.calls);
  EXPECT_EQ(0u, probe.lock_held);
  EXPECT_EQ(6u, pools.pool.size());
}

namespace
{
  // An update_descriptor of `value` to sequence 1, effective at `epoch`.
  envelope_record_v2 update_record(const identity &value, identity &updated, uint64_t epoch)
  {
    updated = value;
    updated.descriptor.sequence = value.descriptor.sequence + 1;
    updated.descriptor.effective_epoch = epoch;
    lifecycle_record_v2 lifecycle{};
    lifecycle.action = lifecycle_action_v2::update_descriptor;
    lifecycle.previous_descriptor_hash = hash_identity_descriptor_v2(
        cryptonote::TESTNET, genesis, parameters, value.descriptor);
    lifecycle.next_descriptor = updated.descriptor;
    EXPECT_TRUE(sign_lifecycle_record_v2(
        cryptonote::TESTNET, genesis, parameters, lifecycle,
        value.operator_auth.secret_key, value.service.secret_key));
    envelope_record_v2 record{};
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_lifecycle_record_v2(lifecycle, cryptonote::TESTNET, genesis, parameters, record));
    return record;
  }

  std::shared_ptr<const semantic_state_v2> confirm(
      const semantic_state_v2 &state, const contexts &source,
      const std::vector<std::string> &receipts, uint64_t height)
  {
    auto next = std::make_shared<semantic_state_v2>(state);
    for (const std::string &item : receipts)
    {
      semantic_apply_summary_v2 summary{};
      EXPECT_EQ(semantic_status_v2::accepted, next->apply_transaction(
          {record_of(item)}, semantic_transaction_context_v2{height, true, nullptr, nullptr},
          source, summary));
    }
    return next;
  }
}

TEST(epose_relay_pool_incremental_v2, repeated_lifecycle_and_lease_records_verify_once)
{
  const committee_policy_v2 committee = wide_committee();
  const network net(9, committee, "anchor-repeat-enroll");
  paired_pools pools(committee);
  pools.chain.move_to(std::make_shared<const semantic_state_v2>(net.state), net.source,
      window_first + 1, "repeat-enroll-tip", pools.pool);
  lock_probe probe;
  probe.attach(pools.chain, pools.pool);

  const identity late = make_identity("repeat-late", 4);
  const std::string registration = envelope(registration_record(late));
  const std::string lease = envelope(admission_record(late, net.source, 4, 2160));
  const std::string receipt = envelope(net.receipt(4, 0, "repeat-enroll-receipt"));
  std::vector<std::string> accepted;

  // Fresh records, each repeated: one verification per distinct record.
  uint64_t verified = verifications(pools.pool);
  ASSERT_EQ(relay_ingress_status_v2::accepted, pools.admit(
      {registration, registration, lease, registration, lease, lease}, accepted));
  EXPECT_EQ(verified + 2, verifications(pools.pool));
  EXPECT_EQ(2u, probe.calls);

  // Pending lifecycle, lease and receipt records, repeated in one batch:
  // still one verification per type.
  ASSERT_EQ(relay_ingress_status_v2::accepted, pools.admit({receipt}, accepted));
  verified = verifications(pools.pool);
  probe.calls = 0;
  ASSERT_EQ(relay_ingress_status_v2::accepted, pools.admit(
      {registration, lease, receipt, lease, registration, receipt, lease}, accepted));
  EXPECT_EQ(std::vector<std::string>{receipt}, accepted);
  EXPECT_EQ(verified + 2, verifications(pools.pool));
  EXPECT_EQ(2u, probe.calls);

  // Without the cached receipt the batch verifies exactly three records.
  const std::string fresh = envelope(net.receipt(5, 0, "repeat-enroll-fresh"));
  verified = verifications(pools.pool);
  probe.calls = 0;
  ASSERT_EQ(relay_ingress_status_v2::accepted, pools.admit(
      {registration, fresh, lease, fresh, registration, lease}, accepted));
  EXPECT_EQ(std::vector<std::string>{fresh}, accepted);
  EXPECT_EQ(verified + 3, verifications(pools.pool));
  EXPECT_EQ(3u, probe.calls);
  EXPECT_EQ(0u, probe.lock_held);
}

TEST(epose_relay_pool_incremental_v2, lease_repeat_after_same_identity_record_is_reverified)
{
  const committee_policy_v2 committee = wide_committee();
  const network net(9, committee, "anchor-repeat-rebind");
  paired_pools pools(committee);
  pools.chain.move_to(std::make_shared<const semantic_state_v2>(net.state), net.source,
      window_first + 1, "repeat-rebind-tip", pools.pool);
  lock_probe probe;
  probe.attach(pools.chain, pools.pool);

  // Member 0 is canonical with descriptor sequence 0 from epoch 3. Its
  // epoch-4 lease binds to that descriptor until the update rebinds epoch 4.
  const identity &member = net.members[0];
  identity updated{};
  const std::string update = envelope(update_record(member, updated, 4));
  const std::string lease = envelope(admission_record(member, net.source, 4, 2160));
  const std::string rebound = envelope(admission_record(updated, net.source, 4, 2160));
  std::vector<std::string> accepted;

  for (const std::vector<std::string> &batch : std::vector<std::vector<std::string>>{
           {lease, update, lease}, {lease, update, rebound, lease}})
  {
    uint64_t verified = verifications(pools.pool);
    probe.calls = 0;
    EXPECT_EQ(relay_ingress_status_v2::invalid_batch, pools.admit(batch, accepted));
    EXPECT_TRUE(accepted.empty());
    EXPECT_EQ(verified + batch.size(), verifications(pools.pool));
    EXPECT_EQ(batch.size(), probe.calls);
  }

  // The same records in a valid order: the trailing repeat follows its own
  // copy directly and is collapsed.
  const uint64_t verified = verifications(pools.pool);
  probe.calls = 0;
  ASSERT_EQ(relay_ingress_status_v2::accepted, pools.admit(
      {lease, update, rebound, rebound}, accepted));
  EXPECT_EQ(verified + 3, verifications(pools.pool));
  EXPECT_EQ(3u, probe.calls);
}

TEST(epose_relay_pool_incremental_v2, confirmed_receipt_replay_runs_no_verification)
{
  const network net(4, small_committee, "anchor-confirmed");
  paired_pools pools(small_committee);
  pools.chain.move_to(std::make_shared<const semantic_state_v2>(net.state), net.source,
      window_first + 1, "confirmed-pending-tip", pools.pool);
  lock_probe probe;
  probe.attach(pools.chain, pools.pool);

  // Pending and verified, then confirmed by a block.
  const std::string pending = envelope(net.receipt(0, 0, "confirmed-pending"));
  // Confirmed by a block without ever being pending here.
  const std::string direct = envelope(net.receipt(1, 0, "confirmed-direct"));
  std::vector<std::string> accepted;
  ASSERT_EQ(relay_ingress_status_v2::accepted, pools.admit({pending}, accepted));
  ASSERT_EQ(1u, pools.pool.size());

  const auto confirmed = confirm(net.state, net.source, {pending, direct}, window_first + 2);
  pools.chain.move_to(confirmed, net.source, window_first + 2, "confirmed-tip", pools.pool);

  uint64_t verified = verifications(pools.pool);
  probe.calls = 0;
  for (size_t replay = 0; replay < 3; ++replay)
  {
    ASSERT_EQ(relay_ingress_status_v2::accepted, pools.admit({pending}, accepted));
    EXPECT_TRUE(accepted.empty());
  }
  EXPECT_EQ(verified, verifications(pools.pool));
  EXPECT_EQ(0u, probe.calls);
  EXPECT_EQ(0u, pools.pool.size());

  // The first replay of a receipt confirmed elsewhere is verified once.
  ASSERT_EQ(relay_ingress_status_v2::accepted, pools.admit({direct}, accepted));
  EXPECT_TRUE(accepted.empty());
  EXPECT_EQ(verified + 1, verifications(pools.pool));
  EXPECT_EQ(1u, probe.calls);
  verified = verifications(pools.pool);
  probe.calls = 0;
  ASSERT_EQ(relay_ingress_status_v2::accepted, pools.admit({direct, pending, direct}, accepted));
  EXPECT_TRUE(accepted.empty());
  EXPECT_EQ(verified, verifications(pools.pool));
  EXPECT_EQ(0u, probe.calls);

  // After the round window both replays are expired, still unverified.
  pools.chain.move_to(confirmed, net.source, evidence_deadline + 1, "confirmed-late-tip",
      pools.pool);
  for (const std::string &item : {pending, direct})
  {
    EXPECT_EQ(relay_ingress_status_v2::expired, pools.admit({item}, accepted));
    EXPECT_TRUE(accepted.empty());
  }
  EXPECT_EQ(verified, verifications(pools.pool));
  EXPECT_EQ(0u, probe.calls);
  EXPECT_EQ(0u, probe.lock_held);

  // A different snapshot for the epoch changes the context hash: the
  // replay is verified again and the result still matches the reference.
  const network other(4, small_committee, "anchor-confirmed-other");
  const auto reorged = confirm(other.state, other.source, {}, window_first + 2);
  pools.chain.move_to(reorged, other.source, window_first + 2, "confirmed-reorg-tip",
      pools.pool);
  EXPECT_EQ(relay_ingress_status_v2::invalid_batch, pools.admit({pending}, accepted));
  EXPECT_EQ(verified + 1, verifications(pools.pool));
  EXPECT_EQ(1u, probe.calls);
}

TEST(epose_relay_pool_incremental_v2, same_slot_variants_are_verified_off_lock_and_match_reference)
{
  const committee_policy_v2 committee = wide_committee();
  const network net(9, committee, "anchor-variants");
  paired_pools pools(committee);
  pools.chain.move_to(std::make_shared<const semantic_state_v2>(net.state), net.source,
      window_first + 1, "variant-tip", pools.pool);
  lock_probe probe;
  probe.attach(pools.chain, pools.pool);

  // Pooled receipt, then a batch that repeats its variant.
  const std::string v1 = envelope(net.receipt(2, 0, "variant-first"));
  const std::string v2 = envelope(net.receipt(2, 0, "variant-second"));
  std::vector<std::string> accepted;
  ASSERT_EQ(relay_ingress_status_v2::accepted, pools.admit({v1}, accepted));
  uint64_t verified = verifications(pools.pool);
  probe.calls = 0;
  ASSERT_EQ(relay_ingress_status_v2::accepted, pools.admit({v2, v2, v1}, accepted));
  EXPECT_EQ(std::vector<std::string>{v1}, accepted);
  EXPECT_EQ(verified + 2, verifications(pools.pool));
  EXPECT_EQ(2u, probe.calls);

  // A fresh slot: the first variant is queued, the second one sees it.
  const std::string w1 = envelope(net.receipt(3, 0, "variant-fresh-first"));
  const std::string w2 = envelope(net.receipt(3, 0, "variant-fresh-second"));
  verified = verifications(pools.pool);
  probe.calls = 0;
  ASSERT_EQ(relay_ingress_status_v2::accepted, pools.admit({w1, w2, w1, w2}, accepted));
  EXPECT_EQ(std::vector<std::string>{w1}, accepted);
  EXPECT_EQ(verified + 2, verifications(pools.pool));
  EXPECT_EQ(2u, probe.calls);
  EXPECT_EQ(0u, probe.lock_held);
  EXPECT_EQ(2u, pools.pool.size());
}

TEST(epose_relay_pool_incremental_v2, slot_occupant_change_after_planning_replans_the_batch)
{
  const committee_policy_v2 committee = wide_committee();
  const network net(9, committee, "anchor-occupant");
  relay_record_pool_v2 pool = new_pool(committee);
  reference::relay_record_pool_v2 old = old_pool(committee);
  test_chain chain;
  chain.move_to(std::make_shared<const semantic_state_v2>(net.state), net.source,
      window_first + 1, "occupant-tip", pool);
  const std::string w1 = envelope(net.receipt(4, 0, "occupant-first"));
  const std::string w2 = envelope(net.receipt(4, 0, "occupant-second"));

  // A concurrent admission of w2 commits after this batch was planned
  // against an empty slot, at the same tip and generation.
  test_chain other;
  other.state = chain.state;
  other.source = chain.source;
  other.height = chain.height;
  other.tip = chain.tip;
  bool injected = false;
  chain.on_verify = [&]() {
    if (injected)
      return;
    injected = true;
    std::vector<std::string> inner;
    EXPECT_EQ(relay_ingress_status_v2::accepted, admit_new({w2}, other, pool, inner));
    EXPECT_EQ(std::vector<std::string>{w2}, inner);
  };
  // Prime the sync so the inner admission sees an unchanged generation.
  pool.sync_tip(chain.tip, chain.height, *chain.state, *chain.source);
  const size_t captures = chain.captures;
  std::vector<std::string> accepted;
  const relay_ingress_status_v2 status = admit_new({w1, w2}, chain, pool, accepted);
  EXPECT_TRUE(injected);
  EXPECT_EQ(captures + 2, chain.captures);

  std::vector<std::string> accepted_old;
  ASSERT_EQ(relay_ingress_status_v2::accepted, admit_old({w2}, chain, old, accepted_old));
  const relay_ingress_status_v2 status_old = admit_old({w1, w2}, chain, old, accepted_old);
  EXPECT_EQ(static_cast<int>(status_old), static_cast<int>(status));
  EXPECT_EQ(accepted_old, accepted);
  EXPECT_EQ(std::vector<std::string>{w2}, accepted);
  EXPECT_EQ(old.record_ids(), pool.record_ids());
  EXPECT_EQ(1u, pool.size());
}

// ---------------------------------------------------------------------------
// Round anchors after epoch start: a reorg can replace the
// round-1 anchor block while the frozen snapshot stays the same. Only the
// anchor component of the cache key can see that.

TEST(epose_relay_pool_incremental_v2, round_anchor_reorg_without_snapshot_change_drops_cached_receipts)
{
  const committee_policy_v2 two_rounds{1, 1, 2, 1, 1, {0, 300}};
  ASSERT_TRUE(two_rounds.valid());
  const network net(5, two_rounds, "anchor-round-one");
  contexts branch_a = net.source;
  branch_a.round1 = hash_text("round-one-block-a");
  contexts branch_b = net.source;
  branch_b.round1 = hash_text("round-one-block-b");
  uint64_t first = 0;
  uint64_t last = 0;
  ASSERT_TRUE(receipt_inclusion_window_v2(timing, two_rounds, 3, 1, first, last));
  const uint64_t height = first + 20;
  ASSERT_LE(height, last);

  relay_record_pool_v2 pool = new_pool(two_rounds);
  reference::relay_record_pool_v2 old = old_pool(two_rounds);
  test_chain chain;
  const auto state = std::make_shared<const semantic_state_v2>(net.state);
  chain.move_to(state, branch_a, height, "round-one-a", pool);
  std::vector<std::string> accepted;
  std::vector<std::string> accepted_old;
  for (size_t subject = 0; subject < net.members.size(); ++subject)
  {
    const auto assigned = net.state.membership().committee(
        3, 1, net.members[subject].service.public_key, branch_a.round1);
    ASSERT_FALSE(assigned.empty());
    const identity &signer = net.by_key(assigned.front().verifier_public_key);
    const std::string encoded = envelope(receipt_record(net.state, net.members[subject],
        signer.service.public_key, signer.service.secret_key, branch_a.round1,
        "round-one-" + std::to_string(subject), 1));
    ASSERT_EQ(relay_ingress_status_v2::accepted, admit_new({encoded}, chain, pool, accepted));
    ASSERT_EQ(relay_ingress_status_v2::accepted, admit_old({encoded}, chain, old, accepted_old));
    ASSERT_EQ(std::vector<std::string>{encoded}, accepted);
  }
  ASSERT_EQ(net.members.size(), pool.size());

  // Another tip with the same anchors: every receipt is a cache hit.
  chain.move_to(state, branch_a, height, "round-one-a-sibling", pool);
  const relay_pool_diagnostics_v2 before = pool.diagnostics();
  pool.sync_tip(chain.tip, chain.height, *chain.state, *chain.source);
  const relay_pool_diagnostics_v2 hit = pool.diagnostics();
  EXPECT_EQ(before.cache_hits + net.members.size(), hit.cache_hits);
  EXPECT_EQ(before.prune_rechecked_receipts, hit.prune_rechecked_receipts);

  // Reorg that replaces only the round-1 anchor block: same state object,
  // same snapshot, same round-0 anchor.
  chain.move_to(state, branch_b, height, "round-one-b", pool);
  pool.sync_tip(chain.tip, chain.height, *chain.state, *chain.source);
  old.prune_unusable_receipts(height, *state, branch_b);
  const relay_pool_diagnostics_v2 dropped = pool.diagnostics();
  EXPECT_EQ(hit.cache_invalidations + net.members.size(), dropped.cache_invalidations);
  EXPECT_EQ(old.record_ids(), pool.record_ids());
  EXPECT_EQ(0u, pool.size());
  std::vector<relay_record_selection_v2> selected;
  ASSERT_EQ(relay_record_status_v2::accepted,
      pool.select_for_template(height, selected, chain.state.get()));
  EXPECT_TRUE(selected.empty());
  expect_selection_valid(selected, *chain.state, *chain.source, height);
}

// ---------------------------------------------------------------------------
// Concurrency: admissions, block add / pop / reorg and
// template selection on separate threads, with a real chain lock. After
// quiescence the pool must equal the old full prune of the same records.

namespace
{
  class locked_chain final : public relay_chain_view_v2
  {
  public:
    std::mutex chain_lock;
    std::vector<tip_entry> tips;  // guarded by chain_lock
    std::atomic<size_t> captures{0};

    bool capture(
        relay_record_pool_v2 &pool,
        const relay_context_requests_v2 &requests,
        relay_tip_snapshot_v2 &snapshot) override
    {
      std::lock_guard<std::mutex> guard(chain_lock);
      ++captures;
      const tip_entry &tip = tips.back();
      snapshot.generation =
          pool.sync_tip(hash_text(tip.name), tip.height, *tip.state, tip.branch->source);
      snapshot.height = tip.height;
      snapshot.state = tip.state;
      requests.capture(tip.branch->source, snapshot.contexts);
      return true;
    }
  };
}

TEST(epose_relay_pool_incremental_v2, concurrent_admit_block_and_template_match_full_prune_after_quiescence)
{
  const network a(5, small_committee, "anchor-stress-a");
  const network b(5, small_committee, "anchor-stress-b", &a.members);
  std::vector<std::string> candidates;
  for (const network *branch : {&a, &b})
    for (size_t subject = 0; subject < branch->members.size(); ++subject)
      for (const char *nonce : {"first", "variant"})
        candidates.push_back(envelope(branch->receipt(subject, 0,
            std::string("stress-") + nonce + (branch == &a ? "-a-" : "-b-")
                + std::to_string(subject))));
  const identity late = make_identity("stress-late-member", 4);
  candidates.push_back(envelope(registration_record(late)));
  candidates.push_back(envelope(admission_record(late, a.source, 4, 2160)));

  // Heights around the evidence deadline: qualification close, round expiry
  // and reorgs between two committees all happen during the run.
  const uint64_t start_height = evidence_deadline - 16;
  relay_record_pool_v2 pool = new_pool(small_committee);
  locked_chain chain;
  size_t tip_counter = 0;  // guarded by chain_lock
  const auto base = [&](const network &branch) {
    return tip_entry{std::make_shared<const semantic_state_v2>(branch.state),
        &branch, start_height, "stress-base-" + std::to_string(tip_counter++)};
  };
  chain.tips.push_back(base(a));

  // Caller holds chain_lock, as Blockchain does around apply_epose_block.
  const auto add_block = [&](std::mt19937 &rng) {
    const tip_entry tip = chain.tips.back();
    pool.note_tip_changed();
    semantic_state_v2 next = *tip.state;
    std::vector<envelope_record_v2> confirmed;
    const size_t count = rng() % 3;
    for (size_t i = 0; i < count; ++i)
    {
      const envelope_record_v2 record = record_of(candidates[rng() % candidates.size()]);
      semantic_apply_summary_v2 summary{};
      auto undo = next.checkpoint();
      if (next.apply_transaction({record},
              semantic_transaction_context_v2{tip.height, false, nullptr, nullptr},
              tip.branch->source, summary, &undo)
          == semantic_status_v2::accepted)
        confirmed.push_back(record);
      else
        next.rollback(undo);
    }
    if (tip.height + 1 == evidence_deadline)
    {
      EXPECT_EQ(pipeline_status_v2::accepted, next.close_qualification(3, evidence_deadline));
    }
    pool.erase_confirmed_records(confirmed, tip.height);
    chain.tips.push_back(tip_entry{std::make_shared<const semantic_state_v2>(std::move(next)),
        tip.branch, tip.height + 1, "stress-block-" + std::to_string(tip_counter++)});
  };

  constexpr size_t batches_per_admitter = 150;
  constexpr unsigned admitters = 3;
  // Each tip change makes in-flight admissions retry. The chain thread does
  // a bounded number of changes so that every admission converges.
  constexpr size_t chain_changes = 120;
  std::atomic<unsigned> running{admitters};
  std::atomic<bool> chain_done{false};
  std::atomic<size_t> admissions{0};
  std::atomic<size_t> accepted_batches{0};
  std::atomic<size_t> unexpected{0};
  std::atomic<size_t> blocks{0};
  std::atomic<size_t> pops{0};
  std::atomic<size_t> reorgs{0};
  std::atomic<size_t> templates{0};
  std::atomic<size_t> invalid_selections{0};
  std::vector<std::thread> threads;
  for (unsigned t = 0; t < admitters; ++t)
    threads.emplace_back([&, t]() {
      std::mt19937 rng(1000 + t);
      // At least batches_per_admitter batches, and keep admitting until the
      // chain has settled, so the final tip sees admissions.
      size_t settled = 0;
      for (size_t i = 0; i < batches_per_admitter || settled < 20; ++i)
      {
        if (chain_done.load())
          ++settled;
        std::vector<std::string> batch;
        if (t == 0)
        {
          // Round-start duplicates: the same few envelopes again and again,
          // mostly through the lock-free fast path.
          const std::string &item = candidates[rng() % 4];
          batch.assign(1 + rng() % 3, item);
        }
        else
          for (size_t n = 1 + rng() % 4; n > 0; --n)
            batch.push_back(candidates[rng() % candidates.size()]);
        std::vector<std::string> accepted;
        const relay_ingress_status_v2 status = admit_relay_envelopes_v2(
            batch, relay_envelope_limits(), relay_policy(), chain, pool, accepted);
        ++admissions;
        if (status == relay_ingress_status_v2::accepted)
          ++accepted_batches;
        else if (!accepted.empty())
          ++unexpected;
        if (status == relay_ingress_status_v2::invalid_configuration
            || status == relay_ingress_status_v2::context_unavailable)
          ++unexpected;
      }
      --running;
    });
  threads.emplace_back([&]() {
    std::mt19937 rng(77);
    for (size_t change = 0; change < chain_changes && running.load() != 0; ++change)
    {
      {
        std::lock_guard<std::mutex> guard(chain.chain_lock);
        const uint32_t action = rng() % 5;
        if (action < 2)
        {
          if (chain.tips.back().height < evidence_deadline + 2)
          {
            add_block(rng);
            ++blocks;
          }
        }
        else if (action < 4)
        {
          if (chain.tips.size() > 1)
          {
            pool.note_tip_changed();
            chain.tips.pop_back();
            ++pops;
          }
        }
        else
        {
          const uint64_t target = chain.tips.back().height + 1;
          const network &other = chain.tips.back().branch == &a ? b : a;
          pool.note_tip_changed();
          while (chain.tips.size() > 1)
            chain.tips.pop_back();
          chain.tips.back() = base(other);
          while (chain.tips.back().height < target)
            add_block(rng);
          ++reorgs;
        }
      }
      std::this_thread::sleep_for(std::chrono::microseconds(200 + rng() % 2000));
    }
    {
      // Settle below the evidence deadline, so the pool is not trivially
      // empty at quiescence.
      std::lock_guard<std::mutex> guard(chain.chain_lock);
      pool.note_tip_changed();
      while (chain.tips.size() > 1 && chain.tips.back().height + 4 > evidence_deadline)
        chain.tips.pop_back();
      ++pops;
    }
    chain_done = true;
  });
  threads.emplace_back([&]() {
    while (running.load() != 0)
    {
      std::vector<relay_record_selection_v2> selected;
      tip_entry tip;
      relay_record_status_v2 status;
      {
        // Blockchain::create_block_template: sync and select under the lock.
        std::lock_guard<std::mutex> guard(chain.chain_lock);
        tip = chain.tips.back();
        pool.sync_tip(hash_text(tip.name), tip.height, *tip.state, tip.branch->source);
        status = pool.select_for_template(tip.height, selected, tip.state.get());
      }
      ++templates;
      if (status != relay_record_status_v2::accepted)
        ++unexpected;
      if (!selection_valid(selected, *tip.state, tip.branch->source, tip.height))
        ++invalid_selections;
      std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
  });
  for (std::thread &thread : threads)
    thread.join();

  EXPECT_EQ(0u, unexpected.load());
  EXPECT_EQ(0u, invalid_selections.load());
  EXPECT_GE(admissions.load(), admitters * batches_per_admitter);
  EXPECT_GT(accepted_batches.load(), 0u);
  EXPECT_GT(templates.load(), 0u);
  EXPECT_GT(blocks.load() + pops.load() + reorgs.load(), 0u);

  // Quiescence: the pool, synced at the final tip, equals the old full prune
  // of the same records at that tip.
  const tip_entry tip = chain.tips.back();
  pool.sync_tip(hash_text(tip.name), tip.height, *tip.state, tip.branch->source);
  std::vector<crypto::hash> ids = pool.record_ids();
  std::sort(ids.begin(), ids.end());
  reference::relay_record_pool_v2 old = old_pool(small_committee);
  for (const crypto::hash &id : ids)
  {
    bool found = false;
    for (const std::string &candidate : candidates)
    {
      reference::relay_record_pool_v2 single = old_pool(small_committee);
      if (single.enqueue(record_of(candidate), tip.height) != relay_record_status_v2::accepted
          || single.record_ids().front() != id)
        continue;
      EXPECT_EQ(relay_record_status_v2::accepted, old.enqueue(record_of(candidate), tip.height));
      found = true;
      break;
    }
    EXPECT_TRUE(found);
  }
  old.prune_unusable_receipts(tip.height, *tip.state, tip.branch->source);
  std::vector<crypto::hash> ids_old = old.record_ids();
  std::sort(ids_old.begin(), ids_old.end());
  EXPECT_EQ(ids_old, ids);
  EXPECT_EQ(old.bytes(), pool.bytes());
  std::vector<relay_record_selection_v2> selected_old;
  std::vector<relay_record_selection_v2> selected_new;
  ASSERT_EQ(old.select_for_template(tip.height, selected_old, tip.state.get()),
      pool.select_for_template(tip.height, selected_new, tip.state.get()));
  EXPECT_EQ(selection_ids(selected_old), selection_ids(selected_new));
  EXPECT_TRUE(selection_valid(selected_new, *tip.state, tip.branch->source, tip.height));
  EXPECT_LT(tip.height, evidence_deadline);
  EXPECT_FALSE(ids.empty());

  const relay_pool_diagnostics_v2 diagnostics = pool.diagnostics();
  std::cout << "[relay-stress] admissions=" << admissions.load()
            << " accepted=" << accepted_batches.load()
            << " captures=" << chain.captures.load()
            << " blocks=" << blocks.load() << " pops=" << pops.load()
            << " reorgs=" << reorgs.load() << " templates=" << templates.load()
            << " final_height=" << tip.height << " final_pool=" << ids.size()
            << " short_circuited=" << diagnostics.duplicates_short_circuited
            << " cache_hits=" << diagnostics.cache_hits
            << " verifications=" << diagnostics.verifications
            << " prune_runs=" << diagnostics.prune_runs << std::endl;
}
