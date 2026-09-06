// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>

#include <cstring>
#include <map>
#include <string>

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "epose/coordinator_v2.h"
#include "epose/record_codec_v2.h"
#include "epose/service_receipt_v2.h"

namespace
{
  using namespace qwertycoin::epose;

  crypto::hash hash_text(const std::string &text)
  {
    return crypto::cn_fast_hash(text.data(), text.size());
  }

  struct key_pair
  {
    crypto::public_key public_key{};
    crypto::secret_key secret_key{};
  };

  key_pair make_keys()
  {
    key_pair out{};
    crypto::generate_keys(out.public_key, out.secret_key);
    return out;
  }

  struct enrollment
  {
    key_pair service = make_keys();
    key_pair operator_key = make_keys();
    key_pair reward_view = make_keys();
    key_pair reward_spend = make_keys();
    identity_descriptor_v2 descriptor{};
  };

  struct contexts final : canonical_context_source_v2
  {
    mutable std::map<uint64_t, crypto::hash> blocks;

    crypto::hash hash_for(uint64_t height) const
    {
      const auto inserted = blocks.emplace(
          height, height == 0
              ? hash_text("coordinator-genesis")
              : hash_text("coordinator-block-" + std::to_string(height)));
      return inserted.first->second;
    }

    bool block_hash(uint64_t height, crypto::hash &hash) const override
    {
      hash = hash_for(height);
      return true;
    }

    bool round_anchor(uint64_t epoch, uint64_t round, crypto::hash &hash) const override
    {
      if (epoch == 0 || round != 0)
        return false;
      hash = hash_for(epoch * 720 - 60);
      return true;
    }
  };

  consensus_parameters_v2 parameters(empty_qualification_policy_v2 empty_policy)
  {
    consensus_parameters_v2 out{};
    out.nettype = cryptonote::TESTNET;
    out.genesis_hash = hash_text("coordinator-genesis");
    out.parameter_set_hash = hash_text("coordinator-parameters");
    out.timing = {0, 720, 60};
    out.admission = {admission_work_algorithm_v2::randomx, 1, 1};
    out.committee = {2, 2, 1, 1, 1, {0}};
    out.limits.max_envelopes_per_transaction = 2;
    out.limits.envelope.max_envelope_bytes = 4096;
    out.limits.envelope.max_records = 8;
    out.limits.envelope.max_record_payload_bytes = 2048;
    out.limits.envelope.max_signature_verifications = 16;
    out.limits.envelope.max_admission_verifications = 4;
    out.limits.envelope.supported_record_versions = {0, 1, 1, 1, 1, 1};
    out.limits.block = {16384, 32, 32, 8};
    out.limits.max_recent_undo_blocks = 1440;
    out.empty_policy = empty_policy;
    out.state_commitment_schema = 1;
    return out;
  }

  enrollment make_enrollment(
      const consensus_parameters_v2 &configuration,
      const std::string &endpoint)
  {
    enrollment out{};
    out.descriptor.identity_id = derive_identity_id_v2(
        configuration.nettype, configuration.genesis_hash,
        configuration.parameter_set_hash, out.operator_key.public_key);
    out.descriptor.service_public_key = out.service.public_key;
    out.descriptor.operator_authorization_public_key = out.operator_key.public_key;
    out.descriptor.reward_address.m_view_public_key = out.reward_view.public_key;
    out.descriptor.reward_address.m_spend_public_key = out.reward_spend.public_key;
    out.descriptor.endpoint_descriptor_hash = hash_text(endpoint);
    out.descriptor.effective_epoch = 1;
    out.descriptor.expiry_epoch = 3;
    return out;
  }

