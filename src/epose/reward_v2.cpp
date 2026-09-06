// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include "epose/reward_v2.h"

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "epose/envelope_v2.h"
#include "epose/record_codec_v2.h"
#include "epose/verification_v2.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>

namespace
{
  using qwertycoin::epose::service_payment_context_v2;

  bool checked_add(uint64_t left, uint64_t right, uint64_t &out)
  {
    if (left > std::numeric_limits<uint64_t>::max() - right)
      return false;
    out = left + right;
    return true;
  }

  uint64_t basis_points(uint64_t amount, uint64_t bps)
  {
    return amount / 10000 * bps + (amount % 10000) * bps / 10000;
  }

  template <typename T>
  void append_bytes(std::string &out, const T &value)
  {
    out.append(reinterpret_cast<const char *>(&value), sizeof(T));
  }

  void append_u8(std::string &out, uint8_t value)
  {
    out.push_back(static_cast<char>(value));
  }

  void append_u32_le(std::string &out, uint32_t value)
  {
    for (unsigned shift = 0; shift < 32; shift += 8)
      out.push_back(static_cast<char>((value >> shift) & 0xff));
  }

  void append_u64_le(std::string &out, uint64_t value)
  {
    for (unsigned shift = 0; shift < 64; shift += 8)
      out.push_back(static_cast<char>((value >> shift) & 0xff));
  }

  void append_network(std::string &out, cryptonote::network_type nettype)
  {
    const auto &network_id = cryptonote::get_config(nettype).NETWORK_ID;
    for (const auto byte : network_id)
      append_u8(out, byte);
  }

  bool valid_nonnull_key(const crypto::public_key &key)
  {
    return key != crypto::null_pkey && crypto::check_key(key);
  }

  bool context_shape_valid(const service_payment_context_v2 &context)
  {
    if (context.nettype == cryptonote::UNDEFINED
        || context.genesis_hash == crypto::null_hash
        || context.parameter_set_hash == crypto::null_hash
        || context.parent_hash == crypto::null_hash
        || context.qualification_hash == crypto::null_hash
        || context.coinbase_commitment == crypto::null_hash
        || context.service_reward == 0
        || context.outputs.empty()
        || !valid_nonnull_key(context.payee_service_public_key)
        || !valid_nonnull_key(context.reward_address.m_view_public_key)
        || !valid_nonnull_key(context.reward_address.m_spend_public_key)
        || !valid_nonnull_key(context.transaction_public_key))
      return false;

    uint64_t sum = 0;
    uint32_t previous_index = 0;
    bool first = true;
    std::vector<crypto::public_key> keys;
    keys.reserve(context.outputs.size());
    for (const auto &output : context.outputs)
    {
      if (!valid_nonnull_key(output.output_public_key)
          || output.amount == 0
          || (!first && output.output_index <= previous_index))
        return false;
      first = false;
      previous_index = output.output_index;
      if (!checked_add(sum, output.amount, sum))
        return false;
      if (std::find(keys.begin(), keys.end(), output.output_public_key) != keys.end())
        return false;
      keys.push_back(output.output_public_key);
    }
    return sum == context.service_reward;
  }

  crypto::hash fast_hash(const std::string &blob)
  {
    return crypto::cn_fast_hash(blob.data(), blob.size());
  }

  crypto::hash payment_record_hash(
      const qwertycoin::epose::envelope_record_v2 &record)
  {
    std::string blob("QWC_EPOSE_VALIDATED_PAYMENT_RECORD_V2");
    append_u8(blob, record.type);
    append_u8(blob, record.version);
    append_u64_le(blob, record.payload.size());
    blob.append(record.payload);
    return fast_hash(blob);
  }

  crypto::public_key derivation_as_public_key(const crypto::key_derivation &derivation)
  {
    static_assert(sizeof(crypto::public_key) == sizeof(crypto::key_derivation), "key sizes differ");
    crypto::public_key out{};
    std::memcpy(&out, &derivation, sizeof(out));
    return out;
  }

