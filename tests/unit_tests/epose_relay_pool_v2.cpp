// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <set>
#include <tuple>

#include "epose/lifecycle_v2.h"
#include "epose/record_codec_v2.h"
#include "epose/relay_pool_v2.h"
#include "string_tools.h"

namespace
{
  using namespace qwertycoin::epose;

  struct key_pair
  {
    crypto::public_key public_key{};
    crypto::secret_key secret_key{};
  };

  key_pair keys()
  {
    key_pair pair{};
    crypto::generate_keys(pair.public_key, pair.secret_key);
    return pair;
  }

  crypto::hash hash_text(const char *text)
  {
    return crypto::cn_fast_hash(text, std::strlen(text));
  }

  crypto::hash relay_genesis() { return hash_text("relay-genesis"); }
  crypto::hash relay_parameters() { return hash_text("relay-parameters"); }

  envelope_limits_v2 envelope_limits()
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

  relay_record_pool_v2 pool(
      uint64_t receipt_slot_dedup_height =
          std::numeric_limits<uint64_t>::max())
  {
    return relay_record_pool_v2(
        epoch_timing_v2{0, 720, 60}, envelope_limits(),
        relay_queue_limits_v2{4, 8192, 1, 1, 2048, 2048},
        relay_template_limits_v2{2, 4096, 1, 1, 2048, 2048},
        receipt_slot_dedup_height);
  }

