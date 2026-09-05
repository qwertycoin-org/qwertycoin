// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "service_node.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/variant/get.hpp>

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "epose/service_epoch.h"

namespace
{
  void append_u8(std::string &out, uint8_t value)
  {
    out.push_back(static_cast<char>(value));
  }

  void append_u64_le(std::string &out, uint64_t value)
  {
    for (unsigned int shift = 0; shift < 64; shift += 8)
      out.push_back(static_cast<char>((value >> shift) & 0xff));
  }

  void append_bool(std::string &out, bool value)
  {
    append_u8(out, value ? 1 : 0);
  }

  void append_hash(std::string &out, const crypto::hash &value)
  {
    out.append(reinterpret_cast<const char *>(&value), sizeof(value));
  }

  void append_public_key(std::string &out, const crypto::public_key &value)
  {
    out.append(reinterpret_cast<const char *>(&value), sizeof(value));
  }

  void append_secret_key(std::string &out, const crypto::secret_key &value)
  {
    out.append(reinterpret_cast<const char *>(&unwrap(unwrap(value))), sizeof(crypto::secret_key));
  }

  void append_address_public(std::string &out, const cryptonote::account_public_address &address)
  {
    append_public_key(out, address.m_spend_public_key);
    append_public_key(out, address.m_view_public_key);
  }

  void append_registered_reward(std::string &out, const qwertycoin::epose::service_node_identity &identity)
  {
    append_public_key(out, identity.reward_address.m_spend_public_key);
    append_secret_key(out, identity.reward_view_secret_key);
  }

  bool read_bytes(const std::string &blob, size_t &offset, void *out, size_t size)
  {
    if (offset > blob.size() || blob.size() - offset < size)
      return false;
    std::memcpy(out, blob.data() + offset, size);
    offset += size;
    return true;
  }

  bool read_u8(const std::string &blob, size_t &offset, uint8_t &value)
  {
    if (offset >= blob.size())
      return false;
    value = static_cast<uint8_t>(blob[offset]);
    ++offset;
    return true;
  }

  bool read_u64_le(const std::string &blob, size_t &offset, uint64_t &value)
  {
    if (offset > blob.size() || blob.size() - offset < sizeof(uint64_t))
      return false;
    value = 0;
    for (unsigned int shift = 0; shift < 64; shift += 8)
      value |= static_cast<uint64_t>(static_cast<unsigned char>(blob[offset++])) << shift;
    return true;
  }

  bool read_bool(const std::string &blob, size_t &offset, bool &value)
  {
    uint8_t raw = 0;
    if (!read_u8(blob, offset, raw) || raw > 1)
      return false;
    value = raw == 1;
    return true;
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

  std::vector<uint64_t> decompose_service_reward_amount(uint64_t amount)
  {
    std::vector<uint64_t> out_amounts;
    cryptonote::decompose_amount_into_digits(amount, 0,
      [&out_amounts](uint64_t a_chunk) { out_amounts.push_back(a_chunk); },
      [&out_amounts](uint64_t a_dust) { out_amounts.push_back(a_dust); });
    std::sort(out_amounts.begin(), out_amounts.end());
    return out_amounts;
  }

  int count_leading_zero_bits(const crypto::hash &hash)
  {
    int count = 0;
    const auto *bytes = reinterpret_cast<const unsigned char *>(&hash);
    for (size_t i = 0; i < sizeof(hash); ++i)
    {
      unsigned char byte = bytes[i];
      for (int bit = 7; bit >= 0; --bit)
      {
        if ((byte & (1u << bit)) != 0)
          return count;
        ++count;
      }
    }
    return count;
  }

  bool is_epose_nonce_subtype(const std::string &extra_nonce)
  {
    if (extra_nonce.empty())
      return false;

    const uint8_t subtype = static_cast<uint8_t>(extra_nonce[0]);
    return subtype == qwertycoin::epose::EPOSE_TX_EXTRA_NONCE_REGISTRATION
        || subtype == qwertycoin::epose::EPOSE_TX_EXTRA_NONCE_ATTESTATION;
  }
}

namespace qwertycoin
{
namespace epose
{
  uint64_t epoch_for_height(uint64_t height)
  {
    return height / EPOSE_EPOCH_LENGTH;
  }

