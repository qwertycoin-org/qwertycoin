// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/service_epoch.h"
#include "epose/hash_transcript.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace
{
  using qwertycoin::epose::transcript::append_u64_le;
  using qwertycoin::epose::transcript::append_network;
  using qwertycoin::epose::transcript::hash_blob;
  using qwertycoin::epose::transcript::append_bytes;
  using qwertycoin::epose::transcript::bytes_equal;
  using qwertycoin::epose::transcript::bytes_less;

  crypto::hash calculate_selection_score(
      const crypto::hash &epoch_seed,
      const crypto::public_key &subject_public_key,
      const crypto::public_key &candidate_public_key,
      cryptonote::network_type nettype,
      uint64_t epoch)
  {
    std::string blob;
    blob.reserve(16 + sizeof(crypto::hash) + sizeof(crypto::public_key) * 2 + sizeof(uint64_t) + 16);
    blob.append("QWC_EPOSE_SELECT_V1");
    append_network(blob, nettype);
    append_u64_le(blob, epoch);
    append_bytes(blob, epoch_seed);
    append_bytes(blob, subject_public_key);
    append_bytes(blob, candidate_public_key);
    return hash_blob(blob);
  }
}

namespace qwertycoin
{
namespace epose
{
  crypto::hash calculate_epoch_seed(
      cryptonote::network_type nettype,
      uint64_t epoch,
      const crypto::hash &seed_block_hash)
  {
    std::string blob;
    blob.reserve(16 + 16 + sizeof(uint64_t) + sizeof(crypto::hash));
    blob.append("QWC_EPOSE_SEED_V1");
    append_network(blob, nettype);
    append_u64_le(blob, epoch);
    append_bytes(blob, seed_block_hash);
    return hash_blob(blob);
  }

  std::vector<verifier_assignment> select_verifiers(
      const std::vector<service_node_identity> &registered_nodes,
      const crypto::public_key &subject_public_key,
      cryptonote::network_type nettype,
      uint64_t epoch,
      const crypto::hash &epoch_seed,
      size_t committee_size)
  {
    std::vector<verifier_assignment> assignments;
    assignments.reserve(registered_nodes.size());

    for (const service_node_identity &candidate : registered_nodes)
    {
      if (!identity_active_in_epoch(candidate, epoch) || bytes_equal(candidate.service_public_key, subject_public_key))
        continue;

      assignments.push_back({
          candidate.service_public_key,
          calculate_selection_score(epoch_seed, subject_public_key, candidate.service_public_key, nettype, epoch)});
    }

    std::sort(assignments.begin(), assignments.end(), [](const verifier_assignment &left, const verifier_assignment &right) {
      if (bytes_less(left.selection_score, right.selection_score))
        return true;
      if (bytes_less(right.selection_score, left.selection_score))
        return false;
      return bytes_less(left.verifier_public_key, right.verifier_public_key);
    });

    assignments.erase(std::unique(assignments.begin(), assignments.end(), [](const verifier_assignment &left, const verifier_assignment &right) {
      return bytes_equal(left.verifier_public_key, right.verifier_public_key);
    }), assignments.end());

    if (assignments.size() > committee_size)
      assignments.resize(committee_size);

    return assignments;
  }

  crypto::hash calculate_challenge_hash(
      const crypto::hash &epoch_seed,
      const crypto::public_key &subject_public_key,
      const crypto::public_key &verifier_public_key,
      uint64_t epoch,
      uint64_t round)
  {
    std::string blob;
    blob.reserve(16 + sizeof(crypto::hash) + sizeof(crypto::public_key) * 2 + sizeof(uint64_t) * 2);
    blob.append("QWC_EPOSE_CHAL_V1");
    append_u64_le(blob, epoch);
    append_u64_le(blob, round);
    append_bytes(blob, epoch_seed);
    append_bytes(blob, subject_public_key);
    append_bytes(blob, verifier_public_key);
    return hash_blob(blob);
  }

  crypto::hash calculate_response_hash(
      const crypto::hash &challenge_hash,
      const crypto::hash &observed_tip_hash,
      const crypto::public_key &subject_public_key,
      const crypto::public_key &verifier_public_key,
      uint64_t epoch)
  {
    std::string blob;
    blob.reserve(16 + sizeof(crypto::hash) * 2 + sizeof(crypto::public_key) * 2 + sizeof(uint64_t));
    blob.append("QWC_EPOSE_RESP_V1");
    append_u64_le(blob, epoch);
    append_bytes(blob, challenge_hash);
    append_bytes(blob, observed_tip_hash);
    append_bytes(blob, subject_public_key);
    append_bytes(blob, verifier_public_key);
    return hash_blob(blob);
  }
} // namespace epose
} // namespace qwertycoin
