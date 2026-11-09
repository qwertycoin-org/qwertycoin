// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include "epose_journal_test_helpers.h"

#include <cstring>
#include <limits>

#include "epose/lifecycle_v2.h"
#include "epose/record_codec_v2.h"
#include "epose/service_node.h"
#include "epose/state_image_v2.h"

namespace
{
  using namespace qwertycoin::epose;

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

  crypto::hash hash_text(const char *text)
  {
    return crypto::cn_fast_hash(text, std::strlen(text));
  }

  struct fixture
  {
    cryptonote::network_type nettype = cryptonote::TESTNET;
    crypto::hash genesis = hash_text("genesis");
    crypto::hash parameters = hash_text("parameters");
    keys service = make_keys();
    keys operator_auth = make_keys();
    keys reward_spend = make_keys();
    keys reward_view = make_keys();
  };

  identity_descriptor_v2 descriptor(const fixture &f, uint64_t sequence, uint64_t effective, uint64_t expiry)
  {
    identity_descriptor_v2 out{};
    out.identity_id = derive_identity_id_v2(f.nettype, f.genesis, f.parameters, f.operator_auth.public_key);
    out.service_public_key = f.service.public_key;
    out.operator_authorization_public_key = f.operator_auth.public_key;
    out.reward_address.m_spend_public_key = f.reward_spend.public_key;
    out.reward_address.m_view_public_key = f.reward_view.public_key;
    out.endpoint_descriptor_hash = hash_text("endpoint");
    out.sequence = sequence;
    out.effective_epoch = effective;
    out.expiry_epoch = expiry;
    return out;
  }

  lifecycle_record_v2 signed_record(
      const fixture &f,
      lifecycle_action_v2 action,
      const crypto::hash &previous,
      const identity_descriptor_v2 &next,
      const crypto::secret_key &service_secret)
  {
    lifecycle_record_v2 record{};
    record.action = action;
    record.previous_descriptor_hash = previous;
    record.next_descriptor = next;
    EXPECT_TRUE(sign_lifecycle_record_v2(
        f.nettype, f.genesis, f.parameters, record, f.operator_auth.secret_key, service_secret));
    return record;
  }

  lifecycle_record_v2 registration(const fixture &f, uint64_t effective = 2, uint64_t expiry = 10)
  {
    return signed_record(f, lifecycle_action_v2::register_identity, crypto::null_hash,
        descriptor(f, 0, effective, expiry), f.service.secret_key);
  }
}

TEST(epose_lifecycle_v2, registration_is_future_effective_and_idempotent)
{
  fixture f;
  lifecycle_registry_v2 registry(f.nettype, f.genesis, f.parameters);
  const auto record = registration(f);
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(registry, record, 1, 2));
  EXPECT_EQ(lifecycle_status_v2::idempotent_duplicate, epose_test_journal::apply(registry, record, 1, 2));
  auto invalid_duplicate = record;
  reinterpret_cast<unsigned char *>(&invalid_duplicate.service_signature)[0] ^= 1;
  EXPECT_EQ(lifecycle_status_v2::invalid_service_signature, epose_test_journal::apply(registry, invalid_duplicate, 1, 2));
  EXPECT_EQ(nullptr, registry.descriptor_for_epoch(record.next_descriptor.identity_id, 1));
  ASSERT_NE(nullptr, registry.descriptor_for_epoch(record.next_descriptor.identity_id, 2));
  EXPECT_EQ(nullptr, registry.descriptor_for_epoch(record.next_descriptor.identity_id, 10));
  EXPECT_EQ(nullptr, registry.descriptor_for_epoch(record.next_descriptor.identity_id, 11));
}

TEST(epose_lifecycle_v2, reward_binding_is_context_and_address_bound)
{
  fixture f;
  const crypto::hash binding = hash_reward_binding_v2(
      f.nettype, f.genesis, f.parameters, descriptor(f, 0, 2, 10).reward_address);
  EXPECT_NE(crypto::null_hash, binding);
  EXPECT_NE(binding, hash_reward_binding_v2(
      cryptonote::MAINNET, f.genesis, f.parameters, descriptor(f, 0, 2, 10).reward_address));
  auto changed = descriptor(f, 0, 2, 10).reward_address;
  changed.m_spend_public_key = make_keys().public_key;
  EXPECT_NE(binding, hash_reward_binding_v2(f.nettype, f.genesis, f.parameters, changed));
  EXPECT_EQ(crypto::null_hash, hash_reward_binding_v2(
      cryptonote::UNDEFINED, f.genesis, f.parameters, changed));
}

