// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_basic/tx_extra.h"
#include "cryptonote_config.h"
#include "epose/envelope_v2.h"

namespace
{
  using namespace cryptonote;
  using namespace qwertycoin::epose;

  envelope_limits_v2 limits()
  {
    envelope_limits_v2 out{};
    out.max_envelope_bytes = 1024;
    out.max_records = 4;
    out.max_record_payload_bytes = 512;
    out.max_signature_verifications = 8;
    out.max_admission_verifications = 2;
    return out;
  }

  envelope_status_v2 strict_parse(const std::vector<uint8_t> &extra, uint8_t major, contract_extra_scope scope)
  {
    std::vector<envelope_record_v2> records;
    envelope_budget_v2 budget{};
    return parse_transaction_extra_v2(extra, major, 1, limits(), records, budget, scope);
  }

  crypto::hash hash_of(char c)
  {
    crypto::hash h;
    memset(&h, c, sizeof(h));
    return h;
  }

  tx_extra_vm_call sample_call(bool with_mint)
  {
    tx_extra_vm_call call{};
    call.kind = tx_extra_vm_call::kind_invoke;
    call.contract_id = hash_of(0x11);
    call.method = 7;
    call.args = std::string("\x01\x02\x03", 3);
    call.gas_limit = 123456;
    call.gas_price = 3;
    call.deposit = 1000000;
    if (with_mint)
    {
      tx_extra_vm_mint_destination mint{};
      memset(&mint.key, 0x22, sizeof(mint.key));
      mint.view_tag.data = 0x5a;
      memset(&mint.tx_key, 0x33, sizeof(mint.tx_key));
      call.mint.push_back(mint);
    }
    return call;
  }

  std::vector<uint8_t> extra_with_root(const crypto::hash &root)
  {
    std::vector<uint8_t> extra;
    EXPECT_TRUE(add_vm_state_root_to_tx_extra(extra, root));
    return extra;
  }

  std::vector<uint8_t> extra_with_call(const tx_extra_vm_call &call)
  {
    std::vector<uint8_t> extra;
    EXPECT_TRUE(add_vm_call_to_tx_extra(extra, call));
    return extra;
  }

  transaction_prefix prefix_with(const std::vector<uint8_t> &extra)
  {
    transaction_prefix tx;
    tx.extra = extra;
    return tx;
  }
}

TEST(contract_tx_extra, state_root_roundtrip_and_tag)
{
  const crypto::hash root = hash_of(0x42);
  const std::vector<uint8_t> extra = extra_with_root(root);
  ASSERT_EQ(1u + sizeof(crypto::hash), extra.size());
  EXPECT_EQ(0x07, extra[0]);
  crypto::hash out = crypto::null_hash;
  ASSERT_EQ(1u, get_tx_vm_state_root(extra, out));
  EXPECT_EQ(root, out);
  EXPECT_EQ(0u, get_tx_vm_state_root(std::vector<uint8_t>{}, out));
}

TEST(contract_tx_extra, call_roundtrip_with_and_without_mint)
{
  for (bool with_mint : {false, true})
  {
    const tx_extra_vm_call call = sample_call(with_mint);
    const std::vector<uint8_t> extra = extra_with_call(call);
    ASSERT_FALSE(extra.empty());
    EXPECT_EQ(0x08, extra[0]);
    tx_extra_vm_call out{};
    ASSERT_EQ(1u, get_tx_vm_call(extra, out));
    EXPECT_EQ(1u, out.version);
    EXPECT_EQ(call.kind, out.kind);
    EXPECT_EQ(call.contract_id, out.contract_id);
    EXPECT_EQ(call.method, out.method);
    EXPECT_EQ(call.code, out.code);
    EXPECT_EQ(call.args, out.args);
    EXPECT_EQ(call.gas_limit, out.gas_limit);
    EXPECT_EQ(call.gas_price, out.gas_price);
    EXPECT_EQ(call.deposit, out.deposit);
    ASSERT_EQ(call.mint.size(), out.mint.size());
    if (with_mint)
    {
      EXPECT_EQ(call.mint[0].key, out.mint[0].key);
      EXPECT_EQ(call.mint[0].view_tag.data, out.mint[0].view_tag.data);
      EXPECT_EQ(call.mint[0].tx_key, out.mint[0].tx_key);
    }
  }
}

