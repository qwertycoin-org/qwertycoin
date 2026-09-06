// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <vector>

#include "crypto/crypto.h"
#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_config.h"
#include "epose/membership_v2.h"

namespace qwertycoin
{
namespace epose
{
  struct envelope_limits_v2;
  struct envelope_record_v2;
  struct service_payment_expectation_v2;
  struct verification_counters_v2;
  constexpr uint64_t EPOSE_SERVICE_REWARD_BPS_V2 = 1000;

  enum class empty_qualification_policy_v2 : uint8_t
  {
    unset = 0,
    miner_fallback = 1,
    permanent_nonissuance = 2
  };

  enum class reward_status_v2
  {
    accepted,
    invalid_basis_points,
    unresolved_empty_policy,
    arithmetic_overflow,
    invalid_height,
    empty_qualification,
    invalid_context,
    invalid_transaction_key,
    invalid_derivation,
    invalid_output_allocation,
    invalid_payment_proof
  };

  struct reward_allocation_v2
  {
    uint64_t scheduled_subsidy = 0;
    uint64_t transaction_fees = 0;
    uint64_t miner_subsidy = 0;
    uint64_t miner_fees = 0;
    uint64_t service_reward = 0;
    uint64_t permanently_unissued = 0;
    uint64_t issued_subsidy = 0;
    uint64_t coinbase_total = 0;
    // This advances by the scheduled subsidy even when an approved policy
    // permanently leaves part of it unissued.
    uint64_t emission_advance = 0;
  };

  reward_status_v2 calculate_reward_allocation_v2(
      uint64_t scheduled_subsidy,
      uint64_t transaction_fees,
      bool has_qualified_payee,
      empty_qualification_policy_v2 empty_policy,
      reward_allocation_v2 &allocation,
      uint64_t service_reward_bps = EPOSE_SERVICE_REWARD_BPS_V2);

  reward_status_v2 select_service_payee_v2(
      const qualification_set_v2 &qualification,
      const crypto::hash &payout_seed,
      const crypto::hash &genesis_hash,
      const crypto::hash &parameter_set_hash,
      const epoch_timing_v2 &timing,
      uint64_t payout_epoch,
      uint64_t height,
      crypto::public_key &selected);

  struct service_payment_output_v2
  {
    uint32_t output_index = 0;
    uint64_t amount = 0;
    crypto::public_key output_public_key{};
  };

  // The caller computes coinbase_commitment from canonical coinbase bytes
  // with the payment-proof record omitted. This prevents a circular hash.
  struct service_payment_context_v2
  {
    cryptonote::network_type nettype = cryptonote::UNDEFINED;
    crypto::hash genesis_hash{};
    crypto::hash parameter_set_hash{};
    uint64_t height = 0;
    crypto::hash parent_hash{};
    uint64_t payout_epoch = 0;
    crypto::hash qualification_hash{};
    crypto::public_key payee_service_public_key{};
    cryptonote::account_public_address reward_address{};
    uint64_t service_reward = 0;
    crypto::public_key transaction_public_key{};
    crypto::hash coinbase_commitment{};
    std::vector<service_payment_output_v2> outputs;
  };

  struct scoped_payment_proof_v2
  {
    crypto::public_key derivation{};
    crypto::signature proof{};
  };

  class validated_service_payment_v2
  {
  public:
    validated_service_payment_v2() = default;
    validated_service_payment_v2(const validated_service_payment_v2 &) = default;
    validated_service_payment_v2 &operator=(const validated_service_payment_v2 &) = default;

    const service_payment_context_v2 &context() const;
    bool matches(const envelope_record_v2 &record) const;

  private:
    service_payment_context_v2 context_{};
    crypto::hash record_hash_{};

    friend reward_status_v2 validate_coinbase_service_payment_v2(
        const cryptonote::transaction &, uint8_t, size_t,
        const envelope_limits_v2 &, const service_payment_expectation_v2 &,
        validated_service_payment_v2 &, verification_counters_v2 *);
  };

  struct service_payment_expectation_v2
  {
    cryptonote::network_type nettype = cryptonote::UNDEFINED;
    crypto::hash genesis_hash{};
    crypto::hash parameter_set_hash{};
    uint64_t height = 0;
    crypto::hash parent_hash{};
    uint64_t payout_epoch = 0;
    crypto::hash qualification_hash{};
    crypto::public_key payee_service_public_key{};
    cryptonote::account_public_address reward_address{};
    uint64_t service_reward = 0;
  };

  crypto::hash hash_service_payment_context_v2(
      const service_payment_context_v2 &context,
      const crypto::public_key &derivation);

  reward_status_v2 generate_scoped_payment_proof_v2(
      const service_payment_context_v2 &context,
      const crypto::secret_key &transaction_secret_key,
      scoped_payment_proof_v2 &proof);

  reward_status_v2 verify_scoped_payment_proof_v2(
      const service_payment_context_v2 &context,
      const scoped_payment_proof_v2 &proof,
      verification_counters_v2 *counters = nullptr);

  // Hashes canonical Coinbase bytes after removing only v2 payment-proof
  // records. The caller checks removed_proofs against whether a payout is due.
  // This is the shared non-circular commitment used by proof construction and
  // validation; unrelated EPoSE and wallet metadata remain committed.
  reward_status_v2 canonical_coinbase_commitment_v2(
      const cryptonote::transaction &coinbase,
      uint8_t major_version,
      size_t max_envelopes_per_transaction,
      const envelope_limits_v2 &limits,
      crypto::hash &commitment,
      size_t &removed_proofs);

  // Builds and verifies the payment context from the actual canonical
  // Coinbase. Exactly one proof record is required and every output claimed
  // for the service reward is derived from that proof's scoped derivation.
  reward_status_v2 verify_coinbase_service_payment_v2(
      const cryptonote::transaction &coinbase,
      uint8_t major_version,
      size_t max_envelopes_per_transaction,
      const envelope_limits_v2 &limits,
      const service_payment_expectation_v2 &expected,
      service_payment_context_v2 &context,
      verification_counters_v2 *counters = nullptr);

  // Produces a capability bound to the complete authenticated proof record and
  // actual Coinbase. Only this function can create one; semantic application
  // consumes it without repeating the transaction-proof verification.
  reward_status_v2 validate_coinbase_service_payment_v2(
      const cryptonote::transaction &coinbase,
      uint8_t major_version,
      size_t max_envelopes_per_transaction,
      const envelope_limits_v2 &limits,
      const service_payment_expectation_v2 &expected,
      validated_service_payment_v2 &validated,
      verification_counters_v2 *counters = nullptr);

  // Appends the one scoped proof required for a prescribed service payout.
  // The Coinbase is changed only after the complete proof-bearing candidate
  // validates against the same production verifier used by block acceptance.
  reward_status_v2 append_coinbase_service_payment_proof_v2(
      cryptonote::transaction &coinbase,
      const crypto::secret_key &transaction_secret_key,
      uint8_t major_version,
      size_t max_envelopes_per_transaction,
      const envelope_limits_v2 &limits,
      const service_payment_expectation_v2 &expected,
      service_payment_context_v2 &context);
} // namespace epose
} // namespace qwertycoin