  envelope_record_v2 lifecycle_record(uint64_t effective_epoch)
  {
    const key_pair service = keys();
    const key_pair authority = keys();
    const key_pair reward_view = keys();
    const key_pair reward_spend = keys();
    const crypto::hash genesis = relay_genesis();
    const crypto::hash parameters = relay_parameters();
    lifecycle_record_v2 lifecycle{};
    lifecycle.action = lifecycle_action_v2::register_identity;
    lifecycle.next_descriptor.identity_id = derive_identity_id_v2(
        cryptonote::TESTNET, genesis, parameters, authority.public_key);
    lifecycle.next_descriptor.service_public_key = service.public_key;
    lifecycle.next_descriptor.operator_authorization_public_key = authority.public_key;
    lifecycle.next_descriptor.reward_address.m_view_public_key = reward_view.public_key;
    lifecycle.next_descriptor.reward_address.m_spend_public_key = reward_spend.public_key;
    lifecycle.next_descriptor.endpoint_descriptor_hash = hash_text("relay-endpoint");
    lifecycle.next_descriptor.effective_epoch = effective_epoch;
    lifecycle.next_descriptor.expiry_epoch = effective_epoch + 4;
    EXPECT_TRUE(sign_lifecycle_record_v2(
        cryptonote::TESTNET, genesis, parameters, lifecycle,
        authority.secret_key, service.secret_key));
    envelope_record_v2 record{};
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_lifecycle_record_v2(
            lifecycle, cryptonote::TESTNET, genesis, parameters, record));
    return record;
  }

  std::vector<envelope_record_v2> dependent_lifecycle_records()
  {
    const key_pair service = keys();
    const key_pair authority = keys();
    const key_pair reward_view = keys();
    const key_pair reward_spend = keys();
    identity_descriptor_v2 descriptor{};
    descriptor.identity_id = derive_identity_id_v2(
        cryptonote::TESTNET, relay_genesis(), relay_parameters(),
        authority.public_key);
    descriptor.service_public_key = service.public_key;
    descriptor.operator_authorization_public_key = authority.public_key;
    descriptor.reward_address.m_view_public_key = reward_view.public_key;
    descriptor.reward_address.m_spend_public_key = reward_spend.public_key;
    descriptor.endpoint_descriptor_hash = hash_text("relay-dependent-endpoint");
    descriptor.effective_epoch = 1;
    descriptor.expiry_epoch = 5;

    lifecycle_record_v2 registration{};
    registration.action = lifecycle_action_v2::register_identity;
    registration.next_descriptor = descriptor;
    EXPECT_TRUE(sign_lifecycle_record_v2(
        cryptonote::TESTNET, relay_genesis(), relay_parameters(), registration,
        authority.secret_key, service.secret_key));

    lifecycle_record_v2 update{};
    update.action = lifecycle_action_v2::update_descriptor;
    update.previous_descriptor_hash = hash_identity_descriptor_v2(
        cryptonote::TESTNET, relay_genesis(), relay_parameters(), descriptor);
    update.next_descriptor = descriptor;
    update.next_descriptor.sequence = 1;
    update.next_descriptor.effective_epoch = 2;
    update.next_descriptor.expiry_epoch = 6;
    EXPECT_TRUE(sign_lifecycle_record_v2(
        cryptonote::TESTNET, relay_genesis(), relay_parameters(), update,
        authority.secret_key, service.secret_key));

    std::vector<envelope_record_v2> records(2);
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_lifecycle_record_v2(
            registration, cryptonote::TESTNET, relay_genesis(),
            relay_parameters(), records[0]));
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_lifecycle_record_v2(
            update, cryptonote::TESTNET, relay_genesis(), relay_parameters(),
            records[1]));
    return records;
  }

  void write_u64_le(std::string &bytes, size_t offset, uint64_t value)
  {
    for (unsigned shift = 0; shift < 64; shift += 8)
      bytes[offset + shift / 8] =
          static_cast<char>((value >> shift) & 0xff);
  }

  envelope_record_v2 receipt_record(
      uint64_t epoch,
      uint64_t round = 0,
      uint8_t subject = 1,
      uint8_t verifier = 2,
      uint8_t variant = 0)
  {
    envelope_record_v2 record{};
    record.type = static_cast<uint8_t>(record_type_v2::service_receipt);
    record.version = EPOSE_SERVICE_RECEIPT_RECORD_VERSION_V2;
    record.payload.assign(EPOSE_SERVICE_RECEIPT_PAYLOAD_BYTES_V2, '\0');
    record.payload[0] = static_cast<char>(EPOSE_PROTOCOL_VERSION_V2);
    record.payload[1] = static_cast<char>(service_kind_v2::canonical_object);
    write_u64_le(record.payload, 2, epoch);
    write_u64_le(record.payload, 10, round);
    record.payload[18] = 1;   // snapshot hash
    record.payload[50] = 2;   // anchor hash
    record.payload[82] = static_cast<char>(subject);
    record.payload[114] = static_cast<char>(verifier);
    record.payload[146] = 3;  // endpoint hash
    record.payload[178] = static_cast<char>(4 + variant); // nonce
    record.payload[210] = 5;  // requested object
    record.payload[242] = 5;  // response object
    record.payload[274] = static_cast<char>(6 + variant); // subject signature
    record.payload[338] = static_cast<char>(7 + variant); // verifier signature
    return record;
  }

  relay_policy_v2 relay_policy()
  {
    return {
        relay_queue_limits_v2{4, 8192, 1, 1, 2048, 2048},
        relay_template_limits_v2{2, 4096, 1, 1, 2048, 2048},
        std::numeric_limits<uint64_t>::max()};
  }

  class fixed_contexts final : public canonical_context_source_v2
  {
  public:
    bool block_hash(uint64_t, crypto::hash &hash) const override
    {
      hash = hash_text("relay-context");
      return true;
    }
    bool round_anchor(uint64_t, uint64_t, crypto::hash &hash) const override
    {
      hash = hash_text("relay-anchor");
      return true;
    }
  };
}

TEST(epose_relay_pool_v2, compiled_mainnet_final_policy_matches_manifest)
{
  crypto::hash genesis{};
  crypto::hash parameters{};
  ASSERT_TRUE(epee::string_tools::hex_to_pod(
      "4f95857586e2c66063c277370eda99cd75897d773af09f0c3cd1e22f7e87db39",
      genesis));
  ASSERT_TRUE(epee::string_tools::hex_to_pod(
      "2c26755094535871dd3ede7bd1b50aba82a9fb6831f0a17f32968eb0385145c6",
      parameters));

  relay_policy_v2 policy{};
  ASSERT_TRUE(compiled_relay_policy_v2(
      cryptonote::MAINNET, genesis, parameters, policy));
  EXPECT_TRUE(policy.valid());
  EXPECT_EQ(2048u, policy.queue.max_items);
  EXPECT_EQ(1048576u, policy.queue.max_bytes);
  EXPECT_EQ(256u, policy.mining_template.max_items);
  EXPECT_EQ(32768u, policy.mining_template.max_bytes);
  EXPECT_EQ(QWC_EPOSE_RELAY_HARDENING_HEIGHT,
      policy.receipt_slot_dedup_height);

  crypto::hash wrong = parameters;
  reinterpret_cast<unsigned char *>(&wrong)[0] ^= 1;
  EXPECT_FALSE(compiled_relay_policy_v2(
      cryptonote::MAINNET, genesis, wrong, policy));
  EXPECT_FALSE(policy.valid());
}