  bool cofactor_cleared_view_key(const crypto::public_key &view_key, crypto::public_key &cleared)
  {
    crypto::secret_key one{};
    reinterpret_cast<unsigned char *>(&one)[0] = 1;
    crypto::key_derivation derivation{};
    if (!crypto::generate_key_derivation(view_key, one, derivation))
      return false;
    cleared = derivation_as_public_key(derivation);
    return true;
  }

  crypto::key_derivation public_key_as_derivation(const crypto::public_key &key)
  {
    static_assert(sizeof(crypto::public_key) == sizeof(crypto::key_derivation), "key sizes differ");
    crypto::key_derivation out{};
    std::memcpy(&out, &key, sizeof(out));
    return out;
  }
}

namespace qwertycoin
{
namespace epose
{
  const service_payment_context_v2 &validated_service_payment_v2::context() const
  {
    return context_;
  }

  bool validated_service_payment_v2::matches(
      const envelope_record_v2 &record) const
  {
    return record_hash_ != crypto::null_hash
        && payment_record_hash(record) == record_hash_;
  }

  reward_status_v2 calculate_reward_allocation_v2(
      uint64_t scheduled_subsidy,
      uint64_t transaction_fees,
      bool has_qualified_payee,
      empty_qualification_policy_v2 empty_policy,
      reward_allocation_v2 &allocation,
      uint64_t service_reward_bps)
  {
    allocation = {};
    if (service_reward_bps > 10000)
      return reward_status_v2::invalid_basis_points;

    const uint64_t service_allocation = basis_points(scheduled_subsidy, service_reward_bps);
    reward_allocation_v2 next{};
    next.scheduled_subsidy = scheduled_subsidy;
    next.transaction_fees = transaction_fees;
    next.miner_fees = transaction_fees;
    next.emission_advance = scheduled_subsidy;

    if (has_qualified_payee)
    {
      next.miner_subsidy = scheduled_subsidy - service_allocation;
      next.service_reward = service_allocation;
    }
    else
    {
      if (empty_policy == empty_qualification_policy_v2::unset)
        return reward_status_v2::unresolved_empty_policy;
      if (empty_policy == empty_qualification_policy_v2::miner_fallback)
        next.miner_subsidy = scheduled_subsidy;
      else if (empty_policy == empty_qualification_policy_v2::permanent_nonissuance)
      {
        next.miner_subsidy = scheduled_subsidy - service_allocation;
        next.permanently_unissued = service_allocation;
      }
      else
        return reward_status_v2::unresolved_empty_policy;
    }

    if (!checked_add(next.miner_subsidy, next.service_reward, next.issued_subsidy)
        || !checked_add(next.issued_subsidy, transaction_fees, next.coinbase_total))
      return reward_status_v2::arithmetic_overflow;
    allocation = next;
    return reward_status_v2::accepted;
  }

  reward_status_v2 select_service_payee_v2(
      const qualification_set_v2 &qualification,
      const crypto::hash &payout_seed,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      const epoch_timing_v2 &timing,
      uint64_t payout_epoch,
      uint64_t height,
      crypto::public_key &selected)
  {
    selected = {};
    if (qualification.qualified_nodes.empty())
      return reward_status_v2::empty_qualification;
    uint64_t expected_payout_epoch = 0;
    uint64_t payout_epoch_start = 0;
    uint64_t payout_epoch_end = 0;
    if (!checked_add(qualification.epoch, 1, expected_payout_epoch)
        || payout_epoch != expected_payout_epoch
        || !timing.valid()
        || !timing.epoch_start(payout_epoch, payout_epoch_start)
        || !timing.epoch_end(payout_epoch, payout_epoch_end)
        || payout_seed == crypto::null_hash || genesis_hash == crypto::null_hash
        || parameter_set_hash == crypto::null_hash || qualification.snapshot_hash == crypto::null_hash
        || qualification.qualification_hash == crypto::null_hash || qualification.closed_height == 0)
      return reward_status_v2::invalid_context;
    if (height < payout_epoch_start || height > payout_epoch_end)
      return reward_status_v2::invalid_height;

    struct ranked
    {
      crypto::hash score{};
      crypto::public_key key{};
    };
    std::vector<ranked> nodes;
    nodes.reserve(qualification.qualified_nodes.size());
    std::vector<crypto::public_key> seen;
    seen.reserve(qualification.qualified_nodes.size());
    for (const auto &key : qualification.qualified_nodes)
    {
      if (!valid_nonnull_key(key) || std::find(seen.begin(), seen.end(), key) != seen.end())
        return reward_status_v2::invalid_context;
      seen.push_back(key);
      std::string blob("QWC_EPOSE_PAYOUT_V2");
      append_bytes(blob, genesis_hash);
      append_bytes(blob, parameter_set_hash);
      append_u64_le(blob, qualification.epoch);
      append_bytes(blob, qualification.qualification_hash);
      append_u64_le(blob, payout_epoch);
      append_bytes(blob, payout_seed);
      append_bytes(blob, key);
      nodes.push_back({fast_hash(blob), key});
    }
    std::sort(nodes.begin(), nodes.end(), [](const ranked &left, const ranked &right) {
      const int score_order = std::memcmp(&left.score, &right.score, sizeof(left.score));
      return score_order != 0 ? score_order < 0 : std::memcmp(&left.key, &right.key, sizeof(left.key)) < 0;
    });
    selected = nodes[(height - payout_epoch_start) % nodes.size()].key;
    return reward_status_v2::accepted;
  }

