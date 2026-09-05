// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "epose/service_node.h"

namespace qwertycoin
{
namespace epose
{
  constexpr size_t EPOSE_VERIFIER_COMMITTEE_SIZE = 9;

  struct verifier_assignment
  {
    crypto::public_key verifier_public_key{};
    crypto::hash selection_score{};
  };

  crypto::hash calculate_epoch_seed(
      cryptonote::network_type nettype,
      uint64_t epoch,
      const crypto::hash &seed_block_hash);

  std::vector<verifier_assignment> select_verifiers(
      const std::vector<service_node_identity> &registered_nodes,
      const crypto::public_key &subject_public_key,
      cryptonote::network_type nettype,
      uint64_t epoch,
      const crypto::hash &epoch_seed,
      size_t committee_size = EPOSE_VERIFIER_COMMITTEE_SIZE);

  crypto::hash calculate_challenge_hash(
      const crypto::hash &epoch_seed,
      const crypto::public_key &subject_public_key,
      const crypto::public_key &verifier_public_key,
      uint64_t epoch,
      uint64_t round);

  crypto::hash calculate_response_hash(
      const crypto::hash &challenge_hash,
      const crypto::hash &observed_tip_hash,
      const crypto::public_key &subject_public_key,
      const crypto::public_key &verifier_public_key,
      uint64_t epoch);
} // namespace epose
} // namespace qwertycoin