TEST(epose_relay_pool_v2, derives_deadlines_and_preserves_both_template_classes)
{
  auto relay = pool();
  const envelope_record_v2 enrollment_one = lifecycle_record(1);
  const envelope_record_v2 enrollment_two = lifecycle_record(2);
  const envelope_record_v2 evidence = receipt_record(1);
  ASSERT_TRUE(relay.valid());
  EXPECT_EQ(relay_record_status_v2::accepted, relay.enqueue(enrollment_one, 1));
  EXPECT_EQ(relay_record_status_v2::accepted, relay.enqueue(enrollment_two, 1));
  EXPECT_EQ(relay_record_status_v2::accepted, relay.enqueue(evidence, 1));
  EXPECT_EQ(relay_record_status_v2::idempotent_duplicate,
      relay.enqueue(evidence, 1));

  std::vector<relay_record_selection_v2> selected;
  ASSERT_EQ(relay_record_status_v2::accepted,
      relay.select_for_template(1, selected));
  ASSERT_EQ(2u, selected.size());
  EXPECT_TRUE(std::any_of(selected.begin(), selected.end(), [](const relay_record_selection_v2 &item) {
    return item.record.type == static_cast<uint8_t>(record_type_v2::service_receipt);
  }));
  EXPECT_TRUE(std::any_of(selected.begin(), selected.end(), [](const relay_record_selection_v2 &item) {
    return item.record.type == static_cast<uint8_t>(record_type_v2::identity_descriptor);
  }));
}

TEST(epose_relay_pool_v2, rejects_coinbase_proofs_and_prunes_derived_deadlines)
{
  auto relay = pool();
  envelope_record_v2 proof{};
  proof.type = static_cast<uint8_t>(record_type_v2::service_payment_proof);
  proof.version = EPOSE_PAYMENT_PROOF_RECORD_VERSION_V2;
  proof.payload.assign(EPOSE_PAYMENT_PROOF_PAYLOAD_BYTES_V2, '\0');
  EXPECT_EQ(relay_record_status_v2::payment_proof_forbidden,
      relay.enqueue(proof, 1));
  EXPECT_EQ(relay_record_status_v2::accepted,
      relay.enqueue(lifecycle_record(1), 1));
  EXPECT_EQ(relay_record_status_v2::accepted,
      relay.enqueue(receipt_record(1), 1));
  EXPECT_EQ(2u, relay.size());
  relay.prune_expired(660);
  EXPECT_EQ(1u, relay.size());
  relay.prune_expired(1380);
  EXPECT_EQ(0u, relay.size());
  EXPECT_EQ(0u, relay.bytes());
}

TEST(epose_relay_pool_v2, confirmed_records_are_removed_by_complete_record_id)
{
  auto relay = pool();
  const envelope_record_v2 receipt = receipt_record(1);
  ASSERT_EQ(relay_record_status_v2::accepted, relay.enqueue(receipt, 1));
  std::vector<relay_record_selection_v2> selected;
  ASSERT_EQ(relay_record_status_v2::accepted,
      relay.select_for_template(1, selected));
  ASSERT_EQ(1u, selected.size());
  relay.erase_confirmed({selected.front().id});
  EXPECT_EQ(0u, relay.size());
  EXPECT_EQ(0u, relay.bytes());

  ASSERT_EQ(relay_record_status_v2::accepted, relay.enqueue(receipt, 1));
  relay.erase_confirmed_records({receipt}, 1);
  EXPECT_EQ(0u, relay.size());
  EXPECT_EQ(0u, relay.bytes());
}