  crypto::hash hash_service_payment_context_v2(
      const service_payment_context_v2 &context,
      const crypto::public_key &derivation)
  {
    std::string blob("QWC_EPOSE_PAYMENT_PROOF_V2");
    append_network(blob, context.nettype);
    append_bytes(blob, context.genesis_hash);
    append_bytes(blob, context.parameter_set_hash);
    append_u64_le(blob, context.height);
    append_bytes(blob, context.parent_hash);
    append_u64_le(blob, context.payout_epoch);
    append_bytes(blob, context.qualification_hash);
    append_bytes(blob, context.payee_service_public_key);
    append_bytes(blob, context.reward_address.m_view_public_key);
    append_bytes(blob, context.reward_address.m_spend_public_key);
    append_u64_le(blob, context.service_reward);
    append_bytes(blob, context.transaction_public_key);
    append_bytes(blob, context.coinbase_commitment);
    append_u64_le(blob, context.outputs.size());
    for (const auto &output : context.outputs)
    {
      append_u32_le(blob, output.output_index);
      append_u64_le(blob, output.amount);
      append_bytes(blob, output.output_public_key);
    }
    append_bytes(blob, derivation);
    return fast_hash(blob);
  }

  reward_status_v2 generate_scoped_payment_proof_v2(
      const service_payment_context_v2 &context,
      const crypto::secret_key &transaction_secret_key,
      scoped_payment_proof_v2 &proof)
  {
    proof = {};
    if (!context_shape_valid(context))
      return reward_status_v2::invalid_context;
    crypto::public_key expected_transaction_key{};
    if (!crypto::secret_key_to_public_key(transaction_secret_key, expected_transaction_key)
        || expected_transaction_key != context.transaction_public_key)
      return reward_status_v2::invalid_transaction_key;

    crypto::key_derivation derivation{};
    if (!crypto::generate_key_derivation(context.reward_address.m_view_public_key, transaction_secret_key, derivation))
      return reward_status_v2::invalid_derivation;
    proof.derivation = derivation_as_public_key(derivation);
    for (const auto &output : context.outputs)
    {
      crypto::public_key expected{};
      if (!crypto::derive_public_key(derivation, output.output_index, context.reward_address.m_spend_public_key, expected)
          || expected != output.output_public_key)
        return reward_status_v2::invalid_output_allocation;
    }
    const crypto::hash transcript = hash_service_payment_context_v2(context, proof.derivation);
    crypto::public_key cleared_view_key{};
    if (!cofactor_cleared_view_key(context.reward_address.m_view_public_key, cleared_view_key))
      return reward_status_v2::invalid_derivation;
    crypto::generate_tx_proof(
        transcript,
        context.transaction_public_key,
        cleared_view_key,
        boost::none,
        proof.derivation,
        transaction_secret_key,
        proof.proof);
    return reward_status_v2::accepted;
  }