  uint64_t epoch_start_height(uint64_t epoch)
  {
    if (epoch > std::numeric_limits<uint64_t>::max() / EPOSE_EPOCH_LENGTH)
      throw std::overflow_error("EPoSE epoch start height overflow");
    return epoch * EPOSE_EPOCH_LENGTH;
  }

  uint64_t epoch_end_height(uint64_t epoch)
  {
    const uint64_t start = epoch_start_height(epoch);
    if (start > std::numeric_limits<uint64_t>::max() - (EPOSE_EPOCH_LENGTH - 1))
      throw std::overflow_error("EPoSE epoch end height overflow");
    return start + EPOSE_EPOCH_LENGTH - 1;
  }

  uint64_t epoch_seed_height(uint64_t epoch)
  {
    const uint64_t start = epoch_start_height(epoch);
    return start > EPOSE_FINALITY_DEPTH ? start - EPOSE_FINALITY_DEPTH : 0;
  }

  uint64_t reward_source_epoch_for_height(uint64_t height)
  {
    const uint64_t reward_epoch = epoch_for_height(height);
    return reward_epoch == 0 ? 0 : reward_epoch - 1;
  }

  crypto::hash make_endpoint_commitment(const std::string &endpoint)
  {
    std::string blob("QWC_EPOSE_ENDPOINT_V1");
    blob.append(endpoint);
    return hash_blob(blob);
  }

  std::string serialize_identity(const service_node_identity &identity)
  {
    std::string blob;
    blob.reserve(EPOSE_IDENTITY_BLOB_SIZE);
    append_u8(blob, identity.version);
    append_public_key(blob, identity.service_public_key);
    append_registered_reward(blob, identity);
    append_hash(blob, identity.endpoint_commitment);
    append_u64_le(blob, identity.registration_epoch);
    append_u64_le(blob, identity.expiry_epoch);
    append_u64_le(blob, identity.admission_nonce);
    append_hash(blob, identity.admission_hash);
    blob.append(reinterpret_cast<const char *>(&identity.signature), sizeof(identity.signature));
    return blob;
  }

  bool parse_identity(const std::string &blob, service_node_identity &identity)
  {
    if (blob.size() != EPOSE_IDENTITY_BLOB_SIZE)
      return false;

    size_t offset = 0;
    return read_u8(blob, offset, identity.version)
        && read_bytes(blob, offset, &identity.service_public_key, sizeof(identity.service_public_key))
        && read_bytes(blob, offset, &identity.reward_address.m_spend_public_key, sizeof(identity.reward_address.m_spend_public_key))
        && read_bytes(blob, offset, &unwrap(unwrap(identity.reward_view_secret_key)), sizeof(identity.reward_view_secret_key))
        && crypto::secret_key_to_public_key(identity.reward_view_secret_key, identity.reward_address.m_view_public_key)
        && read_bytes(blob, offset, &identity.endpoint_commitment, sizeof(identity.endpoint_commitment))
        && read_u64_le(blob, offset, identity.registration_epoch)
        && read_u64_le(blob, offset, identity.expiry_epoch)
        && read_u64_le(blob, offset, identity.admission_nonce)
        && read_bytes(blob, offset, &identity.admission_hash, sizeof(identity.admission_hash))
        && read_bytes(blob, offset, &identity.signature, sizeof(identity.signature))
        && offset == blob.size();
  }

  std::string make_registration_tx_extra_nonce(const service_node_identity &identity)
  {
    std::string extra_nonce;
    extra_nonce.reserve(EPOSE_IDENTITY_BLOB_SIZE + 1);
    append_u8(extra_nonce, EPOSE_TX_EXTRA_NONCE_REGISTRATION);
    extra_nonce.append(serialize_identity(identity));
    return extra_nonce;
  }