TEST(epose_lifecycle_v2, post_cutoff_and_retroactive_changes_fail_closed)
{
  fixture f;
  lifecycle_registry_v2 registry(f.nettype, f.genesis, f.parameters);
  auto record = registration(f, 2, 10);
  EXPECT_EQ(lifecycle_status_v2::retroactive_change, epose_test_journal::apply(registry, record, 2, 3));
  record = registration(f, 1, 10);
  EXPECT_EQ(lifecycle_status_v2::retroactive_change, epose_test_journal::apply(registry, record, 2, 2));
}

TEST(epose_lifecycle_v2, window_caps_future_changes_without_changing_schema_one)
{
  fixture f;
  const auto distant = registration(f, 20, 30);
  lifecycle_registry_v2 legacy(f.nettype, f.genesis, f.parameters);
  lifecycle_registry_v2 window(f.nettype, f.genesis, f.parameters);
  epose_test_journal::activate_window(window);
  EXPECT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(legacy, distant, 1, 2));
  EXPECT_EQ(lifecycle_status_v2::invalid_transition,
      epose_test_journal::apply(window, distant, 1, 2));
  EXPECT_EQ(lifecycle_status_v2::accepted,
      epose_test_journal::apply(window, registration(f, 9, 20), 1, 2));
}

TEST(epose_lifecycle_v2, window_caps_lease_duration_at_thirty_epochs)
{
  fixture f;
  lifecycle_registry_v2 legacy(f.nettype, f.genesis, f.parameters);
  lifecycle_registry_v2 window(f.nettype, f.genesis, f.parameters);
  epose_test_journal::activate_window(window);
  const auto longest = registration(f, 2, 2 + EPOSE_REGISTRATION_TTL_EPOCHS);
  const auto excessive = registration(f, 2, 3 + EPOSE_REGISTRATION_TTL_EPOCHS);
  const auto unbounded = registration(f, 2, std::numeric_limits<uint64_t>::max());
  EXPECT_EQ(lifecycle_status_v2::accepted,
      epose_test_journal::apply(window, longest, 1, 2));
  EXPECT_EQ(lifecycle_status_v2::invalid_descriptor,
      epose_test_journal::apply(window, excessive, 1, 2));
  EXPECT_EQ(lifecycle_status_v2::invalid_descriptor,
      epose_test_journal::apply(window, unbounded, 1, 2));
  EXPECT_EQ(lifecycle_status_v2::accepted,
      epose_test_journal::apply(legacy, unbounded, 1, 2));
}

TEST(epose_lifecycle_v2, window_uses_activated_retention_for_future_changes)
{
  fixture f;
  lifecycle_registry_v2 registry(f.nettype, f.genesis, f.parameters);
  auto activation = registry.checkpoint();
  registry.activate_window(3, &activation);
  EXPECT_EQ(lifecycle_status_v2::invalid_transition,
      epose_test_journal::apply(registry, registration(f, 5, 12), 1, 2));
  lifecycle_registry_v2 within(f.nettype, f.genesis, f.parameters);
  epose_test_journal::activate_window(within, 3);
  EXPECT_EQ(lifecycle_status_v2::accepted,
      epose_test_journal::apply(within, registration(f, 4, 12), 1, 2));
  registry.rollback(activation);
  EXPECT_EQ(lifecycle_status_v2::accepted,
      epose_test_journal::apply(registry, registration(f, 20, 30), 1, 2));
}