  reward_status_v2 verify_scoped_payment_proof_v2(
      const service_payment_context_v2 &context,
      const scoped_payment_proof_v2 &proof,
      verification_counters_v2 *counters)
  {
    if (!context_shape_valid(context))
      return reward_status_v2::invalid_context;
    if (!valid_nonnull_key(proof.derivation))
      return reward_status_v2::invalid_derivation;
    const crypto::hash transcript = hash_service_payment_context_v2(context, proof.derivation);
    crypto::public_key cleared_view_key{};
    if (!cofactor_cleared_view_key(context.reward_address.m_view_public_key, cleared_view_key))
      return reward_status_v2::invalid_derivation;
    if (counters != nullptr)
      ++counters->signatures;
    if (!crypto::check_tx_proof(
            transcript,
            context.transaction_public_key,
            cleared_view_key,
            boost::none,
            proof.derivation,
            proof.proof,
            2))
      return reward_status_v2::invalid_payment_proof;

    const crypto::key_derivation derivation = public_key_as_derivation(proof.derivation);
    for (const auto &output : context.outputs)
    {
      crypto::public_key expected{};
      if (!crypto::derive_public_key(derivation, output.output_index, context.reward_address.m_spend_public_key, expected)
          || expected != output.output_public_key)
        return reward_status_v2::invalid_output_allocation;
    }
    return reward_status_v2::accepted;
  }

  reward_status_v2 canonical_coinbase_commitment_v2(
      const cryptonote::transaction &coinbase,
      uint8_t major_version,
      size_t max_envelopes_per_transaction,
      const envelope_limits_v2 &limits,
      crypto::hash &commitment,
      size_t &removed_proofs)
  {
    commitment = {};
    removed_proofs = 0;
    if (!cryptonote::is_coinbase(coinbase))
      return reward_status_v2::invalid_context;

    std::vector<uint8_t> stripped_extra;
    size_t removed = 0;
    if (strip_transaction_record_type_v2(
            coinbase.extra, major_version, max_envelopes_per_transaction,
            limits, record_type_v2::service_payment_proof,
            stripped_extra, removed) != envelope_status_v2::accepted)
      return reward_status_v2::invalid_context;

    cryptonote::transaction canonical = coinbase;
    canonical.extra = std::move(stripped_extra);
    canonical.invalidate_hashes();
    const cryptonote::blobdata bytes = cryptonote::tx_to_blob(canonical);
    if (bytes.empty())
      return reward_status_v2::invalid_context;
    commitment = crypto::cn_fast_hash(bytes.data(), bytes.size());
    removed_proofs = removed;
    return commitment == crypto::null_hash
        ? reward_status_v2::invalid_context
        : reward_status_v2::accepted;
  }