  bool parse_registration_tx_extra_nonce(const std::string &extra_nonce, service_node_identity &identity)
  {
    if (extra_nonce.size() != EPOSE_IDENTITY_BLOB_SIZE + 1)
      return false;
    if (static_cast<uint8_t>(extra_nonce[0]) != EPOSE_TX_EXTRA_NONCE_REGISTRATION)
      return false;
    return parse_identity(extra_nonce.substr(1), identity);
  }

  bool extract_registrations_from_tx_extra(const std::vector<uint8_t> &tx_extra, std::vector<service_node_identity> &registrations)
  {
    registrations.clear();

    std::vector<cryptonote::tx_extra_field> fields;
    if (!cryptonote::parse_tx_extra(tx_extra, fields))
      return false;

    for (const cryptonote::tx_extra_field &field : fields)
    {
      if (field.type() != typeid(cryptonote::tx_extra_nonce))
        continue;

      const cryptonote::tx_extra_nonce &extra_nonce = boost::get<cryptonote::tx_extra_nonce>(field);
      if (!is_epose_nonce_subtype(extra_nonce.nonce))
        continue;

      service_node_identity identity;
      if (static_cast<uint8_t>(extra_nonce.nonce[0]) == EPOSE_TX_EXTRA_NONCE_REGISTRATION)
      {
        if (!parse_registration_tx_extra_nonce(extra_nonce.nonce, identity))
          return false;
        registrations.push_back(identity);
      }
    }

    return true;
  }

  crypto::hash hash_registration_message(const service_node_identity &identity, cryptonote::network_type nettype)
  {
    std::string blob("QWC_EPOSE_REGISTRATION_V1");
    append_network(blob, nettype);
    append_u8(blob, identity.version);
    append_public_key(blob, identity.service_public_key);
    append_address_public(blob, identity.reward_address);
    append_secret_key(blob, identity.reward_view_secret_key);
    append_hash(blob, identity.endpoint_commitment);
    append_u64_le(blob, identity.registration_epoch);
    append_u64_le(blob, identity.expiry_epoch);
    append_u64_le(blob, identity.admission_nonce);
    append_hash(blob, identity.admission_hash);
    return hash_blob(blob);
  }

  void sign_registration(service_node_identity &identity, const crypto::secret_key &service_secret_key, cryptonote::network_type nettype)
  {
    crypto::generate_signature(hash_registration_message(identity, nettype), identity.service_public_key, service_secret_key, identity.signature);
  }

  bool verify_registration_signature(const service_node_identity &identity, cryptonote::network_type nettype)
  {
    if (identity.version != EPOSE_PROTOCOL_VERSION)
      return false;
    if (!crypto::check_key(identity.service_public_key))
      return false;
    if (!crypto::check_key(identity.reward_address.m_spend_public_key))
      return false;
    crypto::public_key derived_reward_view_public_key{};
    if (!crypto::secret_key_to_public_key(identity.reward_view_secret_key, derived_reward_view_public_key))
      return false;
    if (!public_key_equal(derived_reward_view_public_key, identity.reward_address.m_view_public_key))
      return false;
    if (identity.expiry_epoch <= identity.registration_epoch)
      return false;
    if (identity.expiry_epoch - identity.registration_epoch > EPOSE_REGISTRATION_TTL_EPOCHS)
      return false;
    return crypto::check_signature(hash_registration_message(identity, nettype), identity.service_public_key, identity.signature);
  }

  crypto::hash calculate_admission_hash(const service_node_identity &identity, cryptonote::network_type nettype, const crypto::hash &previous_epoch_hash)
  {
    std::string blob("QWC_EPOSE_ADMISSION_V1");
    append_network(blob, nettype);
    append_public_key(blob, identity.service_public_key);
    append_address_public(blob, identity.reward_address);
    append_secret_key(blob, identity.reward_view_secret_key);
    append_hash(blob, identity.endpoint_commitment);
    append_u64_le(blob, identity.registration_epoch);
    append_hash(blob, previous_epoch_hash);
    append_u64_le(blob, identity.admission_nonce);
    crypto::hash admission_hash{};
    crypto::rx_slow_hash(previous_epoch_hash.data, blob.data(), blob.size(), admission_hash.data);
    return admission_hash;
  }

