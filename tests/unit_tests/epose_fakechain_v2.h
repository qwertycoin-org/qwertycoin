// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

// A real Blockchain on FAKECHAIN at QWC-HF17 with EPoSE-v2 parameters.

#pragma once

#include <gtest/gtest.h>

#include <cstdlib>
#include <vector>

#include <boost/filesystem.hpp>

#include "blockchain_db/blockchain_db.h"
#include "blockchain_db/lmdb/db_lmdb.h"
#include "cryptonote_basic/account.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_config.h"
#include "cryptonote_core/blockchain.h"
#include "cryptonote_core/cryptonote_tx_utils.h"
#include "epose/coordinator_v2.h"
#include "misc_log_ex.h"

namespace epose_fakechain_v2
{
  // A fresh directory for the chain database. QWC_TEST_LOG, when set, names
  // the log categories to print.
  inline boost::filesystem::path temp_directory(const char *pattern)
  {
    if (const char *categories = std::getenv("QWC_TEST_LOG"))
    {
      mlog_configure("", true);
      mlog_set_log(categories);
    }
    const boost::filesystem::path directory =
        boost::filesystem::temp_directory_path() / boost::filesystem::unique_path(pattern);
    boost::filesystem::create_directories(directory);
    return directory;
  }

  inline const cryptonote::test_options &options()
  {
    static const std::pair<uint8_t, uint64_t> hard_forks[] = {
        {HF_VERSION_QWC_EPOSE, 0}, {0, 0}};
    static const cryptonote::test_options out{hard_forks, 0};
    return out;
  }

  inline crypto::hash genesis_hash()
  {
    cryptonote::block genesis{};
    EXPECT_TRUE(cryptonote::generate_genesis_block(
        genesis, cryptonote::get_config(cryptonote::FAKECHAIN).GENESIS_TX,
        cryptonote::get_config(cryptonote::FAKECHAIN).GENESIS_NONCE,
        HF_VERSION_QWC_EPOSE, HF_VERSION_QWC_EPOSE));
    return cryptonote::get_block_hash(genesis);
  }

  inline qwertycoin::epose::consensus_parameters_v2 parameters()
  {
    using namespace qwertycoin::epose;
    consensus_parameters_v2 out{};
    out.nettype = cryptonote::FAKECHAIN;
    out.genesis_hash = genesis_hash();
    crypto::cn_fast_hash("blockchain-tip-view-parameters", 30, out.parameter_set_hash);
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
    out.empty_policy = empty_qualification_policy_v2::miner_fallback;
    out.state_commitment_schema = 1;
    return out;
  }

  // LMDB with faults the test switches on.
  class faulty_lmdb final : public cryptonote::BlockchainLMDB
  {
  public:
    bool fail_commitment_reads = false;
    // Reads refused while fail_commitment_reads is set; each failed replay
    // stops at its first refused read.
    mutable unsigned failed_commitment_reads = 0;
    // pop_block throws once, after this many more pops; negative never throws.
    int pops_before_throw = -1;
    // add_block throws once, after this many more adds; negative never throws.
    int adds_before_throw = -1;

    bool get_epose_state_commitment_v2(uint64_t height,
        cryptonote::epose_state_commitment_v2 &commitment) const override
    {
      if (fail_commitment_reads)
      {
        ++failed_commitment_reads;
        return false;
      }
      return BlockchainLMDB::get_epose_state_commitment_v2(height, commitment);
    }

    uint64_t add_block(const std::pair<cryptonote::block, cryptonote::blobdata> &blk,
        size_t block_weight, uint64_t long_term_block_weight,
        const cryptonote::difficulty_type &cumulative_difficulty,
        const uint64_t &coins_generated,
        const std::vector<std::pair<cryptonote::transaction, cryptonote::blobdata>> &txs,
        const cryptonote::epose_state_commitment_v2 *epose_commitment = nullptr) override
    {
      if (adds_before_throw == 0)
      {
        adds_before_throw = -1;
        throw cryptonote::DB_ERROR("injected add failure");
      }
      if (adds_before_throw > 0)
        --adds_before_throw;
      return BlockchainLMDB::add_block(blk, block_weight, long_term_block_weight,
          cumulative_difficulty, coins_generated, txs, epose_commitment);
    }

    void pop_block(cryptonote::block &blk, std::vector<cryptonote::transaction> &txs) override
    {
      if (pops_before_throw == 0)
      {
        pops_before_throw = -1;
        throw cryptonote::DB_ERROR("injected pop failure");
      }
      if (pops_before_throw > 0)
        --pops_before_throw;
      BlockchainLMDB::pop_block(blk, txs);
    }
  };

  // A relay policy that fits parameters(): FAKECHAIN has no compiled one,
  // and without a policy the chain runs no relay pool.
  inline qwertycoin::epose::relay_policy_v2 relay_policy()
  {
    return {
        qwertycoin::epose::relay_queue_limits_v2{64, 65536, 16, 48, 16384, 49152},
        qwertycoin::epose::relay_template_limits_v2{8, 4096, 2, 6, 1024, 3072},
        0};
  }

  // `parameters` must outlive `bc`, which takes ownership of `db`.
  inline bool init(cryptonote::Blockchain &bc, const boost::filesystem::path &directory,
      const qwertycoin::epose::consensus_parameters_v2 &parameters,
      cryptonote::BlockchainDB *db = nullptr,
      const qwertycoin::epose::relay_policy_v2 *relay_policy = nullptr)
  {
    if (db == nullptr)
      db = cryptonote::new_db();
    if (db == nullptr)
      return false;
    db->open(directory.string(), DBF_FAST);
    return bc.init(db, cryptonote::FAKECHAIN, true, &options(), 1,
        nullptr, &parameters, relay_policy);
  }

  // A miner template on the current tip; difficulty is fixed at 1.
  inline cryptonote::block template_on_tip(
      cryptonote::Blockchain &bc, const cryptonote::account_base &miner)
  {
    cryptonote::block b{};
    cryptonote::difficulty_type difficulty = 0;
    uint64_t height = 0, reward = 0, seed_height = 0;
    crypto::hash seed_hash{};
    EXPECT_TRUE(bc.create_block_template(b, miner.get_keys().m_account_address,
        difficulty, height, reward, cryptonote::blobdata(), seed_height, seed_hash));
    EXPECT_EQ(1u, difficulty);
    return b;
  }

  // One block inside the batch core::handle_incoming_blocks opens around
  // each add.
  inline bool add_block(cryptonote::Blockchain &bc, const cryptonote::block &b,
      cryptonote::block_verification_context &bvc)
  {
    std::vector<cryptonote::block_complete_entry> entries(1);
    entries.front().block = cryptonote::block_to_blob(b);
    std::vector<cryptonote::block> parsed;
    if (!bc.prepare_handle_incoming_blocks(entries, parsed))
      return false;
    const bool added = bc.add_new_block(b, bvc);
    return bc.cleanup_handle_incoming_blocks(true) && added;
  }
}