  reward_status_v2 verify_coinbase_service_payment_v2(
      const cryptonote::transaction &coinbase,
      uint8_t major_version,
      size_t max_envelopes_per_transaction,
      const envelope_limits_v2 &limits,
      const service_payment_expectation_v2 &expected,
      service_payment_context_v2 &context,
      verification_counters_v2 *counters)
  {
    context = {};
    if (!cryptonote::is_coinbase(coinbase)
        || expected.nettype == cryptonote::UNDEFINED
        || expected.genesis_hash == crypto::null_hash
        || expected.parameter_set_hash == crypto::null_hash
        || expected.parent_hash == crypto::null_hash
        || expected.qualification_hash == crypto::null_hash
        || expected.service_reward == 0
        || !valid_nonnull_key(expected.payee_service_public_key)
        || !valid_nonnull_key(expected.reward_address.m_view_public_key)
        || !valid_nonnull_key(expected.reward_address.m_spend_public_key))
      return reward_status_v2::invalid_context;

    std::vector<envelope_record_v2> records;
    envelope_budget_v2 budget{};
    if (parse_transaction_extra_v2(
            coinbase.extra, major_version, max_envelopes_per_transaction,
            limits, records, budget) != envelope_status_v2::accepted)
      return reward_status_v2::invalid_context;

    scoped_payment_proof_v2 proof{};
    size_t proof_records = 0;
    for (const envelope_record_v2 &record : records)
    {
      if (record.type != static_cast<uint8_t>(record_type_v2::service_payment_proof))
        continue;
      if (++proof_records != 1
          || decode_payment_proof_record_structure_v2(record, proof)
              != record_codec_status_v2::accepted)
        return reward_status_v2::invalid_payment_proof;
    }
    if (proof_records != 1 || !valid_nonnull_key(proof.derivation))
      return reward_status_v2::invalid_payment_proof;

    service_payment_context_v2 next{};
    next.nettype = expected.nettype;
    next.genesis_hash = expected.genesis_hash;
    next.parameter_set_hash = expected.parameter_set_hash;
    next.height = expected.height;
    next.parent_hash = expected.parent_hash;
    next.payout_epoch = expected.payout_epoch;
    next.qualification_hash = expected.qualification_hash;
    next.payee_service_public_key = expected.payee_service_public_key;
    next.reward_address = expected.reward_address;
    next.service_reward = expected.service_reward;
    next.transaction_public_key = cryptonote::get_tx_pub_key_from_extra(coinbase);
    if (!valid_nonnull_key(next.transaction_public_key))
      return reward_status_v2::invalid_transaction_key;

    size_t removed = 0;
    if (canonical_coinbase_commitment_v2(
            coinbase, major_version, max_envelopes_per_transaction, limits,
            next.coinbase_commitment, removed) != reward_status_v2::accepted
        || removed != 1)
      return reward_status_v2::invalid_payment_proof;

    const crypto::key_derivation derivation = public_key_as_derivation(proof.derivation);
    for (size_t output_index = 0; output_index < coinbase.vout.size(); ++output_index)
    {
      if (output_index > std::numeric_limits<uint32_t>::max())
        return reward_status_v2::invalid_output_allocation;
      crypto::public_key output_public_key{};
      if (!cryptonote::get_output_public_key(coinbase.vout[output_index], output_public_key))
        return reward_status_v2::invalid_output_allocation;
      crypto::public_key expected_output_key{};
      if (!crypto::derive_public_key(
              derivation, output_index,
              expected.reward_address.m_spend_public_key,
              expected_output_key))
        return reward_status_v2::invalid_derivation;
      if (output_public_key == expected_output_key)
      {
        if (coinbase.vout[output_index].amount == 0)
          return reward_status_v2::invalid_output_allocation;
        next.outputs.push_back({
            static_cast<uint32_t>(output_index),
            coinbase.vout[output_index].amount,
            output_public_key});
      }
    }

    if (verify_scoped_payment_proof_v2(next, proof, counters)
        != reward_status_v2::accepted)
      return reward_status_v2::invalid_payment_proof;
    context = std::move(next);
    return reward_status_v2::accepted;
  }

  reward_status_v2 validate_coinbase_service_payment_v2(
      const cryptonote::transaction &coinbase,
      uint8_t major_version,
      size_t max_envelopes_per_transaction,
      const envelope_limits_v2 &limits,
      const service_payment_expectation_v2 &expected,
      validated_service_payment_v2 &validated,
      verification_counters_v2 *counters)
  {
    validated.context_ = {};
    validated.record_hash_ = {};
    service_payment_context_v2 context{};
    const reward_status_v2 status = verify_coinbase_service_payment_v2(
        coinbase, major_version, max_envelopes_per_transaction,
        limits, expected, context, counters);
    if (status != reward_status_v2::accepted)
      return status;

    std::vector<envelope_record_v2> records;
    envelope_budget_v2 budget{};
    if (parse_transaction_extra_v2(
            coinbase.extra, major_version, max_envelopes_per_transaction,
            limits, records, budget) != envelope_status_v2::accepted)
      return reward_status_v2::invalid_payment_proof;
    const auto proof = std::find_if(
        records.begin(), records.end(), [](const envelope_record_v2 &record) {
          return record.type
              == static_cast<uint8_t>(record_type_v2::service_payment_proof);
        });
    if (proof == records.end())
      return reward_status_v2::invalid_payment_proof;
    validated.context_ = std::move(context);
    validated.record_hash_ = payment_record_hash(*proof);
    return reward_status_v2::accepted;
  }