TEST(epose_relay_pool_v2, receipt_slot_hardening_activates_exactly_at_height_20000)
{
  auto relay = pool(QWC_EPOSE_RELAY_HARDENING_HEIGHT);
  const envelope_record_v2 first = receipt_record(27, 0, 1, 2, 1);
  const envelope_record_v2 second = receipt_record(27, 0, 1, 2, 2);
  const envelope_record_v2 third = receipt_record(27, 0, 1, 2, 3);

  ASSERT_EQ(relay_record_status_v2::accepted,
      relay.enqueue(first, QWC_EPOSE_RELAY_HARDENING_HEIGHT - 1));
  ASSERT_EQ(relay_record_status_v2::accepted,
      relay.enqueue(second, QWC_EPOSE_RELAY_HARDENING_HEIGHT - 1));
  ASSERT_EQ(2u, relay.size());

  std::vector<relay_record_selection_v2> selected;
  ASSERT_EQ(relay_record_status_v2::accepted,
      relay.select_for_template(
          QWC_EPOSE_RELAY_HARDENING_HEIGHT, selected));
  EXPECT_EQ(1u, relay.size());
  ASSERT_EQ(1u, selected.size());
  EXPECT_EQ(relay_record_status_v2::idempotent_duplicate,
      relay.enqueue(third, QWC_EPOSE_RELAY_HARDENING_HEIGHT));
  EXPECT_EQ(1u, relay.size());

  // Relay policy is height-derived and therefore follows a reorg below the
  // activation boundary without altering canonical EPoSE state.
  EXPECT_EQ(relay_record_status_v2::accepted,
      relay.enqueue(third, QWC_EPOSE_RELAY_HARDENING_HEIGHT - 1));
  EXPECT_EQ(2u, relay.size());

  // Crossing the same boundary again after a reorg deterministically restores
  // the hardened view without requiring a process restart.
  selected.clear();
  ASSERT_EQ(relay_record_status_v2::accepted,
      relay.select_for_template(
          QWC_EPOSE_RELAY_HARDENING_HEIGHT, selected));
  EXPECT_EQ(1u, relay.size());
  ASSERT_EQ(1u, selected.size());
}

TEST(epose_relay_pool_v2, mixed_old_and_new_nodes_build_the_same_unique_template)
{
  auto legacy = pool();
  auto hardened = pool(QWC_EPOSE_RELAY_HARDENING_HEIGHT);
  const std::vector<envelope_record_v2> records{
      receipt_record(27, 0, 1, 2, 1),
      receipt_record(27, 1, 1, 3, 1)};

  for (const envelope_record_v2 &record : records)
  {
    ASSERT_EQ(relay_record_status_v2::accepted,
        legacy.enqueue(record, QWC_EPOSE_RELAY_HARDENING_HEIGHT));
    ASSERT_EQ(relay_record_status_v2::accepted,
        hardened.enqueue(record, QWC_EPOSE_RELAY_HARDENING_HEIGHT));
  }

  std::vector<relay_record_selection_v2> legacy_template;
  std::vector<relay_record_selection_v2> hardened_template;
  ASSERT_EQ(relay_record_status_v2::accepted,
      legacy.select_for_template(
          QWC_EPOSE_RELAY_HARDENING_HEIGHT, legacy_template));
  ASSERT_EQ(relay_record_status_v2::accepted,
      hardened.select_for_template(
          QWC_EPOSE_RELAY_HARDENING_HEIGHT, hardened_template));
  ASSERT_EQ(legacy_template.size(), hardened_template.size());
  ASSERT_EQ(records.size(), legacy_template.size());
  for (size_t index = 0; index < records.size(); ++index)
  {
    EXPECT_EQ(legacy_template[index].id, hardened_template[index].id);
    EXPECT_EQ(legacy_template[index].record.type,
        hardened_template[index].record.type);
    EXPECT_EQ(legacy_template[index].record.version,
        hardened_template[index].record.version);
    EXPECT_EQ(legacy_template[index].record.payload,
        hardened_template[index].record.payload);
  }
}

