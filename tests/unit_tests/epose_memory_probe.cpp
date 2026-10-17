// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

// Deterministic, synthetic replay/undo workload. This is NOT a mainnet RSS
// forecast. Build the identical file with EPOSE_MEMORY_BASELINE on the pinned
// reference revision to compare canonical traces and process peak RSS.
#include <array>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "epose/block_transition_v2.h"
#include "epose/record_codec_v2.h"
#include "string_tools.h"
#include "../crypto/crypto-tests.h"

namespace
{
  using namespace qwertycoin::epose;
  void require(bool ok) { if (!ok) throw std::runtime_error("EPoSE memory probe invariant failed"); }
  crypto::hash hash(const std::string &s) { return crypto::cn_fast_hash(s.data(), s.size()); }
  const crypto::hash genesis = hash("epose-memory-genesis");
  const crypto::hash parameters = hash("epose-memory-parameters");
  const epoch_timing_v2 timing{0, 18, 3};
  const admission_policy_v2 admission_policy{admission_work_algorithm_v2::randomx, 1};
  const committee_policy_v2 committee{9, 6, 3, 2, 1, {0, 5, 10}, 100};

  struct key_pair { crypto::public_key pub{}; crypto::secret_key sec{}; };
  key_pair key(const std::string &label)
  {
    key_pair result;
    crypto::secret_key seed{};
    const auto digest = hash(label);
    static_assert(sizeof(seed) == sizeof(digest), "key seed size");
    std::memcpy(seed.data, &digest, sizeof(digest));
    crypto::generate_keys(result.pub, result.sec, seed, true);
    return result;
  }
  struct identity
  {
    key_pair service, operator_key;
    identity_descriptor_v2 descriptor{};
  };
  class contexts final : public canonical_context_source_v2
  {
  public:
    std::map<uint64_t, crypto::hash> blocks;
    bool block_hash(uint64_t height, crypto::hash &out) const override
    {
      const auto found = blocks.find(height);
      if (found == blocks.end()) return false;
      out = found->second;
      return true;
    }
    bool round_anchor(uint64_t epoch, uint64_t round, crypto::hash &out) const override
    {
      return block_hash(round == 0 ? epoch * 18 - 3 : epoch * 18 + round * 5, out);
    }
  };
  envelope_record_v2 registration(const identity &member)
  {
    lifecycle_record_v2 lifecycle{};
    lifecycle.next_descriptor = member.descriptor;
    require(sign_lifecycle_record_v2(cryptonote::TESTNET, genesis, parameters, lifecycle,
        member.operator_key.sec, member.service.sec));
    envelope_record_v2 out{};
    require(encode_lifecycle_record_v2(lifecycle, cryptonote::TESTNET, genesis, parameters, out)
        == record_codec_status_v2::accepted);
    return out;
  }
  envelope_record_v2 admission(const identity &member, uint64_t epoch, const crypto::hash &anchor)
  {
    admission_lease_v2 lease{};
    lease.member.service_public_key = member.service.pub;
    lease.member.identity_id = member.descriptor.identity_id;
    lease.member.operator_authorization_public_key = member.operator_key.pub;
    lease.member.descriptor_hash = hash_identity_descriptor_v2(
        cryptonote::TESTNET, genesis, parameters, member.descriptor);
    lease.member.reward_binding_hash = hash_reward_binding_v2(
        cryptonote::TESTNET, genesis, parameters, member.descriptor.reward_address);
    lease.member.endpoint_descriptor_hash = member.descriptor.endpoint_descriptor_hash;
    lease.target_epoch = epoch;
    lease.work_algorithm = static_cast<uint8_t>(admission_policy.algorithm);
    lease.leading_zero_bits = admission_policy.leading_zero_bits;
    lease.admission_context_height = (epoch - 1) * 18;
    lease.admission_context_hash = anchor;
    const admission_context_v2 context{
        cryptonote::TESTNET, genesis, parameters, lease.admission_context_height, anchor};
    do { lease.work_hash = calculate_admission_work_v2(lease, context); ++lease.nonce; }
    while (!admission_work_meets_target_v2(lease.work_hash, lease.leading_zero_bits));
    --lease.nonce;
    lease.lease_hash = calculate_admission_lease_hash_v2(lease, context);
    envelope_record_v2 out{};
    require(encode_admission_lease_record_v2(lease, context, admission_policy, out)
        == record_codec_status_v2::accepted);
    return out;
  }
}

