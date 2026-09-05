// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "crypto/crypto.h"
#include "crypto/hash.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_config.h"

namespace qwertycoin
{
namespace epose
{
  constexpr uint8_t EPOSE_PROTOCOL_VERSION = 1;
  constexpr uint64_t EPOSE_EPOCH_LENGTH = 720;
  constexpr uint64_t EPOSE_FINALITY_DEPTH = 60;
  constexpr uint64_t EPOSE_REGISTRATION_TTL_EPOCHS = 30;
  constexpr uint64_t EPOSE_STATE_RETENTION_EPOCHS = 2;
  constexpr uint64_t EPOSE_SERVICE_REWARD_BPS = 1000;
  constexpr uint8_t EPOSE_ADMISSION_LEADING_ZERO_BITS = 16;
  constexpr size_t EPOSE_IDENTITY_BLOB_SIZE = 249;
  constexpr size_t EPOSE_ATTESTATION_BLOB_SIZE = 234;
  constexpr uint8_t EPOSE_TX_EXTRA_NONCE_REGISTRATION = 0x70;
  constexpr uint8_t EPOSE_TX_EXTRA_NONCE_ATTESTATION = 0x71;

  struct service_node_identity
  {
    uint8_t version = EPOSE_PROTOCOL_VERSION;
    crypto::public_key service_public_key{};
    cryptonote::account_public_address reward_address{};
    crypto::secret_key reward_view_secret_key{};
    crypto::hash endpoint_commitment{};
    uint64_t registration_epoch = 0;
    uint64_t expiry_epoch = 0;
    uint64_t admission_nonce = 0;
    crypto::hash admission_hash{};
    crypto::signature signature{};
  };

  struct service_attestation
  {
    uint8_t version = EPOSE_PROTOCOL_VERSION;
    crypto::public_key verifier_public_key{};
    crypto::public_key subject_public_key{};
    uint64_t epoch = 0;
    crypto::hash challenge_hash{};
    crypto::hash response_hash{};
    crypto::hash observed_tip_hash{};
    bool service_ok = false;
    crypto::signature signature{};
  };

  struct reward_split
  {
    uint64_t miner_reward = 0;
    uint64_t service_reward = 0;
  };

  uint64_t epoch_for_height(uint64_t height);
  uint64_t epoch_start_height(uint64_t epoch);
  uint64_t epoch_end_height(uint64_t epoch);
  uint64_t epoch_seed_height(uint64_t epoch);
  uint64_t reward_source_epoch_for_height(uint64_t height);

  crypto::hash make_endpoint_commitment(const std::string &endpoint);
  std::string serialize_identity(const service_node_identity &identity);
  bool parse_identity(const std::string &blob, service_node_identity &identity);
  std::string make_registration_tx_extra_nonce(const service_node_identity &identity);
  bool parse_registration_tx_extra_nonce(const std::string &extra_nonce, service_node_identity &identity);
  bool extract_registrations_from_tx_extra(const std::vector<uint8_t> &tx_extra, std::vector<service_node_identity> &registrations);
  crypto::hash hash_registration_message(const service_node_identity &identity, cryptonote::network_type nettype);
  void sign_registration(service_node_identity &identity, const crypto::secret_key &service_secret_key, cryptonote::network_type nettype);
  bool verify_registration_signature(const service_node_identity &identity, cryptonote::network_type nettype);
  crypto::hash calculate_admission_hash(const service_node_identity &identity, cryptonote::network_type nettype, const crypto::hash &previous_epoch_hash);
  bool admission_hash_meets_target(const crypto::hash &hash, uint8_t leading_zero_bits);
  bool verify_admission_proof(const service_node_identity &identity, cryptonote::network_type nettype, const crypto::hash &previous_epoch_hash, uint8_t leading_zero_bits = EPOSE_ADMISSION_LEADING_ZERO_BITS);

  crypto::hash hash_attestation_message(const service_attestation &attestation, cryptonote::network_type nettype);
  std::string serialize_attestation(const service_attestation &attestation);
  bool parse_attestation(const std::string &blob, service_attestation &attestation);
  std::string make_attestation_tx_extra_nonce(const service_attestation &attestation);
  bool parse_attestation_tx_extra_nonce(const std::string &extra_nonce, service_attestation &attestation);
  bool extract_attestations_from_tx_extra(const std::vector<uint8_t> &tx_extra, std::vector<service_attestation> &attestations);
  void sign_attestation(service_attestation &attestation, const crypto::secret_key &verifier_secret_key, cryptonote::network_type nettype);
  bool verify_attestation_signature(const service_attestation &attestation, cryptonote::network_type nettype);

  bool identity_active_in_epoch(const service_node_identity &identity, uint64_t epoch);
  uint64_t required_attestations_for_committee_size(size_t actual_committee_size);
  std::vector<crypto::public_key> qualified_nodes(
      const std::vector<service_node_identity> &registered_nodes,
      const std::vector<service_attestation> &attestations,
      uint64_t epoch,
      cryptonote::network_type nettype);

  reward_split split_block_reward(uint64_t base_reward, uint64_t fees, uint64_t service_reward_bps = EPOSE_SERVICE_REWARD_BPS);
  bool validate_service_reward_output(
      const cryptonote::transaction &miner_tx,
      const cryptonote::account_public_address &reward_address,
      const crypto::secret_key &reward_view_secret_key,
      uint64_t service_reward);
  bool select_service_payee(
      const std::vector<crypto::public_key> &qualified_service_nodes,
      const crypto::hash &epoch_seed,
      uint64_t height,
      crypto::public_key &selected);
} // namespace epose
} // namespace qwertycoin
