// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <cstring>
#include <limits>

#include "epose/resource_policy_v2.h"

namespace
{
  using namespace qwertycoin::epose;

  crypto::hash hash_text(const char *text)
  {
    return crypto::cn_fast_hash(text, std::strlen(text));
  }

  endpoint_descriptor_v2 descriptor(crypto::secret_key &secret)
  {
    endpoint_descriptor_v2 out{};
    crypto::generate_keys(out.service_public_key, secret);
    out.transport = endpoint_transport_v2::dns;
    out.host = "seed-00.qwertycoin.org";
    out.port = 8197;
    out.service_kind = 1;
    out.service_version = 1;
    out.sequence = 7;
    out.expiry_epoch = 20;
    EXPECT_TRUE(sign_endpoint_descriptor_v2(cryptonote::TESTNET, hash_text("genesis"),
        hash_text("parameters"), out, secret));
    return out;
  }

  relay_item_v2 relay_item(
      const char *id, relay_class_v2 record_class, size_t bytes, uint64_t deadline)
  {
    return {hash_text(id), record_class, bytes, deadline};
  }
}

TEST(epose_resource_policy_v2, signed_canonical_descriptor_is_context_bound)
{
  crypto::secret_key secret{};
  const auto value = descriptor(secret);
  EXPECT_EQ(resource_status_v2::accepted, validate_endpoint_descriptor_v2(
      cryptonote::TESTNET, hash_text("genesis"), hash_text("parameters"), value));
  EXPECT_EQ(resource_status_v2::invalid_signature, validate_endpoint_descriptor_v2(
      cryptonote::TESTNET, hash_text("other-genesis"), hash_text("parameters"), value));
}

TEST(epose_resource_policy_v2, dns_and_literal_hosts_must_be_canonical)
{
  crypto::secret_key secret{};
  auto value = descriptor(secret);
  value.host = "Seed-00.qwertycoin.org";
  EXPECT_EQ(resource_status_v2::noncanonical_host, validate_endpoint_descriptor_v2(
      cryptonote::TESTNET, hash_text("genesis"), hash_text("parameters"), value));
  value.host = "bad-.example.org";
  EXPECT_EQ(resource_status_v2::noncanonical_host, validate_endpoint_descriptor_v2(
      cryptonote::TESTNET, hash_text("genesis"), hash_text("parameters"), value));
  value.host = "localhost";
  EXPECT_EQ(resource_status_v2::noncanonical_host, validate_endpoint_descriptor_v2(
      cryptonote::TESTNET, hash_text("genesis"), hash_text("parameters"), value));
  value.transport = endpoint_transport_v2::tcp_ipv4;
  value.host = "8.8.8.8";
  ASSERT_TRUE(sign_endpoint_descriptor_v2(cryptonote::TESTNET, hash_text("genesis"),
      hash_text("parameters"), value, secret));
  EXPECT_EQ(resource_status_v2::accepted, validate_endpoint_descriptor_v2(
      cryptonote::TESTNET, hash_text("genesis"), hash_text("parameters"), value));
}

TEST(epose_resource_policy_v2, private_metadata_and_mapped_addresses_are_prohibited)
{
  EXPECT_FALSE(public_probe_address_v2("127.0.0.1"));
  EXPECT_FALSE(public_probe_address_v2("10.0.0.1"));
  EXPECT_FALSE(public_probe_address_v2("169.254.169.254"));
  EXPECT_FALSE(public_probe_address_v2("192.168.1.1"));
  EXPECT_FALSE(public_probe_address_v2("192.0.0.1"));
  EXPECT_TRUE(public_probe_address_v2("192.0.1.1"));
  EXPECT_FALSE(public_probe_address_v2("::1"));
  EXPECT_FALSE(public_probe_address_v2("fc00::1"));
  EXPECT_FALSE(public_probe_address_v2("::ffff:127.0.0.1"));
  EXPECT_FALSE(public_probe_address_v2("100::1"));
  EXPECT_FALSE(public_probe_address_v2("2001:2::1"));
  EXPECT_TRUE(public_probe_address_v2("8.8.8.8"));
  EXPECT_TRUE(public_probe_address_v2("2606:4700:4700::1111"));
}

TEST(epose_resource_policy_v2, dns_resolution_is_bounded_and_rechecked)
{
  EXPECT_EQ(resource_status_v2::resolution_empty, validate_resolved_targets_v2({}, 2));
  EXPECT_EQ(resource_status_v2::resolution_limit_exceeded,
      validate_resolved_targets_v2({"8.8.8.8", "1.1.1.1", "9.9.9.9"}, 2));
  EXPECT_EQ(resource_status_v2::accepted,
      validate_resolved_targets_v2({"8.8.8.8", "2606:4700:4700::1111"}, 2));
  EXPECT_EQ(resource_status_v2::prohibited_address,
      validate_resolved_targets_v2({"8.8.8.8", "169.254.169.254"}, 2));
}

