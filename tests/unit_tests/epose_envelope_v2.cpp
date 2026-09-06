// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "epose/envelope_v2.h"

namespace
{
  using namespace qwertycoin::epose;

  envelope_limits_v2 limits()
  {
    envelope_limits_v2 out{};
    out.max_envelope_bytes = 1024;
    out.max_records = 4;
    out.max_record_payload_bytes = 512;
    out.max_signature_verifications = 8;
    out.max_admission_verifications = 2;
    out.supported_record_versions[static_cast<size_t>(record_type_v2::identity_descriptor)] = 1;
    out.supported_record_versions[static_cast<size_t>(record_type_v2::admission_lease)] = 1;
    out.supported_record_versions[static_cast<size_t>(record_type_v2::service_receipt)] = 1;
    return out;
  }

  envelope_record_v2 record(record_type_v2 type, const std::string &payload)
  {
    return {static_cast<uint8_t>(type), 1, payload};
  }
}

TEST(epose_envelope_v2, canonical_envelope_and_outer_field_roundtrip)
{
  const std::vector<envelope_record_v2> expected{
      record(record_type_v2::identity_descriptor, "descriptor"),
      record(record_type_v2::service_receipt, "receipt")};
  std::string field;
  envelope_budget_v2 encoded_budget{};
  ASSERT_EQ(envelope_status_v2::accepted, encode_tx_extra_envelope_field_v2(expected, limits(), field, encoded_budget));
  ASSERT_EQ(0x05, static_cast<uint8_t>(field[0]));
  std::vector<envelope_record_v2> actual;
  envelope_budget_v2 parsed_budget{};
  ASSERT_EQ(envelope_status_v2::accepted, parse_tx_extra_envelope_field_v2(field, limits(), actual, parsed_budget));
  ASSERT_EQ(expected.size(), actual.size());
  EXPECT_EQ(expected[0].payload, actual[0].payload);
  EXPECT_EQ(expected[1].payload, actual[1].payload);
  EXPECT_EQ(3u, parsed_budget.signature_verifications);
  EXPECT_EQ(encoded_budget.bytes, parsed_budget.bytes);
}

TEST(epose_envelope_v2, noncanonical_outer_varints_and_wrong_tag_fail_closed)
{
  std::string field;
  envelope_budget_v2 budget{};
  ASSERT_EQ(envelope_status_v2::accepted, encode_tx_extra_envelope_field_v2({record(record_type_v2::service_receipt, "x")}, limits(), field, budget));
  std::vector<envelope_record_v2> records;
  std::string noncanonical_tag("\x85\x00", 2);
  noncanonical_tag.append(field.substr(1));
  EXPECT_EQ(envelope_status_v2::noncanonical_varint, parse_tx_extra_envelope_field_v2(noncanonical_tag, limits(), records, budget));
  std::string overflowing_varint(10, static_cast<char>(0xff));
  EXPECT_EQ(envelope_status_v2::malformed_outer_field, parse_tx_extra_envelope_field_v2(overflowing_varint, limits(), records, budget));
  field[0] = 0x06;
  EXPECT_EQ(envelope_status_v2::wrong_tag, parse_tx_extra_envelope_field_v2(field, limits(), records, budget));
}

TEST(epose_envelope_v2, unknown_versions_types_and_flags_fail_closed)
{
  std::string encoded;
  envelope_budget_v2 budget{};
  ASSERT_EQ(envelope_status_v2::accepted, encode_envelope_v2({record(record_type_v2::service_receipt, "x")}, limits(), encoded, budget));
  std::vector<envelope_record_v2> records;
  auto changed = encoded;
  changed[0] = 'X';
  EXPECT_EQ(envelope_status_v2::bad_magic, parse_envelope_v2(changed, limits(), records, budget));
  changed = encoded;
  changed[4] = 2;
  EXPECT_EQ(envelope_status_v2::unsupported_envelope_version, parse_envelope_v2(changed, limits(), records, budget));
  changed = encoded;
  changed[5] = 1;
  EXPECT_EQ(envelope_status_v2::nonzero_flags, parse_envelope_v2(changed, limits(), records, budget));
  changed = encoded;
  changed[12] = 99;
  EXPECT_EQ(envelope_status_v2::unknown_record_type, parse_envelope_v2(changed, limits(), records, budget));
  changed = encoded;
  changed[13] = 2;
  EXPECT_EQ(envelope_status_v2::unsupported_record_version, parse_envelope_v2(changed, limits(), records, budget));
  changed = encoded;
  changed[14] = 1;
  EXPECT_EQ(envelope_status_v2::nonzero_flags, parse_envelope_v2(changed, limits(), records, budget));
}

TEST(epose_envelope_v2, malformed_sizes_counts_truncation_and_trailing_bytes_fail_closed)
{
  std::string encoded;
  envelope_budget_v2 budget{};
  ASSERT_EQ(envelope_status_v2::accepted, encode_envelope_v2({record(record_type_v2::service_receipt, "payload")}, limits(), encoded, budget));
  std::vector<envelope_record_v2> records;
  auto changed = encoded;
  changed[8] = static_cast<char>(static_cast<uint8_t>(changed[8]) + 1);
  EXPECT_EQ(envelope_status_v2::record_size_mismatch, parse_envelope_v2(changed, limits(), records, budget));
  changed = encoded;
  changed[6] = 2;
  EXPECT_EQ(envelope_status_v2::record_count_mismatch, parse_envelope_v2(changed, limits(), records, budget));
  changed = encoded.substr(0, encoded.size() - 1);
  EXPECT_EQ(envelope_status_v2::record_size_mismatch, parse_envelope_v2(changed, limits(), records, budget));
  changed = encoded;
  changed.append("x");
  EXPECT_EQ(envelope_status_v2::record_size_mismatch, parse_envelope_v2(changed, limits(), records, budget));
}

