// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/service_epoch.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace
{
  void append_u64_le(std::string &out, uint64_t value)
  {
    for (unsigned int shift = 0; shift < 64; shift += 8)
      out.push_back(static_cast<char>((value >> shift) & 0xff));
  }

  void append_hash(std::string &out, const crypto::hash &value)
  {
    out.append(reinterpret_cast<const char *>(&value), sizeof(value));
  }

  void append_public_key(std::string &out, const crypto::public_key &value)
  {
    out.append(reinterpret_cast<const char *>(&value), sizeof(value));
  }

  void append_network(std::string &out, cryptonote::network_type nettype)
  {
    const auto &network_id = cryptonote::get_config(nettype).NETWORK_ID;
    for (const auto byte : network_id)
      out.push_back(static_cast<char>(byte));
  }

  crypto::hash hash_blob(const std::string &blob)
  {
    return crypto::cn_fast_hash(blob.data(), blob.size());
  }

  bool public_key_equal(const crypto::public_key &left, const crypto::public_key &right)
  {
    return std::memcmp(&left, &right, sizeof(left)) == 0;
  }

  bool public_key_less(const crypto::public_key &left, const crypto::public_key &right)
  {
    return std::memcmp(&left, &right, sizeof(left)) < 0;
  }

  bool hash_less(const crypto::hash &left, const crypto::hash &right)
  {
    return std::memcmp(&left, &right, sizeof(left)) < 0;
  }

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
    append_hash(blob, epoch_seed);
    append_public_key(blob, subject_public_key);
    append_public_key(blob, candidate_public_key);
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
    append_hash(blob, seed_block_hash);
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
      if (!identity_active_in_epoch(candidate, epoch) || public_key_equal(candidate.service_public_key, subject_public_key))
        continue;

      assignments.push_back({
          candidate.service_public_key,
          calculate_selection_score(epoch_seed, subject_public_key, candidate.service_public_key, nettype, epoch)});
    }

    std::sort(assignments.begin(), assignments.end(), [](const verifier_assignment &left, const verifier_assignment &right) {
      if (hash_less(left.selection_score, right.selection_score))
        return true;
      if (hash_less(right.selection_score, left.selection_score))
        return false;
      return public_key_less(left.verifier_public_key, right.verifier_public_key);
    });

    assignments.erase(std::unique(assignments.begin(), assignments.end(), [](const verifier_assignment &left, const verifier_assignment &right) {
      return public_key_equal(left.verifier_public_key, right.verifier_public_key);
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
    append_hash(blob, epoch_seed);
    append_public_key(blob, subject_public_key);
    append_public_key(blob, verifier_public_key);
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
    append_hash(blob, challenge_hash);
    append_hash(blob, observed_tip_hash);
    append_public_key(blob, subject_public_key);
    append_public_key(blob, verifier_public_key);
    return hash_blob(blob);
  }
} // namespace epose
} // namespace qwertycoin
