// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstring>
#include <boost/optional/optional_io.hpp>

#include "cryptonote_basic/account.h"
#include "cryptonote_core/contract_state.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "device/device.hpp"
#include "epose/envelope_v2.h"
#include "epose/record_codec_v2.h"
#include "epose/reward_v2.h"

namespace epose_reward_payee_fixture
{
  using namespace qwertycoin::epose;

  constexpr uint64_t coinbase_height = 1440;
  constexpr uint64_t block_fees = 25;
  constexpr uint64_t minted_per_destination = 7000;

  inline crypto::hash hash_text(const char *text)
  {
    return crypto::cn_fast_hash(text, std::strlen(text));
  }

  inline crypto::public_key random_public_key()
  {
    return cryptonote::keypair::generate(hw::get_device("default")).pub;
  }

  inline cryptonote::account_public_address fresh_address()
  {
    cryptonote::account_base account;
    account.generate();
    return account.get_keys().m_account_address;
  }

  inline envelope_limits_v2 limits()
  {
    envelope_limits_v2 value{};
    value.max_envelope_bytes = 4096;
    value.max_records = 8;
    value.max_record_payload_bytes = 2048;
    value.max_signature_verifications = 16;
    value.max_admission_verifications = 4;
    value.supported_record_versions = {0, 1, 1, 1, 1, 1};
    return value;
  }

  inline cryptonote::miner_contract_outputs contract_outputs_with_two_mints()
  {
    cryptonote::miner_contract_outputs outputs;
    outputs.state_root = hash_text("payee-fixture-state-root");
    for (const uint8_t view_tag : {0x11, 0x22})
    {
      cryptonote::contract_mint mint;
      mint.amount = minted_per_destination;
      mint.destination.key = random_public_key();
      mint.destination.view_tag.data = view_tag;
      mint.destination.tx_key = random_public_key();
      outputs.mints.push_back(mint);
    }
    return outputs;
  }

  inline uint64_t paid_amount(const service_payment_context_v2 &context)
  {
    uint64_t paid = 0;
    for (const service_payment_output_v2 &output : context.outputs)
      paid += output.amount;
    return paid;
  }

  struct payee_coinbase
  {
    uint8_t hard_fork = 0;
    envelope_limits_v2 envelope_limits = limits();
    service_payment_expectation_v2 expectation{};
    service_payment_context_v2 generated{};
    cryptonote::transaction coinbase{};
    cryptonote::keypair transaction_key = cryptonote::keypair::generate(hw::get_device("default"));
    uint64_t scheduled_subsidy = 0;
    bool built = false;

    payee_coinbase(
        uint8_t build_hard_fork,
        cryptonote::network_type nettype,
        bool miner_is_payee,
        const cryptonote::miner_contract_outputs *contract_outputs = nullptr)
      : hard_fork(build_hard_fork)
    {
      get_scheduled_subsidy(build_hard_fork);
      const cryptonote::account_public_address payee_address = fresh_address();
      const cryptonote::account_public_address miner_address =
          miner_is_payee ? payee_address : fresh_address();

      expectation.nettype = nettype;
      expectation.genesis_hash = hash_text("payee-fixture-genesis");
      expectation.parameter_set_hash = hash_text("payee-fixture-parameters");
      expectation.height = coinbase_height;
      expectation.parent_hash = hash_text("payee-fixture-parent");
      expectation.payout_epoch = 2;
      expectation.qualification_hash = hash_text("payee-fixture-qualification");
      crypto::secret_key ignored{};
      crypto::generate_keys(expectation.payee_service_public_key, ignored);
      expectation.reward_address = payee_address;
      expectation.service_reward = scheduled_subsidy / 10;

      const cryptonote::miner_service_payment_v2 payment{
          &expectation, 0, scheduled_subsidy + block_fees, 2,
          &envelope_limits, nullptr, &generated};
      built = cryptonote::construct_miner_tx(
          coinbase_height, CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5, 0, 1,
          block_fees, miner_address, coinbase, {}, 1, build_hard_fork, &payment, contract_outputs, &transaction_key.sec);
    }

