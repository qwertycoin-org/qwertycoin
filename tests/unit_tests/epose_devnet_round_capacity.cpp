// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "gtest/gtest.h"

#include "epose/compiled_profile_v2.h"
#include "epose/coordinator_v2.h"
#include "epose/membership_v2.h"
#include "string_tools.h"

namespace
{
  constexpr uint64_t devnet_node_count = 10;
  constexpr uint64_t coinbase_receipts_per_block = 8;

  uint64_t blocks_needed_for_full_round(uint64_t committee_size)
  {
    const uint64_t receipts = devnet_node_count * committee_size;
    return (receipts + coinbase_receipts_per_block - 1) / coinbase_receipts_per_block;
  }

  bool devnet_parameters(qwertycoin::epose::consensus_parameters_v2 &parameters)
  {
    crypto::hash genesis{};
    return epee::string_tools::hex_to_pod(qwertycoin::epose::DEVNET_GENESIS_HASH_V2, genesis)
        && qwertycoin::epose::compiled_consensus_parameters_v2(cryptonote::DEVNET, genesis, parameters);
  }
}

TEST(epose_devnet_round_capacity, every_round_window_carries_a_full_ten_node_round)
{
  qwertycoin::epose::consensus_parameters_v2 parameters;
  ASSERT_TRUE(devnet_parameters(parameters));
  const uint64_t needed = blocks_needed_for_full_round(parameters.committee.committee_size);
  for (uint64_t round = 0; round < parameters.committee.round_count; ++round)
  {
    uint64_t first = 0;
    uint64_t last = 0;
    ASSERT_TRUE(qwertycoin::epose::receipt_inclusion_window_v2(
        parameters.timing, parameters.committee, 5, round, first, last));
    EXPECT_GE(last - first + 1, needed) << "round " << round;
  }
}

TEST(epose_devnet_round_capacity, round_windows_do_not_overlap_and_stay_inside_the_epoch)
{
  qwertycoin::epose::consensus_parameters_v2 parameters;
  ASSERT_TRUE(devnet_parameters(parameters));
  uint64_t epoch_start = 0;
  uint64_t evidence_deadline = 0;
  ASSERT_TRUE(parameters.timing.epoch_start(5, epoch_start));
  ASSERT_TRUE(parameters.timing.evidence_deadline(5, evidence_deadline));
  uint64_t previous_last = 0;
  for (uint64_t round = 0; round < parameters.committee.round_count; ++round)
  {
    uint64_t first = 0;
    uint64_t last = 0;
    ASSERT_TRUE(qwertycoin::epose::receipt_inclusion_window_v2(
        parameters.timing, parameters.committee, 5, round, first, last));
    EXPECT_GE(first, epoch_start);
    EXPECT_LE(last, evidence_deadline);
    if (round != 0)
      EXPECT_GT(first, previous_last);
    previous_last = last;
  }
}

TEST(epose_devnet_round_capacity, three_missing_verifiers_leave_no_receipt_margin)
{
  qwertycoin::epose::consensus_parameters_v2 parameters;
  ASSERT_TRUE(devnet_parameters(parameters));
  const size_t stopped_nodes = 3;
  const size_t live_verifiers = parameters.committee.committee_size - stopped_nodes;
  EXPECT_EQ(parameters.committee.threshold, live_verifiers);
  EXPECT_EQ(live_verifiers,
      qwertycoin::epose::required_receipts_for_committee_size_v2(
          parameters.committee.committee_size));
}

TEST(epose_devnet_round_capacity, final_round_cannot_carry_every_thirteen_node_receipt)
{
  qwertycoin::epose::consensus_parameters_v2 parameters;
  ASSERT_TRUE(devnet_parameters(parameters));
  uint64_t first = 0;
  uint64_t last = 0;
  ASSERT_TRUE(qwertycoin::epose::receipt_inclusion_window_v2(
      parameters.timing, parameters.committee, 5, 2, first, last));
  const uint64_t full_round_receipts = 13 * parameters.committee.committee_size;
  EXPECT_EQ(14u, last - first + 1);
  EXPECT_LT((last - first + 1) * coinbase_receipts_per_block,
      full_round_receipts);
}