TEST(epose_lifecycle_v2, payout_or_endpoint_update_requires_operator_and_service_signatures)
{
  fixture f;
  lifecycle_registry_v2 registry(f.nettype, f.genesis, f.parameters);
  const auto initial = registration(f);
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(registry, initial, 1, 2));
  auto next = descriptor(f, 1, 4, 12);
  next.reward_address.m_spend_public_key = make_keys().public_key;
  next.endpoint_descriptor_hash = hash_text("new-endpoint");
  auto update = signed_record(f, lifecycle_action_v2::update_descriptor,
      hash_identity_descriptor_v2(f.nettype, f.genesis, f.parameters, initial.next_descriptor),
      next, f.service.secret_key);
  reinterpret_cast<unsigned char *>(&update.operator_signature)[0] ^= 1;
  EXPECT_EQ(lifecycle_status_v2::invalid_operator_signature, epose_test_journal::apply(registry, update, 3, 4));
  update = signed_record(f, lifecycle_action_v2::update_descriptor,
      hash_identity_descriptor_v2(f.nettype, f.genesis, f.parameters, initial.next_descriptor),
      next, f.service.secret_key);
  reinterpret_cast<unsigned char *>(&update.service_signature)[0] ^= 1;
  EXPECT_EQ(lifecycle_status_v2::invalid_service_signature, epose_test_journal::apply(registry, update, 3, 4));
}

TEST(epose_lifecycle_v2, service_key_recovery_needs_offline_authority_and_new_key)
{
  fixture f;
  lifecycle_registry_v2 registry(f.nettype, f.genesis, f.parameters);
  const auto initial = registration(f);
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(registry, initial, 1, 2));

  const keys replacement = make_keys();
  auto next = descriptor(f, 1, 4, 12);
  next.service_public_key = replacement.public_key;
  auto recovery = signed_record(f, lifecycle_action_v2::recover_service_key,
      hash_identity_descriptor_v2(f.nettype, f.genesis, f.parameters, initial.next_descriptor),
      next, replacement.secret_key);
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(registry, recovery, 3, 4));
  ASSERT_NE(nullptr, registry.descriptor_for_epoch(next.identity_id, 3));
  EXPECT_EQ(initial.next_descriptor.service_public_key,
      registry.descriptor_for_epoch(next.identity_id, 3)->service_public_key);
  ASSERT_NE(nullptr, registry.descriptor_for_epoch(next.identity_id, 4));
  EXPECT_EQ(replacement.public_key, registry.descriptor_for_epoch(next.identity_id, 4)->service_public_key);
}

TEST(epose_lifecycle_v2, active_service_key_is_unique_across_identities)
{
  fixture first;
  fixture second;
  second.genesis = first.genesis;
  second.parameters = first.parameters;
  second.service = first.service;
  lifecycle_registry_v2 registry(first.nettype, first.genesis, first.parameters);
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(registry, registration(first), 1, 2));
  EXPECT_EQ(lifecycle_status_v2::service_key_in_use,
      epose_test_journal::apply(registry, registration(second), 1, 2));
  EXPECT_EQ(nullptr, registry.descriptor_for_epoch(
      derive_identity_id_v2(second.nettype, second.genesis, second.parameters,
          second.operator_auth.public_key), 2));
}

TEST(epose_lifecycle_v2, recovery_rejects_another_active_identity_key)
{
  fixture first;
  fixture second;
  second.genesis = first.genesis;
  second.parameters = first.parameters;
  lifecycle_registry_v2 registry(first.nettype, first.genesis, first.parameters);
  const lifecycle_record_v2 first_registration = registration(first);
  const lifecycle_record_v2 second_registration = registration(second);
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(registry, first_registration, 1, 2));
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(registry, second_registration, 1, 2));

  identity_descriptor_v2 recovered = descriptor(second, 1, 4, 12);
  recovered.service_public_key = first.service.public_key;
  const lifecycle_record_v2 recovery = signed_record(
      second, lifecycle_action_v2::recover_service_key,
      hash_identity_descriptor_v2(
          second.nettype, second.genesis, second.parameters,
          second_registration.next_descriptor),
      recovered, first.service.secret_key);
  EXPECT_EQ(lifecycle_status_v2::service_key_in_use, epose_test_journal::apply(registry, recovery, 3, 4));
  ASSERT_NE(nullptr, registry.descriptor_for_epoch(recovered.identity_id, 4));
  EXPECT_EQ(second.service.public_key,
      registry.descriptor_for_epoch(recovered.identity_id, 4)->service_public_key);
}