TEST(contract_tx_extra, strict_parser_gates_all_contract_fields)
{
  std::vector<tx_extra_field> fields;
  for (const std::vector<uint8_t> &extra : {extra_with_root(hash_of(1)), extra_with_call(sample_call(false))})
  {
    EXPECT_FALSE(parse_tx_extra_strict(extra, fields, true, false));
    EXPECT_TRUE(parse_tx_extra_strict(extra, fields, true, true));
  }
}

TEST(contract_tx_extra, sort_places_root_before_epose_and_call_after_window)
{
  std::vector<uint8_t> extra;
  ASSERT_TRUE(add_extra_nonce_to_tx_extra(extra, "nonce"));
  ASSERT_TRUE(add_vm_call_to_tx_extra(extra, sample_call(false)));
  ASSERT_TRUE(add_height_window_to_tx_extra(extra, {1, 2}));
  ASSERT_TRUE(add_epose_v2_to_tx_extra(extra, "env"));
  ASSERT_TRUE(add_vm_state_root_to_tx_extra(extra, hash_of(3)));
  crypto::public_key pk{};
  ASSERT_TRUE(add_tx_pub_key_to_extra(extra, pk));

  std::vector<uint8_t> sorted;
  ASSERT_TRUE(sort_tx_extra(extra, sorted, false, true));
  std::vector<tx_extra_field> fields;
  ASSERT_TRUE(parse_tx_extra(sorted, fields, true));
  ASSERT_EQ(6u, fields.size());
  EXPECT_EQ(typeid(tx_extra_pub_key), fields[0].type());
  EXPECT_EQ(typeid(tx_extra_vm_state_root), fields[1].type());
  EXPECT_EQ(typeid(tx_extra_epose_v2), fields[2].type());
  EXPECT_EQ(typeid(tx_extra_height_window), fields[3].type());
  EXPECT_EQ(typeid(tx_extra_vm_call), fields[4].type());
  EXPECT_EQ(typeid(tx_extra_nonce), fields[5].type());
}

TEST(contract_tx_extra, scope_rules)
{
  std::vector<tx_extra_field> root{tx_extra_vm_state_root{hash_of(1)}};
  std::vector<tx_extra_field> call{sample_call(false)};
  std::vector<tx_extra_field> window{tx_extra_height_window{1, 2}};
  std::vector<tx_extra_field> two_roots{root[0], root[0]};
  std::vector<tx_extra_field> two_calls{call[0], call[0]};
  std::vector<tx_extra_field> call_and_window{call[0], window[0]};

  EXPECT_TRUE(check_contract_extra_fields(root, contract_extra_scope::coinbase));
  EXPECT_FALSE(check_contract_extra_fields(call, contract_extra_scope::coinbase));
  EXPECT_FALSE(check_contract_extra_fields(window, contract_extra_scope::coinbase));
  EXPECT_FALSE(check_contract_extra_fields(two_roots, contract_extra_scope::coinbase));

  EXPECT_FALSE(check_contract_extra_fields(root, contract_extra_scope::transaction));
  EXPECT_TRUE(check_contract_extra_fields(call, contract_extra_scope::transaction));
  EXPECT_TRUE(check_contract_extra_fields(call_and_window, contract_extra_scope::transaction));
  EXPECT_FALSE(check_contract_extra_fields(two_calls, contract_extra_scope::transaction));

  EXPECT_TRUE(check_contract_extra_fields(root, contract_extra_scope::either));
  EXPECT_FALSE(check_contract_extra_fields(two_calls, contract_extra_scope::either));
}

