// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "cryptonote_config.h"
#include "epose/membership_v2.h"
#include "epose/service_receipt_v2.h"

namespace qwertycoin
{
namespace epose
{
  using chainlock_context_v1 = receipt_context_v2;

  static_assert(QWC_CHAINLOCK_SIGNER_BITMAP_SIZE * 8 >= QWC_CHAINLOCK_QUORUM_MAX, "signer bitmap must cover the quorum");
  static_assert(QWC_CHAINLOCK_SIGNATURE_SIZE == sizeof(crypto::signature), "chainlock signature size");
  static_assert(QWC_CHAINLOCK_HEADER_SIZE == sizeof(uint64_t) + sizeof(crypto::hash) + sizeof(uint64_t) + QWC_CHAINLOCK_SIGNER_BITMAP_SIZE, "chainlock header size");
  static_assert(QWC_CHAINLOCK_MAX_SIZE < 128 * 128, "chainlock field length must fit a two-byte varint");

  struct chainlock_quorum_v1
  {
    uint64_t epoch = 0;
    std::vector<crypto::public_key> members;

    bool can_lock() const;
    size_t threshold() const;
    bool index_of(const crypto::public_key &member, size_t &index) const;
  };

  std::vector<crypto::public_key> chainlock_candidates_v1(
      const membership_snapshot_v2 &snapshot,
      const qualification_set_v2 &qualified);

  std::vector<crypto::public_key> chainlock_candidates_v1(
      const std::vector<crypto::public_key> &snapshot_members,
      const std::vector<crypto::public_key> &qualified_members);

  chainlock_quorum_v1 select_chainlock_quorum_v1(
      uint64_t epoch,
      const crypto::hash &anchor_hash,
      const std::vector<crypto::public_key> &candidates,
      const chainlock_context_v1 &context);

  struct chainlock_v1
  {
    uint64_t height = 0;
    crypto::hash block_hash{};
    uint64_t epoch = 0;
    std::array<uint8_t, QWC_CHAINLOCK_SIGNER_BITMAP_SIZE> signers{};
    std::vector<crypto::signature> signatures;

    size_t signer_count() const;
    bool has_signer(size_t index) const;
    std::vector<size_t> signer_indices() const;
  };

  std::string serialize_chainlock_v1(const chainlock_v1 &lock);
  bool parse_chainlock_v1(const std::string &blob, chainlock_v1 &lock);

  crypto::hash hash_chainlock_message_v1(
      uint64_t height,
      const crypto::hash &block_hash,
      uint64_t epoch,
      const chainlock_context_v1 &context);

  bool sign_chainlock_share_v1(
      uint64_t height,
      const crypto::hash &block_hash,
      const chainlock_quorum_v1 &quorum,
      const crypto::public_key &signer_public_key,
      const crypto::secret_key &signer_secret_key,
      const chainlock_context_v1 &context,
      chainlock_v1 &share);

  bool verify_chainlock_v1(
      const chainlock_v1 &lock,
      const chainlock_quorum_v1 &quorum,
      const chainlock_context_v1 &context,
      bool check_signatures);

  bool chainlock_complete_v1(const chainlock_v1 &lock, const chainlock_quorum_v1 &quorum);

  bool merge_chainlock_share_v1(chainlock_v1 &aggregate, const chainlock_v1 &share, size_t threshold);

  struct chainlock_signer_state_v1
  {
    uint64_t height = 0;
    crypto::hash block_hash{};
  };

  bool load_chainlock_signer_state_v1(const std::string &path, chainlock_signer_state_v1 &state, std::string &error);
  bool save_chainlock_signer_state_v1(const std::string &path, const chainlock_signer_state_v1 &state, std::string &error);
}
}