  bool admission_hash_meets_target(const crypto::hash &hash, uint8_t leading_zero_bits)
  {
    return count_leading_zero_bits(hash) >= leading_zero_bits;
  }

  bool verify_admission_proof(const service_node_identity &identity, cryptonote::network_type nettype, const crypto::hash &previous_epoch_hash, uint8_t leading_zero_bits)
  {
    const crypto::hash calculated = calculate_admission_hash(identity, nettype, previous_epoch_hash);
    if (calculated != identity.admission_hash)
      return false;
    return admission_hash_meets_target(identity.admission_hash, leading_zero_bits);
  }

  crypto::hash hash_attestation_message(const service_attestation &attestation, cryptonote::network_type nettype)
  {
    std::string blob("QWC_EPOSE_ATTESTATION_V1");
    append_network(blob, nettype);
    append_u8(blob, attestation.version);
    append_public_key(blob, attestation.verifier_public_key);
    append_public_key(blob, attestation.subject_public_key);
    append_u64_le(blob, attestation.epoch);
    append_hash(blob, attestation.challenge_hash);
    append_hash(blob, attestation.response_hash);
    append_hash(blob, attestation.observed_tip_hash);
    append_bool(blob, attestation.service_ok);
    return hash_blob(blob);
  }

  std::string serialize_attestation(const service_attestation &attestation)
  {
    std::string blob;
    blob.reserve(EPOSE_ATTESTATION_BLOB_SIZE);
    append_u8(blob, attestation.version);
    append_public_key(blob, attestation.verifier_public_key);
    append_public_key(blob, attestation.subject_public_key);
    append_u64_le(blob, attestation.epoch);
    append_hash(blob, attestation.challenge_hash);
    append_hash(blob, attestation.response_hash);
    append_hash(blob, attestation.observed_tip_hash);
    append_bool(blob, attestation.service_ok);
    blob.append(reinterpret_cast<const char *>(&attestation.signature), sizeof(attestation.signature));
    return blob;
  }

  bool parse_attestation(const std::string &blob, service_attestation &attestation)
  {
    if (blob.size() != EPOSE_ATTESTATION_BLOB_SIZE)
      return false;

    size_t offset = 0;
    return read_u8(blob, offset, attestation.version)
        && read_bytes(blob, offset, &attestation.verifier_public_key, sizeof(attestation.verifier_public_key))
        && read_bytes(blob, offset, &attestation.subject_public_key, sizeof(attestation.subject_public_key))
        && read_u64_le(blob, offset, attestation.epoch)
        && read_bytes(blob, offset, &attestation.challenge_hash, sizeof(attestation.challenge_hash))
        && read_bytes(blob, offset, &attestation.response_hash, sizeof(attestation.response_hash))
        && read_bytes(blob, offset, &attestation.observed_tip_hash, sizeof(attestation.observed_tip_hash))
        && read_bool(blob, offset, attestation.service_ok)
        && read_bytes(blob, offset, &attestation.signature, sizeof(attestation.signature))
        && offset == blob.size();
  }

  std::string make_attestation_tx_extra_nonce(const service_attestation &attestation)
  {
    std::string extra_nonce;
    extra_nonce.reserve(EPOSE_ATTESTATION_BLOB_SIZE + 1);
    append_u8(extra_nonce, EPOSE_TX_EXTRA_NONCE_ATTESTATION);
    extra_nonce.append(serialize_attestation(attestation));
    return extra_nonce;
  }

  bool parse_attestation_tx_extra_nonce(const std::string &extra_nonce, service_attestation &attestation)
  {
    if (extra_nonce.size() != EPOSE_ATTESTATION_BLOB_SIZE + 1)
      return false;
    if (static_cast<uint8_t>(extra_nonce[0]) != EPOSE_TX_EXTRA_NONCE_ATTESTATION)
      return false;
    return parse_attestation(extra_nonce.substr(1), attestation);
  }