  envelope_record_v2 lifecycle_record(
      const consensus_parameters_v2 &configuration,
      const enrollment &value)
  {
    lifecycle_record_v2 lifecycle{};
    lifecycle.next_descriptor = value.descriptor;
    EXPECT_TRUE(sign_lifecycle_record_v2(
        configuration.nettype, configuration.genesis_hash,
        configuration.parameter_set_hash, lifecycle,
        value.operator_key.secret_key, value.service.secret_key));
    envelope_record_v2 record{};
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_lifecycle_record_v2(
            lifecycle, configuration.nettype, configuration.genesis_hash,
            configuration.parameter_set_hash, record));
    return record;
  }

  envelope_record_v2 admission_record(
      const consensus_parameters_v2 &configuration,
      const enrollment &value)
  {
    admission_lease_v2 lease{};
    lease.member.service_public_key = value.descriptor.service_public_key;
    lease.member.identity_id = value.descriptor.identity_id;
    lease.member.operator_authorization_public_key =
        value.descriptor.operator_authorization_public_key;
    lease.member.descriptor_hash = hash_identity_descriptor_v2(
        configuration.nettype, configuration.genesis_hash,
        configuration.parameter_set_hash, value.descriptor);
    lease.member.reward_binding_hash = hash_reward_binding_v2(
        configuration.nettype, configuration.genesis_hash,
        configuration.parameter_set_hash, value.descriptor.reward_address);
    lease.member.endpoint_descriptor_hash = value.descriptor.endpoint_descriptor_hash;
    lease.target_epoch = 1;
    lease.work_algorithm = static_cast<uint8_t>(configuration.admission.algorithm);
    lease.leading_zero_bits = configuration.admission.leading_zero_bits;
    lease.admission_context_height = 0;
    lease.admission_context_hash = configuration.genesis_hash;
    const admission_context_v2 context{
        configuration.nettype, configuration.genesis_hash,
        configuration.parameter_set_hash, 0, configuration.genesis_hash};
    do
    {
      lease.work_hash = calculate_admission_work_v2(lease, context);
      ++lease.nonce;
    } while (!admission_work_meets_target_v2(
        lease.work_hash, configuration.admission.leading_zero_bits));
    --lease.nonce;
    lease.lease_hash = calculate_admission_lease_hash_v2(lease, context);
    envelope_record_v2 record{};
    EXPECT_EQ(record_codec_status_v2::accepted,
        encode_admission_lease_record_v2(
            lease, context, configuration.admission, record));
    return record;
  }

  cryptonote::transaction transaction_with(
      const consensus_parameters_v2 &configuration,
      const std::vector<envelope_record_v2> &records)
  {
    cryptonote::transaction transaction{};
    envelope_budget_v2 budget{};
    EXPECT_EQ(envelope_status_v2::accepted,
        append_transaction_envelope_v2(
            records, HF_VERSION_QWC_EPOSE,
            configuration.limits.max_envelopes_per_transaction,
            configuration.limits.envelope, transaction.extra, budget));
    return transaction;
  }

  cryptonote::transaction coinbase(uint64_t height, uint64_t amount)
  {
    cryptonote::transaction tx{};
    tx.version = 2;
    tx.unlock_time = height + CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW;
    cryptonote::txin_gen input{};
    input.height = height;
    tx.vin.push_back(input);
    cryptonote::txout_to_key target{};
    crypto::secret_key ignored{};
    crypto::generate_keys(target.key, ignored);
    cryptonote::tx_out output{amount, target};
    tx.vout.push_back(output);
    return tx;
  }

  coordinator_status_v2 connect_empty(
      consensus_coordinator_v2 &coordinator,
      const contexts &source,
      uint64_t height,
      uint64_t coinbase_amount,
      coordinator_result_v2 &result,
      uint8_t major_version = HF_VERSION_QWC_EPOSE)
  {
    cryptonote::transaction miner = coinbase(height, coinbase_amount);
    const coordinator_block_v2 block{
        major_version,
        height,
        source.hash_for(height),
        height == 0 ? crypto::null_hash : source.hash_for(height - 1),
        1000,
        25,
        &miner,
        {}};
    return coordinator.connect_block(block, source, result);
  }

  coordinator_status_v2 connect_with_transactions(
      consensus_coordinator_v2 &coordinator,
      const contexts &source,
      uint64_t height,
      cryptonote::transaction &miner,
      const std::vector<const cryptonote::transaction *> &transactions,
      coordinator_result_v2 &result)
  {
    const coordinator_block_v2 block{
        HF_VERSION_QWC_EPOSE,
        height,
        source.hash_for(height),
        height == 0 ? crypto::null_hash : source.hash_for(height - 1),
        1000,
        25,
        &miner,
        transactions};
    return coordinator.connect_block(block, source, result);
  }

  void connect_through(
      consensus_coordinator_v2 &coordinator,
      const contexts &source,
      uint64_t last_height)
  {
    for (uint64_t height = 0; height <= last_height; ++height)
    {
      coordinator_result_v2 result{};
      ASSERT_EQ(coordinator_status_v2::accepted,
          connect_empty(coordinator, source, height, 1025, result))
          << "height " << height;
    }
  }
}