int main(int argc, char **argv)
{
  try
  {
    // Test-only deterministic RNG, already used by cncrypto-tests. Never linked
    // into production targets; all identities and signatures here are synthetic.
    setup_random();
    const uint64_t epochs = argc > 1 ? std::stoull(argv[1]) : 32;
    require(epochs >= 2 && epochs <= 1000);
    block_transition_limits_v2 limits{};
    limits.max_envelopes_per_transaction = 1;
    limits.envelope = {65536, 256, 65536, 2048, 8, {0, 1, 1, 1, 1, 1}};
    limits.block = {262144, 1024, 2048, 8};
    limits.max_recent_undo_blocks = 2160;
    block_transition_v2 chain(cryptonote::TESTNET, genesis, parameters, timing,
        admission_policy, committee, limits);
    require(chain.valid());
    std::vector<identity> members;
    for (size_t i = 0; i < 4; ++i)
    {
      const auto label = "memory-node-" + std::to_string(i);
      identity member{};
      member.service = key(label + "service");
      member.operator_key = key(label + "operator");
      auto &d = member.descriptor;
      d.identity_id = derive_identity_id_v2(cryptonote::TESTNET, genesis, parameters, member.operator_key.pub);
      d.service_public_key = member.service.pub;
      d.operator_authorization_public_key = member.operator_key.pub;
      d.reward_address = {key(label + "spend").pub, key(label + "view").pub};
      d.endpoint_descriptor_hash = hash(label + "endpoint");
      d.effective_epoch = 1;
      d.expiry_epoch = epochs + 10;
      members.push_back(member);
    }
    contexts source;
    crypto::hash trace{};
    std::vector<crypto::hash> state_hashes;
    const uint64_t last_height = (epochs + 1) * 18 - 4;
    for (uint64_t height = 0; height <= last_height; ++height)
    {
      std::vector<envelope_record_v2> records;
      const uint64_t epoch = height / 18;
      const uint64_t offset = height % 18;
      if (height == 1)
        for (const auto &member : members) records.push_back(registration(member));
      if (offset == 1 && epoch < epochs)
        for (const auto &member : members)
          records.push_back(admission(member, epoch + 1, source.blocks.at(epoch * 18)));
      if (epoch > 0 && (offset == 0 || offset == 6 || offset == 11))
      {
        const uint64_t round = offset == 0 ? 0 : offset == 6 ? 1 : 2;
        crypto::hash anchor{};
        require(source.round_anchor(epoch, round, anchor));
        for (const auto &subject : members)
          for (const auto &assignment : chain.state().membership().committee(epoch, round, subject.service.pub, anchor))
          {
            const identity *verifier = nullptr;
            for (const auto &member : members)
              if (member.service.pub == assignment.verifier_public_key) verifier = &member;
            require(verifier != nullptr);
            authenticated_service_receipt_v2 receipt{};
            receipt.challenge.epoch = epoch;
            receipt.challenge.round = round;
            receipt.challenge.snapshot_hash = chain.state().membership().snapshot(epoch)->snapshot_hash;
            receipt.challenge.anchor_hash = anchor;
            receipt.challenge.subject_public_key = subject.service.pub;
            receipt.challenge.verifier_public_key = verifier->service.pub;
            receipt.challenge.endpoint_descriptor_hash = subject.descriptor.endpoint_descriptor_hash;
            receipt.challenge.nonce = hash("nonce-" + std::to_string(height));
            receipt.challenge.requested_object_hash = anchor;
            receipt.response_object_hash = anchor;
            const receipt_context_v2 context{cryptonote::TESTNET, genesis, parameters};
            sign_subject_response_v2(receipt, subject.service.sec, context);
            sign_verifier_receipt_v2(receipt, verifier->service.sec, context);
            envelope_record_v2 record{};
            require(encode_service_receipt_record_v2(receipt, context, record) == record_codec_status_v2::accepted);
            records.push_back(record);
          }
      }
      cryptonote::transaction miner{}, transaction{};
      envelope_budget_v2 budget{};
      if (!records.empty())
        require(append_transaction_envelope_v2(records, 17, 1, limits.envelope, transaction.extra, budget)
            == envelope_status_v2::accepted);
      source.blocks[height] = height == 0 ? genesis : hash("memory-block-" + std::to_string(height));
      block_apply_summary_v2 summary{};
      require(chain.apply_block(17, height, source.blocks.at(height),
          {{&miner, true, nullptr}, {&transaction, false, nullptr}}, source, summary)
          == block_transition_status_v2::accepted);
      const auto state_hash = chain.state().state_hash();
      state_hashes.push_back(state_hash);
      std::array<crypto::hash, 2> trace_input{{trace, state_hash}};
      trace = crypto::cn_fast_hash(trace_input.data(), sizeof(trace_input));
      if (offset == 14 && epoch > 0)
      {
        require(chain.state().membership().qualification(epoch)->qualified_nodes.size() == members.size());
        std::cout << "{\"epoch\":" << epoch << ",\"receipts\":" << chain.state().membership().receipt_count();
#ifndef EPOSE_MEMORY_BASELINE
        const auto usage = chain.state().membership().memory_usage();
        std::cout << ",\"state_payload\":" << usage.vector_payload_bytes + usage.map_value_bytes
            + chain.state().lifecycle().memory_usage().vector_payload_bytes
            << ",\"undo_payload\":" << chain.undo_payload_bytes();
#endif
        std::cout << "}\n";
      }
    }
    // Disconnect across receipt, qualification and freeze blocks, then verify
    // every historical commitment, not just the final state.
    for (uint64_t height = last_height; height > last_height - 36; --height)
    {
      require(chain.disconnect_tip(height) == block_transition_status_v2::accepted);
      require(chain.state().state_hash() == state_hashes.at(height - 1));
    }
    std::cout << "{\"trace\":\"" << epee::string_tools::pod_to_hex(trace)
        << "\",\"blocks\":" << last_height + 1 << ",\"rollback_blocks\":36,\"state_hashes\":[";
    for (size_t i = 0; i < state_hashes.size(); ++i)
    {
      if (i != 0) std::cout << ',';
      std::cout << '"' << epee::string_tools::pod_to_hex(state_hashes[i]) << '"';
    }
    std::cout << "]}\n";
    return 0;
  }
  catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