TEST(epose_lifecycle_v2, replaced_service_key_can_be_reused_after_it_is_inactive)
{
  fixture first;
  fixture successor;
  successor.genesis = first.genesis;
  successor.parameters = first.parameters;
  successor.service = first.service;
  lifecycle_registry_v2 registry(first.nettype, first.genesis, first.parameters);
  const lifecycle_record_v2 initial = registration(first, 2, 10);
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(registry, initial, 1, 2));

  const keys replacement = make_keys();
  identity_descriptor_v2 recovered = descriptor(first, 1, 4, 12);
  recovered.service_public_key = replacement.public_key;
  const lifecycle_record_v2 recovery = signed_record(
      first, lifecycle_action_v2::recover_service_key,
      hash_identity_descriptor_v2(first.nettype, first.genesis, first.parameters,
          initial.next_descriptor),
      recovered, replacement.secret_key);
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(registry, recovery, 3, 4));

  const lifecycle_record_v2 reused = registration(successor, 4, 10);
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(registry, reused, 3, 4));
  EXPECT_EQ(first.service.public_key,
      registry.descriptor_for_epoch(reused.next_descriptor.identity_id, 4)->service_public_key);
}

TEST(epose_lifecycle_v2, wrong_sequence_previous_hash_and_illegal_key_change_fail)
{
  fixture f;
  lifecycle_registry_v2 registry(f.nettype, f.genesis, f.parameters);
  const auto initial = registration(f);
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(registry, initial, 1, 2));
  const crypto::hash previous = hash_identity_descriptor_v2(f.nettype, f.genesis, f.parameters, initial.next_descriptor);

  auto next = descriptor(f, 2, 4, 12);
  auto update = signed_record(f, lifecycle_action_v2::update_descriptor, previous, next, f.service.secret_key);
  EXPECT_EQ(lifecycle_status_v2::wrong_sequence, epose_test_journal::apply(registry, update, 3, 4));
  next.sequence = 1;
  update = signed_record(f, lifecycle_action_v2::update_descriptor, hash_text("wrong"), next, f.service.secret_key);
  EXPECT_EQ(lifecycle_status_v2::wrong_previous_descriptor, epose_test_journal::apply(registry, update, 3, 4));

  const keys replacement = make_keys();
  next.service_public_key = replacement.public_key;
  update = signed_record(f, lifecycle_action_v2::update_descriptor, previous, next, replacement.secret_key);
  EXPECT_EQ(lifecycle_status_v2::invalid_transition, epose_test_journal::apply(registry, update, 3, 4));
}

TEST(epose_lifecycle_v2, renewal_extends_expiry_without_mutating_frozen_epochs)
{
  fixture f;
  lifecycle_registry_v2 registry(f.nettype, f.genesis, f.parameters);
  const auto initial = registration(f, 2, 5);
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(registry, initial, 1, 2));
  auto next = descriptor(f, 1, 4, 12);
  const auto renewal = signed_record(f, lifecycle_action_v2::renew_lease,
      hash_identity_descriptor_v2(f.nettype, f.genesis, f.parameters, initial.next_descriptor),
      next, f.service.secret_key);
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(registry, renewal, 3, 4));
  EXPECT_EQ(0u, registry.descriptor_for_epoch(next.identity_id, 3)->sequence);
  EXPECT_EQ(1u, registry.descriptor_for_epoch(next.identity_id, 4)->sequence);
  EXPECT_NE(nullptr, registry.descriptor_for_epoch(next.identity_id, 11));
  EXPECT_EQ(nullptr, registry.descriptor_for_epoch(next.identity_id, 12));
}

TEST(epose_lifecycle_v2, expired_service_key_can_be_reused_at_the_expiry_epoch)
{
  fixture first;
  fixture successor;
  successor.genesis = first.genesis;
  successor.parameters = first.parameters;
  successor.service = first.service;
  lifecycle_registry_v2 registry(first.nettype, first.genesis, first.parameters);
  ASSERT_EQ(lifecycle_status_v2::accepted,
      epose_test_journal::apply(registry, registration(first, 2, 4), 1, 2));
  EXPECT_EQ(nullptr, registry.descriptor_for_epoch(
      descriptor(first, 0, 2, 4).identity_id, 4));

  const lifecycle_record_v2 reused = registration(successor, 4, 10);
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(registry, reused, 3, 4));
  ASSERT_NE(nullptr, registry.descriptor_for_epoch(
      reused.next_descriptor.identity_id, 4));
  EXPECT_EQ(first.service.public_key,
      registry.descriptor_for_epoch(
          reused.next_descriptor.identity_id, 4)->service_public_key);
}

