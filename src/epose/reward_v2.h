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
    uint64_t issued_total = 0;
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
      uint64_t payout_epoch,
      uint64_t payout_epoch_start,
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

  crypto::hash hash_service_payment_context_v2(
      const service_payment_context_v2 &context,
      const crypto::public_key &derivation);

  reward_status_v2 generate_scoped_payment_proof_v2(
      const service_payment_context_v2 &context,
      const crypto::secret_key &transaction_secret_key,
      scoped_payment_proof_v2 &proof);

  reward_status_v2 verify_scoped_payment_proof_v2(
      const service_payment_context_v2 &context,
      const scoped_payment_proof_v2 &proof);
} // namespace epose
} // namespace qwertycoin
