// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

// ISOLATED RESEARCH PROTOTYPE: compiled only into epose_unit_tests.
// Not a transaction-extra record, production RPC or Mainnet activation switch.
#include <array>
#include <boost/optional.hpp>
#include <string>
#include <vector>
#include "epose/record_codec_v2.h"
#include "epose/service_receipt_v2.h"

namespace qwertycoin { namespace epose { namespace release_policy_prototype {
using key_set = std::array<crypto::public_key, 3>;
enum class operation : uint8_t { publish = 1, schedule = 2, cancel = 3, rotate = 4 };
struct record
{
  uint8_t version = 1;
  operation op = operation::publish;
  uint64_t sequence = 0;
  crypto::hash previous{};
  uint64_t generation = 0;
  uint64_t release_id = 0;
  uint64_t predecessor = 0;
  std::string label;
  uint64_t target_epoch = 0;
  uint64_t reference_sequence = 0;
  key_set next_keys{};
  uint8_t signer_mask = 0;
  uint8_t next_signer_mask = 0;
  std::array<crypto::signature, 3> signatures{};
  std::array<crypto::signature, 3> next_signatures{};
};
struct configuration
{
  receipt_context_v2 context{};
  epoch_timing_v2 timing{};
  boost::optional<uint64_t> h0; // absent is disabled, never a numeric sentinel
  uint64_t first_enforced_epoch = 0;
  uint64_t minimum_notice_blocks = 0;
  key_set authority{};
};
struct epoch_view
{
  uint64_t epoch = 0, lock_height = 0, head = 0, minimum = 0, sequence = 0;
  crypto::hash policy_hash{};
};
enum class status
{
  accepted, disabled, invalid_configuration, nonsequential_height,
  resource_limit, malformed_record, before_h0, invalid_authorization,
  bad_continuation, invalid_release, invalid_schedule, too_late,
  unknown_release, below_minimum, policy_unavailable, wrong_policy,
  invalid_receipt
};
constexpr size_t max_policy_records = 4096;
constexpr size_t max_record_bytes = 768;
constexpr uint64_t undo_depth = 128;
std::string encode(const record &r);
bool decode(const std::string &bytes, record &r);
crypto::hash signing_hash(const record &r, const receipt_context_v2 &context);
void sign(record &r, size_t index, const crypto::secret_key &key,
    const receipt_context_v2 &context, bool next = false);

class chain
{
public:
  explicit chain(configuration config);
  bool valid() const;
  bool lock_height(uint64_t epoch, uint64_t &height) const;
  status apply_block(uint64_t height, const std::vector<std::string> &policy_records);
  bool disconnect_tip(); // bounded; beyond undo_depth requires canonical replay
  bool view(uint64_t epoch, epoch_view &out) const;
  status eligible(uint64_t release, uint64_t epoch) const;
  status eligible(uint64_t release, uint64_t epoch, uint64_t inclusion_height) const;
  uint64_t next_sequence() const;
  crypto::hash tip_policy_hash() const;
  crypto::hash state_hash() const;
  const configuration &config() const { return config_; }
private:
  struct entry { uint64_t height; record value; crypto::hash hash; };
  configuration config_;
  bool valid_ = false;
  boost::optional<uint64_t> tip_;
  uint64_t retained_from_ = 0;
  std::vector<entry> records_;
  status validate(const record &r, uint64_t height) const;
};

// Separate versioned wrapper: legacy signatures/domains are NOT reinterpreted.
// Two extra signatures bind release claims to an already authenticated service
// transcript. This intentionally costs four checks in the proof-of-concept.
struct receipt
{
  uint8_t version = 1;
  authenticated_service_receipt_v2 service{};
  uint64_t subject_release = 0, verifier_release = 0;
  crypto::hash policy_hash{};
  crypto::signature subject_claim{}, verifier_claim{};
};
std::string encode_receipt(const receipt &r);
bool decode_receipt(const std::string &bytes, const receipt_context_v2 &context, receipt &r);
void sign_subject(receipt &r, const crypto::secret_key &key, const receipt_context_v2 &context);
void sign_verifier(receipt &r, const crypto::secret_key &key, const receipt_context_v2 &context);
status apply_receipt(const receipt &r, const chain &policies,
    membership_pipeline_v2 &membership, uint64_t height, const crypto::hash &round_anchor);
}}}