TEST(epose_lifecycle_v2, deregistration_is_delayed_and_does_not_erase_history)
{
  fixture f;
  lifecycle_registry_v2 registry(f.nettype, f.genesis, f.parameters);
  const auto initial = registration(f, 2, 20);
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(registry, initial, 1, 2));
  auto next = descriptor(f, 1, 5, 5);
  const auto deregister = signed_record(f, lifecycle_action_v2::deregister_identity,
      hash_identity_descriptor_v2(f.nettype, f.genesis, f.parameters, initial.next_descriptor),
      next, f.service.secret_key);
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(registry, deregister, 4, 5));
  EXPECT_NE(nullptr, registry.descriptor_for_epoch(next.identity_id, 4));
  EXPECT_EQ(nullptr, registry.descriptor_for_epoch(next.identity_id, 5));
  EXPECT_NE(crypto::null_hash, registry.state_hash());
}

TEST(epose_lifecycle_v2, deregistration_is_terminal_and_later_records_cannot_rewrite_history)
{
  fixture f;
  lifecycle_registry_v2 registry(f.nettype, f.genesis, f.parameters);
  const auto initial = registration(f, 2, 20);
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(registry, initial, 1, 2));

  auto stopped = descriptor(f, 1, 5, 5);
  const auto deregister = signed_record(f, lifecycle_action_v2::deregister_identity,
      hash_identity_descriptor_v2(f.nettype, f.genesis, f.parameters, initial.next_descriptor),
      stopped, f.service.secret_key);
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(registry, deregister, 4, 5));

  auto renewal_descriptor = descriptor(f, 2, 7, 12);
  const auto renewal = signed_record(f, lifecycle_action_v2::renew_lease,
      hash_identity_descriptor_v2(f.nettype, f.genesis, f.parameters, stopped),
      renewal_descriptor, f.service.secret_key);
  EXPECT_EQ(lifecycle_status_v2::invalid_transition, epose_test_journal::apply(registry, renewal, 6, 7));
  EXPECT_NE(nullptr, registry.descriptor_for_epoch(stopped.identity_id, 4));
  EXPECT_EQ(nullptr, registry.descriptor_for_epoch(stopped.identity_id, 5));
  EXPECT_EQ(nullptr, registry.descriptor_for_epoch(stopped.identity_id, 6));
  EXPECT_EQ(nullptr, registry.descriptor_for_epoch(stopped.identity_id, 7));
}

TEST(epose_lifecycle_v2, folded_retirement_keeps_terminal_rejections)
{
  fixture f;
  lifecycle_registry_v2 registry(f.nettype, f.genesis, f.parameters);
  const auto initial = registration(f, 2, 20);
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(registry, initial, 1, 2));
  const auto stopped = descriptor(f, 1, 5, 5);
  const auto deregister = signed_record(f, lifecycle_action_v2::deregister_identity,
      hash_identity_descriptor_v2(f.nettype, f.genesis, f.parameters, initial.next_descriptor),
      stopped, f.service.secret_key);
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(registry, deregister, 4, 5));
  const auto duplicate_registration = registration(f, 7, 12);
  const auto renewal = signed_record(f, lifecycle_action_v2::renew_lease,
      hash_identity_descriptor_v2(f.nettype, f.genesis, f.parameters, stopped),
      descriptor(f, 2, 7, 12), f.service.secret_key);
  EXPECT_EQ(lifecycle_status_v2::identity_already_exists,
      epose_test_journal::apply(registry, duplicate_registration, 6, 7));
  EXPECT_EQ(lifecycle_status_v2::invalid_transition,
      epose_test_journal::apply(registry, renewal, 6, 7));
  auto wrong_previous = signed_record(f, lifecycle_action_v2::renew_lease,
      hash_text("wrong-previous"), descriptor(f, 2, 7, 12), f.service.secret_key);
  EXPECT_EQ(lifecycle_status_v2::wrong_previous_descriptor,
      epose_test_journal::apply(registry, wrong_previous, 6, 7));
  fixture successor;
  successor.service = f.service;
  const auto reused_service = registration(successor, 7, 12);
  ASSERT_EQ(lifecycle_status_v2::accepted,
      epose_test_journal::apply(registry, reused_service, 6, 7));
  const size_t before = registry.state_bytes();
  EXPECT_EQ(1u, epose_test_journal::fold_retired(registry, 5).size());
  EXPECT_LT(registry.state_bytes(), before);
  EXPECT_EQ(lifecycle_status_v2::identity_already_exists,
      epose_test_journal::apply(registry, duplicate_registration, 6, 7));
  EXPECT_EQ(lifecycle_status_v2::identity_not_found,
      epose_test_journal::apply(registry, renewal, 6, 7));
  EXPECT_EQ(lifecycle_status_v2::identity_not_found,
      epose_test_journal::apply(registry, wrong_previous, 6, 7));
  EXPECT_EQ(lifecycle_status_v2::idempotent_duplicate,
      epose_test_journal::apply(registry, reused_service, 6, 7));
  EXPECT_TRUE(epose_test_journal::fold_retired(registry, 5).empty());
}