TEST(epose_relay_pool_v2, restart_and_resync_reconstruct_hardened_slot_identity)
{
  // The relay queue is intentionally non-persistent. A restarted node derives
  // the active policy from the canonical height and rebuilds only unique slots
  // as receipts are replayed or resubmitted.
  auto restarted = pool(QWC_EPOSE_RELAY_HARDENING_HEIGHT);
  const envelope_record_v2 first = receipt_record(27, 2, 7, 8, 1);
  const envelope_record_v2 second = receipt_record(27, 2, 7, 8, 2);
  const envelope_record_v2 third = receipt_record(27, 2, 7, 8, 3);

  ASSERT_EQ(relay_record_status_v2::accepted,
      restarted.enqueue(first, QWC_EPOSE_RELAY_HARDENING_HEIGHT));
  EXPECT_EQ(relay_record_status_v2::idempotent_duplicate,
      restarted.enqueue(second, QWC_EPOSE_RELAY_HARDENING_HEIGHT));
  EXPECT_EQ(relay_record_status_v2::idempotent_duplicate,
      restarted.enqueue(third, QWC_EPOSE_RELAY_HARDENING_HEIGHT));
  ASSERT_EQ(1u, restarted.size());

  std::vector<relay_record_selection_v2> selected;
  ASSERT_EQ(relay_record_status_v2::accepted,
      restarted.select_for_template(
          QWC_EPOSE_RELAY_HARDENING_HEIGHT, selected));
  ASSERT_EQ(1u, selected.size());
  restarted.erase_confirmed_records(
      {third}, QWC_EPOSE_RELAY_HARDENING_HEIGHT);
  EXPECT_EQ(0u, restarted.size());
  EXPECT_EQ(0u, restarted.bytes());
}

TEST(epose_relay_pool_v2, hardening_purges_all_variants_of_a_confirmed_slot)
{
  auto relay = pool(QWC_EPOSE_RELAY_HARDENING_HEIGHT);
  const envelope_record_v2 first = receipt_record(27, 1, 3, 4, 1);
  const envelope_record_v2 second = receipt_record(27, 1, 3, 4, 2);
  const envelope_record_v2 confirmed = receipt_record(27, 1, 3, 4, 3);
  ASSERT_EQ(relay_record_status_v2::accepted,
      relay.enqueue(first, QWC_EPOSE_RELAY_HARDENING_HEIGHT - 1));
  ASSERT_EQ(relay_record_status_v2::accepted,
      relay.enqueue(second, QWC_EPOSE_RELAY_HARDENING_HEIGHT - 1));
  ASSERT_EQ(2u, relay.size());

  relay.erase_confirmed_records(
      {confirmed}, QWC_EPOSE_RELAY_HARDENING_HEIGHT);
  EXPECT_EQ(0u, relay.size());
  EXPECT_EQ(0u, relay.bytes());
}

TEST(epose_relay_pool_v2, hardening_bounds_retries_and_preserves_all_unique_round_slots)
{
  relay_record_pool_v2 relay(
      epoch_timing_v2{0, 720, 60}, envelope_limits(),
      relay_queue_limits_v2{2048, 1048576, 512, 512, 262144, 262144},
      relay_template_limits_v2{256, 32768, 64, 64, 8192, 8192},
      QWC_EPOSE_RELAY_HARDENING_HEIGHT);
  ASSERT_TRUE(relay.valid());

  constexpr uint64_t epoch = 27;
  constexpr size_t subjects = 18;
  constexpr size_t rounds = 3;
  constexpr size_t verifiers = 9;
  for (size_t subject = 0; subject < subjects; ++subject)
    for (size_t round = 0; round < rounds; ++round)
      for (size_t verifier = 0; verifier < verifiers; ++verifier)
        for (uint8_t variant = 0; variant < 4; ++variant)
        {
          const auto status = relay.enqueue(
              receipt_record(
                  epoch, round, static_cast<uint8_t>(subject + 1),
                  static_cast<uint8_t>(verifier + 64), variant),
              QWC_EPOSE_RELAY_HARDENING_HEIGHT);
          EXPECT_EQ(variant == 0
                  ? relay_record_status_v2::accepted
                  : relay_record_status_v2::idempotent_duplicate,
              status);
        }
  ASSERT_EQ(subjects * rounds * verifiers, relay.size());

  std::set<std::tuple<uint64_t, uint64_t, uint8_t, uint8_t>> observed;
  while (relay.size() != 0)
  {
    std::vector<relay_record_selection_v2> selected;
    ASSERT_EQ(relay_record_status_v2::accepted,
        relay.select_for_template(
            QWC_EPOSE_RELAY_HARDENING_HEIGHT, selected));
    ASSERT_FALSE(selected.empty());
    std::vector<envelope_record_v2> confirmed;
    confirmed.reserve(selected.size());
    for (const relay_record_selection_v2 &item : selected)
    {
      authenticated_service_receipt_v2 receipt{};
      ASSERT_EQ(record_codec_status_v2::accepted,
          decode_service_receipt_record_structure_v2(item.record, receipt));
      EXPECT_TRUE(observed.emplace(
          receipt.challenge.epoch, receipt.challenge.round,
          static_cast<uint8_t>(receipt.challenge.subject_public_key.data[0]),
          static_cast<uint8_t>(receipt.challenge.verifier_public_key.data[0])).second);
      confirmed.push_back(item.record);
    }
    relay.erase_confirmed_records(
        confirmed, QWC_EPOSE_RELAY_HARDENING_HEIGHT);
  }

  EXPECT_EQ(subjects * rounds * verifiers, observed.size());
  for (size_t round = 0; round < rounds; ++round)
    EXPECT_EQ(subjects * verifiers,
        static_cast<size_t>(std::count_if(
            observed.begin(), observed.end(), [round](const auto &slot) {
              return std::get<1>(slot) == round;
            })));
}