  bool extract_attestations_from_tx_extra(const std::vector<uint8_t> &tx_extra, std::vector<service_attestation> &attestations)
  {
    attestations.clear();

    std::vector<cryptonote::tx_extra_field> fields;
    if (!cryptonote::parse_tx_extra(tx_extra, fields))
      return false;

    for (const cryptonote::tx_extra_field &field : fields)
    {
      if (field.type() != typeid(cryptonote::tx_extra_nonce))
        continue;

      const cryptonote::tx_extra_nonce &extra_nonce = boost::get<cryptonote::tx_extra_nonce>(field);
      if (!is_epose_nonce_subtype(extra_nonce.nonce))
        continue;

      service_attestation attestation;
      if (static_cast<uint8_t>(extra_nonce.nonce[0]) == EPOSE_TX_EXTRA_NONCE_ATTESTATION)
      {
        if (!parse_attestation_tx_extra_nonce(extra_nonce.nonce, attestation))
          return false;
        attestations.push_back(attestation);
      }
    }

    return true;
  }

  void sign_attestation(service_attestation &attestation, const crypto::secret_key &verifier_secret_key, cryptonote::network_type nettype)
  {
    crypto::generate_signature(hash_attestation_message(attestation, nettype), attestation.verifier_public_key, verifier_secret_key, attestation.signature);
  }

  bool verify_attestation_signature(const service_attestation &attestation, cryptonote::network_type nettype)
  {
    if (attestation.version != EPOSE_PROTOCOL_VERSION)
      return false;
    if (!attestation.service_ok)
      return false;
    if (public_key_equal(attestation.verifier_public_key, attestation.subject_public_key))
      return false;
    if (!crypto::check_key(attestation.verifier_public_key) || !crypto::check_key(attestation.subject_public_key))
      return false;
    return crypto::check_signature(hash_attestation_message(attestation, nettype), attestation.verifier_public_key, attestation.signature);
  }

  bool identity_active_in_epoch(const service_node_identity &identity, uint64_t epoch)
  {
    return identity.registration_epoch <= epoch && epoch < identity.expiry_epoch;
  }

  uint64_t required_attestations_for_committee_size(size_t actual_committee_size)
  {
    if (actual_committee_size == 0)
      return 0;

    const uint64_t n = static_cast<uint64_t>(actual_committee_size);
    const uint64_t quotient = n / 3;
    const uint64_t remainder = n % 3;
    return quotient * 2 + (remainder == 0 ? 0 : remainder == 1 ? 1 : 2);
  }

  std::vector<crypto::public_key> qualified_nodes(
      const std::vector<service_node_identity> &registered_nodes,
      const std::vector<service_attestation> &attestations,
      uint64_t epoch,
      cryptonote::network_type nettype)
  {
    std::vector<crypto::public_key> qualified;
    // Only the committee size is needed here. Concrete verifier membership is
    // validated when attestations are applied with the canonical epoch seed.
    const crypto::hash epoch_seed = calculate_epoch_seed(nettype, epoch, crypto::null_hash);
    for (const service_node_identity &node : registered_nodes)
    {
      if (!identity_active_in_epoch(node, epoch))
        continue;

      const auto committee = select_verifiers(
          registered_nodes,
          node.service_public_key,
          nettype,
          epoch,
          epoch_seed,
          EPOSE_VERIFIER_COMMITTEE_SIZE);
      const uint64_t required_attestations = required_attestations_for_committee_size(committee.size());
      if (required_attestations == 0)
        continue;

      std::vector<crypto::public_key> voters;
      for (const service_attestation &attestation : attestations)
      {
        if (attestation.epoch != epoch || !public_key_equal(attestation.subject_public_key, node.service_public_key))
          continue;
        if (!verify_attestation_signature(attestation, nettype))
          continue;
        if (std::find_if(voters.begin(), voters.end(), [&](const crypto::public_key &voter) { return public_key_equal(voter, attestation.verifier_public_key); }) == voters.end())
          voters.push_back(attestation.verifier_public_key);
      }

      if (voters.size() >= required_attestations)
        qualified.push_back(node.service_public_key);
    }

    std::sort(qualified.begin(), qualified.end(), public_key_less);
    qualified.erase(std::unique(qualified.begin(), qualified.end(), public_key_equal), qualified.end());
    return qualified;
  }