TEST(epose_lifecycle_v2, expired_lease_can_register_again_after_fold)
{
  fixture original;
  lifecycle_registry_v2 registry(original.nettype, original.genesis, original.parameters);
  epose_test_journal::activate_window(registry);
  const auto first = registration(original, 2, 4);
  ASSERT_EQ(lifecycle_status_v2::accepted,
      epose_test_journal::apply(registry, first, 1, 2));
  const auto renewed = signed_record(original, lifecycle_action_v2::renew_lease,
      hash_identity_descriptor_v2(original.nettype, original.genesis,
          original.parameters, first.next_descriptor),
      descriptor(original, 1, 5, 7), original.service.secret_key);
  ASSERT_EQ(lifecycle_status_v2::accepted,
      epose_test_journal::apply(registry, renewed, 4, 5));
  EXPECT_EQ(1u, epose_test_journal::fold_retired(registry, 7).size());
  EXPECT_EQ(nullptr, registry.latest(first.next_descriptor.identity_id));
  EXPECT_EQ(lifecycle_status_v2::accepted,
      epose_test_journal::apply(registry, registration(original, 9, 14), 8, 9));
}

TEST(epose_lifecycle_v2, deregistration_retires_operator_and_service_keys)
{
  fixture original;
  lifecycle_registry_v2 registry(original.nettype, original.genesis, original.parameters);
  epose_test_journal::activate_window(registry);
  const auto first = registration(original, 2, 12);
  ASSERT_EQ(lifecycle_status_v2::accepted,
      epose_test_journal::apply(registry, first, 1, 2));
  const auto stopped = descriptor(original, 1, 5, 5);
  const auto deregister = signed_record(original, lifecycle_action_v2::deregister_identity,
      hash_identity_descriptor_v2(original.nettype, original.genesis,
          original.parameters, first.next_descriptor),
      stopped, original.service.secret_key);
  ASSERT_EQ(lifecycle_status_v2::accepted,
      epose_test_journal::apply(registry, deregister, 4, 5));
  fixture successor;
  successor.service = original.service;
  EXPECT_EQ(lifecycle_status_v2::service_key_in_use,
      epose_test_journal::apply(registry, registration(successor, 7, 12), 6, 7));
  EXPECT_EQ(1u, epose_test_journal::fold_retired(registry, 5).size());
  EXPECT_EQ(lifecycle_status_v2::identity_already_exists,
      epose_test_journal::apply(registry, registration(original, 7, 12), 6, 7));
  EXPECT_EQ(lifecycle_status_v2::service_key_in_use,
      epose_test_journal::apply(registry, registration(successor, 7, 12), 6, 7));
}