TEST(epose_relay_pool_v2, invalid_local_limits_fail_closed)
{
  relay_record_pool_v2 invalid(
      epoch_timing_v2{0, 720, 60}, envelope_limits(),
      relay_queue_limits_v2{},
      relay_template_limits_v2{2, 4096, 1, 1, 2048, 2048},
      QWC_EPOSE_RELAY_HARDENING_HEIGHT);
  EXPECT_FALSE(invalid.valid());
  EXPECT_EQ(relay_record_status_v2::invalid_configuration,
      invalid.enqueue(receipt_record(1), 1));
  std::vector<relay_record_selection_v2> selected;
  EXPECT_EQ(relay_record_status_v2::invalid_configuration,
      invalid.select_for_template(1, selected));
  EXPECT_TRUE(selected.empty());
}

TEST(epose_relay_pool_v2, relay_policy_must_fit_the_backing_queue)
{
  relay_policy_v2 policy = relay_policy();
  EXPECT_TRUE(policy.valid());
  policy.mining_template.max_items = 5;
  EXPECT_FALSE(policy.valid());
  policy.mining_template.max_items = 2;
  policy.mining_template.max_bytes = 8193;
  EXPECT_FALSE(policy.valid());
}

TEST(epose_relay_pool_v2, semantic_ingress_is_authenticated_idempotent_and_batch_atomic)
{
  const epoch_timing_v2 timing{0, 720, 60};
  const admission_policy_v2 admission{
      admission_work_algorithm_v2::randomx, 1, 1};
  const committee_policy_v2 committee{1, 1, 1, 1, 1, {0}};
  semantic_state_v2 state(
      cryptonote::TESTNET, relay_genesis(), relay_parameters(),
      timing, admission, committee);
  ASSERT_TRUE(state.valid());
  auto relay = pool();
  const envelope_record_v2 record = lifecycle_record(1);
  envelope_budget_v2 ignored{};
  std::string encoded;
  ASSERT_EQ(envelope_status_v2::accepted,
      encode_envelope_v2({record}, envelope_limits(), encoded, ignored));
  std::string corrupted = encoded;
  ASSERT_FALSE(corrupted.empty());
  corrupted.back() ^= 1;
  const fixed_contexts contexts{};
  std::vector<std::string> accepted;

  EXPECT_EQ(relay_ingress_status_v2::invalid_batch,
      admit_relay_envelopes_v2(
          {encoded, corrupted}, 1, envelope_limits(), relay_policy(),
          state, contexts, relay, accepted));
  EXPECT_EQ(0u, relay.size());
  EXPECT_TRUE(accepted.empty());

  ASSERT_EQ(relay_ingress_status_v2::accepted,
      admit_relay_envelopes_v2(
          {encoded}, 1, envelope_limits(), relay_policy(),
          state, contexts, relay, accepted));
  ASSERT_EQ(1u, relay.size());
  ASSERT_EQ(1u, accepted.size());
  EXPECT_EQ(encoded, accepted.front());

  ASSERT_EQ(relay_ingress_status_v2::accepted,
      admit_relay_envelopes_v2(
          {encoded}, 1, envelope_limits(), relay_policy(),
          state, contexts, relay, accepted));
  EXPECT_EQ(1u, relay.size());
  EXPECT_TRUE(accepted.empty());

  EXPECT_EQ(relay_ingress_status_v2::invalid_batch,
      admit_relay_envelopes_v2(
          {corrupted}, 1, envelope_limits(), relay_policy(),
          state, contexts, relay, accepted));
  EXPECT_EQ(1u, relay.size());
  EXPECT_TRUE(accepted.empty());
}