    reward_status_v2 rebuild_with_primary_miner_key()
    {
      crypto::key_derivation derivation{};
      if (!built || generated.outputs.empty() || !service_derivation(derivation))
        return reward_status_v2::invalid_context;
      const uint32_t service_start = generated.outputs.front().output_index;
      for (size_t i = 0; i < service_start; ++i)
      {
        crypto::public_key output_key{};
        if (!crypto::derive_public_key(derivation, i,
                expectation.reward_address.m_spend_public_key, output_key))
          return reward_status_v2::invalid_derivation;
        crypto::view_tag view_tag{};
        crypto::derive_view_tag(derivation, i, view_tag);
        cryptonote::set_tx_out(coinbase.vout[i].amount, output_key,
            hard_fork >= HF_VERSION_VIEW_TAGS, view_tag, coinbase.vout[i]);
      }
      std::vector<uint8_t> extra;
      size_t removed = 0;
      if (strip_transaction_record_type_v2(coinbase.extra, hard_fork, 2,
              envelope_limits, record_type_v2::service_payment_proof, extra, removed)
              != envelope_status_v2::accepted || removed != 1
          || !cryptonote::remove_field_from_tx_extra(extra,
              typeid(cryptonote::tx_extra_additional_pub_keys)))
        return reward_status_v2::invalid_payment_proof;
      coinbase.extra = std::move(extra);
      coinbase.invalidate_hashes();
      const auto status = append_coinbase_service_payment_proof_v2(coinbase,
          transaction_key.sec, hard_fork, 2, envelope_limits, expectation, service_start, generated);
      built = status == reward_status_v2::accepted;
      return status;
    }

    reward_status_v2 verify(uint8_t verify_hard_fork, service_payment_context_v2 &context) const
    {
      return verify_coinbase_service_payment_v2(
          coinbase, verify_hard_fork, 2, envelope_limits, expectation, context);
    }

    reward_status_v2 verify() const
    {
      service_payment_context_v2 ignored{};
      return verify(hard_fork, ignored);
    }

    bool proof_record(envelope_record_v2 &record) const
    {
      std::vector<envelope_record_v2> records;
      envelope_budget_v2 budget{};
      if (parse_transaction_extra_v2(
              coinbase.extra, hard_fork, 2, envelope_limits, records, budget)
          != envelope_status_v2::accepted)
        return false;
      for (const envelope_record_v2 &candidate : records)
        if (candidate.type == static_cast<uint8_t>(record_type_v2::service_payment_proof))
        {
          record = candidate;
          return true;
        }
      return false;
    }

    bool service_derivation(crypto::key_derivation &derivation) const
    {
      envelope_record_v2 record{};
      scoped_payment_proof_v2 proof{};
      if (!proof_record(record)
          || decode_payment_proof_record_structure_v2(record, proof)
              != record_codec_status_v2::accepted)
        return false;
      std::memcpy(&derivation, &proof.derivation, sizeof(derivation));
      return true;
    }

    bool rewrite_service_output_start(const boost::optional<uint32_t> &start)
    {
      envelope_record_v2 record{};
      if (!proof_record(record))
        return false;
      record.payload.resize(EPOSE_PAYMENT_PROOF_PAYLOAD_BYTES_V2);
      if (start)
        for (unsigned shift = 0; shift < 32; shift += 8)
          record.payload.push_back(static_cast<char>((*start >> shift) & 0xff));
      std::vector<uint8_t> stripped;
      size_t removed = 0;
      envelope_budget_v2 budget{};
      if (strip_transaction_record_type_v2(
              coinbase.extra, hard_fork, 2, envelope_limits,
              record_type_v2::service_payment_proof, stripped, removed)
              != envelope_status_v2::accepted
          || removed != 1
          || append_transaction_envelope_v2(
                 {record}, hard_fork, 2, envelope_limits, stripped, budget)
              != envelope_status_v2::accepted)
        return false;
      coinbase.extra = std::move(stripped);
      coinbase.invalidate_hashes();
      return true;
    }

  private:
    void get_scheduled_subsidy(uint8_t hard_fork)
    {
      cryptonote::get_block_reward(
          CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5, 1, 0,
          scheduled_subsidy, hard_fork);
    }
  };
}