TEST(epose_resource_policy_v2, probe_budget_bounds_bytes_timeout_concurrency_and_peers)
{
  probe_limits_v2 limits{2, 2, 1, 32, 64, 1000};
  probe_budget_v2 budget(limits);
  const crypto::hash a = hash_text("peer-a");
  const crypto::hash b = hash_text("peer-b");
  EXPECT_EQ(resource_status_v2::request_too_large, budget.acquire(a, 33, 64, 1000));
  EXPECT_EQ(resource_status_v2::response_too_large, budget.acquire(a, 32, 65, 1000));
  EXPECT_EQ(resource_status_v2::timeout_too_large, budget.acquire(a, 32, 64, 1001));
  ASSERT_EQ(resource_status_v2::accepted, budget.acquire(a, 32, 64, 1000));
  EXPECT_EQ(resource_status_v2::peer_limit_exceeded, budget.acquire(a, 1, 1, 1));
  ASSERT_EQ(resource_status_v2::accepted, budget.acquire(b, 1, 1, 1));
  EXPECT_EQ(resource_status_v2::concurrency_exceeded, budget.acquire(hash_text("peer-c"), 1, 1, 1));
  EXPECT_EQ(2u, budget.active());
  budget.release(a);
  EXPECT_EQ(1u, budget.active());
}

TEST(epose_resource_policy_v2, unknown_admission_context_never_allocates_cache)
{
  admission_context_cache_v2 cache(1);
  const crypto::hash allowed = hash_text("allowed");
  EXPECT_EQ(resource_status_v2::unknown_admission_context,
      cache.admit(hash_text("attacker"), {allowed}));
  EXPECT_EQ(0u, cache.size());
  EXPECT_EQ(resource_status_v2::accepted, cache.admit(allowed, {allowed}));
  EXPECT_EQ(resource_status_v2::accepted, cache.admit(allowed, {allowed}));
  EXPECT_EQ(1u, cache.size());
  EXPECT_EQ(resource_status_v2::context_cache_full,
      cache.admit(hash_text("second"), {allowed, hash_text("second")}));
}

TEST(epose_resource_policy_v2, obsolete_admission_context_is_evicted_before_capacity_check)
{
  admission_context_cache_v2 cache(1);
  const crypto::hash context_a = hash_text("context-a");
  const crypto::hash context_b = hash_text("context-b");
  ASSERT_EQ(resource_status_v2::accepted, cache.admit(context_a, {context_a}));
  ASSERT_EQ(1u, cache.size());
  EXPECT_EQ(resource_status_v2::accepted, cache.admit(context_b, {context_b}));
  EXPECT_EQ(1u, cache.size());
  EXPECT_EQ(resource_status_v2::unknown_admission_context, cache.admit(context_a, {context_b}));
}

TEST(epose_resource_policy_v2, rpc_pages_are_bounded_without_integer_wrap)
{
  uint64_t end = 0;
  EXPECT_EQ(resource_status_v2::accepted, validate_rpc_page_v2(10, 20, 100, 1000, 200, end));
  EXPECT_EQ(30u, end);
  EXPECT_EQ(resource_status_v2::invalid_page, validate_rpc_page_v2(10, 101, 100, 1000, 200, end));
  EXPECT_EQ(resource_status_v2::scan_limit_exceeded, validate_rpc_page_v2(190, 20, 100, 1000, 200, end));
  EXPECT_EQ(resource_status_v2::scan_limit_exceeded,
      validate_rpc_page_v2(std::numeric_limits<uint64_t>::max(), 1,
          1, std::numeric_limits<uint64_t>::max(), std::numeric_limits<uint64_t>::max(), end));
}

TEST(epose_resource_policy_v2, invalid_transport_and_literal_family_fail_closed)
{
  crypto::secret_key secret{};
  auto value = descriptor(secret);
  value.transport = static_cast<endpoint_transport_v2>(99);
  EXPECT_EQ(resource_status_v2::invalid_descriptor, validate_endpoint_descriptor_v2(
      cryptonote::TESTNET, hash_text("genesis"), hash_text("parameters"), value));
  value.transport = endpoint_transport_v2::tcp_ipv6;
  value.host = "8.8.8.8";
  ASSERT_TRUE(sign_endpoint_descriptor_v2(cryptonote::TESTNET, hash_text("genesis"),
      hash_text("parameters"), value, secret));
  EXPECT_EQ(resource_status_v2::noncanonical_host, validate_endpoint_descriptor_v2(
      cryptonote::TESTNET, hash_text("genesis"), hash_text("parameters"), value));
}