TEST(epose_relay_pool_v2, semantic_ingress_reports_queue_exhaustion_and_keeps_batch_atomic)
{
  const epoch_timing_v2 timing{0, 720, 60};
  const admission_policy_v2 admission{
      admission_work_algorithm_v2::randomx, 1, 1};
  const committee_policy_v2 committee{1, 1, 1, 1, 1, {0}};
  semantic_state_v2 state(
      cryptonote::TESTNET, relay_genesis(), relay_parameters(),
      timing, admission, committee);
  ASSERT_TRUE(state.valid());

  relay_policy_v2 policy{
      relay_queue_limits_v2{2, 8192, 1, 1, 2048, 2048},
      relay_template_limits_v2{2, 4096, 1, 1, 2048, 2048},
      QWC_EPOSE_RELAY_HARDENING_HEIGHT};
  ASSERT_TRUE(policy.valid());
  relay_record_pool_v2 relay(
      timing, envelope_limits(), policy.queue, policy.mining_template,
      policy.receipt_slot_dedup_height);
  const auto records = dependent_lifecycle_records();
  std::vector<std::string> encoded;
  for (const auto &record : records)
  {
    envelope_budget_v2 ignored{};
    std::string envelope;
    ASSERT_EQ(envelope_status_v2::accepted,
        encode_envelope_v2({record}, envelope_limits(), envelope, ignored));
    encoded.push_back(std::move(envelope));
  }

  const fixed_contexts contexts{};
  std::vector<std::string> accepted;
  EXPECT_EQ(relay_ingress_status_v2::capacity_exhausted,
      admit_relay_envelopes_v2(
          encoded, 1, envelope_limits(), policy, state, contexts,
          relay, accepted));
  EXPECT_EQ(0u, relay.size());
  EXPECT_TRUE(accepted.empty());
}

TEST(epose_relay_pool_v2, ordered_single_record_envelopes_share_one_atomic_semantic_preview)
{
  const epoch_timing_v2 timing{0, 720, 60};
  const admission_policy_v2 admission{
      admission_work_algorithm_v2::randomx, 1, 1};
  const committee_policy_v2 committee{1, 1, 1, 1, 1, {0}};
  semantic_state_v2 state(
      cryptonote::TESTNET, relay_genesis(), relay_parameters(),
      timing, admission, committee);
  ASSERT_TRUE(state.valid());
  auto relay = pool();
  const auto records = dependent_lifecycle_records();
  std::vector<std::string> encoded;
  for (const auto &record : records)
  {
    envelope_budget_v2 ignored{};
    std::string envelope;
    ASSERT_EQ(envelope_status_v2::accepted,
        encode_envelope_v2({record}, envelope_limits(), envelope, ignored));
    encoded.push_back(std::move(envelope));
  }
  const fixed_contexts contexts{};
  std::vector<std::string> accepted;

  ASSERT_EQ(relay_ingress_status_v2::accepted,
      admit_relay_envelopes_v2(
          encoded, 1, envelope_limits(), relay_policy(), state, contexts,
          relay, accepted));
  EXPECT_EQ(2u, relay.size());
  EXPECT_EQ(encoded, accepted);

  auto reversed_pool = pool();
  std::reverse(encoded.begin(), encoded.end());
  EXPECT_EQ(relay_ingress_status_v2::invalid_batch,
      admit_relay_envelopes_v2(
          encoded, 1, envelope_limits(), relay_policy(), state, contexts,
          reversed_pool, accepted));
  EXPECT_EQ(0u, reversed_pool.size());
  EXPECT_TRUE(accepted.empty());
}