TEST(contract_tx_extra, legacy_fields_before_fork)
{
  const transaction_prefix root = prefix_with(extra_with_root(hash_of(1)));
  const transaction_prefix call = prefix_with(extra_with_call(sample_call(true)));
  EXPECT_FALSE(check_tx_contract_extra(root, HF_VERSION_QWC_EPOSE, contract_extra_scope::coinbase));
  EXPECT_FALSE(check_tx_contract_extra(call, HF_VERSION_QWC_EPOSE, contract_extra_scope::coinbase));
  EXPECT_TRUE(check_tx_contract_extra(call, HF_VERSION_QWC_EPOSE, contract_extra_scope::transaction));
  tx_extra_vm_call parsed{};
  EXPECT_TRUE(contract_call_before_activation(get_tx_vm_call(call.extra, parsed), HF_VERSION_QWC_EPOSE));
  EXPECT_FALSE(check_tx_contract_extra(call, HF_VERSION_QWC_CONTRACTS, contract_extra_scope::coinbase));
  EXPECT_TRUE(check_tx_contract_extra(root, HF_VERSION_QWC_CONTRACTS, contract_extra_scope::coinbase));
  EXPECT_TRUE(check_tx_contract_extra(call, HF_VERSION_QWC_CONTRACTS, contract_extra_scope::transaction));
  EXPECT_TRUE(check_tx_contract_extra(prefix_with({}), HF_VERSION_QWC_EPOSE, contract_extra_scope::transaction));

  EXPECT_EQ(envelope_status_v2::malformed_transaction_extra, strict_parse(root.extra, HF_VERSION_QWC_EPOSE, contract_extra_scope::coinbase));
  EXPECT_EQ(envelope_status_v2::malformed_transaction_extra, strict_parse(call.extra, HF_VERSION_QWC_EPOSE, contract_extra_scope::transaction));
}

TEST(contract_tx_extra, envelope_gate_by_transaction_kind)
{
  const std::vector<uint8_t> root = extra_with_root(hash_of(1));
  const std::vector<uint8_t> call = extra_with_call(sample_call(false));
  EXPECT_EQ(envelope_status_v2::accepted, strict_parse(root, HF_VERSION_QWC_CONTRACTS, contract_extra_scope::coinbase));
  EXPECT_EQ(envelope_status_v2::malformed_transaction_extra, strict_parse(root, HF_VERSION_QWC_CONTRACTS, contract_extra_scope::transaction));
  EXPECT_EQ(envelope_status_v2::accepted, strict_parse(call, HF_VERSION_QWC_CONTRACTS, contract_extra_scope::transaction));
  EXPECT_EQ(envelope_status_v2::malformed_transaction_extra, strict_parse(call, HF_VERSION_QWC_CONTRACTS, contract_extra_scope::coinbase));

  std::vector<uint8_t> two_roots = root;
  two_roots.insert(two_roots.end(), root.begin(), root.end());
  EXPECT_EQ(envelope_status_v2::malformed_transaction_extra, strict_parse(two_roots, HF_VERSION_QWC_CONTRACTS, contract_extra_scope::coinbase));
}

TEST(contract_tx_extra, noncanonical_call_varint_rejected)
{
  std::vector<uint8_t> extra = extra_with_call(sample_call(false));
  ASSERT_EQ(1u, extra[1]);
  extra[1] = 0x81;
  extra.insert(extra.begin() + 2, 0x00);
  EXPECT_NE(envelope_status_v2::accepted, strict_parse(extra, HF_VERSION_QWC_CONTRACTS, contract_extra_scope::transaction));
}

TEST(contract_tx_extra, strict_parsing_only_for_calls)
{
  std::vector<uint8_t> legacy_extra{0xff};
  EXPECT_TRUE(check_tx_contract_extra(prefix_with(legacy_extra), HF_VERSION_QWC_CONTRACTS, contract_extra_scope::transaction));
  EXPECT_TRUE(check_tx_contract_extra(prefix_with(legacy_extra), HF_VERSION_QWC_CONTRACTS, contract_extra_scope::coinbase));

  std::vector<uint8_t> call_extra = extra_with_call(sample_call(false));
  call_extra.push_back(0xff);
  EXPECT_FALSE(check_tx_contract_extra(prefix_with(call_extra), HF_VERSION_QWC_CONTRACTS, contract_extra_scope::transaction));
}