TEST(epose_lifecycle_v2, batched_retirement_restores_journal_and_image)
{
  fixture context;
  lifecycle_registry_v2 registry(context.nettype, context.genesis, context.parameters);
  epose_test_journal::activate_window(registry);
  std::vector<fixture> identities(24);
  for (const auto &identity : identities)
  {
    const auto first = registration(identity, 2, 12);
    ASSERT_EQ(lifecycle_status_v2::accepted,
        epose_test_journal::apply(registry, first, 1, 2));
    const auto stopped = descriptor(identity, 1, 5, 5);
    const auto deregister = signed_record(identity, lifecycle_action_v2::deregister_identity,
        hash_identity_descriptor_v2(identity.nettype, identity.genesis,
            identity.parameters, first.next_descriptor),
        stopped, identity.service.secret_key);
    ASSERT_EQ(lifecycle_status_v2::accepted,
        epose_test_journal::apply(registry, deregister, 4, 5));
  }
  const crypto::hash before = registry.state_hash();
  auto undo = registry.checkpoint();
  EXPECT_EQ(identities.size(), registry.fold_retired(5, &undo).size());
  const crypto::hash after = registry.state_hash();
  state_image_writer_v2 writer;
  ASSERT_TRUE(registry.write_image(writer));
  const std::string image = writer.data();
  lifecycle_registry_v2 restored(context.nettype, context.genesis, context.parameters);
  state_image_reader_v2 reader(image);
  ASSERT_TRUE(restored.read_image(reader));
  epose_test_journal::activate_window(restored);
  EXPECT_EQ(after, restored.state_hash());
  registry.rollback(undo);
  EXPECT_EQ(before, registry.state_hash());
}

TEST(epose_lifecycle_v2, online_and_offline_authorities_must_be_distinct)
{
  fixture f;
  f.operator_auth = f.service;
  lifecycle_registry_v2 registry(f.nettype, f.genesis, f.parameters);
  const auto record = registration(f);
  ASSERT_EQ(lifecycle_status_v2::nonseparated_authorities, epose_test_journal::apply(registry, record, 1, 2));
  EXPECT_EQ(nullptr, registry.latest(record.next_descriptor.identity_id));
}

TEST(epose_lifecycle_v2, folded_identity_indexes_restore_from_journal)
{
  fixture a;
  fixture b;
  fixture c;
  fixture d;
  lifecycle_registry_v2 registry(a.nettype, a.genesis, a.parameters);
  const auto ra = registration(a, 2, 4);
  const auto rb = registration(b, 2, 4);
  const auto rc = registration(c, 2, 4);
  const auto rd = registration(d, 2, 20);
  for (const auto &record : {ra, rb, rc, rd})
    ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(registry, record, 1, 2));
  const crypto::hash before = registry.state_hash();
  auto undo = registry.checkpoint();
  registry.activate_window(QWC_EPOSE_RETAINED_EPOCHS, &undo);
  EXPECT_EQ(3u, registry.fold_retired(4, &undo).size());
  EXPECT_EQ(nullptr, registry.latest(ra.next_descriptor.identity_id));
  EXPECT_NE(nullptr, registry.latest(rd.next_descriptor.identity_id));
  registry.rollback(undo);
  EXPECT_EQ(before, registry.state_hash());
  for (const auto &record : {ra, rb, rc, rd})
    EXPECT_NE(nullptr, registry.latest(record.next_descriptor.identity_id));
}

TEST(epose_lifecycle_v2, state_hash_is_independent_of_identity_arrival_order)
{
  fixture a;
  fixture b;
  lifecycle_registry_v2 first(a.nettype, a.genesis, a.parameters);
  lifecycle_registry_v2 second(a.nettype, a.genesis, a.parameters);
  const auto record_a = registration(a);
  const auto record_b = registration(b);
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(first, record_a, 1, 2));
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(first, record_b, 1, 2));
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(second, record_b, 1, 2));
  ASSERT_EQ(lifecycle_status_v2::accepted, epose_test_journal::apply(second, record_a, 1, 2));
  EXPECT_EQ(first.state_hash(), second.state_hash());
  state_image_writer_v2 first_image;
  state_image_writer_v2 second_image;
  ASSERT_TRUE(first.write_image(first_image));
  ASSERT_TRUE(second.write_image(second_image));
  EXPECT_EQ(first_image.data(), second_image.data());
}

