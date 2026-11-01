// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/chainlock_v1.h"
#include "epose/hash_transcript.h"
#include "file_io_utils.h"
#include "string_tools.h"

#include <algorithm>
#include <boost/filesystem.hpp>
#include <sstream>
#include <cstring>
#include <set>

namespace
{
  using namespace qwertycoin::epose::transcript;
  using qwertycoin::epose::chainlock_v1;

  struct key_order
  {
    bool operator()(const crypto::public_key &left, const crypto::public_key &right) const
    {
      return bytes_less(left, right);
    }
  };

  struct ranked_member
  {
    crypto::hash score{};
    crypto::public_key member{};
  };

  template <typename Members, typename Key>
  std::vector<crypto::public_key> intersect_chainlock_members(
      const Members &members, const std::vector<crypto::public_key> &qualified, Key key)
  {
    const std::set<crypto::public_key, key_order> qualified_nodes(qualified.begin(), qualified.end());
    std::set<crypto::public_key, key_order> candidates;
    for (const auto &member : members)
      if (qualified_nodes.count(key(member)) != 0)
        candidates.insert(key(member));
    return {candidates.begin(), candidates.end()};
  }

  bool read_u64_le(const std::string &blob, size_t &offset, uint64_t &value)
  {
    if (blob.size() - offset < sizeof(uint64_t))
      return false;
    value = 0;
    for (unsigned byte = 0; byte < sizeof(uint64_t); ++byte)
      value |= static_cast<uint64_t>(static_cast<uint8_t>(blob[offset + byte])) << (8 * byte);
    offset += sizeof(uint64_t);
    return true;
  }

  template <typename T>
  bool read_bytes(const std::string &blob, size_t &offset, T &value)
  {
    if (blob.size() - offset < sizeof(T))
      return false;
    std::memcpy(&value, blob.data() + offset, sizeof(T));
    offset += sizeof(T);
    return true;
  }

  void set_signer(chainlock_v1 &lock, size_t index)
  {
    lock.signers[index / 8] |= static_cast<uint8_t>(1u << (index % 8));
  }
}

namespace qwertycoin
{
namespace epose
{
  bool chainlock_quorum_v1::can_lock() const
  {
    return members.size() >= QWC_CHAINLOCK_MIN_NODES;
  }

  size_t chainlock_quorum_v1::threshold() const
  {
    return qwc_chainlock_threshold(members.size());
  }

  bool chainlock_quorum_v1::index_of(const crypto::public_key &member, size_t &index) const
  {
    for (size_t candidate = 0; candidate < members.size(); ++candidate)
    {
      if (bytes_equal(members[candidate], member))
      {
        index = candidate;
        return true;
      }
    }
    return false;
  }

  std::vector<crypto::public_key> chainlock_candidates_v1(
      const membership_snapshot_v2 &snapshot,
      const qualification_set_v2 &qualified)
  {
    return intersect_chainlock_members(snapshot.members, qualified.qualified_nodes,
        [](const frozen_member_v2 &member) { return member.service_public_key; });
  }

  std::vector<crypto::public_key> chainlock_candidates_v1(
      const std::vector<crypto::public_key> &snapshot_members,
      const std::vector<crypto::public_key> &qualified_members)
  {
    return intersect_chainlock_members(snapshot_members, qualified_members,
        [](const crypto::public_key &member) { return member; });
  }

  chainlock_quorum_v1 select_chainlock_quorum_v1(
      uint64_t epoch,
      const crypto::hash &anchor_hash,
      const std::vector<crypto::public_key> &candidates,
      const chainlock_context_v1 &context)
  {
    std::set<crypto::public_key, key_order> unique(candidates.begin(), candidates.end());
    std::vector<ranked_member> ranked;
    ranked.reserve(unique.size());
    for (const crypto::public_key &member : unique)
    {
      std::string blob("QWC_CHAINLOCK_QUORUM_V1");
      append_network(blob, context.nettype);
      append_bytes(blob, context.genesis_hash);
      append_u64_le(blob, epoch);
      append_bytes(blob, anchor_hash);
      append_bytes(blob, member);
      ranked.push_back({hash_blob(blob), member});
    }
    std::sort(ranked.begin(), ranked.end(), [](const ranked_member &left, const ranked_member &right) {
      if (bytes_less(left.score, right.score))
        return true;
      if (bytes_less(right.score, left.score))
        return false;
      return bytes_less(left.member, right.member);
    });
    if (ranked.size() > QWC_CHAINLOCK_QUORUM_MAX)
      ranked.resize(QWC_CHAINLOCK_QUORUM_MAX);

    chainlock_quorum_v1 quorum;
    quorum.epoch = epoch;
    for (const ranked_member &entry : ranked)
      quorum.members.push_back(entry.member);
    return quorum;
  }