TEST(epose_coordinator_v2, incomplete_or_wrong_version_configuration_fails_closed)
{
  consensus_parameters_v2 invalid = parameters(empty_qualification_policy_v2::miner_fallback);
  invalid.committee.committee_size = 0;
  consensus_coordinator_v2 disabled(invalid);
  EXPECT_FALSE(disabled.valid());
  invalid = parameters(empty_qualification_policy_v2::miner_fallback);
  invalid.empty_policy = static_cast<empty_qualification_policy_v2>(255);
  consensus_coordinator_v2 unknown_policy(invalid);
  EXPECT_FALSE(unknown_policy.valid());

  const consensus_parameters_v2 valid = parameters(empty_qualification_policy_v2::miner_fallback);
  consensus_coordinator_v2 coordinator(valid);
  ASSERT_TRUE(coordinator.valid());
  contexts source{};
  coordinator_result_v2 result{};
  EXPECT_EQ(coordinator_status_v2::invalid_block,
      connect_empty(coordinator, source, 0, 1025, result, 16));
  EXPECT_EQ(coordinator_status_v2::invalid_block,
      connect_empty(coordinator, source, 0, 1025, result, 18));
  EXPECT_EQ(crypto::null_hash, result.state_hash);
}

TEST(epose_coordinator_v2, genesis_bootstrap_and_empty_miner_fallback_are_deterministic)
{
  consensus_coordinator_v2 coordinator(
      parameters(empty_qualification_policy_v2::miner_fallback));
  contexts source{};
  connect_through(coordinator, source, 1439);

  coordinator_result_v2 result{};
  ASSERT_EQ(coordinator_status_v2::accepted,
      connect_empty(coordinator, source, 1440, 1025, result));
  EXPECT_FALSE(result.has_service_payee);
  EXPECT_EQ(1000u, result.reward.miner_subsidy);
  EXPECT_EQ(25u, result.reward.miner_fees);
  EXPECT_EQ(0u, result.reward.service_reward);
  EXPECT_EQ(1025u, result.reward.coinbase_total);
  EXPECT_EQ(1u, result.state_commitment_schema);
  EXPECT_NE(crypto::null_hash, result.state_hash);
}

TEST(epose_coordinator_v2, empty_nonissuance_changes_only_post_bootstrap_allocation)
{
  consensus_coordinator_v2 coordinator(
      parameters(empty_qualification_policy_v2::permanent_nonissuance));
  contexts source{};
  connect_through(coordinator, source, 1439);

  coordinator_result_v2 result{};
  EXPECT_EQ(coordinator_status_v2::invalid_coinbase_amount,
      connect_empty(coordinator, source, 1440, 1025, result));
  EXPECT_EQ(crypto::null_hash, result.state_hash);

  ASSERT_EQ(coordinator_status_v2::accepted,
      connect_empty(coordinator, source, 1440, 925, result));
  EXPECT_EQ(900u, result.reward.miner_subsidy);
  EXPECT_EQ(100u, result.reward.permanently_unissued);
  EXPECT_EQ(1000u, result.reward.emission_advance);
  EXPECT_EQ(900u, result.reward.issued_subsidy);
  EXPECT_EQ(925u, result.reward.coinbase_total);
}