  reward_split split_block_reward(uint64_t base_reward, uint64_t fees, uint64_t service_reward_bps)
  {
    if (service_reward_bps > 10000)
      throw std::invalid_argument("EPoSE service reward basis points exceed 100%");
    if (base_reward > std::numeric_limits<uint64_t>::max() - fees)
      throw std::overflow_error("EPoSE total reward overflow");

    const uint64_t total = base_reward + fees;
    const uint64_t service_reward = total / 10000 * service_reward_bps + (total % 10000) * service_reward_bps / 10000;
    return {total - service_reward, service_reward};
  }

  bool validate_service_reward_output(
      const cryptonote::transaction &miner_tx,
      const cryptonote::account_public_address &reward_address,
      const crypto::secret_key &reward_view_secret_key,
      uint64_t service_reward)
  {
    if (service_reward == 0)
      return false;

    const crypto::public_key tx_public_key = cryptonote::get_tx_pub_key_from_extra(miner_tx);
    if (tx_public_key == crypto::null_pkey)
      return false;

    crypto::key_derivation derivation{};
    if (!crypto::generate_key_derivation(tx_public_key, reward_view_secret_key, derivation))
      return false;

    std::vector<crypto::public_key> output_public_keys;
    output_public_keys.reserve(miner_tx.vout.size());
    std::vector<uint64_t> matching_amounts;
    for (size_t output_index = 0; output_index < miner_tx.vout.size(); ++output_index)
    {
      const cryptonote::tx_out &out = miner_tx.vout[output_index];
      crypto::public_key output_public_key{};
      if (!cryptonote::get_output_public_key(out, output_public_key))
        return false;
      for (const crypto::public_key &seen_output_public_key : output_public_keys)
      {
        if (public_key_equal(output_public_key, seen_output_public_key))
          return false;
      }
      output_public_keys.push_back(output_public_key);

      crypto::public_key expected_output_public_key{};
      if (!crypto::derive_public_key(derivation, output_index, reward_address.m_spend_public_key, expected_output_public_key))
        return false;

      if (public_key_equal(output_public_key, expected_output_public_key))
        matching_amounts.push_back(out.amount);
    }

    std::sort(matching_amounts.begin(), matching_amounts.end());
    return matching_amounts == decompose_service_reward_amount(service_reward);
  }

  bool select_service_payee(
      const std::vector<crypto::public_key> &qualified_service_nodes,
      const crypto::hash &epoch_seed,
      uint64_t height,
      crypto::public_key &selected)
  {
    if (qualified_service_nodes.empty())
      return false;

    struct ranked_node
    {
      crypto::hash rank;
      crypto::public_key key;
    };

    std::vector<ranked_node> ranked;
    ranked.reserve(qualified_service_nodes.size());
    for (const crypto::public_key &key : qualified_service_nodes)
    {
      std::string blob("QWC_EPOSE_PAYOUT_V1");
      append_hash(blob, epoch_seed);
      append_public_key(blob, key);
      ranked.push_back({hash_blob(blob), key});
    }

    std::sort(ranked.begin(), ranked.end(), [](const ranked_node &left, const ranked_node &right) {
      const int hash_cmp = std::memcmp(&left.rank, &right.rank, sizeof(left.rank));
      if (hash_cmp != 0)
        return hash_cmp < 0;
      return public_key_less(left.key, right.key);
    });

    selected = ranked[height % ranked.size()].key;
    return true;
  }
} // namespace epose
} // namespace qwertycoin