TEST(epose_envelope_v2, byte_record_and_crypto_budgets_are_charged_before_semantics)
{
  envelope_budget_v2 budget{};
  std::string encoded;
  auto bounded = limits();
  bounded.max_records = 1;
  EXPECT_EQ(envelope_status_v2::record_count_exceeded,
      encode_envelope_v2({record(record_type_v2::service_receipt, "same"), record(record_type_v2::service_receipt, "same")}, bounded, encoded, budget));
  bounded = limits();
  bounded.max_signature_verifications = 1;
  EXPECT_EQ(envelope_status_v2::signature_budget_exceeded,
      encode_envelope_v2({record(record_type_v2::service_receipt, "x")}, bounded, encoded, budget));
  bounded = limits();
  bounded.max_admission_verifications = 1;
  EXPECT_EQ(envelope_status_v2::admission_budget_exceeded,
      encode_envelope_v2({record(record_type_v2::admission_lease, "a"), record(record_type_v2::admission_lease, "b")}, bounded, encoded, budget));
  bounded = limits();
  bounded.max_record_payload_bytes = 2;
  EXPECT_EQ(envelope_status_v2::oversized_record,
      encode_envelope_v2({record(record_type_v2::identity_descriptor, "long")}, bounded, encoded, budget));

  bounded = limits();
  ASSERT_EQ(envelope_status_v2::accepted,
      encode_envelope_v2({record(record_type_v2::service_receipt, "same"), record(record_type_v2::service_receipt, "same")}, bounded, encoded, budget));
  EXPECT_EQ(2u, budget.records);
  EXPECT_EQ(4u, budget.signature_verifications);
}

TEST(epose_envelope_v2, reserved_record_types_remain_disabled_until_payload_specs_exist)
{
  envelope_budget_v2 budget{};
  std::string encoded;
  EXPECT_EQ(envelope_status_v2::unsupported_record_version,
      encode_envelope_v2({record(record_type_v2::descriptor_lifecycle, "reserved")}, limits(), encoded, budget));
  EXPECT_EQ(envelope_status_v2::unsupported_record_version,
      encode_envelope_v2({record(record_type_v2::service_payment_proof, "reserved")}, limits(), encoded, budget));
}

TEST(epose_envelope_v2, invalid_limits_fail_closed)
{
  envelope_limits_v2 invalid{};
  envelope_budget_v2 budget{};
  std::string encoded;
  EXPECT_EQ(envelope_status_v2::invalid_limits, encode_envelope_v2({}, invalid, encoded, budget));
  std::vector<envelope_record_v2> records;
  EXPECT_EQ(envelope_status_v2::invalid_limits, parse_envelope_v2("", invalid, records, budget));
}

TEST(epose_envelope_v2, empty_envelopes_are_rejected)
{
  envelope_budget_v2 budget{};
  std::string encoded;
  EXPECT_EQ(envelope_status_v2::empty_envelope, encode_envelope_v2({}, limits(), encoded, budget));
}

TEST(epose_envelope_v2, coinbase_and_fee_funded_fields_use_the_same_parser)
{
  std::string field;
  envelope_budget_v2 ignored{};
  ASSERT_EQ(envelope_status_v2::accepted,
      encode_tx_extra_envelope_field_v2({record(record_type_v2::service_receipt, "receipt")}, limits(), field, ignored));
  std::vector<envelope_record_v2> coinbase_records;
  std::vector<envelope_record_v2> fee_records;
  envelope_budget_v2 coinbase_budget{};
  envelope_budget_v2 fee_budget{};
  ASSERT_EQ(envelope_status_v2::accepted,
      parse_transaction_envelope_fields_v2({field}, 1, limits(), coinbase_records, coinbase_budget));
  ASSERT_EQ(envelope_status_v2::accepted,
      parse_transaction_envelope_fields_v2({field}, 1, limits(), fee_records, fee_budget));
  EXPECT_EQ(coinbase_records.front().payload, fee_records.front().payload);
  EXPECT_EQ(coinbase_budget.bytes, fee_budget.bytes);
  EXPECT_EQ(coinbase_budget.signature_verifications, fee_budget.signature_verifications);
}

TEST(epose_envelope_v2, transaction_and_block_wide_budgets_fail_before_state_application)
{
  std::string field;
  envelope_budget_v2 ignored{};
  ASSERT_EQ(envelope_status_v2::accepted,
      encode_tx_extra_envelope_field_v2({record(record_type_v2::service_receipt, "receipt")}, limits(), field, ignored));
  std::vector<envelope_record_v2> records;
  envelope_budget_v2 transaction_budget{};
  EXPECT_EQ(envelope_status_v2::envelope_count_exceeded,
      parse_transaction_envelope_fields_v2({field, field}, 1, limits(), records, transaction_budget));
  ASSERT_EQ(envelope_status_v2::accepted,
      parse_transaction_envelope_fields_v2({field}, 1, limits(), records, transaction_budget));

  block_budget_limits_v2 block_limits{2048, 2, 4, 1};
  envelope_budget_v2 block_budget{};
  ASSERT_EQ(envelope_status_v2::accepted,
      charge_block_budget_v2(transaction_budget, block_limits, block_budget));
  ASSERT_EQ(envelope_status_v2::accepted,
      charge_block_budget_v2(transaction_budget, block_limits, block_budget));
  EXPECT_EQ(envelope_status_v2::block_record_budget_exceeded,
      charge_block_budget_v2(transaction_budget, block_limits, block_budget));
  EXPECT_EQ(2u, block_budget.records);
}