TEST(epose_coordinator_v2, settled_qualification_controls_actual_coinbase_payment)
{
  const consensus_parameters_v2 configuration =
      parameters(empty_qualification_policy_v2::miner_fallback);
  consensus_coordinator_v2 coordinator(configuration);
  contexts source{};
  coordinator_result_v2 result{};
  ASSERT_EQ(coordinator_status_v2::accepted,
      connect_empty(coordinator, source, 0, 1025, result));

  std::vector<enrollment> members{
      make_enrollment(configuration, "endpoint-a"),
      make_enrollment(configuration, "endpoint-b"),
      make_enrollment(configuration, "endpoint-c")};
  std::vector<envelope_record_v2> enrollment_records;
  for (const enrollment &member : members)
  {
    enrollment_records.push_back(lifecycle_record(configuration, member));
    enrollment_records.push_back(admission_record(configuration, member));
  }
  cryptonote::transaction enrollment_tx =
      transaction_with(configuration, enrollment_records);
  cryptonote::transaction miner = coinbase(1, 1025);
  ASSERT_EQ(coordinator_status_v2::accepted,
      connect_with_transactions(
          coordinator, source, 1, miner, {&enrollment_tx}, result));
  for (uint64_t height = 2; height <= 719; ++height)
    ASSERT_EQ(coordinator_status_v2::accepted,
        connect_empty(coordinator, source, height, 1025, result));

  const membership_snapshot_v2 *snapshot =
      coordinator.state().membership().snapshot(1);
  ASSERT_NE(nullptr, snapshot);
  ASSERT_EQ(3u, snapshot->members.size());
  const enrollment &subject = members.front();
  const auto committee = coordinator.state().membership().committee(
      1, 0, subject.service.public_key, source.hash_for(660));
  ASSERT_EQ(2u, committee.size());
  std::vector<envelope_record_v2> receipt_records;
  const receipt_context_v2 receipt_context{
      configuration.nettype, configuration.genesis_hash,
      configuration.parameter_set_hash};
  for (const verifier_assignment_v2 &assignment : committee)
  {
    const auto verifier = std::find_if(
        members.begin(), members.end(), [&](const enrollment &candidate) {
          return candidate.service.public_key == assignment.verifier_public_key;
        });
    ASSERT_NE(members.end(), verifier);
    authenticated_service_receipt_v2 receipt{};
    receipt.challenge.epoch = 1;
    receipt.challenge.round = 0;
    receipt.challenge.snapshot_hash = snapshot->snapshot_hash;
    receipt.challenge.anchor_hash = source.hash_for(660);
    receipt.challenge.subject_public_key = subject.service.public_key;
    receipt.challenge.verifier_public_key = verifier->service.public_key;
    receipt.challenge.endpoint_descriptor_hash =
        subject.descriptor.endpoint_descriptor_hash;
    receipt.challenge.nonce = hash_text(
        "nonce-" + std::to_string(receipt_records.size()));
    receipt.challenge.requested_object_hash = hash_text("canonical-object");
    receipt.response_object_hash = receipt.challenge.requested_object_hash;
    sign_subject_response_v2(
        receipt, subject.service.secret_key, receipt_context);
    sign_verifier_receipt_v2(
        receipt, verifier->service.secret_key, receipt_context);
    envelope_record_v2 record{};
    ASSERT_EQ(record_codec_status_v2::accepted,
        encode_service_receipt_record_v2(receipt, receipt_context, record));
    receipt_records.push_back(record);
  }
  cryptonote::transaction receipt_tx =
      transaction_with(configuration, receipt_records);
  miner = coinbase(720, 1025);
  ASSERT_EQ(coordinator_status_v2::accepted,
      connect_with_transactions(
          coordinator, source, 720, miner, {&receipt_tx}, result));
  for (uint64_t height = 721; height <= 1439; ++height)
    ASSERT_EQ(coordinator_status_v2::accepted,
        connect_empty(coordinator, source, height, 1025, result));

  const qualification_set_v2 *qualification =
      coordinator.state().membership().qualification(1);
  ASSERT_NE(nullptr, qualification);
  ASSERT_EQ(1u, qualification->qualified_nodes.size());
  EXPECT_EQ(subject.service.public_key, qualification->qualified_nodes.front());

  crypto::public_key selected{};
  ASSERT_EQ(reward_status_v2::accepted,
      select_service_payee_v2(
          *qualification, source.hash_for(1380), configuration.genesis_hash,
          configuration.parameter_set_hash, configuration.timing,
          2, 1440, selected));
  ASSERT_EQ(subject.service.public_key, selected);

  crypto::public_key transaction_public{};
  crypto::secret_key transaction_secret{};
  crypto::generate_keys(transaction_public, transaction_secret);
  miner = coinbase(1440, 925);
  ASSERT_TRUE(cryptonote::add_tx_pub_key_to_extra(miner, transaction_public));
  crypto::key_derivation derivation{};
  ASSERT_TRUE(crypto::generate_key_derivation(
      subject.descriptor.reward_address.m_view_public_key,
      transaction_secret, derivation));
  cryptonote::txout_to_key service_target{};
  ASSERT_TRUE(crypto::derive_public_key(
      derivation, miner.vout.size(),
      subject.descriptor.reward_address.m_spend_public_key,
      service_target.key));
  cryptonote::tx_out service_output{100, service_target};
  miner.vout.push_back(service_output);

  const service_payment_expectation_v2 expected{
      configuration.nettype,
      configuration.genesis_hash,
      configuration.parameter_set_hash,
      1440,
      source.hash_for(1439),
      2,
      qualification->qualification_hash,
      selected,
      subject.descriptor.reward_address,
      100};
  service_payment_context_v2 generated{};
  ASSERT_EQ(reward_status_v2::accepted,
      append_coinbase_service_payment_proof_v2(
          miner, transaction_secret, HF_VERSION_QWC_EPOSE,
          configuration.limits.max_envelopes_per_transaction,
          configuration.limits.envelope, expected, generated));

  cryptonote::transaction redirected = miner;
  ++redirected.vout.back().amount;
  EXPECT_EQ(coordinator_status_v2::invalid_coinbase_amount,
      connect_with_transactions(
          coordinator, source, 1440, redirected, {}, result));
  EXPECT_EQ(crypto::null_hash, result.state_hash);

  ASSERT_EQ(coordinator_status_v2::accepted,
      connect_with_transactions(
          coordinator, source, 1440, miner, {}, result));
  EXPECT_TRUE(result.has_service_payee);
  EXPECT_EQ(100u, result.reward.service_reward);
  EXPECT_EQ(selected, result.payment.payee_service_public_key);
  EXPECT_EQ(1u, result.payment.outputs.size());
  EXPECT_EQ(100u, result.payment.outputs.front().amount);
}