  reward_status_v2 append_coinbase_service_payment_proof_v2(
      cryptonote::transaction &coinbase,
      const crypto::secret_key &transaction_secret_key,
      uint8_t major_version,
      size_t max_envelopes_per_transaction,
      const envelope_limits_v2 &limits,
      const service_payment_expectation_v2 &expected,
      service_payment_context_v2 &context)
  {
    context = {};
    if (!cryptonote::is_coinbase(coinbase))
      return reward_status_v2::invalid_context;
    crypto::public_key transaction_public_key{};
    if (!crypto::secret_key_to_public_key(
            transaction_secret_key, transaction_public_key)
        || transaction_public_key != cryptonote::get_tx_pub_key_from_extra(coinbase))
      return reward_status_v2::invalid_transaction_key;

    cryptonote::transaction candidate = coinbase;
    service_payment_context_v2 signing{};
    signing.nettype = expected.nettype;
    signing.genesis_hash = expected.genesis_hash;
    signing.parameter_set_hash = expected.parameter_set_hash;
    signing.height = expected.height;
    signing.parent_hash = expected.parent_hash;
    signing.payout_epoch = expected.payout_epoch;
    signing.qualification_hash = expected.qualification_hash;
    signing.payee_service_public_key = expected.payee_service_public_key;
    signing.reward_address = expected.reward_address;
    signing.service_reward = expected.service_reward;
    signing.transaction_public_key = transaction_public_key;
    size_t existing_proofs = 0;
    if (canonical_coinbase_commitment_v2(
            candidate, major_version, max_envelopes_per_transaction, limits,
            signing.coinbase_commitment, existing_proofs) != reward_status_v2::accepted
        || existing_proofs != 0)
      return reward_status_v2::invalid_payment_proof;

    crypto::key_derivation derivation{};
    if (!crypto::generate_key_derivation(
            expected.reward_address.m_view_public_key,
            transaction_secret_key, derivation))
      return reward_status_v2::invalid_derivation;
    for (size_t output_index = 0; output_index < candidate.vout.size(); ++output_index)
    {
      if (output_index > std::numeric_limits<uint32_t>::max())
        return reward_status_v2::invalid_output_allocation;
      crypto::public_key output_public_key{};
      if (!cryptonote::get_output_public_key(candidate.vout[output_index], output_public_key))
        return reward_status_v2::invalid_output_allocation;
      crypto::public_key expected_output_key{};
      if (!crypto::derive_public_key(
              derivation, output_index,
              expected.reward_address.m_spend_public_key,
              expected_output_key))
        return reward_status_v2::invalid_derivation;
      if (output_public_key == expected_output_key)
        signing.outputs.push_back({
            static_cast<uint32_t>(output_index),
            candidate.vout[output_index].amount,
            output_public_key});
    }

    scoped_payment_proof_v2 proof{};
    const reward_status_v2 generated = generate_scoped_payment_proof_v2(
        signing, transaction_secret_key, proof);
    if (generated != reward_status_v2::accepted)
      return generated;
    envelope_record_v2 record{};
    if (encode_payment_proof_record_v2(proof, signing, record)
        != record_codec_status_v2::accepted)
      return reward_status_v2::invalid_payment_proof;
    envelope_budget_v2 budget{};
    if (append_transaction_envelope_v2(
            {record}, major_version, max_envelopes_per_transaction,
            limits, candidate.extra, budget) != envelope_status_v2::accepted)
      return reward_status_v2::invalid_payment_proof;

    service_payment_context_v2 verified{};
    const reward_status_v2 verified_status = verify_coinbase_service_payment_v2(
        candidate, major_version, max_envelopes_per_transaction,
        limits, expected, verified);
    if (verified_status != reward_status_v2::accepted)
      return verified_status;
    coinbase = std::move(candidate);
    coinbase.invalidate_hashes();
    context = std::move(verified);
    return reward_status_v2::accepted;
  }
} // namespace epose
} // namespace qwertycoin