TEST(epose_resource_policy_v2, relay_queue_reserves_capacity_for_enrollment_and_evidence)
{
  deadline_relay_queue_v2 queue(relay_queue_limits_v2{6, 600, 2, 2, 200, 200});
  ASSERT_EQ(resource_status_v2::accepted,
      queue.enqueue(relay_item("enroll-a", relay_class_v2::enrollment, 100, 20), 10));
  ASSERT_EQ(resource_status_v2::accepted,
      queue.enqueue(relay_item("enroll-b", relay_class_v2::enrollment, 100, 21), 10));
  ASSERT_EQ(resource_status_v2::accepted,
      queue.enqueue(relay_item("enroll-c", relay_class_v2::enrollment, 100, 22), 10));
  ASSERT_EQ(resource_status_v2::accepted,
      queue.enqueue(relay_item("enroll-d", relay_class_v2::enrollment, 100, 23), 10));
  EXPECT_EQ(resource_status_v2::relay_queue_full,
      queue.enqueue(relay_item("enroll-e", relay_class_v2::enrollment, 1, 24), 10));
  EXPECT_EQ(resource_status_v2::accepted,
      queue.enqueue(relay_item("evidence-a", relay_class_v2::evidence, 100, 18), 10));
  EXPECT_EQ(resource_status_v2::accepted,
      queue.enqueue(relay_item("evidence-b", relay_class_v2::evidence, 100, 19), 10));
  EXPECT_EQ(4u, queue.size(relay_class_v2::enrollment));
  EXPECT_EQ(2u, queue.size(relay_class_v2::evidence));
  EXPECT_EQ(600u, queue.bytes());
}

TEST(epose_resource_policy_v2, relay_queue_is_idempotent_conflict_safe_and_prunes_deadlines)
{
  deadline_relay_queue_v2 queue(relay_queue_limits_v2{4, 400, 1, 1, 100, 100});
  const auto original = relay_item("same", relay_class_v2::enrollment, 100, 10);
  ASSERT_EQ(resource_status_v2::accepted, queue.enqueue(original, 9));
  EXPECT_EQ(resource_status_v2::idempotent_duplicate, queue.enqueue(original, 9));
  auto conflict = original;
  ++conflict.deadline_height;
  EXPECT_EQ(resource_status_v2::relay_item_conflict, queue.enqueue(conflict, 9));
  EXPECT_EQ(resource_status_v2::relay_item_expired,
      queue.enqueue(relay_item("expired", relay_class_v2::evidence, 10, 8), 9));
  queue.prune_expired(11);
  EXPECT_EQ(0u, queue.size());
  EXPECT_EQ(0u, queue.bytes());
}

TEST(epose_resource_policy_v2, relay_template_selection_preserves_both_shares_and_deadline_order)
{
  deadline_relay_queue_v2 queue(relay_queue_limits_v2{8, 800, 2, 2, 200, 200});
  const auto enrollment_early = relay_item("enroll-early", relay_class_v2::enrollment, 100, 30);
  const auto enrollment_late = relay_item("enroll-late", relay_class_v2::enrollment, 100, 50);
  const auto evidence_early = relay_item("evidence-early", relay_class_v2::evidence, 100, 20);
  const auto evidence_late = relay_item("evidence-late", relay_class_v2::evidence, 100, 40);
  ASSERT_EQ(resource_status_v2::accepted, queue.enqueue(enrollment_late, 10));
  ASSERT_EQ(resource_status_v2::accepted, queue.enqueue(evidence_late, 10));
  ASSERT_EQ(resource_status_v2::accepted, queue.enqueue(enrollment_early, 10));
  ASSERT_EQ(resource_status_v2::accepted, queue.enqueue(evidence_early, 10));

  std::vector<relay_item_v2> selected{{hash_text("stale"), relay_class_v2::evidence, 1, 1}};
  ASSERT_EQ(resource_status_v2::accepted,
      queue.select_for_template(10, relay_template_limits_v2{3, 300, 1, 1, 100, 100}, selected));
  ASSERT_EQ(3u, selected.size());
  EXPECT_EQ(enrollment_early.id, selected[0].id);
  EXPECT_EQ(evidence_early.id, selected[1].id);
  EXPECT_EQ(evidence_late.id, selected[2].id);

  const relay_template_limits_v2 invalid{1, 100, 1, 1, 50, 50};
  EXPECT_EQ(resource_status_v2::invalid_configuration,
      queue.select_for_template(10, invalid, selected));
  EXPECT_TRUE(selected.empty());
}