TEST(contract_tx_extra, call_requires_canonical_extra_order)
{
  std::vector<uint8_t> extra = extra_with_call(sample_call(false));
  ASSERT_TRUE(add_extra_nonce_to_tx_extra(extra, "nonce"));
  EXPECT_TRUE(check_tx_contract_extra(prefix_with(extra), HF_VERSION_QWC_CONTRACTS, contract_extra_scope::transaction));

  std::vector<uint8_t> reordered;
  ASSERT_TRUE(add_extra_nonce_to_tx_extra(reordered, "nonce"));
  const std::vector<uint8_t> call = extra_with_call(sample_call(false));
  reordered.insert(reordered.end(), call.begin(), call.end());
  std::vector<tx_extra_field> fields;
  ASSERT_TRUE(parse_tx_extra_strict(reordered, fields, true, true));
  EXPECT_FALSE(check_tx_contract_extra(prefix_with(reordered), HF_VERSION_QWC_CONTRACTS, contract_extra_scope::transaction));
  EXPECT_TRUE(check_tx_contract_extra(prefix_with(extra), HF_VERSION_QWC_CONTRACTS, contract_extra_scope::transaction));
}

TEST(contract_tx_extra, extra_size_limit_only_raised_for_contract_calls_from_fork)
{
  const transaction_prefix call = prefix_with(extra_with_call(sample_call(false)));
  const transaction_prefix plain = prefix_with(extra_with_root(hash_of(1)));
  EXPECT_EQ(QWC_CONTRACT_TX_EXTRA_MAX_SIZE, get_tx_extra_size_limit(call, HF_VERSION_QWC_CONTRACTS));
  EXPECT_EQ(static_cast<size_t>(MAX_TX_EXTRA_SIZE), get_tx_extra_size_limit(call, HF_VERSION_QWC_EPOSE));
  EXPECT_EQ(static_cast<size_t>(MAX_TX_EXTRA_SIZE), get_tx_extra_size_limit(plain, HF_VERSION_QWC_CONTRACTS));
}

TEST(contract_tx_extra, preactivation_vm_tag_keeps_legacy_ring_hash)
{
  tx_extra_vm_call call = sample_call(false);
  call.signatures.emplace_back();
  transaction_prefix tx = prefix_with(extra_with_call(call));
  tx.version = 2;
  crypto::hash ring_hash{};
  tx_extra_vm_call parsed{};
  ASSERT_EQ(1u, get_tx_vm_call(tx.extra, parsed));
  ASSERT_TRUE(get_contract_ring_hash(tx, ring_hash, HF_VERSION_QWC_EPOSE));
  EXPECT_EQ(get_transaction_prefix_hash(tx), ring_hash);
  EXPECT_TRUE(check_tx_contract_extra(tx, HF_VERSION_QWC_EPOSE, contract_extra_scope::transaction));
  EXPECT_TRUE(contract_call_before_activation(get_tx_vm_call(tx.extra, parsed), HF_VERSION_QWC_EPOSE));
  ASSERT_TRUE(get_contract_ring_hash(tx, ring_hash, HF_VERSION_QWC_CONTRACTS));
  EXPECT_NE(get_transaction_prefix_hash(tx), ring_hash);
}

TEST(contract_tx_extra, coinbase_budget_raised_from_fork)
{
  EXPECT_EQ(static_cast<size_t>(MAX_TX_EXTRA_SIZE), get_coinbase_extra_size_limit(HF_VERSION_QWC_EPOSE));
  EXPECT_EQ(static_cast<size_t>(CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE), get_coinbase_blob_reserved_size(HF_VERSION_QWC_EPOSE));
  EXPECT_GE(get_coinbase_extra_size_limit(HF_VERSION_QWC_CONTRACTS), MAX_TX_EXTRA_SIZE + extra_with_root(hash_of(1)).size());
  EXPECT_GE(get_coinbase_blob_reserved_size(HF_VERSION_QWC_CONTRACTS), CRYPTONOTE_COINBASE_BLOB_RESERVED_SIZE + get_coinbase_extra_size_limit(HF_VERSION_QWC_CONTRACTS));
}