  size_t chainlock_v1::signer_count() const
  {
    size_t count = 0;
    for (uint8_t byte : signers)
      for (; byte != 0; byte &= static_cast<uint8_t>(byte - 1))
        ++count;
    return count;
  }

  bool chainlock_v1::has_signer(size_t index) const
  {
    return index < signers.size() * 8 && (signers[index / 8] >> (index % 8)) & 1u;
  }

  std::vector<size_t> chainlock_v1::signer_indices() const
  {
    std::vector<size_t> indices;
    for (size_t index = 0; index < signers.size() * 8; ++index)
      if (has_signer(index))
        indices.push_back(index);
    return indices;
  }

  std::string serialize_chainlock_v1(const chainlock_v1 &lock)
  {
    std::string blob;
    blob.reserve(QWC_CHAINLOCK_HEADER_SIZE + lock.signatures.size() * sizeof(crypto::signature));
    append_u64_le(blob, lock.height);
    append_bytes(blob, lock.block_hash);
    append_u64_le(blob, lock.epoch);
    append_bytes(blob, lock.signers);
    for (const crypto::signature &signature : lock.signatures)
      append_bytes(blob, signature);
    return blob;
  }

  bool parse_chainlock_v1(const std::string &blob, chainlock_v1 &lock)
  {
    lock = {};
    if (blob.size() < QWC_CHAINLOCK_HEADER_SIZE || blob.size() > QWC_CHAINLOCK_MAX_SIZE)
      return false;
    size_t offset = 0;
    if (!read_u64_le(blob, offset, lock.height)
        || !read_bytes(blob, offset, lock.block_hash)
        || !read_u64_le(blob, offset, lock.epoch)
        || !read_bytes(blob, offset, lock.signers))
      return false;
    const size_t count = lock.signer_count();
    if (count == 0 || blob.size() - offset != count * sizeof(crypto::signature))
      return false;
    lock.signatures.resize(count);
    for (crypto::signature &signature : lock.signatures)
      if (!read_bytes(blob, offset, signature))
        return false;
    return true;
  }

  crypto::hash hash_chainlock_message_v1(
      uint64_t height,
      const crypto::hash &block_hash,
      uint64_t epoch,
      const chainlock_context_v1 &context)
  {
    std::string blob("QWC_CHAINLOCK_V1");
    append_network(blob, context.nettype);
    append_bytes(blob, context.genesis_hash);
    append_bytes(blob, context.parameter_set_hash);
    append_u64_le(blob, epoch);
    append_u64_le(blob, height);
    append_bytes(blob, block_hash);
    return hash_blob(blob);
  }

  bool sign_chainlock_share_v1(
      uint64_t height,
      const crypto::hash &block_hash,
      const chainlock_quorum_v1 &quorum,
      const crypto::public_key &signer_public_key,
      const crypto::secret_key &signer_secret_key,
      const chainlock_context_v1 &context,
      chainlock_v1 &share)
  {
    share = {};
    size_t index = 0;
    if (!quorum.can_lock() || !quorum.index_of(signer_public_key, index))
      return false;
    share.height = height;
    share.block_hash = block_hash;
    share.epoch = quorum.epoch;
    set_signer(share, index);
    share.signatures.resize(1);
    crypto::generate_signature(
        hash_chainlock_message_v1(height, block_hash, quorum.epoch, context),
        signer_public_key,
        signer_secret_key,
        share.signatures.front());
    return true;
  }