TEST(epose_lifecycle_v2, typed_registration_and_lifecycle_codecs_are_authorized_and_atomic)
{
  fixture f;
  const auto initial = registration(f);
  envelope_record_v2 encoded{};
  ASSERT_EQ(record_codec_status_v2::accepted,
      encode_lifecycle_record_v2(initial, f.nettype, f.genesis, f.parameters, encoded));
  EXPECT_EQ(static_cast<uint8_t>(record_type_v2::identity_descriptor), encoded.type);
  EXPECT_EQ(EPOSE_LIFECYCLE_RECORD_VERSION_V2, encoded.version);
  EXPECT_EQ(EPOSE_LIFECYCLE_PAYLOAD_BYTES_V2, encoded.payload.size());

  lifecycle_record_v2 decoded{};
  ASSERT_EQ(record_codec_status_v2::accepted,
      decode_lifecycle_record_v2(encoded, f.nettype, f.genesis, f.parameters, decoded));
  EXPECT_EQ(hash_lifecycle_record_v2(f.nettype, f.genesis, f.parameters, initial),
      hash_lifecycle_record_v2(f.nettype, f.genesis, f.parameters, decoded));

  auto next = descriptor(f, 1, 4, 12);
  const auto update = signed_record(f, lifecycle_action_v2::update_descriptor,
      hash_identity_descriptor_v2(f.nettype, f.genesis, f.parameters, initial.next_descriptor),
      next, f.service.secret_key);
  ASSERT_EQ(record_codec_status_v2::accepted,
      encode_lifecycle_record_v2(update, f.nettype, f.genesis, f.parameters, encoded));
  EXPECT_EQ(static_cast<uint8_t>(record_type_v2::descriptor_lifecycle), encoded.type);
  ASSERT_EQ(record_codec_status_v2::accepted,
      decode_lifecycle_record_v2(encoded, f.nettype, f.genesis, f.parameters, decoded));

  encoded.type = static_cast<uint8_t>(record_type_v2::identity_descriptor);
  decoded = update;
  EXPECT_EQ(record_codec_status_v2::wrong_type,
      decode_lifecycle_record_v2(encoded, f.nettype, f.genesis, f.parameters, decoded));
  EXPECT_EQ(crypto::null_hash, decoded.next_descriptor.identity_id);

  ASSERT_EQ(record_codec_status_v2::accepted,
      encode_lifecycle_record_v2(update, f.nettype, f.genesis, f.parameters, encoded));
  encoded.payload.back() ^= 1;
  EXPECT_EQ(record_codec_status_v2::invalid_record,
      decode_lifecycle_record_v2(encoded, f.nettype, f.genesis, f.parameters, decoded));
  EXPECT_EQ(crypto::null_hash, decoded.next_descriptor.identity_id);
}

TEST(epose_lifecycle_v2, version_two_transport_profile_is_signed_and_encoded)
{
  fixture f;
  auto next = descriptor(f, 0, 2, 10);
  next.version = 2;
  next.endpoint_transport = 3;
  next.dial_capabilities = 3;
  next.dns_address_families = 2;
  const auto initial = signed_record(f, lifecycle_action_v2::register_identity,
      crypto::null_hash, next, f.service.secret_key);
  envelope_record_v2 encoded{};
  ASSERT_EQ(record_codec_status_v2::accepted,
      encode_lifecycle_record_v2(initial, f.nettype, f.genesis, f.parameters, encoded));
  EXPECT_EQ(2, encoded.version);
  EXPECT_EQ(EPOSE_LIFECYCLE_PAYLOAD_BYTES_HF18_V2, encoded.payload.size());
  lifecycle_record_v2 decoded{};
  ASSERT_EQ(record_codec_status_v2::accepted,
      decode_lifecycle_record_v2(encoded, f.nettype, f.genesis, f.parameters, decoded));
  EXPECT_EQ(3, decoded.next_descriptor.dial_capabilities);
  EXPECT_EQ(2, decoded.next_descriptor.dns_address_families);
  decoded.next_descriptor.dial_capabilities = 2;
  EXPECT_EQ(lifecycle_status_v2::invalid_operator_signature,
      validate_lifecycle_record_authorization_v2(
          f.nettype, f.genesis, f.parameters, decoded));
}