  bool verify_chainlock_v1(
      const chainlock_v1 &lock,
      const chainlock_quorum_v1 &quorum,
      const chainlock_context_v1 &context,
      bool check_signatures)
  {
    if (lock.epoch != quorum.epoch || !quorum.can_lock())
      return false;
    const std::vector<size_t> indices = lock.signer_indices();
    if (indices.empty() || indices.size() != lock.signatures.size() || indices.back() >= quorum.members.size())
      return false;
    if (!check_signatures)
      return true;
    const crypto::hash message = hash_chainlock_message_v1(lock.height, lock.block_hash, lock.epoch, context);
    for (size_t position = 0; position < indices.size(); ++position)
      if (!crypto::check_signature(message, quorum.members[indices[position]], lock.signatures[position]))
        return false;
    return true;
  }

  bool chainlock_complete_v1(const chainlock_v1 &lock, const chainlock_quorum_v1 &quorum)
  {
    return quorum.can_lock() && lock.signer_count() == quorum.threshold();
  }

  bool merge_chainlock_share_v1(chainlock_v1 &aggregate, const chainlock_v1 &share, size_t threshold)
  {
    if (aggregate.height != share.height || aggregate.block_hash != share.block_hash || aggregate.epoch != share.epoch)
      return false;
    const std::vector<size_t> share_indices = share.signer_indices();
    if (share_indices.size() != share.signatures.size())
      return false;

    std::vector<std::pair<size_t, crypto::signature>> merged;
    const std::vector<size_t> aggregate_indices = aggregate.signer_indices();
    for (size_t position = 0; position < aggregate_indices.size(); ++position)
      merged.emplace_back(aggregate_indices[position], aggregate.signatures[position]);
    bool added = false;
    for (size_t position = 0; position < share_indices.size() && merged.size() < threshold; ++position)
    {
      if (aggregate.has_signer(share_indices[position]))
        continue;
      merged.emplace_back(share_indices[position], share.signatures[position]);
      added = true;
    }
    if (!added)
      return false;
    std::sort(merged.begin(), merged.end(), [](const std::pair<size_t, crypto::signature> &left, const std::pair<size_t, crypto::signature> &right) {
      return left.first < right.first;
    });
    aggregate.signers = {};
    aggregate.signatures.clear();
    for (const auto &entry : merged)
    {
      set_signer(aggregate, entry.first);
      aggregate.signatures.push_back(entry.second);
    }
    return true;
  }

  bool load_chainlock_signer_state_v1(const std::string &path, chainlock_signer_state_v1 &state, std::string &error)
  {
    state = {};
    boost::system::error_code ec;
    const boost::filesystem::file_status status = boost::filesystem::status(path, ec);
    if (status.type() == boost::filesystem::file_not_found)
      return true;
    if (ec)
    {
      error = "failed to stat chainlock signer state: " + ec.message();
      return false;
    }
    std::string contents;
    if (!epee::file_io_utils::load_file_to_string(path, contents, 256))
    {
      error = "failed to read chainlock signer state";
      return false;
    }
    std::istringstream stream(contents);
    std::string hash_hex;
    std::string trailing;
    if (!(stream >> state.height >> hash_hex) || (stream >> trailing)
        || !epee::string_tools::hex_to_pod(hash_hex, state.block_hash))
    {
      state = {};
      error = "malformed chainlock signer state";
      return false;
    }
    return true;
  }

  bool save_chainlock_signer_state_v1(const std::string &path, const chainlock_signer_state_v1 &state, std::string &error)
  {
    const std::string temporary = path + ".tmp";
    const std::string contents = std::to_string(state.height) + " " + epee::string_tools::pod_to_hex(state.block_hash) + "\n";
    if (!epee::file_io_utils::save_string_to_file(temporary, contents))
    {
      error = "failed to write chainlock signer state";
      return false;
    }
    boost::system::error_code ec;
    boost::filesystem::rename(temporary, path, ec);
    if (ec)
    {
      boost::filesystem::remove(temporary, ec);
      error = "failed to publish chainlock signer state";
      return false;
    }
    return true;
  }
}
}
