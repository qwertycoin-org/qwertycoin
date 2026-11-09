// Copyright (c) 2014-2022, The Monero Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Parts of this file are originally copyright (c) 2012-2013 The Cryptonote developers

#include "epose/record_codec_v2.h"
#include <algorithm>
#include <exception>
#include <cstdio>
#include <iterator>
#include <unordered_set>
#include <chrono>
#include <thread>
#include <boost/asio/dispatch.hpp>
#include <boost/filesystem.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/format.hpp>
#include <boost/scope_exit.hpp>

#include "include_base_utils.h"
#include "cryptonote_basic/cryptonote_basic_impl.h"
#include "tx_pool.h"
#include "blockchain.h"
#include "hdr_info_hf18.h"
#include "blockchain_db/blockchain_db.h"
#include "cryptonote_basic/cryptonote_boost_serialization.h"
#include "cryptonote_basic/height_window.h"
#include "cryptonote_basic/events.h"
#include "cryptonote_config.h"
#include "cryptonote_basic/miner.h"
#include "hardforks/hardforks.h"
#include "misc_language.h"
#include "profile_tools.h"
#include "file_io_utils.h"
#include "int-util.h"
#include "common/threadpool.h"
#include "common/boost_serialization_helper.h"
#include "warnings.h"
#include "crypto/hash.h"
#include "cryptonote_core.h"
#include "alternative_child_index.h"
#include "ringct/rctSigs.h"
#include "common/perf_timer.h"
#include "common/notify.h"
#include "common/varint.h"
#include "common/pruning.h"
#include "common/data_cache.h"
#include "epose/service_node.h"
#include "epose/state_index_v2.h"
#include "time_helper.h"

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "blockchain"

#define FIND_BLOCKCHAIN_SUPPLEMENT_MAX_SIZE (100*1024*1024) // 100 MB

using namespace crypto;

namespace
{
  class epose_context_source final
      : public qwertycoin::epose::canonical_context_source_v2
  {
  public:
    epose_context_source(
        const qwertycoin::epose::consensus_parameters_v2 &parameters,
        std::function<bool(uint64_t, crypto::hash &)> block_hash)
      : parameters_(parameters), block_hash_(std::move(block_hash)) {}

    bool block_hash(uint64_t height, crypto::hash &hash) const override
    {
      return block_hash_(height, hash);
    }

    bool round_anchor(uint64_t epoch, uint64_t round, crypto::hash &hash) const override
    {
      if (round >= parameters_.committee.round_offsets.size())
        return false;
      uint64_t height = 0;
      if (round == 0)
      {
        if (!parameters_.timing.committee_anchor(epoch, height))
          return false;
      }
      else
      {
        if (!parameters_.timing.epoch_start(epoch, height)
            || height > std::numeric_limits<uint64_t>::max()
                - parameters_.committee.round_offsets[round])
          return false;
        height += parameters_.committee.round_offsets[round];
      }
      return block_hash(height, hash);
    }

  private:
    const qwertycoin::epose::consensus_parameters_v2 &parameters_;
    std::function<bool(uint64_t, crypto::hash &)> block_hash_;
  };

  epose_context_source main_chain_context_source(
      const cryptonote::Blockchain &blockchain,
      const qwertycoin::epose::consensus_parameters_v2 &parameters)
  {
    return epose_context_source(parameters,
        [&blockchain](uint64_t height, crypto::hash &hash) {
          hash = blockchain.get_block_id_by_height(height);
          return hash != crypto::null_hash;
        });
  }

  std::function<bool(uint64_t, bool, uint64_t &)> canonical_weight_reader(
      cryptonote::BlockchainDB &db)
  {
    return [&db](uint64_t index, bool long_term, uint64_t &weight) {
      weight = long_term ? db.get_block_long_term_weight(index)
          : db.get_block_weight(index);
      return true;
    };
  }

  uint64_t median_or_zero(std::vector<uint64_t> &weights)
  {
    return weights.empty() ? 0 : epee::misc_utils::median(weights);
  }

  template <typename LongTermMedian>
  bool block_reward_median_weight(uint8_t version, uint64_t short_median,
      LongTermMedian long_term_median, uint64_t &median_weight)
  {
    median_weight = short_median;
    if (version >= HF_VERSION_LONG_TERM_BLOCK_WEIGHT)
    {
      uint64_t long_median = 0;
      if (!long_term_median(long_median))
        return false;
      const uint64_t effective_long_median = std::max<uint64_t>(
          CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5, long_median);
      if (effective_long_median > std::numeric_limits<uint64_t>::max()
              / CRYPTONOTE_SHORT_TERM_BLOCK_WEIGHT_SURGE_FACTOR)
        return false;
      const uint64_t upper = CRYPTONOTE_SHORT_TERM_BLOCK_WEIGHT_SURGE_FACTOR
          * effective_long_median;
      median_weight = version >= HF_VERSION_2021_SCALING
          ? std::min<uint64_t>(
              std::max<uint64_t>(effective_long_median, short_median), upper)
          : std::min<uint64_t>(
              std::max<uint64_t>(CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5,
                  short_median), upper);
    }
    return true;
  }

  struct epose_block_inputs
  {
    uint64_t scheduled_subsidy = 0;
    uint64_t fees = 0;
    uint64_t mints = 0;
  };

  uint64_t constrained_long_term_block_weight(uint64_t block_weight,
      uint64_t long_term_median, uint8_t hf_version)
  {
    if (hf_version < HF_VERSION_LONG_TERM_BLOCK_WEIGHT)
      return block_weight;
    const uint64_t effective = std::max<uint64_t>(
        CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5, long_term_median);
    if (hf_version >= HF_VERSION_2021_SCALING)
    {
      block_weight = std::max<uint64_t>(block_weight, effective * 10 / 17);
      return std::min<uint64_t>(block_weight, effective + effective * 7 / 10);
    }
    return std::min<uint64_t>(block_weight, effective + effective * 2 / 5);
  }

  bool derive_epose_block_inputs(
      uint8_t version, uint64_t height, uint64_t block_weight,
      uint64_t already_generated, uint64_t mints, size_t long_window,
      const std::vector<const cryptonote::transaction *> &transactions,
      const std::function<bool(uint64_t, bool, uint64_t &)> &weight_at,
      epose_block_inputs &inputs)
  {
    inputs = {};
    inputs.mints = mints;
    for (const cryptonote::transaction *tx : transactions)
    {
      if (!tx)
        return false;
      const uint64_t fee = cryptonote::contract_miner_fee(
          *tx, cryptonote::get_tx_fee(*tx), version);
      if (inputs.fees > std::numeric_limits<uint64_t>::max() - fee)
        return false;
      inputs.fees += fee;
    }
    const uint64_t short_count = std::min<uint64_t>(
        height, CRYPTONOTE_REWARD_BLOCKS_WINDOW);
    std::vector<uint64_t> short_weights;
    short_weights.reserve(static_cast<size_t>(short_count));
    for (uint64_t index = height - short_count; index < height; ++index)
    {
      uint64_t weight = 0;
      if (!weight_at(index, false, weight))
        return false;
      short_weights.push_back(weight);
    }
    const uint64_t short_median = median_or_zero(short_weights);
    uint64_t median_weight = 0;
    if (!block_reward_median_weight(version, short_median,
            [&](uint64_t &long_median) {
              const uint64_t long_count = std::min<uint64_t>(height, long_window);
              std::vector<uint64_t> long_weights;
              long_weights.reserve(static_cast<size_t>(long_count));
              for (uint64_t index = height - long_count; index < height; ++index)
              {
                uint64_t weight = 0;
                if (!weight_at(index, true, weight))
                  return false;
                long_weights.push_back(weight);
              }
              long_median = median_or_zero(long_weights);
              return true;
            },
            median_weight))
      return false;
    median_weight = std::max<uint64_t>(median_weight,
        cryptonote::get_min_block_weight(version));
    return cryptonote::get_block_reward(median_weight, block_weight,
        already_generated, inputs.scheduled_subsidy, version);
  }

  class epose_rebuild_defer_guard
  {
  public:
    explicit epose_rebuild_defer_guard(bool &deferred)
      : deferred_(deferred), previous_(deferred)
    {
      deferred_ = true;
    }
    ~epose_rebuild_defer_guard()
    {
      deferred_ = previous_;
    }
  private:
    bool &deferred_;
    bool previous_;
  };
}

//#include "serialization/json_archive.h"

/* TODO:
 *  Clean up code:
 *    Possibly change how outputs are referred to/indexed in blockchain and wallets
 *
 */

using namespace cryptonote;
using epee::string_tools::pod_to_hex;
extern "C" void slow_hash_allocate_state();
extern "C" void slow_hash_free_state();

DISABLE_VS_WARNINGS(4267)

#define MERROR_VER(x) MCERROR("verify", x)

// used to overestimate the block reward when estimating a per kB to use
#define BLOCK_REWARD_OVERESTIMATE (10 * 1000000000000)

//------------------------------------------------------------------
Blockchain::Blockchain(tx_memory_pool& tx_pool) :
  m_db(), m_tx_pool(tx_pool), m_hardfork(NULL), m_timestamps_and_difficulties_height(0), m_reset_timestamps_and_difficulties_height(true), m_current_block_cumul_weight_limit(0), m_current_block_cumul_weight_median(0),
  m_enforce_dns_checkpoints(false), m_max_prepare_blocks_threads(4), m_db_sync_on_blocks(true), m_db_sync_threshold(1), m_db_sync_mode(db_async), m_db_default_sync(false), m_fast_sync(true), m_show_time_stats(false), m_sync_counter(0), m_bytes_to_sync(0), m_cancel(false),
  m_long_term_block_weights_window(CRYPTONOTE_LONG_TERM_BLOCK_WEIGHT_WINDOW_SIZE),
  m_long_term_effective_median_block_weight(0),
  m_long_term_block_weights_cache_tip_hash(crypto::null_hash),
  m_long_term_block_weights_cache_rolling_median(CRYPTONOTE_LONG_TERM_BLOCK_WEIGHT_WINDOW_SIZE),
  m_difficulty_for_next_block_top_hash(crypto::null_hash),
  m_difficulty_for_next_block(1),
  m_btc_valid(false),
  m_batch_success(true),
  m_prepare_height(0),
  m_rct_ver_cache(),
  m_epose_state(),
  m_epose_rebuild_timer(m_async_service)
{
  LOG_PRINT_L3("Blockchain::" << __func__);
}
//------------------------------------------------------------------
Blockchain::~Blockchain()
{
  try { deinit(); }
  catch (const std::exception &e) { /* ignore */ }
}
//------------------------------------------------------------------
bool Blockchain::have_tx(const crypto::hash &id) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  // WARNING: this function does not take m_blockchain_lock, and thus should only call read only
  // m_db functions which do not depend on one another (ie, no getheight + gethash(height-1), as
  // well as not accessing class members, even read only (ie, m_invalid_blocks). The caller must
  // lock if it is otherwise needed.
  return m_db->tx_exists(id);
}
//------------------------------------------------------------------
bool Blockchain::have_tx_keyimg_as_spent(const crypto::key_image &key_im) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  // WARNING: this function does not take m_blockchain_lock, and thus should only call read only
  // m_db functions which do not depend on one another (ie, no getheight + gethash(height-1), as
  // well as not accessing class members, even read only (ie, m_invalid_blocks). The caller must
  // lock if it is otherwise needed.
  return  m_db->has_key_image(key_im);
}
//------------------------------------------------------------------
// This function makes sure that each "input" in an input (mixins) exists
// and collects the public key for each from the transaction it was included in
// via the visitor passed to it.
template <class visitor_t>
bool Blockchain::scan_outputkeys_for_indexes(size_t tx_version, const txin_to_key& tx_in_to_key, visitor_t &vis, const crypto::hash &tx_prefix_hash, uint64_t* pmax_related_block_height) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);

  // ND: Disable locking and make method private.
  //CRITICAL_REGION_LOCAL(m_blockchain_lock);

  // verify that the input has key offsets (that it exists properly, really)
  if(!tx_in_to_key.key_offsets.size())
    return false;

  // cryptonote_format_utils uses relative offsets for indexing to the global
  // outputs list.  that is to say that absolute offset #2 is absolute offset
  // #1 plus relative offset #2.
  // TODO: Investigate if this is necessary / why this is done.
  std::vector<uint64_t> absolute_offsets = relative_output_offsets_to_absolute(tx_in_to_key.key_offsets);
  std::vector<output_data_t> outputs;

  bool found = false;
  auto it = m_scan_table.find(tx_prefix_hash);
  if (it != m_scan_table.end())
  {
    auto its = it->second.find(tx_in_to_key.k_image);
    if (its != it->second.end())
    {
      outputs = its->second;
      found = true;
    }
  }

  if (!found)
  {
    try
    {
      m_db->get_output_key(epee::span<const uint64_t>(&tx_in_to_key.amount, 1), absolute_offsets, outputs, true);
      if (absolute_offsets.size() != outputs.size())
      {
        MERROR_VER("Output does not exist! amount = " << tx_in_to_key.amount);
        return false;
      }
    }
    catch (...)
    {
      MERROR_VER("Output does not exist! amount = " << tx_in_to_key.amount);
      return false;
    }
  }
  else
  {
    // check for partial results and add the rest if needed;
    if (outputs.size() < absolute_offsets.size() && outputs.size() > 0)
    {
      MDEBUG("Additional outputs needed: " << absolute_offsets.size() - outputs.size());
      std::vector < uint64_t > add_offsets;
      std::vector<output_data_t> add_outputs;
      add_outputs.reserve(absolute_offsets.size() - outputs.size());
      for (size_t i = outputs.size(); i < absolute_offsets.size(); i++)
        add_offsets.push_back(absolute_offsets[i]);
      try
      {
        m_db->get_output_key(epee::span<const uint64_t>(&tx_in_to_key.amount, 1), add_offsets, add_outputs, true);
        if (add_offsets.size() != add_outputs.size())
        {
          MERROR_VER("Output does not exist! amount = " << tx_in_to_key.amount);
          return false;
        }
      }
      catch (...)
      {
        MERROR_VER("Output does not exist! amount = " << tx_in_to_key.amount);
        return false;
      }
      outputs.insert(outputs.end(), add_outputs.begin(), add_outputs.end());
    }
  }

  size_t count = 0;
  for (const uint64_t& i : absolute_offsets)
  {
    try
    {
      output_data_t output_index;
      try
      {
        // get tx hash and output index for output
        if (count < outputs.size())
          output_index = outputs.at(count);
        else
          output_index = m_db->get_output_key(tx_in_to_key.amount, i);

        // call to the passed boost visitor to grab the public key for the output
        if (!vis.handle_output(output_index.unlock_time, output_index.pubkey, output_index.commitment))
        {
          MERROR_VER("Failed to handle_output for output no = " << count << ", with absolute offset " << i);
          return false;
        }
      }
      catch (...)
      {
        MERROR_VER("Output does not exist! amount = " << tx_in_to_key.amount << ", absolute_offset = " << i);
        return false;
      }

      // if on last output and pmax_related_block_height not null pointer
      if(++count == absolute_offsets.size() && pmax_related_block_height)
      {
        // set *pmax_related_block_height to tx block height for this output
        auto h = output_index.height;
        if(*pmax_related_block_height < h)
        {
          *pmax_related_block_height = h;
        }
      }

    }
    catch (const OUTPUT_DNE& e)
    {
      MERROR_VER("Output does not exist: " << e.what());
      return false;
    }
    catch (const TX_DNE& e)
    {
      MERROR_VER("Transaction does not exist: " << e.what());
      return false;
    }

  }

  return true;
}
//------------------------------------------------------------------
crypto::hash Blockchain::get_vm_state_root() const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  return m_vm_state.get_root();
}
//------------------------------------------------------------------
uint64_t Blockchain::get_current_blockchain_height() const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  // WARNING: this function does not take m_blockchain_lock, and thus should only call read only
  // m_db functions which do not depend on one another (ie, no getheight + gethash(height-1), as
  // well as not accessing class members, even read only (ie, m_invalid_blocks). The caller must
  // lock if it is otherwise needed.
  return m_db->height();
}
//------------------------------------------------------------------
//FIXME: possibly move this into the constructor, to avoid accidentally
//       dereferencing a null BlockchainDB pointer
bool Blockchain::init(BlockchainDB* db, const network_type nettype, bool offline, const cryptonote::test_options *test_options, difficulty_type fixed_difficulty, const GetCheckpointsCallback& get_checkpoints/* = nullptr*/, const qwertycoin::epose::consensus_parameters_v2 *epose_test_parameters/* = nullptr*/, const qwertycoin::epose::relay_policy_v2 *epose_test_relay_policy/* = nullptr*/)
{
  LOG_PRINT_L3("Blockchain::" << __func__);

  CHECK_AND_ASSERT_MES(nettype != FAKECHAIN || test_options, false, "fake chain network type used without options");

  CRITICAL_REGION_LOCAL(m_tx_pool);
  CRITICAL_REGION_LOCAL1(m_blockchain_lock);

  if (db == nullptr)
  {
    LOG_ERROR("Attempted to init Blockchain with null DB");
    return false;
  }
  if (!db->is_open())
  {
    LOG_ERROR("Attempted to init Blockchain with unopened DB");
    delete db;
    return false;
  }

  m_db = db;

  m_nettype = test_options != NULL ? FAKECHAIN : nettype;
  m_offline = offline;
  m_fixed_difficulty = fixed_difficulty;
  m_epose_state.reset();
  m_epose_block_snapshots.clear();
  m_epose_v2.reset();
  m_epose_v2_parameters = {};
  m_epose_v2_relay_pool.reset();
  m_epose_v2_relay_policy = {};
  m_epose_rebuild_deferred = false;
  m_epose_rebuild_retry_scheduled = false;
  m_epose_rebuild_fatal = false;
  m_epose_switch_busy = false;
  m_epose_switch_mutated = false;
  m_epose_retry_best_chain = false;
  m_epose_retry_chain_attempts.clear();
  m_epose_retry_exhausted_tip = crypto::null_hash;
  ++m_epose_state_validation_generation;
  m_epose_state_validation_height = std::numeric_limits<uint64_t>::max();
  m_epose_rebuild_attempts = 0;
  m_epose_rebuild_shutdown = false;
  m_async_service.restart();
  if (m_hardfork == nullptr)
  {
    if (m_nettype ==  FAKECHAIN)
    {
      // A FAKECHAIN fixture may intentionally start at a historical version,
      // while QWC regtest is HF17-native just like the public chain. Seed the
      // hard-fork controller from the fixture's declared height-zero version
      // instead of unconditionally forcing Monero v1.
      const uint8_t initial_version = test_options != nullptr
          && test_options->hard_forks != nullptr
          && test_options->hard_forks[0].first != 0
          && test_options->hard_forks[0].second == 0
          ? test_options->hard_forks[0].first
          : 1;
      m_hardfork = new HardFork(*db, initial_version, 0);
    }
    else if (m_nettype == DEVNET)
      m_hardfork = new HardFork(*db, HF_VERSION_QWC_EPOSE, 0);
    else if (m_nettype == STAGENET)
      m_hardfork = new HardFork(*db, HF_VERSION_QWC_EPOSE, 0);
    else if (m_nettype == TESTNET)
      m_hardfork = new HardFork(*db, HF_VERSION_QWC_EPOSE, 0);
    else
      m_hardfork = new HardFork(*db, HF_VERSION_QWC_EPOSE, 0);
  }
  if (m_nettype == FAKECHAIN)
  {
    for (size_t n = 0; test_options->hard_forks[n].first; ++n)
      m_hardfork->add_fork(test_options->hard_forks[n].first, test_options->hard_forks[n].second, 0, n + 1);
  }
  else if (m_nettype == TESTNET)
  {
    for (size_t n = 0; n < num_testnet_hard_forks; ++n)
      m_hardfork->add_fork(testnet_hard_forks[n].version, testnet_hard_forks[n].height, testnet_hard_forks[n].threshold, testnet_hard_forks[n].time);
  }
  else if (m_nettype == DEVNET)
  {
    for (size_t n = 0; n < num_devnet_hard_forks; ++n)
      m_hardfork->add_fork(devnet_hard_forks[n].version, devnet_hard_forks[n].height, devnet_hard_forks[n].threshold, devnet_hard_forks[n].time);
  }
  else if (m_nettype == STAGENET)
  {
    for (size_t n = 0; n < num_stagenet_hard_forks; ++n)
      m_hardfork->add_fork(stagenet_hard_forks[n].version, stagenet_hard_forks[n].height, stagenet_hard_forks[n].threshold, stagenet_hard_forks[n].time);
  }
  else
  {
    for (size_t n = 0; n < num_mainnet_hard_forks; ++n)
      m_hardfork->add_fork(mainnet_hard_forks[n].version, mainnet_hard_forks[n].height, mainnet_hard_forks[n].threshold, mainnet_hard_forks[n].time);
  }
  m_hardfork->init();

  m_db->set_hard_fork(m_hardfork);

  // Public QWC-HF17 has no legacy EPoSE fallback. A complete, compiled
  // parameter set must exist before genesis can be accepted. Unit/core tests
  // may inject an explicit fixture only on FAKECHAIN.
  if (m_hardfork->get_ideal_version(0) == HF_VERSION_QWC_EPOSE)
  {
    block genesis_preview{};
    crypto::hash genesis_hash{};
    if (m_db->height() == 0)
    {
      generate_genesis_block(
          genesis_preview, get_config(m_nettype).GENESIS_TX,
          get_config(m_nettype).GENESIS_NONCE,
          HF_VERSION_QWC_EPOSE, HF_VERSION_QWC_EPOSE);
      genesis_hash = get_block_hash(genesis_preview);
    }
    else
      genesis_hash = m_db->get_block_hash_from_height(0);

    bool have_parameters = false;
    if (epose_test_parameters != nullptr)
    {
      CHECK_AND_ASSERT_MES(m_nettype == FAKECHAIN, false,
          "EPoSE test parameters are restricted to FAKECHAIN");
      m_epose_v2_parameters = *epose_test_parameters;
      have_parameters = true;
    }
    else if (m_nettype != FAKECHAIN)
    {
      have_parameters = qwertycoin::epose::compiled_consensus_parameters_v2(
          m_nettype, genesis_hash, m_epose_v2_parameters);
    }

    if (!have_parameters)
    {
      if (m_nettype != FAKECHAIN)
      {
        LOG_ERROR("QWC-HF17/EPoSE-v2 launch parameters are incomplete; refusing to initialize a public chain");
        return false;
      }
    }
    else
    {
      CHECK_AND_ASSERT_MES(
          m_epose_v2_parameters.valid()
              && m_epose_v2_parameters.nettype == m_nettype
              && m_epose_v2_parameters.genesis_hash == genesis_hash,
          false, "Invalid or genesis-mismatched EPoSE-v2 consensus parameters");
      m_epose_v2.reset(new qwertycoin::epose::consensus_coordinator_v2(
          m_epose_v2_parameters));
      CHECK_AND_ASSERT_MES(m_epose_v2->valid(), false,
          "Failed to initialize EPoSE-v2 consensus coordinator");

      bool have_relay_policy = false;
      if (epose_test_relay_policy != nullptr)
      {
        CHECK_AND_ASSERT_MES(m_nettype == FAKECHAIN, false,
            "EPoSE relay test policy is restricted to FAKECHAIN");
        m_epose_v2_relay_policy = *epose_test_relay_policy;
        have_relay_policy = true;
      }
      else if (m_nettype != FAKECHAIN)
      {
        have_relay_policy = qwertycoin::epose::compiled_relay_policy_v2(
            m_nettype, genesis_hash, m_epose_v2_parameters.parameter_set_hash,
            m_epose_v2_relay_policy);
      }
      if (have_relay_policy)
      {
        CHECK_AND_ASSERT_MES(
            m_epose_v2_relay_policy.valid()
                && m_epose_v2_relay_policy.mining_template.max_items
                    <= m_epose_v2_parameters.limits.envelope.max_records
                && m_epose_v2_relay_policy.mining_template.max_bytes
                    <= m_epose_v2_parameters.limits.envelope.max_envelope_bytes
                && m_epose_v2_parameters.limits.max_envelopes_per_transaction >= 2,
            false, "Invalid EPoSE-v2 relay/template policy");
        m_epose_v2_relay_pool.reset(new qwertycoin::epose::relay_record_pool_v2(
            m_epose_v2_parameters.timing,
            m_epose_v2_parameters.committee,
            m_epose_v2_parameters.limits.envelope,
            m_epose_v2_relay_policy.queue,
            m_epose_v2_relay_policy.mining_template,
            m_epose_v2_relay_policy.receipt_slot_dedup_height));
      }
      else if (m_nettype != FAKECHAIN)
      {
        LOG_ERROR("QWC-HF17/EPoSE-v2 relay/template parameters are incomplete; refusing to initialize a public chain");
        return false;
      }
    }
  }

  // if the blockchain is new, add the genesis block
  // this feels kinda kludgy to do it this way, but can be looked at later.
  // TODO: add function to create and store genesis block,
  //       taking testnet into account
  if(!m_db->height())
  {
    MINFO("Blockchain not loaded, generating genesis block.");
    block bl;
    block_verification_context bvc = {};
    const uint8_t genesis_hf_version = m_hardfork->get_ideal_version(0);
    generate_genesis_block(bl, get_config(m_nettype).GENESIS_TX, get_config(m_nettype).GENESIS_NONCE, genesis_hf_version, genesis_hf_version);
    db_wtxn_guard wtxn_guard(m_db);
    add_new_block(bl, bvc);
    CHECK_AND_ASSERT_MES(!bvc.m_verifivation_failed, false, "Failed to add genesis block to blockchain");
  }
  // TODO: if blockchain load successful, verify blockchain against both
  //       hard-coded and runtime-loaded (and enforced) checkpoints.
  else
  {
  }

  if (m_nettype != FAKECHAIN)
  {
    // ensure we fixup anything we found and fix in the future
    m_db->fixup();
  }

  if (test_options && test_options->long_term_block_weight_window)
  {
    m_long_term_block_weights_window = test_options->long_term_block_weight_window;
    m_long_term_block_weights_cache_rolling_median =
        epee::misc_utils::rolling_median_t<uint64_t>(
            m_long_term_block_weights_window);
  }

  if (!load_vm_state())
  {
    LOG_ERROR("Failed to load contract state from blockchain database");
    return false;
  }

  if (!rebuild_epose_state())
  {
    LOG_ERROR("Failed to rebuild EPoSE state from blockchain database");
    return false;
  }

  db_rtxn_guard rtxn_guard(m_db);

  // check how far behind we are
  uint64_t top_block_timestamp = m_db->get_top_block_timestamp();
  uint64_t timestamp_diff = time(NULL) - top_block_timestamp;

  // genesis block has no timestamp, could probably change it to have timestamp of 1397818133...
  if(!top_block_timestamp)
    timestamp_diff = time(NULL) - 1397818133;

  // create general purpose async service queue

  m_async_work_idle = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(m_async_service.get_executor());
  // we only need 1
  m_async_pool.create_thread(boost::bind(&boost::asio::io_context::run, &m_async_service));

#if defined(PER_BLOCK_CHECKPOINT)
  if (m_nettype != FAKECHAIN)
    load_compiled_in_block_hashes(get_checkpoints);
#endif

  MINFO("Blockchain initialized. last block: " << m_db->height() - 1 << ", " << epee::misc_utils::get_time_interval_string(timestamp_diff) << " time ago, current difficulty: " << get_difficulty_for_next_block());

  rtxn_guard.stop();

  uint64_t num_popped_blocks = 0;
  while (!m_db->is_read_only())
  {
    uint64_t top_height;
    const crypto::hash top_id = m_db->top_block_hash(&top_height);
    const block top_block = m_db->get_top_block();
    const uint8_t ideal_hf_version = get_ideal_hard_fork_version(top_height);
    if (ideal_hf_version <= 1 || ideal_hf_version == top_block.major_version)
    {
      if (num_popped_blocks > 0)
        MGINFO("Initial popping done, top block: " << top_id << ", top height: " << top_height << ", block version: " << (uint64_t)top_block.major_version);
      break;
    }
    else
    {
      if (num_popped_blocks == 0)
        MGINFO("Current top block " << top_id << " at height " << top_height << " has version " << (uint64_t)top_block.major_version << " which disagrees with the ideal version " << (uint64_t)ideal_hf_version);
      if (num_popped_blocks % 100 == 0)
        MGINFO("Popping blocks... " << top_height);
      ++num_popped_blocks;
      block popped_block;
      std::vector<transaction> popped_txs;
      try
      {
        m_db->pop_block(popped_block, popped_txs);
      }
      // anything that could cause this to throw is likely catastrophic,
      // so we re-throw
      catch (const std::exception& e)
      {
        MERROR("Error popping block from blockchain: " << e.what());
        throw;
      }
      catch (...)
      {
        MERROR("Error popping block from blockchain, throwing!");
        throw;
      }
    }
  }
  if (num_popped_blocks > 0)
  {
    m_timestamps_and_difficulties_height = 0;
    m_reset_timestamps_and_difficulties_height = true;
    m_hardfork->reorganize_from_chain_height(get_current_blockchain_height());
    uint64_t top_block_height;
    crypto::hash top_block_hash = get_tail_id(top_block_height);
    m_tx_pool.on_blockchain_dec(top_block_height, top_block_hash);
  }

  bool difficulty_ok;
  uint64_t difficulty_recalc_height;
  std::tie(difficulty_ok, difficulty_recalc_height) = check_difficulty_checkpoints();
  if (!difficulty_ok)
  {
    MERROR("Difficulty drift detected!");
    recalculate_difficulties(difficulty_recalc_height);
  }

  {
    db_txn_guard txn_guard(m_db, m_db->is_read_only());
    if (!update_next_cumulative_weight_limit())
      return false;
  }

  if (m_hardfork->get_current_version() >= RX_BLOCK_VERSION)
  {
    const crypto::hash seedhash = get_block_id_by_height(crypto::rx_seedheight(m_db->height()));
    if (seedhash != crypto::null_hash)
      rx_set_main_seedhash(seedhash.data, tools::get_max_concurrency());
  }

  return true;
}
//------------------------------------------------------------------
bool Blockchain::init(BlockchainDB* db, HardFork*& hf, const network_type nettype, bool offline)
{
  if (hf != nullptr)
    m_hardfork = hf;
  bool res = init(db, nettype, offline, NULL);
  if (hf == nullptr)
    hf = m_hardfork;
  return res;
}
//------------------------------------------------------------------
bool Blockchain::store_blockchain()
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  // lock because the rpc_thread command handler also calls this
  CRITICAL_REGION_LOCAL(m_db->m_synchronization_lock);

  TIME_MEASURE_START(save);
  // TODO: make sure sync(if this throws that it is not simply ignored higher
  // up the call stack
  try
  {
    m_db->sync();
  }
  catch (const std::exception& e)
  {
    MERROR(std::string("Error syncing blockchain db: ") + e.what() + "-- shutting down now to prevent issues!");
    throw;
  }
  catch (...)
  {
    MERROR("There was an issue storing the blockchain, shutting down now to prevent issues!");
    throw;
  }

  TIME_MEASURE_FINISH(save);
  if(m_show_time_stats)
    MINFO("Blockchain stored OK, took: " << save << " ms");
  return true;
}
//------------------------------------------------------------------
bool Blockchain::deinit()
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  m_epose_rebuild_shutdown = true;
  m_epose_rebuild_timer.cancel();

  MTRACE("Stopping blockchain read/write activity");

 // stop async service
  m_async_work_idle.reset();
  m_async_pool.join_all();
  m_async_service.stop();

  // as this should be called if handling a SIGSEGV, need to check
  // if m_db is a NULL pointer (and thus may have caused the illegal
  // memory operation), otherwise we may cause a loop.
  try
  {
    if (m_db)
    {
      m_db->close();
      MTRACE("Local blockchain read/write activity stopped successfully");
    }
  }
  catch (const std::exception& e)
  {
    LOG_ERROR(std::string("Error closing blockchain db: ") + e.what());
  }
  catch (...)
  {
    LOG_ERROR("There was an issue closing/storing the blockchain, shutting down now to prevent issues!");
  }

  delete m_hardfork;
  m_hardfork = NULL;
  delete m_db;
  m_db = NULL;
  return true;
}
//------------------------------------------------------------------
// This function removes blocks from the top of blockchain.
// It starts a batch and calls private method pop_block_from_blockchain().
void Blockchain::pop_blocks(uint64_t nblocks)
{
  uint64_t i = 0;
  CRITICAL_REGION_LOCAL(m_tx_pool);
  CRITICAL_REGION_LOCAL1(m_blockchain_lock);

  bool stop_batch = m_db->batch_start();

  try
  {
    const uint64_t blockchain_height = m_db->height();
    if (blockchain_height > 0)
      nblocks = std::min(nblocks, blockchain_height - 1);
    while (i < nblocks)
    {
      pop_block_from_blockchain();
      ++i;
    }
  }
  catch (const std::exception& e)
  {
    LOG_ERROR("Error when popping blocks after processing " << i << " blocks: " << e.what());
    if (stop_batch)
      m_db->batch_abort();
    schedule_epose_recovery();
    return;
  }

  CHECK_AND_ASSERT_THROW_MES(update_next_cumulative_weight_limit(), "Error updating next cumulative weight limit");

  if (stop_batch)
    m_db->batch_stop();
  schedule_epose_recovery();

  if (m_hardfork->get_current_version() >= RX_BLOCK_VERSION)
  {
    const crypto::hash seedhash = get_block_id_by_height(crypto::rx_seedheight(m_db->height()));
    rx_set_main_seedhash(seedhash.data, tools::get_max_concurrency());
  }
}
//------------------------------------------------------------------
// This function tells BlockchainDB to remove the top block from the
// blockchain and then returns all transactions (except the miner tx, of course)
// from it to the tx_pool
block Blockchain::pop_block_from_blockchain()
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  m_timestamps_and_difficulties_height = 0;
  m_reset_timestamps_and_difficulties_height = true;

  block popped_block;
  std::vector<transaction> popped_txs;

  CHECK_AND_ASSERT_THROW_MES(m_db->height() > 1, "Cannot pop the genesis block");

  const uint8_t previous_hf_version = get_current_hard_fork_version();
  const uint64_t popped_height = m_db->height() - 1;
  vm_block_commitment popped_vm_commitment{};
  vm::write_set popped_vm_undo;
  const bool popped_contract_block = m_db->get_vm_commitment(popped_height, popped_vm_commitment);
  const bool popped_vm_undo_kept = popped_contract_block && m_db->get_vm_undo(popped_height, popped_vm_undo);
  try
  {
    m_db->pop_block(popped_block, popped_txs);
  }
  // anything that could cause this to throw is likely catastrophic,
  // so we re-throw
  catch (const std::exception& e)
  {
    LOG_ERROR("Error popping block from blockchain: " << e.what());
    throw;
  }
  catch (...)
  {
    LOG_ERROR("Error popping block from blockchain, throwing!");
    throw;
  }
  m_chainlock_verification_cache.clear();

  if (popped_vm_undo_kept)
    m_vm_state.apply(popped_vm_undo);
  else if (popped_contract_block)
    m_vm_state_stale = true; // H-11

  rollback_epose_block(m_db->height());

  // make sure the hard fork object updates its current version
  m_hardfork->on_block_popped(1);

  // return transactions from popped block to the tx_pool
  size_t pruned = 0;
  for (transaction& tx : popped_txs)
  {
    if (tx.pruned)
    {
      ++pruned;
      continue;
    }
    if (!is_coinbase(tx))
    {
      cryptonote::tx_verification_context tvc = AUTO_VAL_INIT(tvc);

      // FIXME: HardFork
      // Besides the below, popping a block should also remove the last entry
      // in hf_versions.
      uint8_t version = get_ideal_hard_fork_version(m_db->height());

      // We assume that if they were in a block, the transactions are already known to the network
      // as a whole. However, if we had mined that block, that might not be always true. Unlikely
      // though, and always relaying these again might cause a spike of traffic as many nodes
      // re-relay all the transactions in a popped block when a reorg happens. You might notice that
      // we also set the "nic_verified_hf_version" paramater. Since we know we took this transaction
      // from the mempool earlier in this function call, when the mempool has the same current fork
      // version, we can return it without re-verifying the consensus rules on it.
      const bool r = m_tx_pool.add_tx(tx, tvc, relay_method::block, true, version, version);
      if (!r)
      {
        LOG_ERROR("Error returning transaction to tx_pool");
      }
    }
  }
  if (pruned)
    MWARNING(pruned << " pruned txes could not be added back to the txpool");

  m_blocks_longhash_table.clear();
  m_scan_table.clear();

  uint64_t top_block_height;
  crypto::hash top_block_hash = get_tail_id(top_block_height);
  m_tx_pool.on_blockchain_dec(top_block_height, top_block_hash);
  invalidate_block_template_cache();

  const uint8_t new_hf_version = get_current_hard_fork_version();
  if (new_hf_version != previous_hf_version)
  {
    MINFO("Validating txpool for v" << (unsigned)new_hf_version);
    m_tx_pool.validate(new_hf_version);
  }

  return popped_block;
}
//------------------------------------------------------------------
bool Blockchain::reset_and_set_genesis_block(const block& b)
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  m_timestamps_and_difficulties_height = 0;
  m_reset_timestamps_and_difficulties_height = true;
  invalidate_block_template_cache();
  m_db->reset();
  m_db->drop_alt_blocks();
  m_vm_state = vm::state_tree();
  m_vm_state_stale = false;
  m_hardfork->init();
  m_epose_state.reset();
  m_epose_block_snapshots.clear();
  m_epose_rebuild_timer.cancel();
  m_epose_rebuild_retry_scheduled = false;
  m_epose_rebuild_attempts = 0;
  m_epose_rebuild_fatal = false;
  m_epose_retry_best_chain = false;
  if (m_epose_v2)
  {
    CHECK_AND_ASSERT_MES(
        get_block_hash(b) == m_epose_v2_parameters.genesis_hash,
        false, "Refusing to reset EPoSE-v2 to a different genesis");
    m_epose_v2.reset(new qwertycoin::epose::consensus_coordinator_v2(
        m_epose_v2_parameters));
    CHECK_AND_ASSERT_MES(m_epose_v2->valid(), false,
        "Failed to reset EPoSE-v2 coordinator");
    if (m_epose_v2_relay_pool)
      m_epose_v2_relay_pool.reset(new qwertycoin::epose::relay_record_pool_v2(
          m_epose_v2_parameters.timing,
          m_epose_v2_parameters.committee,
          m_epose_v2_parameters.limits.envelope,
          m_epose_v2_relay_policy.queue,
          m_epose_v2_relay_policy.mining_template,
          m_epose_v2_relay_policy.receipt_slot_dedup_height));
  }

  db_wtxn_guard wtxn_guard(m_db);
  block_verification_context bvc = {};
  add_new_block(b, bvc);
  if (!update_next_cumulative_weight_limit())
    return false;
  return bvc.m_added_to_main_chain && !bvc.m_verifivation_failed;
}
//------------------------------------------------------------------
crypto::hash Blockchain::get_tail_id(uint64_t& height) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  return m_db->top_block_hash(&height);
}
//------------------------------------------------------------------
crypto::hash Blockchain::get_tail_id() const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  // WARNING: this function does not take m_blockchain_lock, and thus should only call read only
  // m_db functions which do not depend on one another (ie, no getheight + gethash(height-1), as
  // well as not accessing class members, even read only (ie, m_invalid_blocks). The caller must
  // lock if it is otherwise needed.
  return m_db->top_block_hash();
}
//------------------------------------------------------------------
/*TODO: this function was...poorly written.  As such, I'm not entirely
 *      certain on what it was supposed to be doing.  Need to look into this,
 *      but it doesn't seem terribly important just yet.
 *
 * puts into list <ids> a list of hashes representing certain blocks
 * from the blockchain in reverse chronological order
 *
 * the blocks chosen, at the time of this writing, are:
 *   the most recent 11
 *   powers of 2 less recent from there, so 13, 17, 25, etc...
 *
 */
bool Blockchain::get_short_chain_history(std::list<crypto::hash>& ids, uint64_t& current_height) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  uint64_t i = 0;
  uint64_t current_multiplier = 1;
  uint64_t sz = current_height = m_db->height();

  if(!sz)
    return true;

  db_rtxn_guard rtxn_guard(m_db);
  bool genesis_included = false;
  uint64_t current_back_offset = 1;
  while(current_back_offset < sz)
  {
    ids.push_back(m_db->get_block_hash_from_height(sz - current_back_offset));

    if(sz-current_back_offset == 0)
    {
      genesis_included = true;
    }
    if(i < 10)
    {
      ++current_back_offset;
    }
    else
    {
      current_multiplier *= 2;
      current_back_offset += current_multiplier;
    }
    ++i;
  }

  if (!genesis_included)
  {
    ids.push_back(m_db->get_block_hash_from_height(0));
  }

  return true;
}
//------------------------------------------------------------------
crypto::hash Blockchain::get_block_id_by_height(uint64_t height) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  // WARNING: this function does not take m_blockchain_lock, and thus should only call read only
  // m_db functions which do not depend on one another (ie, no getheight + gethash(height-1), as
  // well as not accessing class members, even read only (ie, m_invalid_blocks). The caller must
  // lock if it is otherwise needed.
  try
  {
    return m_db->get_block_hash_from_height(height);
  }
  catch (const BLOCK_DNE& e)
  {
  }
  catch (const std::exception& e)
  {
    MERROR(std::string("Something went wrong fetching block hash by height: ") + e.what());
    throw;
  }
  catch (...)
  {
    MERROR(std::string("Something went wrong fetching block hash by height"));
    throw;
  }
  return null_hash;
}
//------------------------------------------------------------------
bool Blockchain::is_epose_enabled_for_height(uint64_t height) const
{
  return m_hardfork
      && is_qwc_epose_v2_hardfork(get_ideal_hard_fork_version(height));
}
//------------------------------------------------------------------
bool Blockchain::is_epose_enabled() const
{
  const uint64_t height = m_db ? m_db->height() : 0;
  return height > 0 && is_epose_enabled_for_height(height - 1);
}
//------------------------------------------------------------------
bool Blockchain::is_epose_enabled_at_height(uint64_t height) const
{
  return is_epose_enabled_for_height(height);
}
//------------------------------------------------------------------
uint64_t Blockchain::get_epose_current_epoch() const
{
  const uint64_t height = m_db ? m_db->height() : 0;
  if (m_epose_v2 && height > m_epose_v2_parameters.timing.activation_height)
    return (height - 1 - m_epose_v2_parameters.timing.activation_height)
        / m_epose_v2_parameters.timing.epoch_length;
  return qwertycoin::epose::epoch_for_height(height > 0 ? height - 1 : 0);
}
//------------------------------------------------------------------
bool Blockchain::epose_rpc_ready() const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  return epose_state_at_tip();
}
//------------------------------------------------------------------
Blockchain::epose_rpc_snapshot Blockchain::get_epose_rpc_snapshot(uint64_t requested_epoch) const
{
  // QWC-26 D3
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
  for (;;)
  {
    {
      CRITICAL_REGION_LOCAL(m_blockchain_lock);
      if (!m_epose_rpc_batch_open)
      {
        epose_rpc_snapshot snapshot{};
        snapshot.ready = epose_rpc_ready();
        if (!snapshot.ready)
          return snapshot;
        snapshot.epoch = requested_epoch == 0 ? get_epose_current_epoch() : requested_epoch;
        snapshot.oldest_retained_epoch = get_epose_oldest_retained_epoch_v2();
        if (snapshot.epoch < snapshot.oldest_retained_epoch)
          return snapshot;
        get_epose_identity_descriptors_v2(snapshot.epoch, snapshot.descriptors);
        snapshot.frozen_membership_available = get_epose_membership_snapshot_v2(
            snapshot.epoch, snapshot.frozen_membership) == epose_query_status::found;
        snapshot.round_count = m_epose_v2_parameters.committee.round_count;
        snapshot.committee_size = m_epose_v2_parameters.committee.committee_size;
        snapshot.min_compatible_committee = m_epose_v2_parameters.committee.min_compatible_committee;
        snapshot.attestation_count = get_epose_attestation_count();
        snapshot.state_hash = get_epose_state_hash();
        snapshot.start_height = get_epose_epoch_start_height(snapshot.epoch);
        snapshot.end_height = get_epose_epoch_end_height(snapshot.epoch);
        if (requested_epoch == 0)
          snapshot.qualification_available = get_epose_latest_closed_qualification_epoch(snapshot.qualification_epoch) == epose_query_status::found;
        else
        {
          snapshot.qualification_epoch = requested_epoch;
          qwertycoin::epose::qualification_set_v2 qualification{};
          snapshot.qualification_available = get_epose_qualification_v2(requested_epoch, qualification) == epose_query_status::found;
          if (snapshot.qualification_available)
            snapshot.qualified_nodes = std::move(qualification.qualified_nodes);
        }
        if (requested_epoch == 0 && snapshot.qualification_available)
          get_epose_qualified_service_nodes(snapshot.qualification_epoch, snapshot.qualified_nodes);
        snapshot.qualification_pruned = epose_qualification_pruned_v2(snapshot.qualification_epoch);
        return snapshot;
      }
    }
    if (std::chrono::steady_clock::now() >= deadline)
      return {};
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}
//------------------------------------------------------------------
uint64_t Blockchain::get_epose_epoch_start_height(uint64_t epoch) const
{
  if (m_epose_v2)
  {
    uint64_t height = 0;
    return m_epose_v2_parameters.timing.epoch_start(epoch, height)
        ? height : std::numeric_limits<uint64_t>::max();
  }
  return qwertycoin::epose::epoch_start_height(epoch);
}
//------------------------------------------------------------------
uint64_t Blockchain::get_epose_epoch_end_height(uint64_t epoch) const
{
  if (m_epose_v2)
  {
    uint64_t height = 0;
    return m_epose_v2_parameters.timing.epoch_end(epoch, height)
        ? height : std::numeric_limits<uint64_t>::max();
  }
  return qwertycoin::epose::epoch_end_height(epoch);
}
//------------------------------------------------------------------
bool Blockchain::get_epose_reward_source_epoch_v2(uint64_t height, uint64_t &epoch) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  epoch = 0;
  if (!m_epose_v2 || !m_epose_v2_parameters.timing.valid()
      || height < m_epose_v2_parameters.timing.activation_height)
    return false;
  const uint64_t payout_epoch =
      (height - m_epose_v2_parameters.timing.activation_height)
      / m_epose_v2_parameters.timing.epoch_length;
  if (payout_epoch != 0)
    epoch = payout_epoch - 1;
  return true;
}
//------------------------------------------------------------------
qwertycoin::epose::relay_ingress_status_v2
Blockchain::submit_epose_relay_envelopes_v2(
    const std::vector<blobdata> &envelopes,
    std::vector<blobdata> &accepted,
    qwertycoin::epose::relay_ingress_trace_v2 *trace)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  accepted.clear();
  if (!epose_state_at_tip() || !m_epose_v2 || !m_epose_v2_relay_pool)
  {
    if (trace != nullptr)
      *trace = {qwertycoin::epose::relay_ingress_rejection_v2::invalid_pool_or_policy};
    return envelopes.empty()
        ? qwertycoin::epose::relay_ingress_status_v2::accepted
        : qwertycoin::epose::relay_ingress_status_v2::invalid_configuration;
  }

  epose_context_source contexts(
      main_chain_context_source(*this, m_epose_v2_parameters));

  return qwertycoin::epose::admit_relay_envelopes_v2(
      envelopes, m_db->height(), m_epose_v2_parameters.limits.envelope,
      m_epose_v2_relay_policy, m_epose_v2->preview_state(), contexts,
      *m_epose_v2_relay_pool, accepted, trace);
}
//------------------------------------------------------------------
qwertycoin::epose::relay_pool_diagnostics_v2
Blockchain::get_epose_relay_pool_diagnostics_v2() const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  return m_epose_v2_relay_pool
      ? m_epose_v2_relay_pool->diagnostics()
      : qwertycoin::epose::relay_pool_diagnostics_v2{};
}
//------------------------------------------------------------------
std::vector<qwertycoin::epose::service_node_identity> Blockchain::get_epose_service_nodes() const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (!m_epose_state)
    return {};
  return m_epose_state->registry().registrations();
}
//------------------------------------------------------------------
std::vector<qwertycoin::epose::service_attestation> Blockchain::get_epose_attestations() const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (!m_epose_state)
    return {};
  return m_epose_state->attestations();
}
//------------------------------------------------------------------
Blockchain::epose_query_status Blockchain::get_epose_qualified_service_nodes(
    uint64_t epoch, std::vector<crypto::public_key> &nodes) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  nodes.clear();
  if (!epose_state_at_tip())
    return epose_query_status::absent;
  if (m_epose_v2)
  {
    return query_epose_epoch_v2_at_tip(epoch, [&](const auto &coordinator) {
      const auto *qualification = coordinator.state().membership().qualification(epoch);
      if (qualification == nullptr)
        return false;
      nodes = qualification->qualified_nodes;
      return true;
    });
  }
  if (!m_epose_state)
    return epose_query_status::absent;
  nodes = m_epose_state->qualified_service_nodes(epoch);
  return nodes.empty() ? epose_query_status::absent : epose_query_status::found;
}
//------------------------------------------------------------------
Blockchain::epose_query_status Blockchain::get_epose_latest_closed_qualification_epoch(uint64_t &epoch) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  epoch = 0;
  if (!epose_state_at_tip() || !m_epose_v2 || !m_db || m_db->height() == 0)
    return epose_query_status::absent;
  const uint64_t current_epoch = get_epose_current_epoch();
  const auto &membership = m_epose_v2->state().membership();
  const uint64_t tip_height = m_db->height() - 1;
  uint64_t deadline = 0;
  if (m_epose_v2_parameters.timing.evidence_deadline(current_epoch, deadline)
      && tip_height >= deadline
      && membership.qualification(current_epoch) != nullptr)
  {
    epoch = current_epoch;
    return epose_query_status::found;
  }
  if (current_epoch == 0)
    return epose_query_status::absent;
  return query_epose_epoch_v2_at_tip(current_epoch - 1, [&](const auto &coordinator) {
    if (coordinator.state().membership().qualification(current_epoch - 1) == nullptr)
      return false;
    epoch = current_epoch - 1;
    return true;
  });
}
//------------------------------------------------------------------
template <typename Result, typename Query>
Result Blockchain::query_epose_v2_at_tip(Result fallback, Query &&query) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (!epose_state_at_tip())
    return fallback;
  return m_epose_v2 ? query(*m_epose_v2) : fallback;
}
//------------------------------------------------------------------
template <typename Record, typename Lookup>
bool Blockchain::copy_epose_record_v2(Record &record, Lookup &&lookup) const
{
  return query_epose_v2_at_tip(false, [&](auto &coordinator) {
    const auto *stored = lookup(coordinator);
    if (stored == nullptr)
      return false;
    record = *stored;
    return true;
  });
}
//------------------------------------------------------------------
template <typename Query>
Blockchain::epose_query_status Blockchain::query_epose_epoch_v2_at_tip(uint64_t epoch, Query &&query) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (!epose_state_at_tip() || !m_epose_v2)
    return epose_query_status::absent;
  if (query(*m_epose_v2))
    return epose_query_status::found;
  const auto &membership = m_epose_v2->state().membership();
  return !membership.epoch_recorded(epoch, false) && membership.read_epoch(epoch)
      == qwertycoin::epose::pipeline_status_v2::outside_retained_window
      ? epose_query_status::outside_retained_window : epose_query_status::absent;
}
//------------------------------------------------------------------
template <typename Record, typename Lookup>
Blockchain::epose_query_status Blockchain::copy_epose_epoch_record_v2(
    uint64_t epoch, Record &record, Lookup &&lookup) const
{
  return query_epose_epoch_v2_at_tip(epoch, [&](const auto &coordinator) {
    const auto *stored = lookup(coordinator);
    if (stored == nullptr)
      return false;
    record = *stored;
    return true;
  });
}
//------------------------------------------------------------------
Blockchain::epose_query_status Blockchain::get_epose_identity_descriptors_v2(
    uint64_t epoch, std::vector<qwertycoin::epose::identity_descriptor_v2> &descriptors) const
{
  descriptors.clear();
  return query_epose_epoch_v2_at_tip(epoch, [&](const auto &coordinator) {
    descriptors = coordinator.state().lifecycle().descriptors_for_epoch(epoch);
    return !descriptors.empty();
  });
}
//------------------------------------------------------------------
bool Blockchain::get_epose_latest_lifecycle_record_v2(
    const crypto::hash &identity_id,
    qwertycoin::epose::lifecycle_record_v2 &record) const
{
  return copy_epose_record_v2(record, [&](auto &coordinator) {
    return coordinator.state().lifecycle().latest_record(identity_id);
  });
}
//------------------------------------------------------------------
Blockchain::epose_query_status Blockchain::get_epose_membership_snapshot_v2(
    uint64_t epoch, qwertycoin::epose::membership_snapshot_v2 &snapshot) const
{
  return copy_epose_epoch_record_v2(epoch, snapshot, [&](const auto &coordinator) {
    return coordinator.state().membership().snapshot(epoch);
  });
}
//------------------------------------------------------------------
Blockchain::epose_query_status Blockchain::get_epose_qualification_v2(
    uint64_t epoch, qwertycoin::epose::qualification_set_v2 &qualification) const
{
  return copy_epose_epoch_record_v2(epoch, qualification, [&](const auto &coordinator) {
    return coordinator.state().membership().qualification(epoch);
  });
}
//------------------------------------------------------------------
bool Blockchain::epose_qualification_pruned_v2(uint64_t epoch) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  return epose_state_at_tip() && m_epose_v2
      && m_epose_v2->state().membership().qualification_pruned(epoch);
}
//------------------------------------------------------------------
bool Blockchain::set_chainlock_test_quorum(
    const qwertycoin::epose::epoch_timing_v2 &timing, std::vector<crypto::public_key> candidates)
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (m_nettype != FAKECHAIN || !timing.valid())
    return false;
  m_chainlock_test_quorum = chainlock_test_quorum{timing, std::move(candidates)};
  m_chainlock_verification_cache.clear();
  return true;
}
//------------------------------------------------------------------
bool Blockchain::get_chainlock_timing(qwertycoin::epose::epoch_timing_v2 &timing) const
{
  if (!epose_state_at_tip())
    return false;

  if (m_chainlock_test_quorum)
  {
    timing = m_chainlock_test_quorum->timing;
    return true;
  }
  if (!m_epose_v2)
    return false;
  timing = m_epose_v2_parameters.timing;
  return timing.valid();
}
//------------------------------------------------------------------
bool Blockchain::chainlock_quorum_shared(uint64_t epoch, uint64_t split_height) const
{
  qwertycoin::epose::epoch_timing_v2 timing{};
  uint64_t anchor_height = 0;
  return get_chainlock_timing(timing)
      && timing.committee_anchor(epoch, anchor_height)
      && split_height > anchor_height;
}
//------------------------------------------------------------------
bool Blockchain::get_chainlock_context(qwertycoin::epose::chainlock_context_v1 &context) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (!epose_state_at_tip())
    return false;
  if ((!m_epose_v2 && !m_chainlock_test_quorum) || m_db->height() == 0)
    return false;
  context = {m_nettype, m_db->get_block_hash_from_height(0), m_epose_v2_parameters.parameter_set_hash};
  return true;
}
//------------------------------------------------------------------
bool Blockchain::get_chainlock_epoch(uint64_t height, uint64_t &epoch) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  qwertycoin::epose::epoch_timing_v2 timing{};
  return get_chainlock_timing(timing) && timing.epoch_of(height, epoch);
}
//------------------------------------------------------------------
bool Blockchain::get_chainlock_quorum(uint64_t epoch, qwertycoin::epose::chainlock_quorum_v1 &quorum) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  chainlock_selection_inputs inputs;
  if (get_chainlock_selection_inputs(epoch, inputs) != chainlock_membership_restore::restored)
    return false;
  return select_chainlock_quorum(inputs, quorum);
}
//------------------------------------------------------------------
bool Blockchain::select_chainlock_quorum(const chainlock_selection_inputs &inputs,
    qwertycoin::epose::chainlock_quorum_v1 &quorum) const
{
  const auto candidates = inputs.test_candidates ? *inputs.test_candidates
      : qwertycoin::epose::chainlock_candidates_v1(*inputs.snapshot, *inputs.qualification);
  ++m_chainlock_quorum_selections;
  quorum = qwertycoin::epose::select_chainlock_quorum_v1(inputs.epoch, inputs.anchor_hash, candidates, inputs.context);
  return quorum.can_lock();
}
//------------------------------------------------------------------
Blockchain::chainlock_membership_restore Blockchain::get_chainlock_selection_inputs(
    uint64_t epoch, chainlock_selection_inputs &inputs,
    boost::unique_lock<epee::critical_section> *blockchain_lock, bool allow_reconstruct) const
{
  inputs = {};
  if (!get_chainlock_context(inputs.context))
    return chainlock_membership_restore::unavailable;
  inputs.epoch = epoch;
  if (m_chainlock_test_quorum)
  {
    if (!m_chainlock_test_quorum->timing.committee_anchor(epoch, inputs.anchor_height)
        || inputs.anchor_height >= m_db->height())
      return chainlock_membership_restore::unavailable;
    inputs.anchor_hash = m_db->get_block_hash_from_height(inputs.anchor_height);
    inputs.test_candidates = &m_chainlock_test_quorum->candidates;
    return chainlock_membership_restore::restored;
  }
  if (epoch == 0)
    return chainlock_membership_restore::unavailable;
  const auto &membership = m_epose_v2->state().membership();
  if (m_epose_v2->state().chainlock_membership_stored(epoch))
  {
    inputs.snapshot = membership.snapshot(epoch);
    inputs.qualification = membership.qualification(epoch - 1);
  }
  else
  {
    if (!allow_reconstruct || !blockchain_lock)
      return chainlock_membership_restore::unavailable;
    qwertycoin::epose::membership_snapshot_v2 snapshot;
    qwertycoin::epose::qualification_set_v2 qualification;
    const auto status = restore_chainlock_membership(epoch, *blockchain_lock, snapshot, qualification);
    if (status != chainlock_membership_restore::restored)
      return status;
    inputs.restored_snapshot = std::move(snapshot);
    inputs.restored_qualification = std::move(qualification);
    inputs.snapshot = &*inputs.restored_snapshot;
    inputs.qualification = &*inputs.restored_qualification;
  }
  inputs.anchor_height = inputs.snapshot->anchor_height;
  inputs.anchor_hash = inputs.snapshot->anchor_hash;
  if (inputs.anchor_height >= m_db->height()
      || m_db->get_block_hash_from_height(inputs.anchor_height) != inputs.anchor_hash)
    return chainlock_membership_restore::unavailable;
  return chainlock_membership_restore::restored;
}
//------------------------------------------------------------------
bool Blockchain::chainlock_quorum_inputs_require_reconstruction(uint64_t epoch) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (!m_epose_v2 || m_chainlock_test_quorum || epoch == 0)
    return false;
  return !m_epose_v2->state().chainlock_membership_stored(epoch);
}
//------------------------------------------------------------------
Blockchain::chainlock_membership_restore Blockchain::restore_chainlock_membership(
    uint64_t epoch, boost::unique_lock<epee::critical_section> &blockchain_lock,
    qwertycoin::epose::membership_snapshot_v2 &snapshot,
    qwertycoin::epose::qualification_set_v2 &qualification) const
{
  if (!m_epose_v2 || !m_db->height()
      || !chainlock_quorum_inputs_require_reconstruction(epoch))
    return chainlock_membership_restore::unavailable;
  if (!epose_state_at_tip())
    return chainlock_membership_restore::unavailable;
  const bool restored = m_epose_v2->state().reconstruct_chainlock_membership(
      epoch, snapshot, qualification);
  return restored ? chainlock_membership_restore::restored
      : chainlock_membership_restore::unavailable;
}
//------------------------------------------------------------------
bool Blockchain::get_chainlock_quorum_inputs(uint64_t epoch, chainlock_quorum_inputs &inputs,
    uint64_t offset, uint64_t limit, bool allow_reconstruct) const
{
  boost::unique_lock<epee::critical_section> blockchain_lock(m_blockchain_lock);
  inputs = {};
  chainlock_selection_inputs selection;
  if (epoch == 0 || !get_chainlock_timing(inputs.timing))
    return false;
  if (get_chainlock_selection_inputs(epoch, selection, &blockchain_lock, allow_reconstruct)
      != chainlock_membership_restore::restored)
    return false;
  inputs.context = selection.context;
  inputs.epoch = epoch;
  if (!inputs.timing.epoch_start(epoch - 1, inputs.previous_epoch_start_height)
      || !inputs.timing.epoch_start(epoch, inputs.epoch_start_height)
      || inputs.epoch_start_height >= m_db->height())
    return false;
  inputs.previous_epoch_start_block_id = m_db->get_block_hash_from_height(inputs.previous_epoch_start_height);
  inputs.epoch_start_block_id = m_db->get_block_hash_from_height(inputs.epoch_start_height);
  const auto page_bounds = [offset, limit](uint64_t total) {
    const size_t start = static_cast<size_t>(std::min(offset, total));
    return std::make_pair(start, static_cast<size_t>(std::min(limit, total - start)));
  };
  inputs.snapshot_height = selection.anchor_height;
  inputs.snapshot_block_id = selection.anchor_hash;
  if (selection.test_candidates)
  {
    if (!inputs.timing.evidence_deadline(epoch - 1, inputs.qualification_height))
      return false;
    inputs.snapshot_total = selection.test_candidates->size();
    inputs.qualification_total = inputs.snapshot_total;
    const auto page = page_bounds(inputs.snapshot_total);
    inputs.snapshot_members.assign(selection.test_candidates->begin() + page.first,
        selection.test_candidates->begin() + page.first + page.second);
    inputs.qualified_members = inputs.snapshot_members;
    inputs.test_override = true;
  }
  else
  {
    inputs.snapshot_hash = selection.snapshot->snapshot_hash;
    inputs.qualification_height = selection.qualification->closed_height;
    inputs.qualification_hash = selection.qualification->qualification_hash;
    inputs.snapshot_total = selection.snapshot->members.size();
    inputs.qualification_total = selection.qualification->qualified_nodes.size();
    const auto snapshot_page = page_bounds(inputs.snapshot_total);
    for (size_t index = snapshot_page.first;
        index < snapshot_page.first + snapshot_page.second; ++index)
      inputs.snapshot_members.push_back(selection.snapshot->members[index].service_public_key);
    const auto qualified_page = page_bounds(inputs.qualification_total);
    inputs.qualified_members.assign(selection.qualification->qualified_nodes.begin() + qualified_page.first,
        selection.qualification->qualified_nodes.begin() + qualified_page.first + qualified_page.second);
  }
  inputs.qualification_block_id = m_db->get_block_hash_from_height(inputs.qualification_height);
  return true;
}
//------------------------------------------------------------------
bool Blockchain::verify_complete_chainlock(const qwertycoin::epose::chainlock_v1 &lock, bool check_signatures) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (!epose_state_at_tip())
    return false;

  uint64_t epoch = 0;
  qwertycoin::epose::chainlock_quorum_v1 quorum;
  qwertycoin::epose::chainlock_context_v1 context{};
  if (!get_chainlock_epoch(lock.height, epoch) || epoch != lock.epoch
      || !get_chainlock_quorum(lock.epoch, quorum)
      || !qwertycoin::epose::chainlock_complete_v1(lock, quorum)
      || !get_chainlock_context(context))
    return false;
  if (!check_signatures)
    return qwertycoin::epose::verify_chainlock_v1(lock, quorum, context, false);

  const blobdata lock_blob = qwertycoin::epose::serialize_chainlock_v1(lock);
  blobdata quorum_bytes(reinterpret_cast<const char *>(&quorum.epoch), sizeof(quorum.epoch));
  for (const crypto::public_key &member : quorum.members)
    quorum_bytes.append(reinterpret_cast<const char *>(&member), sizeof(member));
  const crypto::hash quorum_set_hash = crypto::cn_fast_hash(quorum_bytes.data(), quorum_bytes.size());
  blobdata quorum_identity(reinterpret_cast<const char *>(&quorum_set_hash), sizeof(quorum_set_hash));
  quorum_identity += quorum_bytes;
  quorum_identity.push_back(static_cast<char>(context.nettype));
  quorum_identity.append(reinterpret_cast<const char *>(&context.genesis_hash), sizeof(context.genesis_hash));
  quorum_identity.append(reinterpret_cast<const char *>(&context.parameter_set_hash), sizeof(context.parameter_set_hash));
  const auto key = std::make_pair(lock_blob, quorum_identity);
  if (m_chainlock_verification_cache.count(key) != 0)
    return true;
  ++m_chainlock_signature_verifications;
  const bool accepted = qwertycoin::epose::verify_chainlock_v1(lock, quorum, context, true);
  if (accepted)
  {
    const uint64_t oldest = m_db->height() > QWC_LOCK_MAX_AGE_BLOCKS
        ? m_db->height() - QWC_LOCK_MAX_AGE_BLOCKS : 0;
    for (auto cached = m_chainlock_verification_cache.begin();
        cached != m_chainlock_verification_cache.end();)
      cached = cached->second < oldest
          ? m_chainlock_verification_cache.erase(cached) : std::next(cached);
    if (lock.height >= oldest)
    {
      if (m_chainlock_verification_cache.size() >= QWC_CHAINLOCK_QUORUM_MAX)
        m_chainlock_verification_cache.erase(m_chainlock_verification_cache.begin());
      m_chainlock_verification_cache.emplace(key, lock.height);
    }
  }
  return accepted;
}
//------------------------------------------------------------------
bool Blockchain::get_alt_chainlock_quorum(const qwertycoin::epose::chainlock_v1 &lock,
    const std::list<block_extended_info> &alt_chain,
    qwertycoin::epose::chainlock_quorum_v1 &quorum)
{
  if (alt_chain.empty() || !epose_state_at_tip())
    return false;
  qwertycoin::epose::epoch_timing_v2 timing{};
  uint64_t epoch = 0;
  uint64_t anchor_height = 0;
  if (!get_chainlock_timing(timing) || !timing.epoch_of(lock.height, epoch)
      || epoch != lock.epoch || !timing.committee_anchor(epoch, anchor_height))
    return false;
  if (anchor_height < alt_chain.front().height)
    return get_chainlock_quorum(epoch, quorum);

  crypto::hash anchor_hash = crypto::null_hash;
  for (const block_extended_info &entry : alt_chain)
    if (entry.height == anchor_height)
    {
      anchor_hash = get_block_hash(entry.bl);
      break;
    }
  if (anchor_hash == crypto::null_hash)
    return false;

  std::vector<crypto::public_key> candidates;
  if (m_chainlock_test_quorum)
    candidates = m_chainlock_test_quorum->candidates;
  else
  {
    std::unique_ptr<qwertycoin::epose::consensus_coordinator_v2> split_state;
    std::unique_ptr<qwertycoin::epose::consensus_coordinator_v2> anchor_state;
    crypto::hash failed_block{};
    if (preflight_epose_chain(alt_chain, split_state, failed_block, anchor_height, &anchor_state)
            != epose_preflight_result::accepted || !anchor_state)
      return false;
    qwertycoin::epose::membership_snapshot_v2 snapshot;
    qwertycoin::epose::qualification_set_v2 qualification;
    if (!anchor_state->state().reconstruct_chainlock_membership(epoch, snapshot, qualification)
        || snapshot.anchor_height != anchor_height || snapshot.anchor_hash != anchor_hash)
      return false;
    candidates = qwertycoin::epose::chainlock_candidates_v1(snapshot, qualification);
  }
  qwertycoin::epose::chainlock_context_v1 context{};
  if (!get_chainlock_context(context))
    return false;
  ++m_chainlock_quorum_selections;
  quorum = qwertycoin::epose::select_chainlock_quorum_v1(
      epoch, anchor_hash, candidates, context);
  return quorum.can_lock();
}
//------------------------------------------------------------------
bool Blockchain::verify_alt_chainlock(const qwertycoin::epose::chainlock_v1 &lock,
    const std::list<block_extended_info> &alt_chain)
{
  qwertycoin::epose::chainlock_quorum_v1 quorum;
  qwertycoin::epose::chainlock_context_v1 context{};
  return get_alt_chainlock_quorum(lock, alt_chain, quorum)
      && get_chainlock_context(context)
      && qwertycoin::epose::chainlock_complete_v1(lock, quorum)
      && qwertycoin::epose::verify_chainlock_v1(lock, quorum, context, true);
}
//------------------------------------------------------------------
bool Blockchain::get_highest_main_chainlock(uint64_t &height) const
{
  crypto::hash block_hash{};
  return get_highest_main_chainlock(height, block_hash);
}
//------------------------------------------------------------------
bool Blockchain::get_highest_main_chainlock(uint64_t &height, crypto::hash &block_hash) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (!epose_state_at_tip())
    return false;
  const uint64_t chain_height = m_db->height();
  bool found = false;
  m_db->for_chainlocks_descending([&](const chainlock_record &lock) {
    if (lock.tombstoned || lock.height >= chain_height
        || m_db->get_block_hash_from_height(lock.height) != lock.block_hash)
      return true;
    height = lock.height;
    block_hash = lock.block_hash;
    found = true;
    return false;
  });
  uint64_t carrier_height = 0;
  chainlock_record carrier;
  if (m_db->get_last_chainlock_carrier(carrier_height, carrier) && (!found || carrier.height > height))
  {
    height = carrier.height;
    block_hash = carrier.block_hash;
    found = true;
  }
  return found;
}
//------------------------------------------------------------------
bool Blockchain::canonical_chainlock_supersedes(uint64_t height) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (!epose_state_at_tip())
    return false;
  bool verified = false;
  m_db->for_chainlocks_descending([&](const chainlock_record &stored) {
    if (stored.height <= height)
      return false;
    if (stored.tombstoned || stored.height >= m_db->height()
        || m_db->get_block_hash_from_height(stored.height) != stored.block_hash)
      return true;
    qwertycoin::epose::chainlock_v1 lock;
    if (qwertycoin::epose::parse_chainlock_v1(stored.lock, lock)
        && verify_complete_chainlock(lock, true))
    {
      verified = true;
      return false;
    }
    return true;
  });
  return verified;
}
//------------------------------------------------------------------
bool Blockchain::validate_block_chainlock(
    const block &bl, uint64_t height, bool check_signatures, chainlock_record &carrier, bool &has_carrier) const
{
  if (!epose_state_at_tip())
    return false;

  has_carrier = false;
  blobdata blob;
  const size_t count = get_tx_chainlock(bl.miner_tx.extra, blob);
  if (count == 0)
    return true;
  qwertycoin::epose::chainlock_v1 lock;
  if (count != 1 || !is_qwc_contracts_hardfork(bl.major_version) || !qwertycoin::epose::parse_chainlock_v1(blob, lock))
    return false;
  if (lock.height >= height || qwc_chainlock_age_expired(lock.height, height)) // H-32
    return false;
  if (m_db->get_block_hash_from_height(lock.height) != lock.block_hash)
    return false;
  uint64_t last_carrier_height = 0;
  chainlock_record last;
  if (m_db->get_last_chainlock_carrier(last_carrier_height, last) && lock.height <= last.height)
    return false;
  chainlock_record stored;
  if (m_db->get_chainlock(lock.height, lock.block_hash, stored) && stored.tombstoned)
    return false;
  if (!verify_complete_chainlock(lock, check_signatures))
    return false;
  carrier = {lock.height, lock.block_hash, false, blob};
  has_carrier = true;
  return true;
}
//------------------------------------------------------------------
bool Blockchain::select_template_chainlock(uint64_t height, blobdata &blob) const
{
  if (!epose_state_at_tip() || height == 0)
    return false;

  uint64_t last_carrier_height = 0;
  chainlock_record last;
  const bool has_last = m_db->get_last_chainlock_carrier(last_carrier_height, last);
  const uint64_t oldest = height > QWC_LOCK_MAX_AGE_BLOCKS
      ? height - QWC_LOCK_MAX_AGE_BLOCKS : 0;
  for (uint64_t candidate_height = height; candidate_height-- > oldest;)
  {
    if (has_last && candidate_height <= last.height)
      break;
    if (candidate_height >= m_db->height())
      continue;
    chainlock_record stored;
    if (!m_db->get_chainlock(candidate_height,
            m_db->get_block_hash_from_height(candidate_height), stored) || stored.tombstoned)
      continue;
    qwertycoin::epose::chainlock_v1 lock;
    if (!qwertycoin::epose::parse_chainlock_v1(stored.lock, lock) || !verify_complete_chainlock(lock, true))
      continue;
    blob = stored.lock;
    return true;
  }
  return false;
}
//------------------------------------------------------------------
void Blockchain::tombstone_main_chainlocks_from(uint64_t height)
{
  if (!epose_state_at_tip())
    return;

  const uint64_t chain_height = m_db->height();
  std::vector<chainlock_record> conflicting;
  m_db->for_chainlocks_descending([&](const chainlock_record &lock) {
    if (lock.height < height)
      return false;
    if (!lock.tombstoned && lock.height < chain_height
        && m_db->get_block_hash_from_height(lock.height) == lock.block_hash)
      conflicting.push_back({lock.height, lock.block_hash, true, {}});
    return true;
  });
  for (const chainlock_record &tombstone : conflicting)
  {
    MWARNING("Checkpoint overrides chainlock at height " << tombstone.height << ", tombstoning " << tombstone.block_hash);
    m_db->put_chainlock(tombstone);
  }
}
//------------------------------------------------------------------
Blockchain::alt_chainlock_evaluation Blockchain::evaluate_alt_chainlocks(const std::list<block_extended_info> &alt_chain)
{
  if (!epose_state_at_tip())
    return {alt_chainlock_verdict::blocked};

  const uint64_t split_height = alt_chain.front().height;
  std::unordered_map<uint64_t, crypto::hash> alt_hashes;
  for (const block_extended_info &bei : alt_chain)
    alt_hashes.emplace(bei.height, get_block_hash(bei.bl));
  const auto in_alt_chain = [&alt_hashes](uint64_t height, const crypto::hash &hash) {
    const auto it = alt_hashes.find(height);
    return it != alt_hashes.end() && it->second == hash;
  };

  uint64_t alt_lock = 0;
  crypto::hash alt_lock_hash{};
  bool has_alt = false;
  m_db->for_chainlocks_descending([&](const chainlock_record &lock) {
    if (lock.height < split_height)
      return false;
    if (lock.tombstoned || !in_alt_chain(lock.height, lock.block_hash))
      return true;
    qwertycoin::epose::chainlock_v1 candidate;
    if (qwertycoin::epose::parse_chainlock_v1(lock.lock, candidate)
        && verify_alt_chainlock(candidate, alt_chain))
    {
      alt_lock = lock.height;
      alt_lock_hash = lock.block_hash;
      has_alt = true;
      return false;
    }
    return true;
  });

  for (const block_extended_info &bei : alt_chain)
  {
    blobdata blob;
    qwertycoin::epose::chainlock_v1 lock;
    if (get_tx_chainlock(bei.bl.miner_tx.extra, blob) != 1 || !qwertycoin::epose::parse_chainlock_v1(blob, lock)
        || !in_alt_chain(lock.height, lock.block_hash) || (has_alt && lock.height <= alt_lock))
      continue;
    if (!verify_alt_chainlock(lock, alt_chain))
      continue;
    alt_lock = lock.height;
    alt_lock_hash = lock.block_hash;
    has_alt = true;
  }

  uint64_t main_lock = 0;
  crypto::hash main_lock_hash{};
  const bool has_main = get_highest_main_chainlock(main_lock, main_lock_hash);
  if (has_alt && (!has_main || alt_lock > main_lock
          || (is_qwc_contracts_hardfork(alt_chain.back().bl.major_version)
              && alt_lock == main_lock && alt_lock_hash < main_lock_hash)))
    return {alt_chainlock_verdict::switch_verified, alt_lock, alt_lock_hash};
  if (has_main && main_lock >= split_height)
    return {alt_chainlock_verdict::blocked};
  return {alt_chainlock_verdict::difficulty};
}
//------------------------------------------------------------------
void Blockchain::prune_chainlock_partials()
{
  const uint64_t chain_height = m_db->height();
  for (auto it = m_chainlock_partials.begin(); it != m_chainlock_partials.end();)
  {
    if (qwc_chainlock_age_expired(it->first.first, chain_height))
      it = m_chainlock_partials.erase(it);
    else
      ++it;
  }
  while (m_chainlock_partials.size() > QWC_CHAINLOCK_PENDING_MAX)
    m_chainlock_partials.erase(m_chainlock_partials.begin());
}
//------------------------------------------------------------------
void Blockchain::prune_irrecoverable_alternatives()
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (!m_db || m_db->is_read_only() || m_db->height() == 0
      || !is_qwc_contracts_hardfork(get_ideal_hard_fork_version()))
    return;
  if (m_db->batch_active())
  {
    m_alternative_prune_pending = true;
    return;
  }
  struct alternative_parent
  {
    uint64_t height;
    crypto::hash parent;
  };
  std::unordered_map<crypto::hash, alternative_parent> blocks;
  std::vector<crypto::hash> remove;
  blocks.reserve(m_db->get_alt_block_count());
  m_db->for_all_alt_blocks([&](const crypto::hash &hash,
      const alt_block_data_t &data, const blobdata_ref *blob) {
    block parsed{};
    if (blob && parse_and_validate_block_from_blob(*blob, parsed))
      blocks.emplace(hash, alternative_parent{data.height, parsed.prev_id});
    else
      remove.push_back(hash);
    return true;
  }, true);
  const uint64_t tip_height = m_db->height() - 1;
  const uint64_t cutoff = tip_height > QWC_LOCK_MAX_AGE_BLOCKS
      ? tip_height - QWC_LOCK_MAX_AGE_BLOCKS : 0;
  std::unordered_set<crypto::hash> retained;
  const auto retain_ancestors = [&](crypto::hash hash) {
    auto found = blocks.find(hash);
    while (found != blocks.end() && retained.insert(hash).second)
    {
      hash = found->second.parent;
      found = blocks.find(hash);
    }
  };
  for (const auto &entry : blocks)
    if (entry.second.height >= cutoff)
      retain_ancestors(entry.first);
  m_db->for_chainlocks_descending([&](const chainlock_record &lock) {
    if (!lock.tombstoned && lock.height >= cutoff
        && blocks.count(lock.block_hash) != 0)
      retain_ancestors(lock.block_hash);
    return true;
  });
  for (const auto &entry : blocks)
    if (retained.count(entry.first) == 0)
      remove.push_back(entry.first);
  if (remove.empty())
  {
    m_alternative_prune_pending = false;
    return;
  }
  const bool stop_batch = m_db->batch_start();
  if (!stop_batch)
  {
    m_alternative_prune_pending = true;
    return;
  }
  try
  {
    size_t removed = 0;
    for (const crypto::hash &hash : remove)
    {
      if (m_alternative_prune_removal_hook)
        m_alternative_prune_removal_hook(removed);
      m_db->remove_alt_block(hash);
      ++removed;
    }
    m_db->batch_stop();
    m_alternative_prune_pending = false;
  }
  catch (...)
  {
    m_db->batch_abort();
    m_alternative_prune_pending = true;
    throw;
  }
}
//------------------------------------------------------------------
void Blockchain::prune_after_canonical_advance(uint64_t previous_height)
{
  if (!m_db || m_db->height() == 0
      || !is_qwc_contracts_hardfork(get_current_hard_fork_version()))
    return;
  const uint64_t height = m_db->height();
  if (!m_alternative_prune_pending
      && (height <= previous_height
          || height / 16 <= previous_height / 16))
    return;
  try
  {
    prune_irrecoverable_alternatives();
  }
  catch (const std::exception &error)
  {
    m_alternative_prune_pending = true;
    MERROR("Alternative pruning deferred: " << error.what());
  }
  catch (...)
  {
    m_alternative_prune_pending = true;
    MERROR("Alternative pruning deferred after an unknown failure");
  }
}
//------------------------------------------------------------------
Blockchain::chainlock_ingress Blockchain::handle_incoming_chainlock(const blobdata &blob, blobdata &relay_blob)
{
  CRITICAL_REGION_LOCAL(m_tx_pool);
  CRITICAL_REGION_LOCAL1(m_blockchain_lock);
  if (!epose_state_at_tip())
    return chainlock_ingress::ignored;
  relay_blob.clear();
  qwertycoin::epose::chainlock_v1 lock;
  if (!qwertycoin::epose::parse_chainlock_v1(blob, lock))
    return chainlock_ingress::rejected;
  if (!is_qwc_contracts_hardfork(m_hardfork->get_ideal_version(lock.height))
      || qwc_chainlock_age_expired(lock.height, m_db->height()))
    return chainlock_ingress::ignored;

  chainlock_record stored;
  if (m_db->get_chainlock(lock.height, lock.block_hash, stored))
    return chainlock_ingress::ignored;
  if (!m_checkpoints.check_block(lock.height, lock.block_hash))
  {
    m_db->put_chainlock({lock.height, lock.block_hash, true, {}});
    return chainlock_ingress::ignored;
  }

  uint64_t epoch = 0;
  if (!get_chainlock_epoch(lock.height, epoch))
    return chainlock_ingress::ignored;
  if (epoch != lock.epoch)
    return chainlock_ingress::rejected;

  const auto remember_pending = [&] {
    if (m_chainlock_pending.size() < QWC_CHAINLOCK_PENDING_MAX
        && std::find(m_chainlock_pending.begin(), m_chainlock_pending.end(), blob)
            == m_chainlock_pending.end())
      m_chainlock_pending.push_back(blob);
  };
  std::list<block_extended_info> lock_branch;
  if (m_db->get_alt_block(lock.block_hash, nullptr, nullptr))
  {
    std::vector<uint64_t> timestamps;
    block_verification_context bvc{};
    if (!build_alt_chain(lock.block_hash, lock_branch, timestamps, bvc))
    {
      remember_pending();
      return chainlock_ingress::ignored;
    }
  }
  else if (lock.height >= m_db->height()
      || m_db->get_block_hash_from_height(lock.height) != lock.block_hash)
  {
    remember_pending();
    return chainlock_ingress::ignored;
  }
  qwertycoin::epose::chainlock_quorum_v1 quorum;
  if (!(lock_branch.empty() ? get_chainlock_quorum(lock.epoch, quorum)
          : get_alt_chainlock_quorum(lock, lock_branch, quorum)))
  {
    remember_pending();
    return chainlock_ingress::ignored;
  }

  const auto key = std::make_pair(lock.height, lock.block_hash);
  auto partial = m_chainlock_partials.find(key);
  if (partial != m_chainlock_partials.end())
  {
    bool adds_signers = false;
    for (size_t i = 0; i < lock.signers.size(); ++i)
      adds_signers |= (lock.signers[i] & ~partial->second.signers[i]) != 0;
    if (!adds_signers)
      return chainlock_ingress::ignored;
  }

  qwertycoin::epose::chainlock_context_v1 context{};
  if (!get_chainlock_context(context) || !qwertycoin::epose::verify_chainlock_v1(lock, quorum, context, true))
    return chainlock_ingress::ignored;

  if (partial == m_chainlock_partials.end())
  {
    qwertycoin::epose::chainlock_v1 aggregate;
    aggregate.height = lock.height;
    aggregate.block_hash = lock.block_hash;
    aggregate.epoch = lock.epoch;
    partial = m_chainlock_partials.emplace(key, aggregate).first;
  }
  qwertycoin::epose::merge_chainlock_share_v1(partial->second, lock, quorum.threshold());
  relay_blob = qwertycoin::epose::serialize_chainlock_v1(partial->second);
  if (qwertycoin::epose::chainlock_complete_v1(partial->second, quorum))
  {
    m_db->put_chainlock({lock.height, lock.block_hash, false, relay_blob});
    m_chainlock_partials.erase(partial);
    MINFO("Chainlock complete for height " << lock.height << " block " << lock.block_hash);
    if (m_db->get_alt_block(lock.block_hash, nullptr, nullptr))
    {
      const crypto::hash best_tip = best_alternative_descendant(lock.block_hash);
      std::list<block_extended_info> alt_chain;
      std::vector<uint64_t> timestamps;
      block_verification_context bvc{};
      if (build_alt_chain(best_tip, alt_chain, timestamps, bvc)
          && !alt_chain.empty()
          && evaluate_alt_chainlocks(alt_chain).verdict == alt_chainlock_verdict::switch_verified)
        switch_to_alternative_blockchain_with_batch(alt_chain);
    }
  }
  prune_chainlock_partials();
  return chainlock_ingress::accepted;
}
//------------------------------------------------------------------
std::vector<blobdata> Blockchain::retry_pending_chainlocks()
{
  CRITICAL_REGION_LOCAL(m_tx_pool);
  CRITICAL_REGION_LOCAL1(m_blockchain_lock);
  if (!epose_state_at_tip())
    return {};
  std::vector<blobdata> pending;
  pending.swap(m_chainlock_pending);
  std::vector<blobdata> relay;
  for (const blobdata &blob : pending)
  {
    blobdata relay_blob;
    if (handle_incoming_chainlock(blob, relay_blob) == chainlock_ingress::accepted)
      relay.push_back(std::move(relay_blob));
  }
  return relay;
}
//------------------------------------------------------------------
bool Blockchain::get_epose_consensus_parameters_v2(
    qwertycoin::epose::consensus_parameters_v2 &parameters) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  parameters = {};
  if (!m_epose_v2)
    return false;
  parameters = m_epose_v2_parameters;
  return true;
}
//------------------------------------------------------------------
uint64_t Blockchain::get_epose_oldest_retained_epoch_v2() const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (!epose_state_at_tip())
    return 0;
  return m_epose_v2
      ? m_epose_v2->state().membership().oldest_retained_epoch()
      : 0;
}
//------------------------------------------------------------------
bool Blockchain::get_epose_state_diagnostics_v2(
    qwertycoin::epose::state_storage_diagnostics_v2 &diagnostics) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  diagnostics = {};
  if (!m_epose_v2)
    return false;
  diagnostics = m_epose_v2->state_diagnostics();
  diagnostics.rebuilding = !epose_state_at_tip();
  diagnostics.fatal = m_epose_rebuild_fatal;
  diagnostics.retry_exhausted = m_epose_retry_exhausted_tip != crypto::null_hash;
  diagnostics.retry_exhausted_tip = m_epose_retry_exhausted_tip;
  if (!diagnostics.rebuilding && m_db && m_db->height() > 0)
  {
    uint64_t snapshot_height = 0;
    if (m_db->get_epose_snapshot_height(m_db->height() - 1, snapshot_height))
      diagnostics.last_snapshot_height = snapshot_height;
  }
  return true;
}
//------------------------------------------------------------------
bool Blockchain::has_epose_admission_v2(
    const crypto::hash &identity_id, uint64_t target_epoch) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (!epose_state_at_tip())
    return false;
  return m_epose_v2 && m_epose_v2->state().membership().has_admission(
      identity_id, target_epoch);
}
//------------------------------------------------------------------
std::vector<qwertycoin::epose::verifier_assignment_v2>
Blockchain::get_epose_committee_v2(
    uint64_t epoch, uint64_t round,
    const crypto::public_key &subject_public_key,
    const crypto::hash &round_anchor_hash) const
{
  return query_epose_v2_at_tip(
      std::vector<qwertycoin::epose::verifier_assignment_v2>{},
      [&](auto &coordinator) {
        return coordinator.state().membership().committee(
            epoch, round, subject_public_key, round_anchor_hash);
      });
}
//------------------------------------------------------------------
bool Blockchain::has_epose_receipt_slot_v2(
    uint64_t epoch, uint64_t round,
    const crypto::public_key &subject_public_key,
    const crypto::public_key &verifier_public_key) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (!epose_state_at_tip())
    return false;
  return m_epose_v2 && m_epose_v2->state().membership().has_receipt_slot(
      epoch, round, subject_public_key, verifier_public_key);
}
//------------------------------------------------------------------
bool Blockchain::get_epose_receipt_coverage_v2(
    const uint64_t epoch,
    const crypto::public_key &subject_public_key,
    std::vector<size_t> &coverage) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (!epose_state_at_tip())
    return false;
  coverage.clear();
  if (!m_epose_v2)
    return false;
  coverage = m_epose_v2->state().membership().receipt_coverage(
      epoch, subject_public_key);
  return true;
}
//------------------------------------------------------------------
uint64_t Blockchain::get_epose_attestation_count() const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (!epose_state_at_tip())
    return 0;
  if (m_epose_v2)
    return m_epose_v2->state().membership().receipt_count();
  if (!m_epose_state)
    return 0;
  return m_epose_state->attestations().size();
}
//------------------------------------------------------------------
crypto::hash Blockchain::get_epose_state_hash() const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (!epose_state_at_tip())
    return crypto::null_hash;
  if (m_epose_v2)
    return m_epose_v2->state().state_hash();
  if (!m_epose_state)
    return crypto::null_hash;
  return m_epose_state->state_hash();
}
//------------------------------------------------------------------
crypto::hash Blockchain::get_epose_epoch_context_hash(uint64_t epoch) const
{
  if (!m_db || m_db->height() == 0)
    return crypto::null_hash;

  const uint64_t seed_height = qwertycoin::epose::epoch_seed_height(epoch);
  if (seed_height >= m_db->height())
    return crypto::null_hash;

  return get_block_id_by_height(seed_height);
}
//------------------------------------------------------------------
bool Blockchain::next_template_reward_inputs_v2(uint64_t height,
    size_t &median_weight, uint64_t &already_generated_coins) const
{
  if (!m_db || height == 0 || height != m_db->height())
    return false;
  median_weight = m_current_block_cumul_weight_limit / 2;
  already_generated_coins = m_db->get_block_already_generated_coins(height - 1);
  return true;
}
//------------------------------------------------------------------
bool Blockchain::plan_epose_reward_v2(
    uint8_t major_version,
    uint64_t height,
    const crypto::hash &parent_hash,
    uint64_t scheduled_subsidy,
    uint64_t fees,
    qwertycoin::epose::coordinator_reward_plan_v2 &plan) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  plan = {};
  if (!epose_state_at_tip() || !m_epose_v2)
    return false;

  epose_context_source contexts(
      main_chain_context_source(*this, m_epose_v2_parameters));

  return m_epose_v2->plan_reward(
      major_version, height, parent_hash, scheduled_subsidy, fees, contexts, plan)
      == qwertycoin::epose::coordinator_status_v2::accepted;
}
//------------------------------------------------------------------
bool Blockchain::preview_epose_reward_v2(
    uint64_t height, qwertycoin::epose::coordinator_reward_plan_v2 &plan) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  plan = {};
  if (!m_db || height == 0 || height != m_db->height() || !epose_state_at_tip())
    return false;

  const uint8_t major_version = get_ideal_hard_fork_version(height);
  size_t median_weight = 0;
  uint64_t already_generated_coins = 0;
  uint64_t scheduled_subsidy = 0;
  return next_template_reward_inputs_v2(height, median_weight,
             already_generated_coins)
      && get_block_reward(median_weight, 0, already_generated_coins,
             scheduled_subsidy, major_version)
      && plan_epose_reward_v2(major_version, height, get_tail_id(),
             scheduled_subsidy, 0, plan);
}
//------------------------------------------------------------------
bool Blockchain::get_epose_block_reward_mapping_v2(
    const crypto::hash &block_hash,
    epose_block_reward_mapping_v2 &mapping,
    bool &outside_retained_window,
    bool &busy) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  mapping = {};
  outside_retained_window = false;
  busy = !epose_state_at_tip();
  if (busy)
    return false;
  if (!m_db || !m_epose_v2 || block_hash == crypto::null_hash)
    return false;

  try
  {
    const uint64_t chain_height = m_db->height();
    if (chain_height == 0 || !m_db->block_exists(block_hash))
      return false;
    const uint64_t height = m_db->get_block_height(block_hash);
    if (height >= chain_height || m_db->get_block_hash_from_height(height) != block_hash)
      return false;
    uint64_t source_epoch = 0;
    if (!get_epose_reward_source_epoch_v2(height, source_epoch))
      return false;
    if (m_epose_v2->state().membership().read_epoch(source_epoch)
        == qwertycoin::epose::pipeline_status_v2::outside_retained_window)
    {
      outside_retained_window = true;
      return false;
    }

    const block bl = m_db->get_block_from_height(height);
    if (get_block_hash(bl) != block_hash || bl.major_version != get_ideal_hard_fork_version(height))
      return false;

    std::vector<transaction> txs = m_db->get_tx_list(bl.tx_hashes);
    if (txs.size() != bl.tx_hashes.size())
      return false;
    uint64_t fees = 0;
    uint64_t contract_mints = 0;
    if (!sum_contract_miner_fees(txs, bl.major_version, fees) || !get_vm_minted(height, bl.major_version, contract_mints))
      return false;

    const uint8_t version = bl.major_version;
    const size_t short_count = static_cast<size_t>(
        std::min<uint64_t>(height, CRYPTONOTE_REWARD_BLOCKS_WINDOW));
    const uint64_t short_start = height - short_count;
    std::vector<uint64_t> short_weights = short_count == 0
        ? std::vector<uint64_t>{}
        : m_db->get_block_weights(short_start, short_count);
    const uint64_t short_median = median_or_zero(short_weights);
    uint64_t median_weight = 0;
    if (!block_reward_median_weight(version, short_median,
            [&](uint64_t &long_median) {
              const size_t long_count = static_cast<size_t>(
                  std::min<uint64_t>(height, m_long_term_block_weights_window));
              const uint64_t long_start = height - long_count;
              std::vector<uint64_t> long_weights = long_count == 0
                  ? std::vector<uint64_t>{}
                  : m_db->get_long_term_block_weights(long_start, long_count);
              long_median = median_or_zero(long_weights);
              return true;
            },
            median_weight))
      return false;
    median_weight = std::max<uint64_t>(
        median_weight, get_min_block_weight(version));

    const uint64_t already_generated = height == 0
        ? 0 : m_db->get_block_already_generated_coins(height - 1);
    uint64_t scheduled_subsidy = 0;
    if (!get_block_reward(
            median_weight, m_db->get_block_weight(height),
            already_generated, scheduled_subsidy, version))
      return false;

    qwertycoin::epose::coordinator_reward_plan_v2 plan{};
    if (!plan_epose_reward_v2(
            bl.major_version, height, bl.prev_id, scheduled_subsidy, fees, plan))
      return false;

    uint64_t actual_coinbase_total = 0;
    for (const tx_out &output : bl.miner_tx.vout)
    {
      if (actual_coinbase_total
          > std::numeric_limits<uint64_t>::max() - output.amount)
        return false;
      actual_coinbase_total += output.amount;
    }
    if (actual_coinbase_total < contract_mints
        || actual_coinbase_total - contract_mints != plan.allocation.coinbase_total) // H-30
      return false;

    qwertycoin::epose::service_payment_context_v2 payment{};
    if (plan.has_service_payee
        && qwertycoin::epose::verify_coinbase_service_payment_v2(
               bl.miner_tx, bl.major_version,
               m_epose_v2_parameters.limits.max_envelopes_per_transaction,
               m_epose_v2_parameters.limits.envelope,
               plan.expectation, payment) !=
            qwertycoin::epose::reward_status_v2::accepted)
      return false;

    const auto *qualification =
        m_epose_v2->state().membership().qualification(source_epoch);
    if (qualification == nullptr)
      return false;
    if (plan.has_service_payee
        && qualification->qualification_hash != plan.expectation.qualification_hash)
      return false;

    mapping.height = height;
    mapping.payout_epoch = plan.has_service_payee
        ? plan.expectation.payout_epoch
        : source_epoch + 1;
    mapping.source_epoch = source_epoch;
    mapping.qualified_count = qualification->qualified_nodes.size();
    mapping.block_hash = block_hash;
    mapping.parent_hash = bl.prev_id;
    mapping.qualification_hash = qualification->qualification_hash;
    mapping.allocation = plan.allocation;
    mapping.has_service_payee = plan.has_service_payee;
    mapping.payment = std::move(payment);
    return true;
  }
  catch (const std::exception &error)
  {
    MWARNING("Failed to resolve canonical EPoSE-v2 block reward mapping: "
        << error.what());
    mapping = {};
    return false;
  }
}
//------------------------------------------------------------------
bool Blockchain::select_epose_template_records_v2(
    uint64_t height,
    std::vector<qwertycoin::epose::envelope_record_v2> &records)
{
  records.clear();
  if (!m_epose_v2_relay_pool)
    return true;
  m_epose_v2_relay_pool->prune_expired(height);

  epose_context_source contexts(
      main_chain_context_source(*this, m_epose_v2_parameters));

  m_epose_v2_relay_pool->prune_unusable_receipts(
      height, m_epose_v2->preview_state(), contexts);
  std::vector<qwertycoin::epose::relay_record_selection_v2> selected;
  if (m_epose_v2_relay_pool->select_for_template(height, selected, &m_epose_v2->state())
      != qwertycoin::epose::relay_record_status_v2::accepted)
    return false;

  qwertycoin::epose::semantic_state_v2 &semantic = m_epose_v2->preview_state();
  auto undo = semantic.checkpoint();
  BOOST_SCOPE_EXIT_ALL(&) { semantic.rollback(undo); };
  std::vector<qwertycoin::epose::envelope_record_v2> candidates;
  for (const auto &selection : selected)
    candidates.push_back(selection.record);
  std::vector<bool> included(candidates.size());
  for (const auto &group : qwertycoin::epose::enrollment_template_groups_v2(
      candidates, semantic.nettype(), semantic.genesis_hash(), semantic.parameter_set_hash()))
  {
    std::vector<qwertycoin::epose::envelope_record_v2> batch;
    for (auto i : group)
      if (!included[i])
        batch.push_back(candidates[i]);
    qwertycoin::epose::semantic_apply_summary_v2 summary{};
    const qwertycoin::epose::semantic_transaction_context_v2 transaction{
        height, true, nullptr, nullptr};
    if (semantic.apply_transaction(
            batch, transaction, contexts, summary, &undo)
        != qwertycoin::epose::semantic_status_v2::accepted)
      continue;
    records.insert(records.end(), batch.begin(), batch.end());
    for (auto i : group)
      included[i] = true;
  }
  return true;
}
//------------------------------------------------------------------
bool Blockchain::apply_epose_block(
    const block &bl,
    const std::vector<std::pair<transaction, blobdata>> &txs,
    uint64_t height,
    size_t block_weight,
    uint64_t scheduled_subsidy,
    uint64_t fees,
    uint64_t contract_mints,
    epose_state_commitment_v2 &commitment,
    std::vector<qwertycoin::epose::envelope_record_v2> &confirmed_records)
{
  commitment = {};
  confirmed_records.clear();
  if (!is_epose_enabled_for_height(height))
    return true;
  if (!epose_state_at_tip())
    return false;
  if (!m_epose_v2)
    return m_nettype == FAKECHAIN;

  epose_context_source contexts(
      main_chain_context_source(*this, m_epose_v2_parameters));

  std::vector<const transaction *> transactions;
  transactions.reserve(txs.size());
  for (const auto &entry : txs)
    transactions.push_back(&entry.first);
  if (m_epose_v2_relay_pool)
  {
    std::vector<const transaction *> all_transactions;
    all_transactions.reserve(transactions.size() + 1);
    all_transactions.push_back(&bl.miner_tx);
    all_transactions.insert(
        all_transactions.end(), transactions.begin(), transactions.end());
    for (const transaction *tx : all_transactions)
    {
      std::vector<qwertycoin::epose::envelope_record_v2> parsed;
      qwertycoin::epose::envelope_budget_v2 ignored{};
      if (qwertycoin::epose::parse_transaction_extra_v2(
              tx->extra, bl.major_version,
              m_epose_v2_parameters.limits.max_envelopes_per_transaction,
              m_epose_v2_parameters.limits.envelope,
              parsed, ignored, tx == &bl.miner_tx ? contract_extra_scope::coinbase : contract_extra_scope::transaction) != qwertycoin::epose::envelope_status_v2::accepted)
        return false;
      confirmed_records.insert(
          confirmed_records.end(), parsed.begin(), parsed.end());
    }
  }
  epose_block_inputs inputs{};
  const uint64_t already_generated = height == 0
      ? 0 : m_db->get_block_already_generated_coins(height - 1);
  const auto canonical_weight = canonical_weight_reader(*m_db);
  if (!derive_epose_block_inputs(bl.major_version, height,
          block_weight,
          already_generated, contract_mints, m_long_term_block_weights_window,
          transactions, canonical_weight, inputs)
      || inputs.scheduled_subsidy != scheduled_subsidy
      || inputs.fees != fees || inputs.mints != contract_mints)
    return false;
  const qwertycoin::epose::coordinator_block_v2 candidate{
      bl.major_version,
      height,
      get_block_hash(bl),
      bl.prev_id,
      inputs.scheduled_subsidy,
      inputs.fees,
      &bl.miner_tx,
      transactions,
      inputs.mints};
  qwertycoin::epose::coordinator_result_v2 result{};
  if (m_epose_v2->connect_block(candidate, contexts, result)
      != qwertycoin::epose::coordinator_status_v2::accepted)
  {
    MERROR_VER("Block at height " << height
        << " failed hardened EPoSE-v2 consensus validation");
    return false;
  }
  ++m_epose_state_validation_generation;
  commitment.schema_version = result.state_commitment_schema;
  commitment.block_hash = candidate.block_hash;
  commitment.state_hash = result.state_hash;
  commitment.parameter_set_hash = result.parameter_set_hash;
  return true;
}
//------------------------------------------------------------------
void Blockchain::rollback_epose_block(uint64_t popped_height)
{
  if (!m_epose_v2 || m_epose_rebuild_deferred)
    return;
  ++m_epose_state_validation_generation;
  const auto status = m_epose_v2->disconnect_tip(popped_height);
  if (status == qwertycoin::epose::block_transition_status_v2::deep_replay_required)
  {
    if (!try_rebuild_epose_state())
    {
      MERROR("Failed to replay EPoSE state after deep rollback at height " << popped_height);
      schedule_epose_recovery();
    }
  }
  else if (status != qwertycoin::epose::block_transition_status_v2::accepted)
  {
    MERROR("Failed to disconnect EPoSE state at height " << popped_height);
    schedule_epose_recovery();
  }
}
//------------------------------------------------------------------
bool Blockchain::rebuild_epose_state(bool full_rebuild, uint64_t target_height, std::unique_ptr<qwertycoin::epose::consensus_coordinator_v2> *scratch)
{
  if (!m_db)
    return false;
  if (!m_epose_v2)
    return m_nettype == FAKECHAIN;

  epose_context_source contexts(
      main_chain_context_source(*this, m_epose_v2_parameters));

  auto rebuilt = std::make_unique<qwertycoin::epose::consensus_coordinator_v2>(
      m_epose_v2_parameters);
  if (!rebuilt->valid())
    return false;

  const uint64_t chain_height = std::min(target_height, m_db->height());
  uint64_t replay_start = 0;
  bool loaded_snapshot = false;
  if (chain_height > 0 && !full_rebuild)
  {
    uint64_t search_height = chain_height - 1;
    uint64_t snapshot_height = 0;
    std::string image;
    while (m_db->get_epose_snapshot(search_height, snapshot_height, image))
    {
      const crypto::hash canonical_hash = m_db->get_block_hash_from_height(snapshot_height);
      epose_state_commitment_v2 committed{};
      const bool has_commitment = m_db->get_epose_state_commitment_v2(
          snapshot_height, committed);
      const crypto::hash expected_parameters = has_commitment
          && committed.schema_version == QWC_EPOSE_ROLLING_STATE_SCHEMA
          ? m_epose_v2_parameters.window_parameter_set_hash
          : m_epose_v2_parameters.parameter_set_hash;
      qwertycoin::epose::state_snapshot_v2 snapshot{};
      if (snapshot_height >= m_epose_v2_parameters.timing.activation_height
          && has_commitment
          && committed.parameter_set_hash == expected_parameters
          && snapshot.restore(image, expected_parameters)
              == qwertycoin::epose::state_index_status_v2::accepted
          && committed.block_hash == canonical_hash
          && snapshot.matches(committed.schema_version, snapshot_height,
              canonical_hash, committed.state_hash)
          && rebuilt->restore_state(snapshot_height, snapshot.state_image)
          && rebuilt->state().state_hash() == snapshot.state_root)
      {
        loaded_snapshot = true;
        replay_start = snapshot_height + 1;
        break;
      }
      MERROR("EPoSE boundary snapshot mismatch at height " << snapshot_height);
      rebuilt = std::make_unique<qwertycoin::epose::consensus_coordinator_v2>(
          m_epose_v2_parameters);
      if (snapshot_height == 0)
        break;
      search_height = snapshot_height - 1;
    }
  }

  const auto replay_from = [&](uint64_t first_height) -> bool {
    for (uint64_t height = first_height; height < chain_height; ++height)
    {
      if (!is_epose_enabled_for_height(height))
        continue;
      const block bl = m_db->get_block_from_height(height);
      const uint8_t ideal_version = m_hardfork->get_ideal_version(height);
      if (!is_qwc_epose_v2_hardfork(bl.major_version)
          || bl.major_version != ideal_version)
      {
        MERROR("Unexpected block version while replaying EPoSE-v2 at height " << height);
        return false;
      }

      std::vector<transaction> txs = m_db->get_tx_list(bl.tx_hashes);
      if (txs.size() != bl.tx_hashes.size())
        return false;
      uint64_t contract_mints = 0;
      if (!get_vm_minted(height, bl.major_version, contract_mints))
        return false;

      std::vector<const transaction *> transaction_pointers;
      transaction_pointers.reserve(txs.size());
      for (const transaction &tx : txs)
        transaction_pointers.push_back(&tx);
      epose_block_inputs inputs{};
      const auto canonical_weight = canonical_weight_reader(*m_db);
      const uint64_t already_generated = height == 0
          ? 0 : m_db->get_block_already_generated_coins(height - 1);
      if (!derive_epose_block_inputs(bl.major_version, height,
              m_db->get_block_weight(height), already_generated, contract_mints,
              m_long_term_block_weights_window, transaction_pointers,
              canonical_weight, inputs))
        return false;
      const qwertycoin::epose::coordinator_block_v2 candidate{
          bl.major_version, height, get_block_hash(bl), bl.prev_id,
          inputs.scheduled_subsidy, inputs.fees, &bl.miner_tx,
          transaction_pointers, inputs.mints};
      qwertycoin::epose::coordinator_result_v2 result{};
      if (rebuilt->connect_block(candidate, contexts, result)
          != qwertycoin::epose::coordinator_status_v2::accepted)
      {
        MERROR("Failed to replay hardened EPoSE-v2 transition at height " << height);
        return false;
      }

      epose_state_commitment_v2 committed{};
      if (!m_db->get_epose_state_commitment_v2(height, committed)
          || committed.schema_version != result.state_commitment_schema
          || committed.block_hash != candidate.block_hash
          || committed.state_hash != result.state_hash
          || committed.parameter_set_hash != result.parameter_set_hash)
      {
        MERROR("EPoSE-v2 state commitment mismatch at height " << height);
        return false;
      }
    }

    return true;
  };

  if (!replay_from(replay_start))
  {
    if (!loaded_snapshot)
      return false;
    MERROR("EPoSE replay after boundary snapshot failed; rebuilding from canonical blocks");
    rebuilt = std::make_unique<qwertycoin::epose::consensus_coordinator_v2>(
        m_epose_v2_parameters);
    if (!rebuilt->valid() || !replay_from(0))
      return false;
  }

  if (scratch)
    *scratch = std::move(rebuilt);
  else
  {
    m_epose_v2 = std::move(rebuilt);
    ++m_epose_state_validation_generation;
  }
  MINFO("Replayed and verified hardened EPoSE-v2 state through "
      << chain_height << " canonical blocks");
  return true;
}
//------------------------------------------------------------------
bool Blockchain::try_rebuild_epose_state(bool full_rebuild)
{
  try
  {
    if (m_epose_rebuild_injection && !m_epose_rebuild_injection(full_rebuild))
      return false;
    return rebuild_epose_state(full_rebuild);
  }
  catch (const std::exception &error)
  {
    MERROR("EPoSE rebuild failed: " << error.what());
    return false;
  }
  catch (...)
  {
    MERROR("EPoSE rebuild failed with an unknown error");
    return false;
  }
}
//------------------------------------------------------------------
bool Blockchain::epose_state_at_tip() const
{
  if (!m_epose_v2 || !m_db || m_epose_rebuild_fatal)
    return !m_epose_rebuild_fatal && !m_epose_v2;
  const uint64_t height = m_db->height();
  if (height == 0 || height - 1 < m_epose_v2_parameters.timing.activation_height)
    return true;
  const crypto::hash tip = m_db->top_block_hash();
  if (m_epose_state_validation_height == height
      && m_epose_state_validation_tip == tip
      && m_epose_state_validation_checked_generation == m_epose_state_validation_generation
      && m_epose_state_validation_coordinator_generation == m_epose_v2->generation())
    return m_epose_state_validation_result;
  uint64_t state_height = 0;
  epose_state_commitment_v2 commitment{};
  const bool aligned = m_epose_v2->tip_height(state_height)
      && state_height == height - 1
      && m_db->get_epose_state_commitment_v2(state_height, commitment)
      && commitment.block_hash == tip;
  const bool result = aligned && commitment.state_hash == m_epose_v2->state_hash();
  m_epose_state_validation_height = height;
  m_epose_state_validation_tip = tip;
  m_epose_state_validation_checked_generation = m_epose_state_validation_generation;
  m_epose_state_validation_coordinator_generation = m_epose_v2->generation();
  m_epose_state_validation_result = result;
  return result;
}
//------------------------------------------------------------------
void Blockchain::schedule_epose_recovery()
{
  if ((epose_state_at_tip() && !m_epose_retry_best_chain)
      || m_epose_rebuild_retry_scheduled || m_epose_rebuild_shutdown
      || m_epose_rebuild_fatal)
    return;
  m_epose_rebuild_retry_scheduled = true;
  m_epose_rebuild_timer.expires_after(std::chrono::seconds(1));
  m_epose_rebuild_timer.async_wait([this](const boost::system::error_code &error) {
    if (m_epose_rebuild_shutdown)
      return;
    const auto mark_fatal = [this] {
      CRITICAL_REGION_LOCAL(m_tx_pool);
      CRITICAL_REGION_LOCAL1(m_blockchain_lock);
      m_epose_rebuild_retry_scheduled = false;
      m_epose_rebuild_fatal = true;
    };
    try
    {
      const auto recover = [this, &error] {
        CRITICAL_REGION_LOCAL(m_tx_pool);
        CRITICAL_REGION_LOCAL1(m_blockchain_lock);
        m_epose_rebuild_retry_scheduled = false;
        if (error == boost::asio::error::operation_aborted)
          return;
        if (error)
          MERROR("EPoSE recovery timer failed: " << error.message());
        const auto resume_best_chain_retry = [this] {
          m_epose_rebuild_attempts = 0;
          retry_best_epose_chain();
        };
        if (epose_state_at_tip())
        {
          resume_best_chain_retry();
          return;
        }
        const bool full = m_epose_rebuild_attempts >= 3;
        ++m_epose_rebuild_attempts;
        if (try_rebuild_epose_state(full) && epose_state_at_tip())
        {
          resume_best_chain_retry();
          return;
        }
        if (full)
        {
          m_epose_rebuild_fatal = true;
          MERROR("FATAL: EPoSE state recovery failed from boundary snapshots and genesis; block validation is stopped");
          return;
        }
        schedule_epose_recovery();
      };
      if (m_epose_recovery_intake)
        m_epose_recovery_intake(recover);
      else if (m_epose_retry_best_chain)
      {
        m_epose_rebuild_retry_scheduled = false;
        m_epose_rebuild_fatal = true;
      }
      else
        recover();
    }
    catch (const std::exception &failure)
    {
      MERROR("EPoSE recovery failed: " << failure.what());
      mark_fatal();
    }
    catch (...)
    {
      MERROR("EPoSE recovery failed");
      mark_fatal();
    }
  });
}
//------------------------------------------------------------------
bool Blockchain::retry_best_epose_chain()
{
  if (!m_epose_retry_best_chain || !epose_state_at_tip())
    return true;
  m_epose_retry_best_chain = false;
  auto tips = get_alternative_tips();
  const bool hf18 = is_qwc_contracts_hardfork(get_current_hard_fork_version());
  if (hf18)
    std::sort(tips.begin(), tips.end(), [](const alternative_tip &left,
        const alternative_tip &right) {
      return left.cumulative_difficulty > right.cumulative_difficulty
          || (left.cumulative_difficulty == right.cumulative_difficulty
              && left.hash < right.hash);
    });
  difficulty_type best_difficulty = m_db->get_block_cumulative_difficulty(m_db->height() - 1);
  crypto::hash best_tip{};
  bool best_verified = false;
  epose_retry_priority best_priority{};
  uint64_t main_lock_height = 0;
  crypto::hash main_lock_hash{};
  const bool has_main_lock = hf18
      && get_highest_main_chainlock(main_lock_height, main_lock_hash);
  std::list<block_extended_info> best;
  std::unordered_map<crypto::hash, alt_chainlock_evaluation> lock_evaluations;
  bool found_best = false;
  for (const auto &candidate : tips)
  {
    if (hf18 && !candidate.potential_lock
        && (best_verified
            || (has_main_lock && main_lock_height >= candidate.split_height)
            || candidate.cumulative_difficulty < best_difficulty
            || (candidate.cumulative_difficulty == best_difficulty
                && (!found_best || !(candidate.hash < best_tip)))))
      continue;
    std::list<block_extended_info> branch;
    std::vector<uint64_t> timestamps;
    block_verification_context bvc{};
    const crypto::hash tip = candidate.hash;
    alt_chainlock_evaluation evaluation{alt_chainlock_verdict::difficulty};
    if (!hf18 || candidate.potential_lock)
    {
      const auto cached = hf18 ? lock_evaluations.find(candidate.lock_source)
          : lock_evaluations.end();
      if (cached != lock_evaluations.end())
        evaluation = cached->second;
      else
      {
        if (!build_alt_chain(tip, branch, timestamps, bvc) || branch.empty())
          continue;
        evaluation = evaluate_alt_chainlocks(branch);
        if (hf18)
          lock_evaluations.emplace(candidate.lock_source, evaluation);
      }
    }
    if (evaluation.verdict == alt_chainlock_verdict::blocked)
      continue;
    if (!hf18)
    {
      if (evaluation.verdict == alt_chainlock_verdict::difficulty
          && candidate.cumulative_difficulty <= best_difficulty)
        continue;
      best_difficulty = candidate.cumulative_difficulty;
      best = std::move(branch);
      found_best = true;
      continue;
    }
    const bool verified = evaluation.verdict == alt_chainlock_verdict::switch_verified;
    const epose_retry_priority priority{verified, evaluation.verified_height,
        evaluation.verified_hash, candidate.cumulative_difficulty, tip};
    if (found_best && !better_epose_retry_priority(priority, best_priority))
      continue;
    if (!found_best && !verified
        && candidate.cumulative_difficulty <= best_difficulty)
      continue;
    if (branch.empty()
        && (!build_alt_chain(tip, branch, timestamps, bvc) || branch.empty()))
      continue;
    best_verified = verified;
    best_priority = priority;
    best_difficulty = candidate.cumulative_difficulty;
    best_tip = tip;
    best = std::move(branch);
    found_best = true;
  }
  if (!best.empty())
  {
    const crypto::hash tip = get_block_hash(best.back().bl);
    if (m_epose_retry_chain_attempts.size() >= 64
        && m_epose_retry_chain_attempts.count(tip) == 0)
      m_epose_retry_chain_attempts.erase(m_epose_retry_chain_attempts.begin());
    unsigned &attempts = m_epose_retry_chain_attempts[tip];
    if (attempts >= 3)
    {
      m_epose_retry_exhausted_tip = tip;
      MERROR("EPoSE alternative chain retry limit reached for " << tip);
      return true;
    }
    ++attempts;
    if (!switch_to_alternative_blockchain_with_batch(best))
    {
      MERROR("Recovered EPoSE state, but the best alternative chain did not pass revalidation");
      if (m_epose_switch_busy)
      {
        if (m_epose_switch_mutated)
          m_batch_success = false;
        return false;
      }
    }
    if (m_epose_retry_chain_attempts.size() > 64)
      m_epose_retry_chain_attempts.erase(m_epose_retry_chain_attempts.begin());
  }
  return true;
}
//------------------------------------------------------------------
template <typename Step>
Blockchain::epose_preflight_result Blockchain::guard_epose_preflight_step(
    const char *failure_message, Step &&step)
{
  try
  {
    return step();
  }
  catch (const std::bad_alloc &)
  {
    return epose_preflight_result::busy;
  }
  catch (const std::exception &failure)
  {
    MERROR(failure_message << failure.what());
    return epose_preflight_result::invalid;
  }
}
//------------------------------------------------------------------
Blockchain::epose_preflight_result Blockchain::preflight_epose_chain(
    const std::list<block_extended_info> &alt_chain,
    std::unique_ptr<qwertycoin::epose::consensus_coordinator_v2> &scratch,
    crypto::hash &failed_block, uint64_t anchor_height,
    std::unique_ptr<qwertycoin::epose::consensus_coordinator_v2> *anchor_state)
{
  ++m_epose_preflight_runs;
  failed_block = crypto::null_hash;
  if (!m_epose_v2)
    return epose_preflight_result::accepted;
  if (!epose_state_at_tip() || alt_chain.empty())
    return epose_preflight_result::busy;
  if (std::none_of(alt_chain.begin(), alt_chain.end(),
          [this](const block_extended_info &entry) {
            return is_epose_enabled_for_height(entry.height);
          }))
    return epose_preflight_result::accepted;
  const uint64_t split_height = alt_chain.front().height;
  if ((m_epose_rebuild_injection && !m_epose_rebuild_injection(false))
      || !rebuild_epose_state(false, split_height, &scratch))
    return epose_preflight_result::busy;
  auto candidate = std::make_unique<qwertycoin::epose::consensus_coordinator_v2>(m_epose_v2_parameters);
  if (!candidate->valid())
    return epose_preflight_result::busy;
  if (split_height > m_epose_v2_parameters.timing.activation_height
      && !candidate->restore_state(split_height - 1, scratch->serialize_state()))
    return epose_preflight_result::busy;

  epose_context_source contexts(m_epose_v2_parameters,
      [this, &alt_chain](uint64_t height, crypto::hash &hash) {
        for (const auto &entry : alt_chain)
          if (entry.height == height)
          {
            hash = get_block_hash(entry.bl);
            return true;
          }
        if (height >= alt_chain.front().height)
          return false;
        hash = get_block_id_by_height(height);
        return hash != crypto::null_hash;
      });

  std::unordered_map<uint64_t, const block_extended_info *> branch_blocks;
  std::unordered_map<uint64_t, uint64_t> branch_long_weights;
  for (const auto &entry : alt_chain)
    branch_blocks.emplace(entry.height, &entry);
  uint64_t generated_before = split_height == 0 ? 0
      : m_db->get_block_already_generated_coins(split_height - 1);

  for (const auto &entry : alt_chain)
  {
    if (entry.height > anchor_height)
      break;
    const block &bl = entry.bl;
    failed_block = get_block_hash(bl);
    if (!is_epose_enabled_for_height(entry.height))
    {
      generated_before = entry.already_generated_coins;
      continue;
    }
    if (!is_qwc_epose_v2_hardfork(bl.major_version))
      return epose_preflight_result::invalid;
    std::vector<transaction> txs;
    txs.reserve(bl.tx_hashes.size());
    for (const crypto::hash &id : bl.tx_hashes)
    {
      transaction tx{};
      if (!m_db->get_tx(id, tx))
      {
        blobdata blob;
        if (!m_tx_pool.get_transaction(id, blob, relay_category::legacy))
        {
          if (!m_tx_pool.get_expired_window_tx(id, tx))
            return epose_preflight_result::busy;
        }
        else
        {
          const epose_preflight_result parsed = guard_epose_preflight_step(
              "EPoSE alternative transaction parsing failed: ", [&] {
                return parse_and_validate_tx_from_blob(blob, tx)
                    ? epose_preflight_result::accepted
                    : epose_preflight_result::invalid;
              });
          if (parsed != epose_preflight_result::accepted)
            return parsed;
        }
      }
      txs.push_back(std::move(tx));
    }
    if (is_qwc_contracts_hardfork(bl.major_version) && !anchor_state)
      return epose_preflight_result::accepted;
    if (entry.height > split_height && !anchor_state)
      return epose_preflight_result::accepted;
    std::vector<const transaction *> pointers;
    pointers.reserve(txs.size());
    for (const transaction &tx : txs)
      pointers.push_back(&tx);
    const auto branch_weight = [this, split_height, &branch_blocks, &branch_long_weights](uint64_t index,
        bool long_term, uint64_t &weight) {
      if (index >= split_height)
      {
        if (long_term)
        {
          const auto found = branch_long_weights.find(index);
          if (found == branch_long_weights.end())
            return false;
          weight = found->second;
          return true;
        }
        const auto found = branch_blocks.find(index);
        if (found == branch_blocks.end())
          return false;
        weight = found->second->block_cumulative_weight;
        return true;
      }
      try
      {
        weight = long_term ? m_db->get_block_long_term_weight(index)
            : m_db->get_block_weight(index);
        return true;
      }
      catch (const std::exception &failure)
      {
        MERROR("EPoSE alternative weight lookup failed: " << failure.what());
        return false;
      }
    };
    if (bl.major_version >= HF_VERSION_LONG_TERM_BLOCK_WEIGHT)
    {
      const uint64_t count = std::min<uint64_t>(entry.height, m_long_term_block_weights_window);
      std::vector<uint64_t> weights;
      weights.reserve(count);
      for (uint64_t index = entry.height - count; index < entry.height; ++index)
      {
        uint64_t weight = 0;
        if (!branch_weight(index, true, weight))
          return epose_preflight_result::busy;
        weights.push_back(weight);
      }
      const uint64_t median = weights.empty() ? 0 : epee::misc_utils::median(weights);
      branch_long_weights.emplace(entry.height,
          constrained_long_term_block_weight(entry.block_cumulative_weight, median, bl.major_version));
    }
    epose_block_inputs inputs{};
    const epose_preflight_result derived = guard_epose_preflight_step(
        "EPoSE alternative reward inputs failed: ", [&] {
          return derive_epose_block_inputs(bl.major_version, entry.height,
                     entry.block_cumulative_weight, generated_before, 0,
                     m_long_term_block_weights_window, pointers, branch_weight,
                     inputs)
              ? epose_preflight_result::accepted
              : epose_preflight_result::busy;
        });
    if (derived != epose_preflight_result::accepted)
      return derived;
    const qwertycoin::epose::coordinator_block_v2 candidate_block{
        bl.major_version, entry.height, get_block_hash(bl), bl.prev_id,
        inputs.scheduled_subsidy, inputs.fees, &bl.miner_tx, pointers,
        inputs.mints};
    qwertycoin::epose::coordinator_result_v2 result{};
    qwertycoin::epose::coordinator_status_v2 status{};
    const epose_preflight_result connected = guard_epose_preflight_step(
        "EPoSE alternative block transition failed: ", [&] {
          status = candidate->connect_block(candidate_block, contexts, result);
          return epose_preflight_result::accepted;
        });
    if (connected != epose_preflight_result::accepted)
      return connected;
    if (status != qwertycoin::epose::coordinator_status_v2::accepted)
      return epose_preflight_result::invalid;
    generated_before = entry.already_generated_coins;
  }
  if (anchor_state)
    *anchor_state = std::move(candidate);
  failed_block = crypto::null_hash;
  return epose_preflight_result::accepted;
}
//------------------------------------------------------------------
bool Blockchain::get_contract_block_context(uint64_t height, contract_block_context &context) const
{
  context = {};
  if (height == 0 || height > m_db->height())
    return false;
  context.height = height;
  context.network = m_nettype;
  context.parent.height = height - 1;
  context.parent.hash = m_db->get_block_hash_from_height(height - 1);
  context.parent.prev = height >= 2 ? m_db->get_block_hash_from_height(height - 2) : crypto::null_hash;
  context.parent.timestamp = m_db->get_block_timestamp(height - 1);
  bind_hdr_info_hf18(*this, context);
  if (m_header_provider_override)
    context.headers = m_header_provider_override;
  return true;
}
//------------------------------------------------------------------
bool Blockchain::make_vm_commitment(uint64_t height, const contract_effects &effects, vm_block_commitment &commitment) const
{
  vm_block_commitment previous{};
  if (height > 0)
    m_db->get_vm_commitment(height - 1, previous);
  if (previous.gas_used > std::numeric_limits<uint64_t>::max() - effects.gas_used
      || previous.deposited > std::numeric_limits<uint64_t>::max() - effects.deposited
      || previous.withdrawn > std::numeric_limits<uint64_t>::max() - effects.withdrawn)
    return false;
  commitment.root = m_vm_state.get_root();
  commitment.gas_used = effects.gas_used;
  commitment.deposited = previous.deposited + effects.deposited;
  commitment.withdrawn = previous.withdrawn + effects.withdrawn;
  return commitment.withdrawn <= commitment.deposited;
}
//------------------------------------------------------------------
bool Blockchain::get_vm_minted(uint64_t height, uint8_t version, uint64_t &minted) const
{
  minted = 0;
  if (!is_qwc_contracts_hardfork(version))
    return true;
  vm_block_commitment current{};
  vm_block_commitment previous{};
  if (!m_db->get_vm_commitment(height, current))
    return false;
  if (height > 0)
    m_db->get_vm_commitment(height - 1, previous);
  if (current.withdrawn < previous.withdrawn)
    return false;
  minted = current.withdrawn - previous.withdrawn;
  return true;
}
//------------------------------------------------------------------
contract_call_verdict Blockchain::dry_run_contract_tx(const transaction &tx, uint64_t fee, contract_effects &effects, bool verify_signatures, uint64_t *height, const std::function<void()> &after_snapshot) const
{
  effects = {};
  contract_block_context context{};
  vm::state_tree state;
  {
    CRITICAL_REGION_LOCAL(m_blockchain_lock);
    if (!get_contract_block_context(m_db->height(), context))
      return contract_call_verdict::failed;
    state = m_vm_state;
  }
  if (height)
    *height = context.height;
  if (after_snapshot)
    after_snapshot();
  contract_block_executor executor(state, context);
  const contract_call_verdict verdict = executor.execute(tx, fee, verify_signatures);
  effects = executor.effects();
  return verdict;
}
//------------------------------------------------------------------
bool Blockchain::get_vm_tip_commitment(uint64_t &height, vm_block_commitment &commitment) const
{
  return get_vm_commitment_at(boost::none, height, commitment);
}
//------------------------------------------------------------------
bool Blockchain::get_vm_commitment_at(boost::optional<uint64_t> requested_height, uint64_t &height, vm_block_commitment &commitment) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  commitment = {};
  const uint64_t chain_height = m_db->height();
  height = requested_height ? *requested_height : (chain_height ? chain_height - 1 : 0);
  return height < chain_height && m_db->get_vm_commitment(height, commitment);
}
//------------------------------------------------------------------
bool Blockchain::get_vm_state_proof(const std::string &key, crypto::hash &root, boost::optional<std::string> &value, vm::state_proof &proof) const
{
  uint64_t height = 0;
  return get_vm_state_proof_at(boost::none, key, height, root, value, proof) == vm_proof_result::ok;
}
//------------------------------------------------------------------
void Blockchain::with_restored_vm_state(const vm::write_set &restore, const std::function<void()> &operation) const
{
  std::exception_ptr failure;
  try
  {
    operation();
  }
  catch (...)
  {
    failure = std::current_exception();
  }
  try
  {
    if (m_vm_restore_hook)
      m_vm_restore_hook();
    m_vm_state.apply(restore);
  }
  catch (...)
  {
    m_vm_state_stale = true;
    if (!failure)
      failure = std::current_exception();
  }
  if (failure)
    std::rethrow_exception(failure);
}
//------------------------------------------------------------------
vm_proof_result Blockchain::get_vm_state_proof_at(boost::optional<uint64_t> requested_height, const std::string &key, uint64_t &height,
    crypto::hash &root, boost::optional<std::string> &value, vm::state_proof &proof) const
{
  const uint64_t observed_height = m_db->height();
  const bool historical = requested_height && observed_height != 0 && *requested_height < observed_height - 1;
  historical_vm_proof_guard guard(m_historical_vm_proof_in_flight, historical);
  if (!guard.acquired())
    return vm_proof_result::busy;
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (m_vm_state_stale)
    return vm_proof_result::rebuilding;
  const uint64_t chain_height = m_db->height();
  if (chain_height == 0)
    return vm_proof_result::unavailable;
  const uint64_t tip = chain_height - 1;
  height = requested_height ? *requested_height : tip;
  if (height > tip)
    return vm_proof_result::unavailable;
  if (tip - height > QWC_VM_UNDO_RETENTION_BLOCKS)
    return vm_proof_result::outside_retention;

  vm::write_set restore;
  vm_proof_result result = vm_proof_result::ok;
  with_restored_vm_state(restore, [&]() {
    if (height < tip)
    {
      for (uint64_t current = tip; current > height; --current)
      {
        vm::write_set undo;
        if (!m_db->get_vm_undo(current, undo)
            && is_qwc_contracts_hardfork(m_db->get_block_from_height(current).major_version))
        {
          result = vm_proof_result::unavailable;
          return;
        }
        for (const auto &entry : undo)
        {
          if (restore.count(entry.first) != 0)
            continue;
          std::string previous;
          if (m_vm_state.get(entry.first, previous))
            restore.emplace(entry.first, std::move(previous));
          else
            restore.emplace(entry.first, boost::none);
        }
        m_vm_state.apply(undo);
      }
    }
    crypto::hash claimed_root{};
    const block target = m_db->get_block_from_height(height);
    const size_t roots = get_tx_vm_state_root(target.miner_tx.extra, claimed_root);
    if (roots != 1 && height != 0 && (height < tip || is_qwc_contracts_hardfork(target.major_version)))
    {
      result = vm_proof_result::unavailable;
      return;
    }
    root = m_vm_state.get_root();
    if (roots == 1 && root != claimed_root)
    {
      MERROR("VM state root mismatch at height " << height << ": claimed " << claimed_root << ", actual " << root);
      if (height == tip)
        m_vm_state_stale = true;
      result = height == tip ? vm_proof_result::rebuilding : vm_proof_result::unavailable;
      return;
    }
    std::string stored;
    value = m_vm_state.get(key, stored) ? boost::optional<std::string>(stored) : boost::none;
    proof = m_vm_state.get_proof(key);
  });
  return result;
}
//------------------------------------------------------------------
chainlock_proof_result Blockchain::get_chainlock_proof(uint64_t requested_height, chainlock_proof &result,
    bool allow_reconstruct) const
{
  boost::unique_lock<epee::critical_section> blockchain_lock(m_blockchain_lock);
  const uint64_t chain_height = m_db->height();
  uint64_t carrier_height = 0;
  chainlock_record carrier;
  if (!m_db->get_last_chainlock_carrier(carrier_height, carrier))
    return chainlock_proof_result::no_lock;
  if (requested_height != 0)
  {
    if (requested_height >= chain_height || requested_height > carrier.height)
      return chainlock_proof_result::height_not_locked;
    if (!m_db->get_chainlock_carrier(requested_height, carrier_height, carrier))
      return chainlock_proof_result::height_not_locked;
  }

  if (carrier_height >= chain_height)
    return chainlock_proof_result::height_not_locked;
  blobdata carried_blob;
  const block carrier_block = m_db->get_block_from_height(carrier_height);
  if (get_tx_chainlock(carrier_block.miner_tx.extra, carried_blob) != 1 || carried_blob != carrier.lock)
    return chainlock_proof_result::height_not_locked;
  qwertycoin::epose::chainlock_v1 lock;
  if (!qwertycoin::epose::parse_chainlock_v1(carrier.lock, lock) || lock.height >= carrier_height
      || lock.height >= chain_height || m_db->get_block_hash_from_height(lock.height) != lock.block_hash)
    return chainlock_proof_result::height_not_locked;
  chainlock_selection_inputs selection;
  const auto selection_status = get_chainlock_selection_inputs(
      lock.epoch, selection, &blockchain_lock, allow_reconstruct);
  if (selection_status != chainlock_membership_restore::restored
      || !select_chainlock_quorum(selection, result.quorum))
    return chainlock_proof_result::height_not_locked;
  if (!qwertycoin::epose::chainlock_complete_v1(lock, result.quorum))
    return chainlock_proof_result::height_not_locked;
  result.height = lock.height;
  result.block_id = lock.block_hash;
  result.locked_block = m_db->get_block_from_height(lock.height);
  result.lock_blob = carrier.lock;
  result.lock_carrier_height = carrier_height;
  if (get_tx_vm_state_root(result.locked_block.miner_tx.extra, result.vm_state_root) != 1)
    return chainlock_proof_result::height_not_locked;

  if (!get_miner_tx_branch(result.locked_block, result.miner_tx_branch))
    return chainlock_proof_result::height_not_locked;
  return chainlock_proof_result::ok;
}
//------------------------------------------------------------------
crypto::hash Blockchain::get_vm_state_root_after(const vm::write_set &writes)
{
  const vm::write_set undo = vm::undo_for(m_vm_state, writes);
  crypto::hash root{};
  with_restored_vm_state(undo, [&]() {
    m_vm_state.apply(writes);
    root = m_vm_state.get_root();
  });
  return root;
}
//------------------------------------------------------------------
bool Blockchain::load_vm_state()
{
  m_vm_state = vm::state_tree();
  m_vm_state_stale = false;
  const uint64_t chain_height = m_db->height();
  if (chain_height == 0)
    return true;
  m_db->for_all_vm_state([this](const std::string &key, const std::string &value) {
    m_vm_state.set(key, value);
    return true;
  });
  vm_block_commitment tip{};
  const bool contracts_tip = is_qwc_contracts_hardfork(m_db->get_block_from_height(chain_height - 1).major_version);
  if (contracts_tip ? m_db->get_vm_commitment(chain_height - 1, tip) && tip.root == m_vm_state.get_root() : m_vm_state.size() == 0)
    return true;
  MWARNING("Contract state does not match the chain tip, replaying contract calls");
  return rebuild_vm_state();
}
//------------------------------------------------------------------
bool Blockchain::rebuild_vm_state()
{
  m_vm_state = vm::state_tree();
  m_vm_state_stale = true;
  db_wtxn_guard wtxn_guard(m_db);
  m_db->reset_vm_state();
  const uint64_t chain_height = m_db->height();
  const uint64_t fork_height = m_hardfork->get_earliest_ideal_height_for_version(HF_VERSION_QWC_CONTRACTS);
  for (uint64_t height = std::max<uint64_t>(fork_height, 1); height < chain_height; ++height)
  {
    const block bl = m_db->get_block_from_height(height);
    if (!is_qwc_contracts_hardfork(bl.major_version))
      continue;
    crypto::hash claimed_root{};
    contract_block_context context{};
    if (get_tx_vm_state_root(bl.miner_tx.extra, claimed_root) != 1 || !get_contract_block_context(height, context))
      return false;
    const std::vector<transaction> txs = m_db->get_tx_list(bl.tx_hashes);
    if (txs.size() != bl.tx_hashes.size())
      return false;
    contract_block_executor executor(m_vm_state, context);
    for (const transaction &tx : txs)
    {
      const contract_call_verdict verdict = executor.execute(tx, get_tx_fee(tx));
      if (!contract_call_accepted(verdict))
      {
        MERROR("Contract call replay failed at height " << height << ": " << contract_call_verdict_name(verdict));
        return false;
      }
    }
    vm_block_update update{};
    update.writes = executor.writes();
    m_vm_state.apply(update.writes);
    if (!make_vm_commitment(height, executor.effects(), update.commitment) || update.commitment.root != claimed_root)
    {
      MERROR("Contract state root mismatch while replaying height " << height);
      return false;
    }
    m_db->replay_vm_block(height, update);
  }
  m_vm_state_stale = false;
  MINFO("Replayed contract state through " << chain_height << " blocks");
  return true;
}
//------------------------------------------------------------------
crypto::hash Blockchain::get_pending_block_id_by_height(uint64_t height) const
{
  if (m_prepare_height && height >= m_prepare_height && height - m_prepare_height < m_prepare_nblocks)
    return (*m_prepare_blocks)[height - m_prepare_height].hash;
  return get_block_id_by_height(height);
}
//------------------------------------------------------------------
bool Blockchain::get_block_by_hash(const crypto::hash &h, block &blk, bool *orphan) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  // try to find block in main chain
  try
  {
    blk = m_db->get_block(h);
    if (orphan)
      *orphan = false;
    return true;
  }
  // try to find block in alternative chain
  catch (const BLOCK_DNE& e)
  {
    alt_block_data_t data;
    cryptonote::blobdata blob;
    if (m_db->get_alt_block(h, &data, &blob))
    {
      if (!cryptonote::parse_and_validate_block_from_blob(blob, blk))
      {
        MERROR("Found block " << h << " in alt chain, but failed to parse it");
        throw std::runtime_error("Found block in alt chain, but failed to parse it");
      }
      if (orphan)
        *orphan = true;
      return true;
    }
  }
  catch (const std::exception& e)
  {
    MERROR(std::string("Something went wrong fetching block by hash: ") + e.what());
    throw;
  }
  catch (...)
  {
    MERROR(std::string("Something went wrong fetching block hash by hash"));
    throw;
  }

  return false;
}
//------------------------------------------------------------------
// This function aggregates the cumulative difficulties and timestamps of the
// last DIFFICULTY_BLOCKS_COUNT blocks and passes them to next_difficulty,
// returning the result of that call.  Ignores the genesis block, and can use
// less blocks than desired if there aren't enough.
difficulty_type Blockchain::get_difficulty_for_next_block()
{
  if (m_fixed_difficulty)
  {
    return m_db->height() ? m_fixed_difficulty : 1;
  }

  LOG_PRINT_L3("Blockchain::" << __func__);

  crypto::hash top_hash = get_tail_id();
  {
    CRITICAL_REGION_LOCAL(m_difficulty_lock);
    // we can call this without the blockchain lock, it might just give us
    // something a bit out of date, but that's fine since anything which
    // requires the blockchain lock will have acquired it in the first place,
    // and it will be unlocked only when called from the getinfo RPC
    if (top_hash == m_difficulty_for_next_block_top_hash)
      return m_difficulty_for_next_block;
  }

  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  std::vector<uint64_t> timestamps;
  std::vector<difficulty_type> difficulties;
  uint64_t height;
  top_hash = get_tail_id(height); // get it again now that we have the lock
  ++height; // top block height to blockchain height
  // ND: Speedup
  // 1. Keep a list of the last 735 (or less) blocks that is used to compute difficulty,
  //    then when the next block difficulty is queried, push the latest height data and
  //    pop the oldest one from the list. This only requires 1x read per height instead
  //    of doing 735 (DIFFICULTY_BLOCKS_COUNT).
  if (m_reset_timestamps_and_difficulties_height)
    m_timestamps_and_difficulties_height = 0;
  if (m_timestamps_and_difficulties_height != 0 && ((height - m_timestamps_and_difficulties_height) == 1) && m_timestamps.size() >= DIFFICULTY_BLOCKS_COUNT)
  {
    uint64_t index = height - 1;
    m_timestamps.push_back(m_db->get_block_timestamp(index));
    m_difficulties.push_back(m_db->get_block_cumulative_difficulty(index));

    while (m_timestamps.size() > DIFFICULTY_BLOCKS_COUNT)
      m_timestamps.erase(m_timestamps.begin());
    while (m_difficulties.size() > DIFFICULTY_BLOCKS_COUNT)
      m_difficulties.erase(m_difficulties.begin());

    m_timestamps_and_difficulties_height = height;
    timestamps = m_timestamps;
    difficulties = m_difficulties;
  }
  else
  {
    uint64_t offset = height - std::min <uint64_t> (height, static_cast<uint64_t>(DIFFICULTY_BLOCKS_COUNT));
    if (offset == 0)
      ++offset;

    timestamps.clear();
    difficulties.clear();
    if (height > offset)
    {
      timestamps.reserve(height - offset);
      difficulties.reserve(height - offset);
    }
    for (; offset < height; offset++)
    {
      timestamps.push_back(m_db->get_block_timestamp(offset));
      difficulties.push_back(m_db->get_block_cumulative_difficulty(offset));
    }

    m_timestamps_and_difficulties_height = height;
    m_timestamps = timestamps;
    m_difficulties = difficulties;
  }
  size_t target = get_difficulty_target();
  difficulty_type diff = next_difficulty(timestamps, difficulties, target);

  CRITICAL_REGION_LOCAL1(m_difficulty_lock);
  m_difficulty_for_next_block_top_hash = top_hash;
  m_difficulty_for_next_block = diff;
  return diff;
}
//------------------------------------------------------------------
std::pair<bool, uint64_t> Blockchain::check_difficulty_checkpoints() const
{
  uint64_t res = 0;
  for (const std::pair<const uint64_t, difficulty_type>& i : m_checkpoints.get_difficulty_points())
  {
    if (i.first >= m_db->height())
      break;
    if (m_db->get_block_cumulative_difficulty(i.first) != i.second)
      return {false, res};
    res = i.first;
  }
  return {true, res};
}
//------------------------------------------------------------------
size_t Blockchain::recalculate_difficulties(boost::optional<uint64_t> start_height_opt)
{
  if (m_fixed_difficulty)
  {
    return 0;
  }
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  const uint64_t start_height = start_height_opt ? *start_height_opt : check_difficulty_checkpoints().second;
  const uint64_t top_height = m_db->height() - 1;
  MGINFO("Recalculating difficulties from height " << start_height << " to height " << top_height);

  std::vector<uint64_t> timestamps;
  std::vector<difficulty_type> difficulties;
  timestamps.reserve(DIFFICULTY_BLOCKS_COUNT + 1);
  difficulties.reserve(DIFFICULTY_BLOCKS_COUNT + 1);
  if (start_height > 1)
  {
    for (uint64_t i = 0; i < DIFFICULTY_BLOCKS_COUNT; ++i)
    {
      uint64_t height = start_height - 1 - i;
      if (height == 0)
        break;
      timestamps.insert(timestamps.begin(), m_db->get_block_timestamp(height));
      difficulties.insert(difficulties.begin(), m_db->get_block_cumulative_difficulty(height));
    }
  }
  difficulty_type last_cum_diff = start_height <= 1 ? start_height : difficulties.back();
  uint64_t drift_start_height = 0;
  std::vector<difficulty_type> new_cumulative_difficulties;
  for (uint64_t height = start_height; height <= top_height; ++height)
  {
    size_t target = block_target(m_nettype, get_ideal_hard_fork_version(height));
    difficulty_type recalculated_diff = next_difficulty(timestamps, difficulties, target);

    boost::multiprecision::uint256_t recalculated_cum_diff_256 = boost::multiprecision::uint256_t(recalculated_diff) + last_cum_diff;
    CHECK_AND_ASSERT_THROW_MES(recalculated_cum_diff_256 <= std::numeric_limits<difficulty_type>::max(), "Difficulty overflow!");
    difficulty_type recalculated_cum_diff = recalculated_cum_diff_256.convert_to<difficulty_type>();

    if (drift_start_height == 0)
    {
      difficulty_type existing_cum_diff = m_db->get_block_cumulative_difficulty(height);
      if (recalculated_cum_diff != existing_cum_diff)
      {
        drift_start_height = height;
        new_cumulative_difficulties.reserve(top_height + 1 - height);
        LOG_ERROR("Difficulty drift found at height:" << height << ", hash:" << m_db->get_block_hash_from_height(height) << ", existing:" << existing_cum_diff << ", recalculated:" << recalculated_cum_diff);
      }
    }
    if (drift_start_height > 0)
    {
      new_cumulative_difficulties.push_back(recalculated_cum_diff);
      if (height % 100000 == 0)
        LOG_ERROR(boost::format("%llu / %llu (%.1f%%)") % height % top_height % (100 * (height - drift_start_height) / float(top_height - drift_start_height)));
    }

    if (height > 0)
    {
      timestamps.push_back(m_db->get_block_timestamp(height));
      difficulties.push_back(recalculated_cum_diff);
    }
    if (timestamps.size() > DIFFICULTY_BLOCKS_COUNT)
    {
      CHECK_AND_ASSERT_THROW_MES(timestamps.size() == DIFFICULTY_BLOCKS_COUNT + 1, "Wrong timestamps size: " << timestamps.size());
      timestamps.erase(timestamps.begin());
      difficulties.erase(difficulties.begin());
    }
    last_cum_diff = recalculated_cum_diff;
  }

  if (drift_start_height > 0)
  {
    LOG_ERROR("Writing to the DB...");
    try
    {
      m_db->correct_block_cumulative_difficulties(drift_start_height, new_cumulative_difficulties);
    }
    catch (const std::exception& e)
    {
      LOG_ERROR("Error correcting cumulative difficulties from height " << drift_start_height << ", what = " << e.what());
    }
    LOG_ERROR("Corrected difficulties for " << new_cumulative_difficulties.size() << " blocks");
    // clear cache
    m_difficulty_for_next_block_top_hash = crypto::null_hash;
    m_timestamps_and_difficulties_height = 0;
  }

  return new_cumulative_difficulties.size();
}
//------------------------------------------------------------------
std::vector<time_t> Blockchain::get_last_block_timestamps(unsigned int blocks) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  uint64_t height = m_db->height();
  if (blocks > height)
    blocks = height;
  std::vector<time_t> timestamps(blocks);
  while (blocks--)
    timestamps[blocks] = m_db->get_block_timestamp(height - blocks - 1);
  return timestamps;
}
//------------------------------------------------------------------
// This function removes blocks from the blockchain until it gets to the
// position where the blockchain switch started and then re-adds the blocks
// that had been removed.
bool Blockchain::rollback_blockchain_switching(std::list<block>& original_chain, uint64_t rollback_height)
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  // fail if rollback_height passed is too high
  if (rollback_height > m_db->height())
  {
    return true;
  }

  m_timestamps_and_difficulties_height = 0;
  m_reset_timestamps_and_difficulties_height = true;

  // remove blocks from blockchain until we get back to where we should be.
  while (m_db->height() != rollback_height)
  {
    pop_block_from_blockchain();
  }
  CHECK_AND_ASSERT_THROW_MES(update_next_cumulative_weight_limit(), "Error updating next cumulative weight limit");

  // make sure the hard fork object updates its current version
  m_hardfork->reorganize_from_chain_height(rollback_height);

  //return back original chain
  for (auto& bl : original_chain)
  {
    block_verification_context bvc = {};
    bool r = handle_block_to_main_chain(bl, bvc);
    if (!r || !bvc.m_added_to_main_chain)
    {
      m_batch_success = false;
      MERROR("PANIC! failed to add (again) block while chain switching during the rollback!");
      return false;
    }
  }

  m_hardfork->reorganize_from_chain_height(rollback_height);

  MINFO("Rollback to height " << rollback_height << " was successful.");
  if (!original_chain.empty())
  {
    MINFO("Restoration to previous blockchain successful as well.");
  }
  return true;
}
//------------------------------------------------------------------
bool Blockchain::abandon_epose_switch(std::list<block>& disconnected_chain, uint64_t rollback_height, const char *rollback_failure_message)
{
  try
  {
    if (!rollback_blockchain_switching(disconnected_chain, rollback_height))
    {
      m_batch_success = false;
      m_epose_rebuild_fatal = true;
    }
  }
  catch (...)
  {
    if (rollback_failure_message != nullptr)
      MERROR(rollback_failure_message);
    m_batch_success = false;
    m_epose_rebuild_fatal = true;
  }
  schedule_epose_recovery();
  return false;
}
//------------------------------------------------------------------
// This function attempts to switch to an alternate chain, returning
// boolean based on success therein.
bool Blockchain::switch_to_alternative_blockchain(std::list<block_extended_info>& alt_chain, bool discard_disconnected_chain, bool invalidate_failed_block)
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  const uint64_t previous_height = m_db->height();

  m_timestamps_and_difficulties_height = 0;
  m_reset_timestamps_and_difficulties_height = true;

  CHECK_AND_ASSERT_MES(alt_chain.size(), false, "switch_to_alternative_blockchain: empty chain passed");
  if (is_qwc_contracts_hardfork(alt_chain.back().bl.major_version)
      && !m_db->batch_active())
    return false;

  if (!m_db->block_exists(alt_chain.front().bl.prev_id))
  {
    LOG_ERROR("Attempting to move to an alternate chain, but it doesn't appear to connect to the main chain!");
    return false;
  }

  std::unique_ptr<qwertycoin::epose::consensus_coordinator_v2> split_state;
  m_epose_switch_busy = false;
  m_epose_switch_mutated = false;
  epose_preflight_result preflight = epose_preflight_result::busy;
  crypto::hash invalid_block = crypto::null_hash;
  try
  {
    preflight = preflight_epose_chain(alt_chain, split_state, invalid_block);
  }
  catch (const std::exception &error)
  {
    MERROR("EPoSE alternative chain preflight failed locally: " << error.what());
  }

  if (preflight != epose_preflight_result::accepted)
  {
    if (preflight == epose_preflight_result::invalid)
    {
      MERROR("Invalid EPoSE transition in alternative chain preflight at " << invalid_block);
      bool remove = false;
      try
      {
        for (const auto &entry : alt_chain)
        {
          const crypto::hash id = get_block_hash(entry.bl);
          remove = remove || id == invalid_block;
          if (remove)
          {
            add_block_as_invalid(entry, id);
            m_db->remove_alt_block(id);
          }
        }
      }
      catch (...)
      {
        m_epose_switch_busy = true;
        m_epose_retry_best_chain = true;
        schedule_epose_recovery();
      }
      return false;
    }
    m_epose_switch_busy = true;
    const crypto::hash tip = get_block_hash(alt_chain.back().bl);
    if (m_epose_retry_chain_attempts.size() >= 64
        && m_epose_retry_chain_attempts.count(tip) == 0)
      m_epose_retry_chain_attempts.erase(m_epose_retry_chain_attempts.begin());
    m_epose_retry_best_chain = m_epose_retry_chain_attempts[tip] < 3;
    if (!m_epose_retry_best_chain)
      m_epose_retry_exhausted_tip = tip;
    MERROR("EPoSE alternative chain preflight is waiting for local state or transactions");
    schedule_epose_recovery();
    return false;
  }

  m_epose_switch_mutated = true;
  if (discard_disconnected_chain)
    tombstone_main_chainlocks_from(alt_chain.front().height);

  std::list<block> disconnected_chain;
  try
  {
    epose_rebuild_defer_guard deferred(m_epose_rebuild_deferred);
    while (m_db->top_block_hash() != alt_chain.front().bl.prev_id)
    {
      block b = pop_block_from_blockchain();
      disconnected_chain.push_front(b);
    }
  }
  catch (...)
  {
    m_epose_switch_busy = true;
    const uint64_t remaining_height = m_db->height();
    if (!rebuild_epose_state(false, remaining_height)
        && !rebuild_epose_state(true, remaining_height))
      m_epose_rebuild_fatal = true;
    return abandon_epose_switch(disconnected_chain, remaining_height, nullptr);
  }
  const uint64_t split_height = m_db->height();
  const std::string split_image = split_state ? split_state->serialize_state() : std::string{};
  if (split_state)
  {
    m_epose_v2 = std::move(split_state);
    ++m_epose_state_validation_generation;
  }
  bool updated_weight_limit = false;
  try
  {
    updated_weight_limit = update_next_cumulative_weight_limit();
  }
  catch (...)
  {
    MERROR("Failed to update block weight limit during EPoSE reorganization");
  }
  if (!updated_weight_limit)
  {
    m_epose_switch_busy = true;
    return abandon_epose_switch(disconnected_chain, split_height,
        "Failed to restore original chain after block weight update failure");
  }

  for(auto alt_ch_iter = alt_chain.begin(); alt_ch_iter != alt_chain.end(); alt_ch_iter++)
  {
    const auto &bei = *alt_ch_iter;
    block_verification_context bvc = {};

    bool r = false;
    try
    {
      r = handle_block_to_main_chain(bei.bl, bvc);
    }
    catch (const std::exception &failure)
    {
      MERROR("EPoSE alternative block apply failed: " << failure.what());
      m_epose_switch_busy = true;
      bvc.m_retryable_busy = true;
    }
    catch (...)
    {
      MERROR("EPoSE alternative block apply failed");
      m_epose_switch_busy = true;
      bvc.m_retryable_busy = true;
    }

    if(!r || !bvc.m_added_to_main_chain)
    {
      MERROR("Failed to switch to alternative blockchain");

      try
      {
        epose_rebuild_defer_guard deferred(m_epose_rebuild_deferred);
        while (m_db->height() > split_height)
          pop_block_from_blockchain();
      }
      catch (...)
      {
        m_epose_switch_busy = true;
        bool restored_chain = false;
        try
        {
          restored_chain = rollback_blockchain_switching(
              disconnected_chain, split_height);
        }
        catch (...)
        {
          MERROR("Failed to restore original chain after alternative pop failure");
        }
        if (!restored_chain)
        {
          m_batch_success = false;
          m_epose_rebuild_fatal = true;
        }
        else if (!epose_state_at_tip() && !try_rebuild_epose_state())
          schedule_epose_recovery();
        return false;
      }
      if (m_epose_v2)
      {
        auto restored = std::make_unique<qwertycoin::epose::consensus_coordinator_v2>(m_epose_v2_parameters);
        if (split_height > m_epose_v2_parameters.timing.activation_height
            && !restored->restore_state(split_height - 1, split_image))
        {
          MERROR("Failed to restore EPoSE split state after rejecting alternative block");
          m_epose_switch_busy = true;
          if (!rebuild_epose_state(false, split_height, &restored)
              && !rebuild_epose_state(true, split_height, &restored))
            m_epose_rebuild_fatal = true;
        }
        if (restored)
        {
          m_epose_v2 = std::move(restored);
          ++m_epose_state_validation_generation;
        }
      }
      bool restored_chain = false;
      try
      {
        restored_chain = rollback_blockchain_switching(disconnected_chain, split_height);
      }
      catch (...)
      {
        m_epose_rebuild_fatal = true;
      }
      if (!restored_chain)
      {
        MERROR("Failed to restore original chain after rejecting alternative block");
        m_batch_success = false;
        m_epose_switch_busy = true;
        if (epose_state_at_tip())
          m_epose_retry_best_chain = true;
        schedule_epose_recovery();
        return false;
      }
      if (!bvc.m_verifivation_failed || bvc.m_retryable_busy || m_epose_switch_busy)
      {
        m_epose_switch_busy = true;
        schedule_epose_recovery();
      }
      const crypto::hash blkid = cryptonote::get_block_hash(bei.bl);
      if (invalidate_failed_block && bvc.m_verifivation_failed && !bvc.m_missing_txs)
        add_block_as_invalid(bei, blkid);
      m_db->remove_alt_block(blkid);
      alt_ch_iter++;

      for(auto alt_ch_to_orph_iter = alt_ch_iter; alt_ch_to_orph_iter != alt_chain.end(); )
      {
        const auto &bei = *alt_ch_to_orph_iter++;
        const crypto::hash blkid = cryptonote::get_block_hash(bei.bl);
        m_db->remove_alt_block(blkid);
      }
      return false;
    }
  }

  const size_t discarded_blocks = disconnected_chain.size();
  if(!discard_disconnected_chain)
  {
    for (auto& old_ch_ent : disconnected_chain)
    {
      block_verification_context bvc = {};
      pool_supplement ps{};
      bool r = handle_alternative_block(old_ch_ent, get_block_hash(old_ch_ent), bvc, ps);
      if(!r)
      {
        MERROR("Failed to push ex-main chain blocks to alternative chain ");
      }
    }
  }

  for (const auto &bei: alt_chain)
  {
    m_db->remove_alt_block(cryptonote::get_block_hash(bei.bl));
  }

  m_hardfork->reorganize_from_chain_height(split_height);

  std::shared_ptr<tools::Notify> reorg_notify = m_reorg_notify;
  if (reorg_notify)
    reorg_notify->notify("%s", std::to_string(split_height).c_str(), "%h", std::to_string(m_db->height()).c_str(),
        "%n", std::to_string(m_db->height() - split_height).c_str(), "%d", std::to_string(discarded_blocks).c_str(), NULL);

  const uint64_t new_height = m_db->height();
  const crypto::hash seedhash = get_block_id_by_height(crypto::rx_seedheight(new_height));

  crypto::hash prev_id;
  if (!get_block_hash(alt_chain.back().bl, prev_id))
    MERROR("Failed to get block hash of an alternative chain's tip");
  else
    send_miner_notifications(new_height, seedhash, prev_id, alt_chain.back().already_generated_coins);

  for (const auto& notifier : m_block_notifiers)
  {
    std::size_t notify_height = split_height;
    for (const auto& bei: alt_chain)
    {
      notifier(notify_height, {std::addressof(bei.bl), 1});
      ++notify_height;
    }
  }

  if (m_hardfork->get_current_version() >= RX_BLOCK_VERSION)
    rx_set_main_seedhash(seedhash.data, tools::get_max_concurrency());

  m_epose_retry_best_chain = false;
  m_epose_retry_chain_attempts.erase(get_block_hash(alt_chain.back().bl));
  m_epose_retry_exhausted_tip = crypto::null_hash;
  MGINFO_GREEN("REORGANIZE SUCCESS! on height: " << split_height << ", new blockchain size: " << m_db->height());
  prune_after_canonical_advance(previous_height);
  return true;
}
//------------------------------------------------------------------
bool Blockchain::switch_to_alternative_blockchain_with_batch(std::list<block_extended_info> &alt_chain)
{
  const bool stop_batch = m_db->batch_start();
  bool switched = false;
  try
  {
    switched = switch_to_alternative_blockchain(alt_chain, false);
  }
  catch (...)
  {
    if (stop_batch)
      m_db->batch_abort();
    throw;
  }
  if (stop_batch)
  {
    if (switched)
      m_db->batch_stop();
    else
      m_db->batch_abort();
  }
  if (switched && !m_db->batch_active() && m_alternative_prune_pending)
    prune_after_canonical_advance(m_db->height());
  if (!switched && !epose_state_at_tip())
    schedule_epose_recovery();
  return switched;
}
//------------------------------------------------------------------
// This function calculates the difficulty target for the block being added to
// an alternate chain.
difficulty_type Blockchain::get_next_difficulty_for_alternative_chain(const std::list<block_extended_info>& alt_chain, block_extended_info& bei) const
{
  if (m_fixed_difficulty)
  {
    return m_db->height() ? m_fixed_difficulty : 1;
  }

  LOG_PRINT_L3("Blockchain::" << __func__);
  std::vector<uint64_t> timestamps;
  std::vector<difficulty_type> cumulative_difficulties;

  // if the alt chain isn't long enough to calculate the difficulty target
  // based on its blocks alone, need to get more blocks from the main chain
  if(alt_chain.size()< DIFFICULTY_BLOCKS_COUNT)
  {
    CRITICAL_REGION_LOCAL(m_blockchain_lock);

    // Figure out start and stop offsets for main chain blocks
    size_t main_chain_stop_offset = alt_chain.size() ? alt_chain.front().height : bei.height;
    size_t main_chain_count = DIFFICULTY_BLOCKS_COUNT - std::min(static_cast<size_t>(DIFFICULTY_BLOCKS_COUNT), alt_chain.size());
    main_chain_count = std::min(main_chain_count, main_chain_stop_offset);
    size_t main_chain_start_offset = main_chain_stop_offset - main_chain_count;

    if(!main_chain_start_offset)
      ++main_chain_start_offset; //skip genesis block

    // get difficulties and timestamps from relevant main chain blocks
    for(; main_chain_start_offset < main_chain_stop_offset; ++main_chain_start_offset)
    {
      timestamps.push_back(m_db->get_block_timestamp(main_chain_start_offset));
      cumulative_difficulties.push_back(m_db->get_block_cumulative_difficulty(main_chain_start_offset));
    }

    // make sure we haven't accidentally grabbed too many blocks...maybe don't need this check?
    CHECK_AND_ASSERT_MES((alt_chain.size() + timestamps.size()) <= DIFFICULTY_BLOCKS_COUNT, false, "Internal error, alt_chain.size()[" << alt_chain.size() << "] + vtimestampsec.size()[" << timestamps.size() << "] NOT <= DIFFICULTY_WINDOW[]" << DIFFICULTY_BLOCKS_COUNT);

    for (const auto &bei : alt_chain)
    {
      timestamps.push_back(bei.bl.timestamp);
      cumulative_difficulties.push_back(bei.cumulative_difficulty);
    }
  }
  // if the alt chain is long enough for the difficulty calc, grab difficulties
  // and timestamps from it alone
  else
  {
    timestamps.resize(static_cast<size_t>(DIFFICULTY_BLOCKS_COUNT));
    cumulative_difficulties.resize(static_cast<size_t>(DIFFICULTY_BLOCKS_COUNT));
    size_t count = 0;
    size_t max_i = timestamps.size()-1;
    // get difficulties and timestamps from most recent blocks in alt chain
    for (const auto &bei: boost::adaptors::reverse(alt_chain))
    {
      timestamps[max_i - count] = bei.bl.timestamp;
      cumulative_difficulties[max_i - count] = bei.cumulative_difficulty;
      count++;
      if(count >= DIFFICULTY_BLOCKS_COUNT)
        break;
    }
  }

  // FIXME: This will fail if fork activation heights are subject to voting
  size_t target = block_target(m_nettype, get_ideal_hard_fork_version(bei.height));

  // calculate the difficulty target for the block and return it
  return next_difficulty(timestamps, cumulative_difficulties, target);
}
//------------------------------------------------------------------
// This function does a sanity check on basic things that all miner
// transactions have in common, such as:
//   one input, of type txin_gen, with height set to the block's height
//   correct miner tx unlock time
//   a non-overflowing tx amount (dubious necessity on this check)
//   valid output types
bool Blockchain::prevalidate_miner_transaction(const block& b, uint64_t height, uint8_t hf_version)
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CHECK_AND_ASSERT_MES(b.miner_tx.vin.size() == 1, false, "coinbase transaction in the block has no inputs");
  CHECK_AND_ASSERT_MES(b.miner_tx.vin[0].type() == typeid(txin_gen), false, "coinbase transaction in the block has the wrong type");
  CHECK_AND_ASSERT_MES(b.miner_tx.version > 1 || hf_version < HF_VERSION_MIN_V2_COINBASE_TX, false, "Invalid coinbase transaction version");

  // for v2 txes (ringct), we only accept empty rct signatures for miner transactions,
  if (hf_version >= HF_VERSION_REJECT_SIGS_IN_COINBASE && b.miner_tx.version >= 2)
  {
    CHECK_AND_ASSERT_MES(b.miner_tx.rct_signatures.type == rct::RCTTypeNull, false, "RingCT signatures not allowed in coinbase transactions");
  }

  if(boost::get<txin_gen>(b.miner_tx.vin[0]).height != height)
  {
    MWARNING("The miner transaction in block has invalid height: " << boost::get<txin_gen>(b.miner_tx.vin[0]).height << ", expected: " << height);
    return false;
  }
  MDEBUG("Miner tx hash: " << get_transaction_hash(b.miner_tx));
  CHECK_AND_ASSERT_MES(b.miner_tx.unlock_time == height + CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW, false, "coinbase transaction transaction has the wrong unlock time=" << b.miner_tx.unlock_time << ", expected " << height + CRYPTONOTE_MINED_MONEY_UNLOCK_WINDOW);

  //check outs overflow
  if(!check_outs_overflow(b.miner_tx))
  {
    MERROR("miner transaction has money overflow in block " << get_block_hash(b));
    return false;
  }

  CHECK_AND_ASSERT_MES(check_output_types(b.miner_tx, hf_version), false, "miner transaction has invalid output type(s) in block " << get_block_hash(b));

  CHECK_AND_ASSERT_MES(check_tx_contract_extra(b.miner_tx, hf_version, contract_extra_scope::coinbase), false, "coinbase transaction carries contract fields not allowed in block " << get_block_hash(b));

  return true;
}
//------------------------------------------------------------------
// This function validates the miner transaction reward
bool Blockchain::validate_miner_transaction(const block& b, size_t cumulative_block_weight, uint64_t fee, uint64_t minted, uint64_t& base_reward, uint64_t already_generated_coins, bool &partial_block_reward, uint8_t version)
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  //validate reward
  uint64_t money_in_use = 0;
  for (auto& o: b.miner_tx.vout)
    money_in_use += o.amount;
  if (money_in_use < minted)
  {
    MERROR_VER("coinbase transaction pays less than its contract mints");
    return false;
  }
  money_in_use -= minted; // H-30
  partial_block_reward = false;

  if (version == 3) {
    for (auto &o: b.miner_tx.vout) {
      if (!is_valid_decomposed_amount(o.amount)) {
        MERROR_VER("miner tx output " << print_money(o.amount) << " is not a valid decomposed amount");
        return false;
      }
    }
  }

  uint64_t median_weight;
  if (version >= HF_VERSION_EFFECTIVE_SHORT_TERM_MEDIAN_IN_PENALTY)
  {
    median_weight = m_current_block_cumul_weight_median;
  }
  else
  {
    std::vector<uint64_t> last_blocks_weights;
    get_last_n_blocks_weights(last_blocks_weights, CRYPTONOTE_REWARD_BLOCKS_WINDOW);
    median_weight = epee::misc_utils::median(last_blocks_weights);
  }
  if (!get_block_reward(median_weight, cumulative_block_weight, already_generated_coins, base_reward, version))
  {
    MERROR_VER("block weight " << cumulative_block_weight << " is bigger than allowed for this blockchain");
    return false;
  }
  if(base_reward + fee < money_in_use)
  {
    MERROR_VER("coinbase transaction spend too much money (" << print_money(money_in_use) << "). Block reward is " << print_money(base_reward + fee) << "(" << print_money(base_reward) << "+" << print_money(fee) << "), cumulative_block_weight " << cumulative_block_weight);
    return false;
  }
  // From hard fork 2 till 12, we allow a miner to claim less block reward than is allowed, in case a miner wants less dust
  if (version < 2 || version >= HF_VERSION_EXACT_COINBASE)
  {
    if(base_reward + fee != money_in_use)
    {
      MDEBUG("coinbase transaction doesn't use full amount of block reward:  spent: " << money_in_use << ",  block reward " << base_reward + fee << "(" << base_reward << "+" << fee << ")");
      return false;
    }
  }
  else
  {
    // from hard fork 2, since a miner can claim less than the full block reward, we update the base_reward
    // to show the amount of coins that were actually generated, the remainder will be pushed back for later
    // emission. This modifies the emission curve very slightly.
    CHECK_AND_ASSERT_MES(money_in_use - fee <= base_reward, false, "base reward calculation bug");
    if(base_reward + fee != money_in_use)
      partial_block_reward = true;
    base_reward = money_in_use - fee;
  }
  // QWC-HF17 reward/payee/proof validation is owned by
  // consensus_coordinator_v2 after the generic inherited reward calculation.
  // The legacy reward-view-secret path must never authorize an HF17 block.
  return true;
}
//------------------------------------------------------------------
// get the block weights of the last <count> blocks, and return by reference <sz>.
void Blockchain::get_last_n_blocks_weights(std::vector<uint64_t>& weights, size_t count) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  auto h = m_db->height();

  // this function is meaningless for an empty blockchain...granted it should never be empty
  if(h == 0)
    return;

  // add weight of last <count> blocks to vector <weights> (or less, if blockchain size < count)
  size_t start_offset = h - std::min<size_t>(h, count);
  weights = m_db->get_block_weights(start_offset, count);
}
//------------------------------------------------------------------
uint64_t Blockchain::get_long_term_block_weight_median(uint64_t start_height, size_t count) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  PERF_TIMER(get_long_term_block_weights);

  CHECK_AND_ASSERT_THROW_MES(count > 0, "count == 0");

  bool cached = false;
  uint64_t blockchain_height = m_db->height();
  uint64_t tip_height = start_height + count - 1;
  crypto::hash tip_hash = crypto::null_hash;
  if (tip_height < blockchain_height && count == (size_t)m_long_term_block_weights_cache_rolling_median.size())
  {
    tip_hash = m_db->get_block_hash_from_height(tip_height);
    cached = tip_hash == m_long_term_block_weights_cache_tip_hash;
  }

  if (cached)
  {
    MTRACE("requesting " << count << " from " << start_height << ", cached");
    return m_long_term_block_weights_cache_rolling_median.median();
  }

  // in the vast majority of uncached cases, most is still cached,
  // as we just move the window one block up:
  if (tip_height > 0 && count == (size_t)m_long_term_block_weights_cache_rolling_median.size() && tip_height < blockchain_height)
  {
    crypto::hash old_tip_hash = m_db->get_block_hash_from_height(tip_height - 1);
    if (old_tip_hash == m_long_term_block_weights_cache_tip_hash)
    {
      MTRACE("requesting " << count << " from " << start_height << ", incremental");
      m_long_term_block_weights_cache_tip_hash = tip_hash;
      m_long_term_block_weights_cache_rolling_median.insert(m_db->get_block_long_term_weight(tip_height));
      return m_long_term_block_weights_cache_rolling_median.median();
    }
  }

  MTRACE("requesting " << count << " from " << start_height << ", uncached");
  std::vector<uint64_t> weights = m_db->get_long_term_block_weights(start_height, count);
  m_long_term_block_weights_cache_tip_hash = tip_hash;
  m_long_term_block_weights_cache_rolling_median.clear();
  for (uint64_t w: weights)
    m_long_term_block_weights_cache_rolling_median.insert(w);
  return m_long_term_block_weights_cache_rolling_median.median();
}
//------------------------------------------------------------------
uint64_t Blockchain::get_current_cumulative_block_weight_limit() const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  return m_current_block_cumul_weight_limit;
}
//------------------------------------------------------------------
uint64_t Blockchain::get_current_cumulative_block_weight_median() const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  return m_current_block_cumul_weight_median;
}
//------------------------------------------------------------------
//TODO: This function only needed minor modification to work with BlockchainDB,
//      and *works*.  As such, to reduce the number of things that might break
//      in moving to BlockchainDB, this function will remain otherwise
//      unchanged for the time being.
//
// This function makes a new block for a miner to mine the hash for
//
// FIXME: this codebase references #if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
// in a lot of places.  That flag is not referenced in any of the code
// nor any of the makefiles, howeve.  Need to look into whether or not it's
// necessary at all.
bool Blockchain::create_block_template(block& b, const crypto::hash *from_block, const account_public_address& miner_address, difficulty_type& diffic, uint64_t& height, uint64_t& expected_reward, const blobdata& ex_nonce, uint64_t &seed_height, crypto::hash &seed_hash)
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  size_t median_weight;
  uint64_t already_generated_coins;
  uint64_t pool_cookie;

  seed_hash = crypto::null_hash;

  m_tx_pool.lock();
  const auto unlock_guard = epee::misc_utils::create_scope_leave_handler([&]() { m_tx_pool.unlock(); });
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if (!epose_state_at_tip())
    return false;
  if (m_epose_v2 && from_block && *from_block != get_tail_id())
  {
    MERROR("EPoSE-v2 block templates for alternative parents require branch-specific state replay");
    return false;
  }
  if (m_btc_valid && !from_block) {
    // The pool cookie is atomic. The lack of locking is OK, as if it changes
    // just as we compare it, we'll just use a slightly old template, but
    // this would be the case anyway if we'd lock, and the change happened
    // just after the block template was created
    if (!memcmp(&miner_address, &m_btc_address, sizeof(cryptonote::account_public_address)) && m_btc_nonce == ex_nonce
      && m_btc_pool_cookie == m_tx_pool.cookie() && m_btc.prev_id == get_tail_id()) {
      MDEBUG("Using cached template");
      const uint64_t now = time(NULL);
      if (m_btc.timestamp < now) // ensures it can't get below the median of the last few blocks
        m_btc.timestamp = now;
      b = m_btc;
      diffic = m_btc_difficulty;
      height = m_btc_height;
      expected_reward = m_btc_expected_reward;
      seed_height = m_btc_seed_height;
      seed_hash = m_btc_seed_hash;
      return true;
    }
    MDEBUG("Not using cached template: address " << (!memcmp(&miner_address, &m_btc_address, sizeof(cryptonote::account_public_address))) << ", nonce " << (m_btc_nonce == ex_nonce) << ", cookie " << (m_btc_pool_cookie == m_tx_pool.cookie()) << ", from_block " << (!!from_block));
    invalidate_block_template_cache();
  }

  if (from_block)
  {
    //build alternative subchain, front -> mainchain, back -> alternative head
    //block is not related with head of main chain
    //first of all - look in alternative chains container
    alt_block_data_t prev_data;
    bool parent_in_alt = m_db->get_alt_block(*from_block, &prev_data, NULL);
    bool parent_in_main = m_db->block_exists(*from_block);
    if (!parent_in_alt && !parent_in_main)
    {
      MERROR("Unknown from block");
      return false;
    }

    //we have new block in alternative chain
    std::list<block_extended_info> alt_chain;
    block_verification_context bvc = {};
    std::vector<uint64_t> timestamps;
    if (!build_alt_chain(*from_block, alt_chain, timestamps, bvc))
      return false;

    if (parent_in_main)
    {
      cryptonote::block prev_block;
      CHECK_AND_ASSERT_MES(get_block_by_hash(*from_block, prev_block), false, "From block not found"); // TODO
      uint64_t from_block_height = cryptonote::get_block_height(prev_block);
      height = from_block_height + 1;
      if (m_hardfork->get_current_version() >= RX_BLOCK_VERSION)
      {
        uint64_t next_height;
        crypto::rx_seedheights(height, &seed_height, &next_height);
        seed_hash = get_block_id_by_height(seed_height);
      }
    }
    else
    {
      height = alt_chain.back().height + 1;
      uint64_t next_height;
      crypto::rx_seedheights(height, &seed_height, &next_height);

      if (alt_chain.size() && alt_chain.front().height <= seed_height)
      {
        for (auto it=alt_chain.begin(); it != alt_chain.end(); it++)
        {
          if (it->height == seed_height+1)
          {
            seed_hash = it->bl.prev_id;
            break;
          }
        }
      }
      else
      {
        seed_hash = get_block_id_by_height(seed_height);
      }
    }
    b.major_version = m_hardfork->get_ideal_version(height);
    b.minor_version = m_hardfork->get_ideal_version();
    b.prev_id = *from_block;

    // cheat and use the weight of the block we start from, virtually certain to be acceptable
    // and use 1.9 times rather than 2 times so we're even more sure
    if (parent_in_main)
    {
      median_weight = m_db->get_block_weight(height - 1);
      already_generated_coins = m_db->get_block_already_generated_coins(height - 1);
    }
    else
    {
      median_weight = prev_data.cumulative_weight - prev_data.cumulative_weight / 20;
      already_generated_coins = alt_chain.back().already_generated_coins;
    }

    // FIXME: consider moving away from block_extended_info at some point
    block_extended_info bei = {};
    bei.bl = b;
    bei.height = alt_chain.size() ? prev_data.height + 1 : m_db->get_block_height(*from_block) + 1;

    diffic = get_next_difficulty_for_alternative_chain(alt_chain, bei);
  }
  else
  {
    height = m_db->height();
    b.major_version = get_ideal_hard_fork_version(height);
    b.minor_version = m_hardfork->get_ideal_version();
    b.prev_id = get_tail_id();
    if (!next_template_reward_inputs_v2(height, median_weight,
            already_generated_coins))
      return false;
    diffic = get_difficulty_for_next_block();
    if (b.major_version >= RX_BLOCK_VERSION)
    {
      uint64_t next_height;
      crypto::rx_seedheights(height, &seed_height, &next_height);
      seed_hash = get_block_id_by_height(seed_height);
    }
  }
  if (from_block && is_qwc_contracts_hardfork(b.major_version))
  {
    MERROR("Contract block templates for alternative parents require branch-specific state replay");
    return false;
  }
  b.tx_hashes.clear();
  b.timestamp = time(NULL);

  uint64_t median_ts;
  if (!check_block_timestamp(b, median_ts))
  {
    b.timestamp = median_ts;
  }

  CHECK_AND_ASSERT_MES(diffic, false, "difficulty overhead.");

  size_t txs_weight;
  uint64_t fee;
  boost::optional<contract_block_executor> contract_executor;
  if (is_qwc_contracts_hardfork(b.major_version))
  {
    contract_block_context contract_context{};
    CHECK_AND_ASSERT_MES(get_contract_block_context(height, contract_context), false, "Failed to derive the contract block context for the template");
    contract_executor.emplace(m_vm_state, contract_context);
  }
  if (!m_tx_pool.fill_block_template(b, median_weight, already_generated_coins, txs_weight, fee, expected_reward, b.major_version, height, contract_executor.get_ptr()))
  {
    return false;
  }
  miner_contract_outputs contract_outputs{};
  if (contract_executor)
  {
    contract_outputs.state_root = get_vm_state_root_after(contract_executor->writes());
    contract_outputs.mints = contract_executor->effects().mints;
    select_template_chainlock(height, contract_outputs.chainlock);
  }
  const miner_contract_outputs *contract_outputs_ptr = contract_executor ? &contract_outputs : nullptr;
  pool_cookie = m_tx_pool.cookie();
#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
  size_t real_txs_weight = 0;
  uint64_t real_fee = 0;
  for(crypto::hash &cur_hash: b.tx_hashes)
  {
    auto cur_res = m_tx_pool.m_transactions.find(cur_hash);
    if (cur_res == m_tx_pool.m_transactions.end())
    {
      LOG_ERROR("Creating block template: error: transaction not found");
      continue;
    }
    tx_memory_pool::tx_details &cur_tx = cur_res->second;
    real_txs_weight += cur_tx.weight;
    real_fee += cur_tx.fee;
    if (cur_tx.weight != get_transaction_weight(cur_tx.tx))
    {
      LOG_ERROR("Creating block template: error: invalid transaction weight");
    }
    if (cur_tx.tx.version == 1)
    {
      uint64_t inputs_amount;
      if (!get_inputs_money_amount(cur_tx.tx, inputs_amount))
      {
        LOG_ERROR("Creating block template: error: cannot get inputs amount");
      }
      else if (cur_tx.fee != inputs_amount - get_outs_money_amount(cur_tx.tx))
      {
        LOG_ERROR("Creating block template: error: invalid fee");
      }
    }
    else
    {
      if (cur_tx.fee != cur_tx.tx.rct_signatures.txnFee)
      {
        LOG_ERROR("Creating block template: error: invalid fee");
      }
    }
  }
  if (txs_weight != real_txs_weight)
  {
    LOG_ERROR("Creating block template: error: wrongly calculated transaction weight");
  }
  if (fee != real_fee)
  {
    LOG_ERROR("Creating block template: error: wrongly calculated fee");
  }
  MDEBUG("Creating block template: height " << height <<
      ", median weight " << median_weight <<
      ", already generated coins " << already_generated_coins <<
      ", transaction weight " << txs_weight <<
      ", fee " << fee);
#endif

  /*
   two-phase miner transaction generation: we don't know exact block weight until we prepare block, but we don't know reward until we know
   block weight, so first miner transaction generated with fake amount of money, and with phase we know think we know expected block weight
   */
  //make blocks coin-base tx looks close to real coinbase tx to get truthful blob weight
  uint8_t hf_version = b.major_version;
  size_t max_outs = hf_version >= 4 ? 1 : 11;
  uint64_t template_base_reward = 0;
  CHECK_AND_ASSERT_MES(
      get_block_reward(median_weight, txs_weight, already_generated_coins,
          template_base_reward, hf_version),
      false, "Failed to calculate first-pass block reward");
  qwertycoin::epose::coordinator_reward_plan_v2 reward_plan{};
  miner_service_payment_v2 service_payment{};
  const miner_service_payment_v2 *service_payment_ptr = nullptr;
  std::vector<qwertycoin::epose::envelope_record_v2> relay_records;
  if (is_qwc_epose_v2_hardfork(hf_version) && m_epose_v2)
  {
    CHECK_AND_ASSERT_MES(
        plan_epose_reward_v2(
            hf_version, height, b.prev_id, template_base_reward, fee, reward_plan),
        false, "Failed to derive first-pass EPoSE-v2 reward plan");
    service_payment.expectation = reward_plan.has_service_payee
        ? &reward_plan.expectation : nullptr;
    service_payment.permanently_unissued =
        reward_plan.allocation.permanently_unissued;
    service_payment.expected_coinbase_total =
        reward_plan.allocation.coinbase_total;
    service_payment.max_envelopes_per_transaction =
        m_epose_v2_parameters.limits.max_envelopes_per_transaction;
    service_payment.limits = &m_epose_v2_parameters.limits.envelope;
    service_payment.nettype = m_epose_v2_parameters.nettype;
    service_payment.genesis_hash = m_epose_v2_parameters.genesis_hash;
    service_payment.parameter_set_hash = m_epose_v2_parameters.parameter_set_hash;
    CHECK_AND_ASSERT_MES(
        select_epose_template_records_v2(height, relay_records),
        false, "Failed to select EPoSE-v2 relay records for template");
    service_payment.carrier_records = relay_records.empty()
        ? nullptr : &relay_records;
    service_payment_ptr = &service_payment;
  }
  else
    CHECK_AND_ASSERT_MES(
        !is_qwc_epose_v2_hardfork(hf_version) || m_nettype == FAKECHAIN,
        false, "HF17 block template requires the EPoSE-v2 coordinator");

  const auto construct_template_miner_tx = [&](size_t current_weight) {
    const crypto::secret_key template_tx_key = m_template_tx_key_override
        ? m_template_tx_key_override(height) : crypto::secret_key{};
    if (service_payment_ptr != nullptr)
      service_payment.carrier_records = relay_records.empty()
          ? nullptr : &relay_records;
    return construct_miner_tx(
        height, median_weight, already_generated_coins,
        current_weight, fee, miner_address, b.miner_tx, ex_nonce,
        max_outs, hf_version, service_payment_ptr, contract_outputs_ptr,
        m_template_tx_key_override ? &template_tx_key : nullptr);
  };

  bool r = construct_template_miner_tx(txs_weight);
  CHECK_AND_ASSERT_MES(r, false, "Failed to construct miner tx, first chance");
  size_t cumulative_weight = txs_weight + get_transaction_weight(b.miner_tx);
#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
  MDEBUG("Creating block template: miner tx weight " << get_transaction_weight(b.miner_tx) <<
      ", cumulative weight " << cumulative_weight);
#endif
  for (size_t try_count = 0; try_count != 10; ++try_count)
  {
    template_base_reward = 0;
    CHECK_AND_ASSERT_MES(
        get_block_reward(median_weight, cumulative_weight,
            already_generated_coins, template_base_reward, hf_version),
        false, "Failed to calculate EPoSE-v2 template reward");
    if (service_payment_ptr != nullptr)
    {
      reward_plan = {};
      CHECK_AND_ASSERT_MES(
          plan_epose_reward_v2(
              hf_version, height, b.prev_id, template_base_reward, fee, reward_plan),
          false, "Failed to derive EPoSE-v2 template reward plan");
      service_payment.expectation = reward_plan.has_service_payee
          ? &reward_plan.expectation : nullptr;
      service_payment.permanently_unissued =
          reward_plan.allocation.permanently_unissued;
      service_payment.expected_coinbase_total =
          reward_plan.allocation.coinbase_total;
    }

    r = construct_template_miner_tx(cumulative_weight);

    CHECK_AND_ASSERT_MES(r, false, "Failed to construct miner tx, second chance");
    size_t coinbase_weight = get_transaction_weight(b.miner_tx);
    if (coinbase_weight > cumulative_weight - txs_weight)
    {
      cumulative_weight = txs_weight + coinbase_weight;
#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
      MDEBUG("Creating block template: miner tx weight " << coinbase_weight <<
          ", cumulative weight " << cumulative_weight << " is greater than before");
#endif
      continue;
    }

    if (coinbase_weight < cumulative_weight - txs_weight)
    {
      // A v2 payment proof commits to the complete proofless Coinbase extra.
      // Legacy zero-byte padding after proof construction would invalidate
      // that commitment, so converge by rebuilding at the actual weight.
      if (service_payment_ptr != nullptr)
      {
        cumulative_weight = txs_weight + coinbase_weight;
        continue;
      }
      size_t delta = cumulative_weight - txs_weight - coinbase_weight;
#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
      MDEBUG("Creating block template: miner tx weight " << coinbase_weight <<
          ", cumulative weight " << txs_weight + coinbase_weight <<
          " is less than before, adding " << delta << " zero bytes");
#endif
      b.miner_tx.extra.insert(b.miner_tx.extra.end(), delta, 0);
      //here  could be 1 byte difference, because of extra field counter is varint, and it can become from 1-byte len to 2-bytes len.
      if (cumulative_weight != txs_weight + get_transaction_weight(b.miner_tx))
      {
        CHECK_AND_ASSERT_MES(cumulative_weight + 1 == txs_weight + get_transaction_weight(b.miner_tx), false, "unexpected case: cumulative_weight=" << cumulative_weight << " + 1 is not equal txs_cumulative_weight=" << txs_weight << " + get_transaction_weight(b.miner_tx)=" << get_transaction_weight(b.miner_tx));
        b.miner_tx.extra.resize(b.miner_tx.extra.size() - 1);
        if (cumulative_weight != txs_weight + get_transaction_weight(b.miner_tx))
        {
          //fuck, not lucky, -1 makes varint-counter size smaller, in that case we continue to grow with cumulative_weight
          MDEBUG("Miner tx creation has no luck with delta_extra size = " << delta << " and " << delta - 1);
          cumulative_weight += delta - 1;
          continue;
        }
        MDEBUG("Setting extra for block: " << b.miner_tx.extra.size() << ", try_count=" << try_count);
      }
    }
    CHECK_AND_ASSERT_MES(cumulative_weight == txs_weight + get_transaction_weight(b.miner_tx), false, "unexpected case: cumulative_weight=" << cumulative_weight << " is not equal txs_cumulative_weight=" << txs_weight << " + get_transaction_weight(b.miner_tx)=" << get_transaction_weight(b.miner_tx));
#if defined(DEBUG_CREATE_BLOCK_TEMPLATE)
    MDEBUG("Creating block template: miner tx weight " << coinbase_weight <<
        ", cumulative weight " << cumulative_weight << " is now good");
#endif

    if (!from_block)
      cache_block_template(b, miner_address, ex_nonce, diffic, height, expected_reward, seed_height, seed_hash, pool_cookie);
    if (service_payment_ptr != nullptr && m_epose_template_plan_observer)
      m_epose_template_plan_observer(reward_plan);
    return true;
  }
  LOG_ERROR("Failed to create_block_template with " << 10 << " tries");
  return false;
}
//------------------------------------------------------------------
bool Blockchain::create_block_template(block& b, const account_public_address& miner_address, difficulty_type& diffic, uint64_t& height, uint64_t& expected_reward, const blobdata& ex_nonce, uint64_t &seed_height, crypto::hash &seed_hash)
{
  return create_block_template(b, NULL, miner_address, diffic, height, expected_reward, ex_nonce, seed_height, seed_hash);
}
//------------------------------------------------------------------
bool Blockchain::get_miner_data(uint8_t& major_version, uint64_t& height, crypto::hash& prev_id, crypto::hash& seed_hash, difficulty_type& difficulty, uint64_t& median_weight, uint64_t& already_generated_coins, std::vector<tx_block_template_backlog_entry>& tx_backlog)
{
  prev_id = m_db->top_block_hash(&height);
  ++height;

  major_version = m_hardfork->get_ideal_version(height);
  if (is_qwc_contracts_hardfork(major_version)) // H-18
  {
    MERROR("Miner data cannot carry the contract state root, use get_block_template");
    return false;
  }

  seed_hash = crypto::null_hash;
  if (m_hardfork->get_current_version() >= RX_BLOCK_VERSION)
  {
    uint64_t seed_height, next_height;
    crypto::rx_seedheights(height, &seed_height, &next_height);
    seed_hash = get_block_id_by_height(seed_height);
  }

  difficulty = get_difficulty_for_next_block();
  median_weight = m_current_block_cumul_weight_median;
  already_generated_coins = m_db->get_block_already_generated_coins(height - 1);

  m_tx_pool.get_block_template_backlog(tx_backlog);

  return true;
}
//------------------------------------------------------------------
// for an alternate chain, get the timestamps from the main chain to complete
// the needed number of timestamps for the BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW.
bool Blockchain::complete_timestamps_vector(uint64_t start_top_height, std::vector<uint64_t>& timestamps) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);

  if(timestamps.size() >= BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW)
    return true;

  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  size_t need_elements = BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW - timestamps.size();
  CHECK_AND_ASSERT_MES(start_top_height < m_db->height(), false, "internal error: passed start_height not < " << " m_db->height() -- " << start_top_height << " >= " << m_db->height());
  size_t stop_offset = start_top_height > need_elements ? start_top_height - need_elements : 0;
  timestamps.reserve(timestamps.size() + start_top_height - stop_offset);
  while (start_top_height != stop_offset)
  {
    timestamps.push_back(m_db->get_block_timestamp(start_top_height));
    --start_top_height;
  }
  return true;
}
//------------------------------------------------------------------
bool Blockchain::build_alt_chain(const crypto::hash &prev_id, std::list<block_extended_info>& alt_chain, std::vector<uint64_t> &timestamps, block_verification_context& bvc) const
{
    //build alternative subchain, front -> mainchain, back -> alternative head
    cryptonote::alt_block_data_t data;
    cryptonote::blobdata blob;
    bool found = m_db->get_alt_block(prev_id, &data, &blob);
    timestamps.clear();
    while(found)
    {
      ++m_alternative_ancestor_walks;
      block_extended_info bei;
      CHECK_AND_ASSERT_MES(cryptonote::parse_and_validate_block_from_blob(blob, bei.bl), false, "Failed to parse alt block");
      bei.height = data.height;
      bei.block_cumulative_weight = data.cumulative_weight;
      bei.cumulative_difficulty = data.cumulative_difficulty_high;
      bei.cumulative_difficulty = (bei.cumulative_difficulty << 64) + data.cumulative_difficulty_low;
      bei.already_generated_coins = data.already_generated_coins;
      timestamps.push_back(bei.bl.timestamp);
      alt_chain.push_front(std::move(bei));
      found = m_db->get_alt_block(bei.bl.prev_id, &data, &blob);
    }

    // if block to be added connects to known blocks that aren't part of the
    // main chain -- that is, if we're adding on to an alternate chain
    if(!alt_chain.empty())
    {
      // make sure alt chain doesn't somehow start past the end of the main chain
      CHECK_AND_ASSERT_MES(m_db->height() > alt_chain.front().height, false, "main blockchain wrong height");

      // make sure that the blockchain contains the block that should connect
      // this alternate chain with it.
      if (!m_db->block_exists(alt_chain.front().bl.prev_id))
      {
        MERROR("alternate chain does not appear to connect to main chain...");
        return false;
      }

      // make sure block connects correctly to the main chain
      auto h = m_db->get_block_hash_from_height(alt_chain.front().height - 1);
      CHECK_AND_ASSERT_MES(h == alt_chain.front().bl.prev_id, false, "alternative chain has wrong connection to main chain");
      complete_timestamps_vector(m_db->get_block_height(alt_chain.front().bl.prev_id), timestamps);
    }
    // if block not associated with known alternate chain
    else
    {
      // if block parent is not part of main chain or an alternate chain,
      // we ignore it
      bool parent_in_main = m_db->block_exists(prev_id);
      CHECK_AND_ASSERT_MES(parent_in_main, false, "internal error: broken imperative condition: parent_in_main");

      complete_timestamps_vector(m_db->get_block_height(prev_id), timestamps);
    }

    return true;
}
//------------------------------------------------------------------
bool Blockchain::record_alternative_switch_result(bool switched, block_verification_context &bvc)
{
  if (switched)
    bvc.m_added_to_main_chain = true;
  else if (m_epose_switch_busy)
    bvc.m_retryable_busy = true;
  else
    bvc.m_verifivation_failed = true;
  return switched;
}
//------------------------------------------------------------------
// If a block is to be added and its parent block is not the current
// main chain top block, then we need to see if we know about its parent block.
// If its parent block is part of a known forked chain, then we need to see
// if that chain is long enough to become the main chain and re-org accordingly
// if so.  If not, we need to hang on to the block in case it becomes part of
// a long forked chain eventually.
bool Blockchain::handle_alternative_block(const block& b, const crypto::hash& id,
  block_verification_context& bvc, pool_supplement& extra_block_txs)
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  m_timestamps_and_difficulties_height = 0;
  m_reset_timestamps_and_difficulties_height = true;
  uint64_t block_height = get_block_height(b);
  if(0 == block_height)
  {
    MERROR_VER("Block with id: " << epee::string_tools::pod_to_hex(id) << " (as alternative), but miner tx says height is 0.");
    bvc.m_verifivation_failed = true;
    return false;
  }
  // this basically says if the blockchain is smaller than the first
  // checkpoint then alternate blocks are allowed.  Alternatively, if the
  // last checkpoint *before* the end of the current chain is also before
  // the block to be added, then this is fine.
  if (!m_checkpoints.is_alternative_block_allowed(get_current_blockchain_height(), block_height))
  {
    MERROR_VER("Block with id: " << id << std::endl << " can't be accepted for alternative chain, block height: " << block_height << std::endl << " blockchain height: " << get_current_blockchain_height());
    bvc.m_verifivation_failed = true;
    return false;
  }

  // this is a cheap test
  const uint8_t hf_version = m_hardfork->get_ideal_version(block_height);
  if (!m_hardfork->check_for_height(b, block_height))
  {
    LOG_PRINT_L1("Block with id: " << id << std::endl << "has old version for height " << block_height);
    bvc.m_verifivation_failed = true;
    return false;
  }

  //block is not related with head of main chain
  //first of all - look in alternative chains container
  alt_block_data_t prev_data;
  bool parent_in_alt = m_db->get_alt_block(b.prev_id, &prev_data, NULL);
  bool parent_in_main = m_db->block_exists(b.prev_id);
  if (parent_in_alt || parent_in_main)
  {
    //we have new block in alternative chain
    std::list<block_extended_info> alt_chain;
    std::vector<uint64_t> timestamps;
    if (!build_alt_chain(b.prev_id, alt_chain, timestamps, bvc))
      return false;

    // FIXME: consider moving away from block_extended_info at some point
    block_extended_info bei = {};
    bei.bl = b;
    const uint64_t prev_height = alt_chain.size() ? prev_data.height : m_db->get_block_height(b.prev_id);
    bei.height = prev_height + 1;
    uint64_t block_reward = get_outs_money_amount(b.miner_tx);
    const uint64_t prev_generated_coins = alt_chain.size() ? prev_data.already_generated_coins : m_db->get_block_already_generated_coins(prev_height);
    bei.already_generated_coins = (block_reward < (MONEY_SUPPLY - prev_generated_coins)) ? prev_generated_coins + block_reward : MONEY_SUPPLY;

    // verify that the block's timestamp is within the acceptable range
    // (not earlier than the median of the last X blocks)
    if(!check_block_timestamp(timestamps, b))
    {
      MERROR_VER("Block with id: " << id << std::endl << " for alternative chain, has invalid timestamp: " << b.timestamp);
      bvc.m_verifivation_failed = true;
      return false;
    }

    bool is_a_checkpoint;
    if(!m_checkpoints.check_block(bei.height, id, is_a_checkpoint))
    {
      LOG_ERROR("CHECKPOINT VALIDATION FAILED");
      bvc.m_verifivation_failed = true;
      return false;
    }

    // Check the block's hash against the difficulty target for its alt chain
    difficulty_type current_diff = get_next_difficulty_for_alternative_chain(alt_chain, bei);
    CHECK_AND_ASSERT_MES(current_diff, false, "!!!!!!! DIFFICULTY OVERHEAD !!!!!!!");
    crypto::hash proof_of_work;
    memset(proof_of_work.data, 0xff, sizeof(proof_of_work.data));
    if (b.major_version >= RX_BLOCK_VERSION)
    {
      crypto::hash seedhash = null_hash;
      uint64_t seedheight = rx_seedheight(bei.height);
      // seedblock is on the alt chain somewhere
      if (alt_chain.size() && alt_chain.front().height <= seedheight)
      {
        for (auto it=alt_chain.begin(); it != alt_chain.end(); it++)
        {
          if (it->height == seedheight+1)
          {
            seedhash = it->bl.prev_id;
            break;
          }
        }
      } else
      {
        seedhash = get_block_id_by_height(seedheight);
      }
      get_altblock_longhash(bei.bl, proof_of_work, seedhash);
    } else
    {
      get_block_longhash(this, bei.bl, proof_of_work, bei.height, 0);
    }
    if(!check_hash(proof_of_work, current_diff))
    {
      MERROR_VER("Block with id: " << id << std::endl << " for alternative chain, does not have enough proof of work: " << proof_of_work << std::endl << " expected difficulty: " << current_diff);
      bvc.m_verifivation_failed = true;
      bvc.m_bad_pow = true;
      return false;
    }

    if(!prevalidate_miner_transaction(b, bei.height, hf_version))
    {
      MERROR_VER("Block with id: " << epee::string_tools::pod_to_hex(id) << " (as alternative) has incorrect miner transaction.");
      bvc.m_verifivation_failed = true;
      return false;
    }

    // FIXME:
    // this brings up an interesting point: consider allowing to get block
    // difficulty both by height OR by hash, not just height.
    difficulty_type main_chain_cumulative_difficulty = m_db->get_block_cumulative_difficulty(m_db->height() - 1);
    if (alt_chain.size())
    {
      bei.cumulative_difficulty = prev_data.cumulative_difficulty_high;
      bei.cumulative_difficulty = (bei.cumulative_difficulty << 64) + prev_data.cumulative_difficulty_low;
    }
    else
    {
      // passed-in block's previous block's cumulative difficulty, found on the main chain
      bei.cumulative_difficulty = m_db->get_block_cumulative_difficulty(m_db->get_block_height(b.prev_id));
    }
    bei.cumulative_difficulty += current_diff;

    // Now that we have the PoW verification out of the way, verify all pool supplement txs
    tx_verification_context tvc{};
    if (!ver_non_input_consensus(extra_block_txs, tvc, b.major_version))
    {
      MERROR_VER("Transaction pool supplement verification failure for alt block " << id);
      bvc.m_verifivation_failed = true;
      return false;
    }

    // Add pool supplement txs to the main mempool with relay_method::block
    CRITICAL_REGION_LOCAL(m_tx_pool);
    for (auto& extra_block_tx : extra_block_txs.txs_by_txid)
    {
      const crypto::hash& txid = extra_block_tx.first;
      transaction& tx = extra_block_tx.second.first;
      const blobdata &tx_blob = extra_block_tx.second.second;

      tx_extra_vm_call call{};
      if (contract_call_before_activation(get_tx_vm_call(tx.extra, call), b.major_version))
      {
        bvc.m_verifivation_failed = true;
        return false;
      }
      tx_verification_context tvc{};
      if (((!m_tx_pool.have_tx(txid, relay_category::legacy) &&
          !m_db->tx_exists(txid) &&
          !m_tx_pool.add_tx(tx, tvc, relay_method::block, /*relayed=*/true, b.major_version, b.major_version))
          || tvc.m_verifivation_failed) && !tvc.m_window_expired) // QWC-3 (h)
      {
        MERROR_VER("Transaction " << txid <<
          " in pool supplement failed to enter main pool for alt block " << id);
        bvc.m_verifivation_failed = true;
        return false;
      }

      // If new incoming tx in alt block passed verification and entered the pool, notify ZMQ
      if (tvc.m_added_to_pool)
        notify_txpool_event({txpool_event{
          .tx = tx,
          .hash = txid,
          .blob_size = tx_blob.size(),
          .weight = get_transaction_weight(tx),
          .res = true}});
    }
    extra_block_txs.txs_by_txid.clear();
    extra_block_txs.nic_verified_hf_version = 0;

    bei.block_cumulative_weight = cryptonote::get_transaction_weight(b.miner_tx);
    for (const crypto::hash &txid: b.tx_hashes)
    {
      cryptonote::tx_memory_pool::tx_details td;
      cryptonote::blobdata blob;
      if (m_tx_pool.have_tx(txid, relay_category::legacy))
      {
        if (m_tx_pool.get_transaction_info(txid, td, true/*include_sensitive_data*/))
        {
          bei.block_cumulative_weight += td.weight;
        }
        else
        {
          MERROR_VER("Transaction is in the txpool, but metadata not found");
          bvc.m_verifivation_failed = true;
          return false;
        }
      }
      else if (m_db->get_pruned_tx_blob(txid, blob))
      {
        cryptonote::transaction tx;
        if (!cryptonote::parse_and_validate_tx_base_from_blob(blob, tx))
        {
          MERROR_VER("Block with id: " << epee::string_tools::pod_to_hex(id) << " (as alternative) refers to unparsable transaction hash " << txid << ".");
          bvc.m_verifivation_failed = true;
          return false;
        }
        bei.block_cumulative_weight += cryptonote::get_pruned_transaction_weight(tx);
      }
      else
      {
        // we can't determine the block weight, set it to 0 and break out of the loop
        bei.block_cumulative_weight = 0;
        break;
      }
    }

    // add block to alternate blocks storage,
    // as well as the current "alt chain" container
    CHECK_AND_ASSERT_MES(!m_db->get_alt_block(id, NULL, NULL), false, "insertion of new alternative block returned as it already exists");
    cryptonote::alt_block_data_t data;
    data.height = bei.height;
    data.cumulative_weight = bei.block_cumulative_weight;
    data.cumulative_difficulty_low = (bei.cumulative_difficulty & 0xffffffffffffffff).convert_to<uint64_t>();
    data.cumulative_difficulty_high = ((bei.cumulative_difficulty >> 64) & 0xffffffffffffffff).convert_to<uint64_t>();
    data.already_generated_coins = bei.already_generated_coins;
    m_db->add_alt_block(id, data, cryptonote::block_to_blob(bei.bl));
    alt_chain.push_back(bei);

    // FIXME: is it even possible for a checkpoint to show up not on the main chain?
    if(is_a_checkpoint)
    {
      //do reorganize!
      MGINFO_GREEN("###### REORGANIZE on height: " << alt_chain.front().height << " of " << m_db->height() - 1 << ", checkpoint is found in alternative chain on height " << bei.height);

      bool r = switch_to_alternative_blockchain(alt_chain, true);

      if(r) bvc.m_added_to_main_chain = true;
      else if (m_epose_switch_busy) bvc.m_retryable_busy = true;
      else bvc.m_verifivation_failed = true;

      return r;
    }
    const alt_chainlock_verdict chainlock_verdict = evaluate_alt_chainlocks(alt_chain).verdict;
    if (chainlock_verdict == alt_chainlock_verdict::blocked)
    {
      MGINFO_BLUE("----- BLOCK ADDED AS ALTERNATIVE ON HEIGHT " << bei.height << ", reorganization blocked by chainlock" << std::endl << "id:\t" << id);
      return true;
    }
    if (chainlock_verdict != alt_chainlock_verdict::difficulty)
    {
      MGINFO_GREEN("###### REORGANIZE on height: " << alt_chain.front().height << " of " << m_db->height() - 1 << ", alternative chain carries a newer chainlock");
      return record_alternative_switch_result(
          switch_to_alternative_blockchain(alt_chain, false, chainlock_verdict == alt_chainlock_verdict::switch_speculative), bvc);
    }
    else if(main_chain_cumulative_difficulty < bei.cumulative_difficulty) //check if difficulty bigger then in main chain
    {
      //do reorganize!
      MGINFO_GREEN("###### REORGANIZE on height: " << alt_chain.front().height << " of " << m_db->height() - 1 << " with cum_difficulty " << m_db->get_block_cumulative_difficulty(m_db->height() - 1) << std::endl << " alternative blockchain size: " << alt_chain.size() << " with cum_difficulty " << bei.cumulative_difficulty);

      return record_alternative_switch_result(switch_to_alternative_blockchain(alt_chain, false), bvc);
    }
    else
    {
      MGINFO_BLUE("----- BLOCK ADDED AS ALTERNATIVE ON HEIGHT " << bei.height << std::endl << "id:\t" << id << std::endl << "PoW:\t" << proof_of_work << std::endl << "difficulty:\t" << current_diff);
      return true;
    }
  }
  else
  {
    //block orphaned
    bvc.m_marked_as_orphaned = true;
    MERROR_VER("Block recognized as orphaned and rejected, id = " << id << ", height " << block_height
        << ", parent in alt " << parent_in_alt << ", parent in main " << parent_in_main
        << " (parent " << b.prev_id << ", current top " << get_tail_id() << ", chain height " << get_current_blockchain_height() << ")");
  }

  return true;
}
//------------------------------------------------------------------
bool Blockchain::get_blocks(uint64_t start_offset, size_t count, std::vector<std::pair<cryptonote::blobdata,block>>& blocks, std::vector<cryptonote::blobdata>& txs) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  if(start_offset >= m_db->height())
    return false;

  if (!get_blocks(start_offset, count, blocks))
  {
    return false;
  }

  for(const auto& blk : blocks)
  {
    std::vector<crypto::hash> missed_ids;
    get_transactions_blobs(blk.second.tx_hashes, txs, missed_ids);
    CHECK_AND_ASSERT_MES(!missed_ids.size(), false, "has missed transactions in own block in main blockchain");
  }

  return true;
}
//------------------------------------------------------------------
bool Blockchain::get_blocks(uint64_t start_offset, size_t count, std::vector<std::pair<cryptonote::blobdata,block>>& blocks) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  const uint64_t height = m_db->height();
  if(start_offset >= height)
    return false;

  blocks.reserve(blocks.size() + height - start_offset);
  for(size_t i = start_offset; i < start_offset + count && i < height;i++)
  {
    blocks.push_back(std::make_pair(m_db->get_block_blob_from_height(i), block()));
    if (!parse_and_validate_block_from_blob(blocks.back().first, blocks.back().second))
    {
      LOG_ERROR("Invalid block");
      return false;
    }
  }
  return true;
}
//------------------------------------------------------------------
//TODO: This function *looks* like it won't need to be rewritten
//      to use BlockchainDB, as it calls other functions that were,
//      but it warrants some looking into later.
//
//FIXME: This function appears to want to return false if any transactions
//       that belong with blocks are missing, but not if blocks themselves
//       are missing.
bool Blockchain::handle_get_objects(NOTIFY_REQUEST_GET_OBJECTS::request& arg, NOTIFY_RESPONSE_GET_OBJECTS::request& rsp)
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  db_rtxn_guard rtxn_guard (m_db);
  rsp.current_blockchain_height = get_current_blockchain_height();
  std::vector<std::pair<cryptonote::blobdata,block>> blocks;
  get_blocks(arg.blocks, blocks, rsp.missed_ids);

  for (size_t i = 0; i < blocks.size(); ++i)
  {
    auto& bl = blocks[i];
    std::vector<crypto::hash> missed_tx_ids;

    rsp.blocks.push_back(block_complete_entry());
    block_complete_entry& e = rsp.blocks.back();

    // FIXME: s/rsp.missed_ids/missed_tx_id/ ?  Seems like rsp.missed_ids
    //        is for missed blocks, not missed transactions as well.
    e.pruned = arg.prune;
    get_transactions_blobs(bl.second.tx_hashes, e.txs, missed_tx_ids, arg.prune);
    if (missed_tx_ids.size() != 0)
    {
      // do not display an error if the peer asked for an unpruned block which we are not meant to have
      if (tools::has_unpruned_block(get_block_height(bl.second), get_current_blockchain_height(), get_blockchain_pruning_seed()))
      {
        LOG_ERROR("Error retrieving blocks, missed " << missed_tx_ids.size()
            << " transactions for block with hash: " << get_block_hash(bl.second)
            << std::endl
        );
      }

      // append missed transaction hashes to response missed_ids field,
      // as done below if any standalone transactions were requested
      // and missed.
      rsp.missed_ids.insert(rsp.missed_ids.end(), missed_tx_ids.begin(), missed_tx_ids.end());
      return false;
    }

    //pack block
    e.block = std::move(bl.first);
    e.block_weight = arg.prune ? m_db->get_block_weight(get_block_height(bl.second)) : 0;
  }

  return true;
}
//------------------------------------------------------------------
bool Blockchain::get_alternative_blocks(std::vector<block>& blocks) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  blocks.reserve(m_db->get_alt_block_count());
  m_db->for_all_alt_blocks([&blocks](const crypto::hash &blkid, const cryptonote::alt_block_data_t &data, const cryptonote::blobdata_ref *blob) {
    if (!blob)
    {
      MERROR("No blob, but blobs were requested");
      return false;
    }
    cryptonote::block bl;
    if (cryptonote::parse_and_validate_block_from_blob(*blob, bl))
      blocks.push_back(std::move(bl));
    else
      MERROR("Failed to parse block from blob");
    return true;
  }, true);
  return true;
}
//------------------------------------------------------------------
size_t Blockchain::get_alternative_blocks_count() const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  return m_db->get_alt_block_count();
}
//------------------------------------------------------------------
// This function adds the output specified by <amount, i> to the result_outs container
// unlocked and other such checks should be done by here.
uint64_t Blockchain::get_num_mature_outputs(uint64_t amount) const
{
  uint64_t num_outs = m_db->get_num_outputs(amount);
  // ensure we don't include outputs that aren't yet eligible to be used
  // outpouts are sorted by height
  const uint64_t blockchain_height = m_db->height();
  while (num_outs > 0)
  {
    const tx_out_index toi = m_db->get_output_tx_and_index(amount, num_outs - 1);
    const uint64_t height = m_db->get_tx_block_height(toi.first);
    if (height + CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE <= blockchain_height)
      break;
    --num_outs;
  }

  return num_outs;
}

crypto::public_key Blockchain::get_output_key(uint64_t amount, uint64_t global_index) const
{
  output_data_t data = m_db->get_output_key(amount, global_index);
  return data.pubkey;
}

//------------------------------------------------------------------
bool Blockchain::get_outs(const COMMAND_RPC_GET_OUTPUTS_BIN::request& req, COMMAND_RPC_GET_OUTPUTS_BIN::response& res) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  res.outs.clear();
  res.outs.reserve(req.outputs.size());

  std::vector<cryptonote::output_data_t> data;
  try
  {
    std::vector<uint64_t> amounts, offsets;
    amounts.reserve(req.outputs.size());
    offsets.reserve(req.outputs.size());
    for (const auto &i: req.outputs)
    {
      amounts.push_back(i.amount);
      offsets.push_back(i.index);
    }
    m_db->get_output_key(epee::span<const uint64_t>(amounts.data(), amounts.size()), offsets, data);
    if (data.size() != req.outputs.size())
    {
      MERROR("Unexpected output data size: expected " << req.outputs.size() << ", got " << data.size());
      return false;
    }
    const uint8_t hf_version = m_hardfork->get_current_version();
    for (const auto &t: data)
      res.outs.push_back({t.pubkey, t.commitment, is_tx_spendtime_unlocked(t.unlock_time, hf_version), t.height, crypto::null_hash});

    if (req.get_txid)
    {
      for (size_t i = 0; i < req.outputs.size(); ++i)
      {
        tx_out_index toi = m_db->get_output_tx_and_index(req.outputs[i].amount, req.outputs[i].index);
        res.outs[i].txid = toi.first;
      }
    }
  }
  catch (const std::exception &e)
  {
    return false;
  }
  return true;
}
//------------------------------------------------------------------
void Blockchain::get_output_key_mask_unlocked(const uint64_t& amount, const uint64_t& index, crypto::public_key& key, rct::key& mask, bool& unlocked) const
{
  const auto o_data = m_db->get_output_key(amount, index);
  key = o_data.pubkey;
  mask = o_data.commitment;
  tx_out_index toi = m_db->get_output_tx_and_index(amount, index);
  const uint8_t hf_version = m_hardfork->get_current_version();
  unlocked = is_tx_spendtime_unlocked(m_db->get_tx_unlock_time(toi.first), hf_version);
}
//------------------------------------------------------------------
bool Blockchain::get_output_distribution(uint64_t amount, uint64_t from_height, uint64_t to_height, uint64_t &start_height, std::vector<uint64_t> &distribution, uint64_t &base) const
{
  // rct outputs don't exist before v4
  if (amount == 0 && m_nettype != network_type::FAKECHAIN)
    start_height = m_hardfork->get_earliest_ideal_height_for_version(HF_VERSION_DYNAMIC_FEE);
  else
    start_height = 0;
  base = 0;

  if (to_height > 0 && to_height < from_height)
    return false;

  if (from_height > start_height)
    start_height = from_height;

  distribution.clear();
  uint64_t db_height = m_db->height();
  if (db_height == 0)
    return false;
  if (start_height >= db_height || to_height >= db_height)
    return false;
  if (amount == 0)
  {
    std::vector<uint64_t> heights;
    heights.reserve(to_height + 1 - start_height);
    const uint64_t real_start_height = start_height > 0 ? start_height-1 : start_height;
    for (uint64_t h = real_start_height; h <= to_height; ++h)
      heights.push_back(h);
    distribution = m_db->get_block_cumulative_rct_outputs(heights);
    if (start_height > 0)
    {
      base = distribution[0];
      distribution.erase(distribution.begin());
    }
    return true;
  }
  else
  {
    return m_db->get_output_distribution(amount, start_height, to_height, distribution, base);
  }
}
//------------------------------------------------------------------
// This function takes a list of block hashes from another node
// on the network to find where the split point is between us and them.
// This is used to see what to send another node that needs to sync.
bool Blockchain::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, uint64_t& starter_offset) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  // make sure the request includes at least the genesis block, otherwise
  // how can we expect to sync from the client that the block list came from?
  if(qblock_ids.empty())
  {
    MCERROR("net.p2p", "Client sent wrong NOTIFY_REQUEST_CHAIN: m_block_ids.size()=" << qblock_ids.size() << ", dropping connection");
    return false;
  }

  db_rtxn_guard rtxn_guard(m_db);
  // make sure that the last block in the request's block list matches
  // the genesis block
  auto gen_hash = m_db->get_block_hash_from_height(0);
  if(qblock_ids.back() != gen_hash)
  {
    MCERROR("net.p2p", "Client sent wrong NOTIFY_REQUEST_CHAIN: genesis block mismatch: " << std::endl << "id: " << qblock_ids.back() << ", " << std::endl << "expected: " << gen_hash << "," << std::endl << " dropping connection");
    return false;
  }

  // Find the first block the foreign chain has that we also have.
  // Assume qblock_ids is in reverse-chronological order.
  auto bl_it = qblock_ids.begin();
  uint64_t split_height = 0;
  for(; bl_it != qblock_ids.end(); bl_it++)
  {
    try
    {
      if (m_db->block_exists(*bl_it, &split_height))
        break;
    }
    catch (const std::exception& e)
    {
      MWARNING("Non-critical error trying to find block by hash in BlockchainDB, hash: " << *bl_it);
      return false;
    }
  }

  // this should be impossible, as we checked that we share the genesis block,
  // but just in case...
  if(bl_it == qblock_ids.end())
  {
    MERROR("Internal error handling connection, can't find split point");
    return false;
  }

  //we start to put block ids INCLUDING last known id, just to make other side be sure
  starter_offset = split_height;
  return true;
}
//------------------------------------------------------------------
difficulty_type Blockchain::block_difficulty(uint64_t i) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  // WARNING: this function does not take m_blockchain_lock, and thus should only call read only
  // m_db functions which do not depend on one another (ie, no getheight + gethash(height-1), as
  // well as not accessing class members, even read only (ie, m_invalid_blocks). The caller must
  // lock if it is otherwise needed.
  try
  {
    return m_db->get_block_difficulty(i);
  }
  catch (const BLOCK_DNE& e)
  {
    MERROR("Attempted to get block difficulty for height above blockchain height");
  }
  return 0;
}
//------------------------------------------------------------------
template<typename T> void reserve_container(std::vector<T> &v, size_t N) { v.reserve(N); }
template<typename T> void reserve_container(std::list<T> &v, size_t N) { }
//------------------------------------------------------------------
//TODO: return type should be void, throw on exception
//       alternatively, return true only if no blocks missed
template<class t_ids_container, class t_blocks_container, class t_missed_container>
bool Blockchain::get_blocks(const t_ids_container& block_ids, t_blocks_container& blocks, t_missed_container& missed_bs) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  reserve_container(blocks, block_ids.size());
  for (const auto& block_hash : block_ids)
  {
    try
    {
      uint64_t height = 0;
      if (m_db->block_exists(block_hash, &height))
      {
        blocks.push_back(std::make_pair(m_db->get_block_blob_from_height(height), block()));
        if (!parse_and_validate_block_from_blob(blocks.back().first, blocks.back().second))
        {
          LOG_ERROR("Invalid block: " << block_hash);
          blocks.pop_back();
          missed_bs.push_back(block_hash);
        }
      }
      else
        missed_bs.push_back(block_hash);
    }
    catch (const std::exception& e)
    {
      return false;
    }
  }
  return true;
}
//------------------------------------------------------------------
static bool fill(BlockchainDB *db, const crypto::hash &tx_hash, cryptonote::blobdata &tx, bool pruned)
{
  if (pruned)
  {
    if (!db->get_pruned_tx_blob(tx_hash, tx))
    {
      MDEBUG("Pruned transaction blob not found for " << tx_hash);
      return false;
    }
  }
  else
  {
    if (!db->get_tx_blob(tx_hash, tx))
    {
      MDEBUG("Transaction blob not found for " << tx_hash);
      return false;
    }
  }
  return true;
}
//------------------------------------------------------------------
static bool fill(BlockchainDB *db, const crypto::hash &tx_hash, tx_blob_entry &tx, bool pruned)
{
  if (!fill(db, tx_hash, tx.blob, pruned))
    return false;
  if (pruned)
  {
    if (is_v1_tx(tx.blob))
    {
      // v1 txes aren't pruned, so fetch the whole thing
      cryptonote::blobdata prunable_blob;
      if (!db->get_prunable_tx_blob(tx_hash, prunable_blob))
      {
        MDEBUG("Prunable transaction blob not found for " << tx_hash);
        return false;
      }
      tx.blob.append(prunable_blob);
      tx.prunable_hash = crypto::null_hash;
    }
    else
    {
      if (!db->get_prunable_tx_hash(tx_hash, tx.prunable_hash))
      {
        MDEBUG("Prunable transaction data hash not found for " << tx_hash);
        return false;
      }
    }
  }
  return true;
}
//------------------------------------------------------------------
//TODO: return type should be void, throw on exception
//       alternatively, return true only if no transactions missed
bool Blockchain::get_transactions_blobs(const std::vector<crypto::hash>& txs_ids, std::vector<cryptonote::blobdata>& txs, std::vector<crypto::hash>& missed_txs, bool pruned) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  txs.reserve(txs_ids.size());
  for (const auto& tx_hash : txs_ids)
  {
    try
    {
      cryptonote::blobdata tx;
      if (fill(m_db, tx_hash, tx, pruned))
        txs.push_back(std::move(tx));
      else
        missed_txs.push_back(tx_hash);
    }
    catch (const std::exception& e)
    {
      return false;
    }
  }
  return true;
}
//------------------------------------------------------------------
bool Blockchain::get_transactions_blobs(const std::vector<crypto::hash>& txs_ids, std::vector<tx_blob_entry>& txs, std::vector<crypto::hash>& missed_txs, bool pruned) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  txs.reserve(txs_ids.size());
  for (const auto& tx_hash : txs_ids)
  {
    try
    {
      tx_blob_entry tx;
      if (fill(m_db, tx_hash, tx, pruned))
        txs.push_back(std::move(tx));
      else
        missed_txs.push_back(tx_hash);
    }
    catch (const std::exception& e)
    {
      return false;
    }
  }
  return true;
}
//------------------------------------------------------------------
size_t get_transaction_version(const cryptonote::blobdata &bd)
{
  size_t version;
  const char* begin = static_cast<const char*>(bd.data());
  const char* end = begin + bd.size();
  int read = tools::read_varint(begin, end, version);
  if (read <= 0)
    throw std::runtime_error("Internal error getting transaction version");
  return version;
}
//------------------------------------------------------------------
template<class t_ids_container, class t_tx_container, class t_missed_container>
bool Blockchain::get_split_transactions_blobs(const t_ids_container& txs_ids, t_tx_container& txs, t_missed_container& missed_txs) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  reserve_container(txs, txs_ids.size());
  for (const auto& tx_hash : txs_ids)
  {
    try
    {
      cryptonote::blobdata tx;
      if (m_db->get_pruned_tx_blob(tx_hash, tx))
      {
        txs.push_back(std::make_tuple(tx_hash, std::move(tx), crypto::null_hash, cryptonote::blobdata()));
        if (!is_v1_tx(std::get<1>(txs.back())) && !m_db->get_prunable_tx_hash(tx_hash, std::get<2>(txs.back())))
        {
          MERROR("Prunable data hash not found for " << tx_hash);
          return false;
        }
        if (!m_db->get_prunable_tx_blob(tx_hash, std::get<3>(txs.back())))
          std::get<3>(txs.back()).clear();
      }
      else
        missed_txs.push_back(tx_hash);
    }
    catch (const std::exception& e)
    {
      return false;
    }
  }
  return true;
}
//------------------------------------------------------------------
template<class t_ids_container, class t_tx_container, class t_missed_container>
bool Blockchain::get_transactions(const t_ids_container& txs_ids, t_tx_container& txs, t_missed_container& missed_txs, bool pruned) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  reserve_container(txs, txs_ids.size());
  for (const auto& tx_hash : txs_ids)
  {
    try
    {
      cryptonote::blobdata tx;
      bool res = pruned ? m_db->get_pruned_tx_blob(tx_hash, tx) : m_db->get_tx_blob(tx_hash, tx);
      if (res)
      {
        txs.push_back(transaction());
        res = pruned ? parse_and_validate_tx_base_from_blob(tx, txs.back()) : parse_and_validate_tx_from_blob(tx, txs.back());
        if (!res)
        {
          LOG_ERROR("Invalid transaction");
          return false;
        }
      }
      else
        missed_txs.push_back(tx_hash);
    }
    catch (const std::exception& e)
    {
      return false;
    }
  }
  return true;
}
//------------------------------------------------------------------
// Find the split point between us and foreign blockchain and return
// (by reference) the most recent common block hash along with up to
// BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT additional (more recent) hashes.
bool Blockchain::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, std::vector<crypto::hash>& hashes, std::vector<uint64_t>* weights, uint64_t& start_height, uint64_t& current_height, bool clip_pruned) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  // if we can't find the split point, return false
  if(!find_blockchain_supplement(qblock_ids, start_height))
  {
    return false;
  }

  db_rtxn_guard rtxn_guard(m_db);
  current_height = get_current_blockchain_height();
  uint64_t stop_height = current_height;
  if (clip_pruned)
  {
    const uint32_t pruning_seed = get_blockchain_pruning_seed();
    if (start_height < tools::get_next_unpruned_block_height(start_height, current_height, pruning_seed))
    {
      MDEBUG("We only have a pruned version of the common ancestor");
      return false;
    }
    stop_height = tools::get_next_pruned_block_height(start_height, current_height, pruning_seed);
  }
  size_t count = 0;
  const size_t reserve = std::min((size_t)(stop_height - start_height), (size_t)BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT);
  hashes.reserve(reserve);
  if (weights)
    weights->reserve(reserve);
  for(size_t i = start_height; i < stop_height && count < BLOCKS_IDS_SYNCHRONIZING_DEFAULT_COUNT; i++, count++)
  {
    hashes.push_back(m_db->get_block_hash_from_height(i));
    if (weights)
      weights->push_back(m_db->get_block_weight(i));
  }

  return true;
}

bool Blockchain::find_blockchain_supplement(const std::list<crypto::hash>& qblock_ids, bool clip_pruned, NOTIFY_RESPONSE_CHAIN_ENTRY::request& resp) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  bool result = find_blockchain_supplement(qblock_ids, resp.m_block_ids, &resp.m_block_weights, resp.start_height, resp.total_height, clip_pruned);
  if (result)
  {
    cryptonote::difficulty_type wide_cumulative_difficulty = m_db->get_block_cumulative_difficulty(resp.total_height - 1);
    resp.cumulative_difficulty = (wide_cumulative_difficulty & 0xffffffffffffffff).convert_to<uint64_t>();
    resp.cumulative_difficulty_top64 = ((wide_cumulative_difficulty >> 64) & 0xffffffffffffffff).convert_to<uint64_t>();
  }

  return result;
}
//------------------------------------------------------------------
//FIXME: change argument to std::vector, low priority
// find split point between ours and foreign blockchain (or start at
// blockchain height <req_start_block>), and return up to max_count FULL
// blocks by reference.
bool Blockchain::find_blockchain_supplement(const uint64_t req_start_block, const std::list<crypto::hash>& qblock_ids, std::vector<std::pair<std::pair<cryptonote::blobdata, crypto::hash>, std::vector<std::pair<crypto::hash, cryptonote::blobdata> > > >& blocks, uint64_t& total_height, uint64_t& start_height, bool pruned, bool get_miner_tx_hash, size_t max_block_count, size_t max_tx_count) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  // if a specific start height has been requested
  if(req_start_block > 0)
  {
    // if requested height is higher than our chain, return false -- we can't help
    if (req_start_block >= m_db->height())
    {
      return false;
    }
    start_height = req_start_block;
  }
  else
  {
    if(!find_blockchain_supplement(qblock_ids, start_height))
    {
      return false;
    }
  }

  db_rtxn_guard rtxn_guard(m_db);
  total_height = get_current_blockchain_height();
  blocks.reserve(std::min(std::min(max_block_count, (size_t)10000), (size_t)(total_height - start_height)));
  CHECK_AND_ASSERT_MES(m_db->get_blocks_from(start_height, 3, max_block_count, max_tx_count, FIND_BLOCKCHAIN_SUPPLEMENT_MAX_SIZE, blocks, pruned, true, get_miner_tx_hash),
      false, "Error getting blocks");

  return true;
}
//------------------------------------------------------------------
bool Blockchain::add_block_as_invalid(const block& bl, const crypto::hash& h)
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  block_extended_info bei = AUTO_VAL_INIT(bei);
  bei.bl = bl;
  return add_block_as_invalid(bei, h);
}
//------------------------------------------------------------------
bool Blockchain::add_block_as_invalid(const block_extended_info& bei, const crypto::hash& h)
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  auto i_res = m_invalid_blocks.insert(std::map<crypto::hash, block_extended_info>::value_type(h, bei));
  CHECK_AND_ASSERT_MES(i_res.second, false, "at insertion invalid by tx returned status existed");
  MINFO("BLOCK ADDED AS INVALID: " << h << std::endl << ", prev_id=" << bei.bl.prev_id << ", m_invalid_blocks count=" << m_invalid_blocks.size());
  return true;
}
//------------------------------------------------------------------
void Blockchain::flush_invalid_blocks()
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  m_invalid_blocks.clear();
}
//------------------------------------------------------------------
bool Blockchain::have_block_unlocked(const crypto::hash& id, int *where) const
{
  // WARNING: this function does not take m_blockchain_lock, and thus should only call read only
  // m_db functions which do not depend on one another (ie, no getheight + gethash(height-1), as
  // well as not accessing class members, even read only (ie, m_invalid_blocks). The caller must
  // lock if it is otherwise needed.
  LOG_PRINT_L3("Blockchain::" << __func__);

  if(m_db->block_exists(id))
  {
    LOG_PRINT_L2("block " << id << " found in main chain");
    if (where) *where = HAVE_BLOCK_MAIN_CHAIN;
    return true;
  }

  if(m_db->get_alt_block(id, NULL, NULL))
  {
    LOG_PRINT_L2("block " << id << " found in alternative chains");
    if (where) *where = HAVE_BLOCK_ALT_CHAIN;
    return true;
  }

  if(m_invalid_blocks.count(id))
  {
    LOG_PRINT_L2("block " << id << " found in m_invalid_blocks");
    if (where) *where = HAVE_BLOCK_INVALID;
    return true;
  }

  return false;
}
//------------------------------------------------------------------
bool Blockchain::have_block(const crypto::hash& id, int *where) const
{
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  return have_block_unlocked(id, where);
}
//------------------------------------------------------------------
bool Blockchain::handle_block_to_main_chain(const block& bl, block_verification_context& bvc)
{
    LOG_PRINT_L3("Blockchain::" << __func__);
    crypto::hash id = get_block_hash(bl);
    pool_supplement ps{};
    return handle_block_to_main_chain(bl, id, bvc, ps);
}
//------------------------------------------------------------------
size_t Blockchain::get_total_transactions() const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  // WARNING: this function does not take m_blockchain_lock, and thus should only call read only
  // m_db functions which do not depend on one another (ie, no getheight + gethash(height-1), as
  // well as not accessing class members, even read only (ie, m_invalid_blocks). The caller must
  // lock if it is otherwise needed.
  return m_db->get_tx_count();
}
//------------------------------------------------------------------
// This function checks each input in the transaction <tx> to make sure it
// has not been used already, and adds its key to the container <keys_this_block>.
//
// This container should be managed by the code that validates blocks so we don't
// have to store the used keys in a given block in the permanent storage only to
// remove them later if the block fails validation.
bool Blockchain::check_for_double_spend(const transaction& tx, key_images_container& keys_this_block) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  struct add_transaction_input_visitor: public boost::static_visitor<bool>
  {
    key_images_container& m_spent_keys;
    BlockchainDB* m_db;
    add_transaction_input_visitor(key_images_container& spent_keys, BlockchainDB* db) :
      m_spent_keys(spent_keys), m_db(db)
    {
    }
    bool operator()(const txin_to_key& in) const
    {
      const crypto::key_image& ki = in.k_image;

      // attempt to insert the newly-spent key into the container of
      // keys spent this block.  If this fails, the key was spent already
      // in this block, return false to flag that a double spend was detected.
      //
      // if the insert into the block-wide spent keys container succeeds,
      // check the blockchain-wide spent keys container and make sure the
      // key wasn't used in another block already.
      auto r = m_spent_keys.insert(ki);
      if(!r.second || m_db->has_key_image(ki))
      {
        //double spend detected
        return false;
      }

      // if no double-spend detected, return true
      return true;
    }

    bool operator()(const txin_gen& tx) const
    {
      return true;
    }
    bool operator()(const txin_to_script& tx) const
    {
      return false;
    }
    bool operator()(const txin_to_scripthash& tx) const
    {
      return false;
    }
  };

  for (const txin_v& in : tx.vin)
  {
    if(!boost::apply_visitor(add_transaction_input_visitor(keys_this_block, m_db), in))
    {
      LOG_ERROR("Double spend detected!");
      return false;
    }
  }

  return true;
}
//------------------------------------------------------------------
bool Blockchain::get_tx_outputs_gindexs(const crypto::hash& tx_id, size_t n_txes, std::vector<std::vector<uint64_t>>& indexs) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  uint64_t tx_index;
  if (!m_db->tx_exists(tx_id, tx_index))
  {
    MERROR_VER("get_tx_outputs_gindexs failed to find transaction with id = " << tx_id);
    return false;
  }
  indexs = m_db->get_tx_amount_output_indices(tx_index, n_txes);
  CHECK_AND_ASSERT_MES(n_txes == indexs.size(), false, "Wrong indexs size");

  return true;
}
//------------------------------------------------------------------
bool Blockchain::get_tx_outputs_gindexs(const crypto::hash& tx_id, std::vector<uint64_t>& indexs) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  uint64_t tx_index;
  if (!m_db->tx_exists(tx_id, tx_index))
  {
    MERROR_VER("get_tx_outputs_gindexs failed to find transaction with id = " << tx_id);
    return false;
  }
  std::vector<std::vector<uint64_t>> indices = m_db->get_tx_amount_output_indices(tx_index, 1);
  CHECK_AND_ASSERT_MES(indices.size() == 1, false, "Wrong indices size");
  indexs = indices.front();
  return true;
}
//------------------------------------------------------------------
//FIXME: it seems this function is meant to be merely a wrapper around
//       another function of the same name, this one adding one bit of
//       functionality.  Should probably move anything more than that
//       (getting the hash of the block at height max_used_block_id)
//       to the other function to keep everything in one place.
// This function overloads its sister function with
// an extra value (hash of highest block that holds an output used as input)
// as a return-by-reference.
bool Blockchain::check_tx_inputs(transaction& tx, uint64_t& max_used_block_height, crypto::hash& max_used_block_id, tx_verification_context &tvc, bool kept_by_block, uint8_t tx_rules_version) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

#if defined(PER_BLOCK_CHECKPOINT)
  // check if we're doing per-block checkpointing
  if (m_db->height() < m_blocks_hash_check.size() && kept_by_block)
  {
    max_used_block_id = null_hash;
    max_used_block_height = 0;
    return true;
  }
#endif

  TIME_MEASURE_START(a);
  bool res = check_tx_inputs(tx, tvc, &max_used_block_height, tx_rules_version);
  TIME_MEASURE_FINISH(a);
  if(m_show_time_stats)
  {
    size_t ring_size = !tx.vin.empty() && tx.vin[0].type() == typeid(txin_to_key) ? boost::get<txin_to_key>(tx.vin[0]).key_offsets.size() : 0;
    MINFO("HASH: " <<  get_transaction_hash(tx) << " I/M/O: " << tx.vin.size() << "/" << ring_size << "/" << tx.vout.size() << " H: " << max_used_block_height << " ms: " << a + m_fake_scan_time << " B: " << get_object_blobsize(tx) << " W: " << get_transaction_weight(tx));
  }
  if (!res)
    return false;

  CHECK_AND_ASSERT_MES(max_used_block_height < m_db->height(), false,  "internal error: max used block index=" << max_used_block_height << " is not less then blockchain size = " << m_db->height());
  max_used_block_id = m_db->get_block_hash_from_height(max_used_block_height);
  return true;
}
//------------------------------------------------------------------
bool Blockchain::check_tx_outputs(const transaction& tx, tx_verification_context &tvc, std::uint8_t hf_version)
{
  LOG_PRINT_L3("Blockchain::" << __func__);

  // from hard fork 2, we forbid dust and compound outputs
  if (hf_version >= 2) {
    for (auto &o: tx.vout) {
      if (tx.version == 1)
      {
        if (!is_valid_decomposed_amount(o.amount)) {
          tvc.m_invalid_output = true;
          return false;
        }
      }
    }
  }

  // in a v2 tx, all outputs must have 0 amount
  if (hf_version >= 3) {
    if (tx.version >= 2) {
      for (auto &o: tx.vout) {
        if (o.amount != 0) {
          tvc.m_invalid_output = true;
          return false;
        }
      }
    }
  }

  // from v8, allow bulletproofs
  if (hf_version < 8) {
    if (tx.version >= 2) {
      const bool bulletproof = rct::is_rct_bulletproof(tx.rct_signatures.type);
      if (bulletproof || !tx.rct_signatures.p.bulletproofs.empty())
      {
        MERROR_VER("Bulletproofs are not allowed before v8");
        tvc.m_invalid_output = true;
        return false;
      }
    }
  }

  // from v9, forbid borromean range proofs
  if (hf_version > 8) {
    if (tx.version >= 2) {
      const bool borromean = rct::is_rct_borromean(tx.rct_signatures.type);
      if (borromean)
      {
        MERROR_VER("Borromean range proofs are not allowed after v8");
        tvc.m_invalid_output = true;
        return false;
      }
    }
  }

  // from v10, allow bulletproofs v2
  if (hf_version < HF_VERSION_SMALLER_BP) {
    if (tx.version >= 2) {
      if (tx.rct_signatures.type == rct::RCTTypeBulletproof2)
      {
        MERROR_VER("Ringct type " << (unsigned)rct::RCTTypeBulletproof2 << " is not allowed before v" << HF_VERSION_SMALLER_BP);
        tvc.m_invalid_output = true;
        return false;
      }
    }
  }

  // from v11, allow only bulletproofs v2
  if (hf_version > HF_VERSION_SMALLER_BP) {
    if (tx.version >= 2) {
      if (tx.rct_signatures.type == rct::RCTTypeBulletproof)
      {
        MERROR_VER("Ringct type " << (unsigned)rct::RCTTypeBulletproof << " is not allowed from v" << (HF_VERSION_SMALLER_BP + 1));
        tvc.m_invalid_output = true;
        return false;
      }
    }
  }

  // from v13, allow CLSAGs
  if (hf_version < HF_VERSION_CLSAG) {
    if (tx.version >= 2) {
      if (tx.rct_signatures.type == rct::RCTTypeCLSAG)
      {
        MERROR_VER("Ringct type " << (unsigned)rct::RCTTypeCLSAG << " is not allowed before v" << HF_VERSION_CLSAG);
        tvc.m_invalid_output = true;
        return false;
      }
    }
  }

  // from v14, allow only CLSAGs
  if (hf_version > HF_VERSION_CLSAG) {
    if (tx.version >= 2) {
      if (tx.rct_signatures.type <= rct::RCTTypeBulletproof2)
      {
        // two MLSAG txes went in due to a bug with txes that went into the txpool before the fork, grandfather them in
        static const char * grandfathered[2] = { "c5151944f0583097ba0c88cd0f43e7fabb3881278aa2f73b3b0a007c5d34e910", "6f2f117cde6fbcf8d4a6ef8974fcac744726574ac38cf25d3322c996b21edd4c" };
        crypto::hash h0, h1;
        epee::string_tools::hex_to_pod(grandfathered[0], h0);
        epee::string_tools::hex_to_pod(grandfathered[1], h1);
        if (cryptonote::get_transaction_hash(tx) == h0 || cryptonote::get_transaction_hash(tx) == h1)
        {
          MDEBUG("Grandfathering cryptonote::get_transaction_hash(tx) in");
        }
        else
        {
          MERROR_VER("Ringct type " << (unsigned)tx.rct_signatures.type << " is not allowed from v" << (HF_VERSION_CLSAG + 1));
          tvc.m_invalid_output = true;
          return false;
        }
      }
    }
  }

  // from v15, allow bulletproofs plus
  if (hf_version < HF_VERSION_BULLETPROOF_PLUS) {
    if (tx.version >= 2) {
      const bool bulletproof_plus = rct::is_rct_bulletproof_plus(tx.rct_signatures.type);
      if (bulletproof_plus || !tx.rct_signatures.p.bulletproofs_plus.empty())
      {
        MERROR_VER("Bulletproofs plus are not allowed before v" << std::to_string(HF_VERSION_BULLETPROOF_PLUS));
        tvc.m_invalid_output = true;
        return false;
      }
    }
  }

  // from v16, forbid bulletproofs
  if (hf_version > HF_VERSION_BULLETPROOF_PLUS) {
    if (tx.version >= 2) {
      const bool bulletproof = rct::is_rct_bulletproof(tx.rct_signatures.type);
      if (bulletproof)
      {
        MERROR_VER("Bulletproof range proofs are not allowed after v" + std::to_string(HF_VERSION_BULLETPROOF_PLUS));
        tvc.m_invalid_output = true;
        return false;
      }
    }
  }

  // from v15, require view tags on outputs
  if (!check_output_types(tx, hf_version))
  {
    tvc.m_invalid_output = true;
    return false;
  }

  return true;
}
//------------------------------------------------------------------
bool Blockchain::have_tx_keyimges_as_spent(const transaction &tx) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  for (const txin_v& in: tx.vin)
  {
    CHECKED_GET_SPECIFIC_VARIANT(in, const txin_to_key, in_to_key, true);
    if(have_tx_keyimg_as_spent(in_to_key.k_image))
      return true;
  }
  return false;
}
bool Blockchain::expand_transaction_2(transaction &tx, const crypto::hash &tx_prefix_hash, const std::vector<std::vector<rct::ctkey>> &pubkeys)
{
  PERF_TIMER(expand_transaction_2);
  CHECK_AND_ASSERT_MES(tx.version == 2, false, "Transaction version is not 2");

  rct::rctSig &rv = tx.rct_signatures;

  // message - hash of the transaction prefix
  rv.message = rct::hash2rct(tx_prefix_hash);

  // mixRing - full and simple store it in opposite ways
  if (rv.type == rct::RCTTypeFull)
  {
    CHECK_AND_ASSERT_MES(!pubkeys.empty() && !pubkeys[0].empty(), false, "empty pubkeys");
    rv.mixRing.resize(pubkeys[0].size());
    for (size_t m = 0; m < pubkeys[0].size(); ++m)
      rv.mixRing[m].clear();
    for (size_t n = 0; n < pubkeys.size(); ++n)
    {
      CHECK_AND_ASSERT_MES(pubkeys[n].size() <= pubkeys[0].size(), false, "More inputs that first ring");
      for (size_t m = 0; m < pubkeys[n].size(); ++m)
      {
        rv.mixRing[m].push_back(pubkeys[n][m]);
      }
    }
  }
  else if (rv.type == rct::RCTTypeSimple || rv.type == rct::RCTTypeBulletproof || rv.type == rct::RCTTypeBulletproof2 || rv.type == rct::RCTTypeCLSAG || rv.type == rct::RCTTypeBulletproofPlus)
  {
    CHECK_AND_ASSERT_MES(!pubkeys.empty() && !pubkeys[0].empty(), false, "empty pubkeys");
    rv.mixRing.resize(pubkeys.size());
    for (size_t n = 0; n < pubkeys.size(); ++n)
    {
      rv.mixRing[n].clear();
      for (size_t m = 0; m < pubkeys[n].size(); ++m)
      {
        rv.mixRing[n].push_back(pubkeys[n][m]);
      }
    }
  }
  else
  {
    CHECK_AND_ASSERT_MES(false, false, "Unsupported rct tx type: " + boost::lexical_cast<std::string>(rv.type));
  }

  // II
  if (rv.type == rct::RCTTypeFull)
  {
    if (!tx.pruned)
    {
      rv.p.MGs.resize(1);
      rv.p.MGs[0].II.resize(tx.vin.size());
      for (size_t n = 0; n < tx.vin.size(); ++n)
        rv.p.MGs[0].II[n] = rct::ki2rct(boost::get<txin_to_key>(tx.vin[n]).k_image);
    }
  }
  else if (rv.type == rct::RCTTypeSimple || rv.type == rct::RCTTypeBulletproof || rv.type == rct::RCTTypeBulletproof2)
  {
    if (!tx.pruned)
    {
      CHECK_AND_ASSERT_MES(rv.p.MGs.size() == tx.vin.size(), false, "Bad MGs size");
      for (size_t n = 0; n < tx.vin.size(); ++n)
      {
        rv.p.MGs[n].II.resize(1);
        rv.p.MGs[n].II[0] = rct::ki2rct(boost::get<txin_to_key>(tx.vin[n]).k_image);
      }
    }
  }
  else if (rv.type == rct::RCTTypeCLSAG || rv.type == rct::RCTTypeBulletproofPlus)
  {
    if (!tx.pruned)
    {
      CHECK_AND_ASSERT_MES(rv.p.CLSAGs.size() == tx.vin.size(), false, "Bad CLSAGs size");
      for (size_t n = 0; n < tx.vin.size(); ++n)
      {
        rv.p.CLSAGs[n].I = rct::ki2rct(boost::get<txin_to_key>(tx.vin[n]).k_image);
      }
    }
  }
  else
  {
    CHECK_AND_ASSERT_MES(false, false, "Unsupported rct tx type: " + boost::lexical_cast<std::string>(rv.type));
  }

  // outPk was already done by handle_incoming_tx

  return true;
}
//------------------------------------------------------------------
// This function validates transaction inputs and their keys.
// FIXME: consider moving functionality specific to one input into
//        check_tx_input() rather than here, and use this function simply
//        to iterate the inputs as necessary (splitting the task
//        using threads, etc.)
bool Blockchain::check_tx_inputs(transaction& tx, tx_verification_context &tvc, uint64_t* pmax_used_block_height, uint8_t tx_rules_version) const
{
  PERF_TIMER(check_tx_inputs);
  LOG_PRINT_L3("Blockchain::" << __func__);
  size_t sig_index = 0;
  if(pmax_used_block_height)
    *pmax_used_block_height = 0;

  const uint8_t hf_version = m_hardfork->get_current_version();
  crypto::hash tx_prefix_hash;
  if (!get_contract_ring_hash(tx, tx_prefix_hash, tx_rules_version))
    return false;

  if (hf_version >= HF_VERSION_MIN_2_OUTPUTS)
  {
    if (tx.version >= 2)
    {
      if (tx.vout.size() < 2)
      {
        MERROR_VER("Tx " << get_transaction_hash(tx) << " has fewer than two outputs");
        tvc.m_too_few_outputs = true;
        return false;
      }
    }
  }

  // from hard fork 2, we require mixin at least 2 unless one output cannot mix with 2 others
  // if one output cannot mix with 2 others, we accept at most 1 output that can mix
  if (hf_version >= 2)
  {
    size_t n_unmixable = 0, n_mixable = 0;
    size_t min_actual_mixin = std::numeric_limits<size_t>::max();
    size_t max_actual_mixin = 0;
    const size_t min_mixin = hf_version >= HF_VERSION_MIN_MIXIN_15 ? 15 : hf_version >= HF_VERSION_MIN_MIXIN_10 ? 10 : hf_version >= HF_VERSION_MIN_MIXIN_6 ? 6 : hf_version >= HF_VERSION_MIN_MIXIN_4 ? 4 : 2;
    for (const auto& txin : tx.vin)
    {
      // non txin_to_key inputs will be rejected below
      if (txin.type() == typeid(txin_to_key))
      {
        const txin_to_key& in_to_key = boost::get<txin_to_key>(txin);
        if (in_to_key.amount == 0)
        {
          // always consider rct inputs mixable. Even if there's not enough rct
          // inputs on the chain to mix with, this is going to be the case for
          // just a few blocks right after the fork at most
          ++n_mixable;
        }
        else
        {
          uint64_t n_outputs = m_db->get_num_outputs(in_to_key.amount);
          MDEBUG("output size " << print_money(in_to_key.amount) << ": " << n_outputs << " available");
          // n_outputs includes the output we're considering
          if (n_outputs <= min_mixin)
            ++n_unmixable;
          else
            ++n_mixable;
        }
        size_t ring_mixin = in_to_key.key_offsets.size() - 1;
        if (ring_mixin < min_actual_mixin)
          min_actual_mixin = ring_mixin;
        if (ring_mixin > max_actual_mixin)
          max_actual_mixin = ring_mixin;
      }
    }
    MDEBUG("Mixin: " << min_actual_mixin << "-" << max_actual_mixin);

    if (hf_version >= HF_VERSION_SAME_MIXIN)
    {
      if (min_actual_mixin != max_actual_mixin)
      {
        MERROR_VER("Tx " << get_transaction_hash(tx) << " has varying ring size (" << (min_actual_mixin + 1) << "-" << (max_actual_mixin + 1) << "), it should be constant");
        tvc.m_low_mixin = true;
        return false;
      }
    }

    // The only circumstance where ring sizes less than expected are
    // allowed is when spending unmixable non-RCT outputs in the chain.
    // Caveat: at HF_VERSION_MIN_MIXIN_15, temporarily allow ring sizes
    // of 11 to allow a grace period in the transition to larger ring size.
    if (min_actual_mixin < min_mixin && !(hf_version == HF_VERSION_MIN_MIXIN_15 && min_actual_mixin == 10))
    {
      if (n_unmixable == 0)
      {
        MERROR_VER("Tx " << get_transaction_hash(tx) << " has too low ring size (" << (min_actual_mixin + 1) << "), and no unmixable inputs");
        tvc.m_low_mixin = true;
        return false;
      }
      if (n_mixable > 1)
      {
        MERROR_VER("Tx " << get_transaction_hash(tx) << " has too low ring size (" << (min_actual_mixin + 1) << "), and more than one mixable input with unmixable inputs");
        tvc.m_low_mixin = true;
        return false;
      }
    } else if ((hf_version > HF_VERSION_MIN_MIXIN_15 && min_actual_mixin > 15)
      || (hf_version == HF_VERSION_MIN_MIXIN_15 && min_actual_mixin != 15 && min_actual_mixin != 10) // grace period to allow either 15 or 10
      || (hf_version < HF_VERSION_MIN_MIXIN_15 && hf_version >= HF_VERSION_MIN_MIXIN_10+2 && min_actual_mixin > 10)
      || ((hf_version == HF_VERSION_MIN_MIXIN_10 || hf_version == HF_VERSION_MIN_MIXIN_10+1) && min_actual_mixin != 10)
    )
    {
      MERROR_VER("Tx " << get_transaction_hash(tx) << " has invalid ring size (" << (min_actual_mixin + 1) << "), it should be " << (min_mixin + 1));
      tvc.m_low_mixin = true;
      return false;
    }

    // min/max tx version based on HF, and we accept v1 txes if having a non mixable
    const size_t max_tx_version = (hf_version <= 3) ? 1 : 2;
    if (tx.version > max_tx_version)
    {
      MERROR_VER("transaction version " << (unsigned)tx.version << " is higher than max accepted version " << max_tx_version);
      tvc.m_verifivation_failed = true;
      return false;
    }
    tx_extra_vm_call contract_call{};
    uint64_t contract_fee = 0;
    uint64_t transaction_fee = 0;
    const bool zero_output_contract_call = is_qwc_contracts_hardfork(hf_version)
        && tx.version == 1 && tx.vout.empty()
        && get_tx_vm_call(tx.extra, contract_call) == 1
        && contract_call_fee_floor(contract_call, contract_fee)
        && get_tx_fee(tx, transaction_fee) && transaction_fee == contract_fee;
    const size_t min_tx_version = (n_unmixable > 0 || zero_output_contract_call
        ? 1 : (hf_version >= HF_VERSION_ENFORCE_RCT) ? 2 : 1);
    if (tx.version < min_tx_version)
    {
      MERROR_VER("transaction version " << (unsigned)tx.version << " is lower than min accepted version " << min_tx_version);
      tvc.m_verifivation_failed = true;
      return false;
    }
  }

  // from v7, sorted ins
  if (hf_version >= 7) {
    const crypto::key_image *last_key_image = NULL;
    for (size_t n = 0; n < tx.vin.size(); ++n)
    {
      const txin_v &txin = tx.vin[n];
      if (txin.type() == typeid(txin_to_key))
      {
        const txin_to_key& in_to_key = boost::get<txin_to_key>(txin);
        if (last_key_image && memcmp(&in_to_key.k_image, last_key_image, sizeof(*last_key_image)) >= 0)
        {
          MERROR_VER("transaction has unsorted inputs");
          tvc.m_verifivation_failed = true;
          return false;
        }
        last_key_image = &in_to_key.k_image;
      }
    }
  }

  std::vector<std::vector<rct::ctkey>> pubkeys(tx.vin.size());
  std::vector < uint64_t > results;
  results.resize(tx.vin.size(), 0);

  tools::threadpool& tpool = tools::threadpool::getInstanceForCompute();
  tools::threadpool::waiter waiter(tpool);
  int threads = tpool.get_max_concurrency();

  uint64_t max_used_block_height = 0;
  if (!pmax_used_block_height)
    pmax_used_block_height = &max_used_block_height;
  for (const auto& txin : tx.vin)
  {
    // make sure output being spent is of type txin_to_key, rather than
    // e.g. txin_gen, which is only used for miner transactions
    CHECK_AND_ASSERT_MES(txin.type() == typeid(txin_to_key), false, "wrong type id in tx input at Blockchain::check_tx_inputs");
    const txin_to_key& in_to_key = boost::get<txin_to_key>(txin);

    // make sure tx output has key offset(s) (is signed to be used)
    CHECK_AND_ASSERT_MES(in_to_key.key_offsets.size(), false, "empty in_to_key.key_offsets in transaction with id " << get_transaction_hash(tx));

    if(have_tx_keyimg_as_spent(in_to_key.k_image))
    {
      MERROR_VER("Key image already spent in blockchain: " << epee::string_tools::pod_to_hex(in_to_key.k_image));
      tvc.m_double_spend = true;
      return false;
    }

    if (tx.version == 1)
    {
      // basically, make sure number of inputs == number of signatures
      CHECK_AND_ASSERT_MES(sig_index < tx.signatures.size(), false, "wrong transaction: not signature entry for input with index= " << sig_index);
    }

    // make sure that output being spent matches up correctly with the
    // signature spending it.
    if (!check_tx_input(tx.version, in_to_key, tx_prefix_hash, tx.version == 1 ? tx.signatures[sig_index] : std::vector<crypto::signature>(), tx.rct_signatures, pubkeys[sig_index], pmax_used_block_height, hf_version))
    {
      MERROR_VER("Failed to check ring signature for tx " << get_transaction_hash(tx) << "  vin key with k_image: " << in_to_key.k_image << "  sig_index: " << sig_index);
      if (pmax_used_block_height) // a default value of NULL is used when called from Blockchain::handle_block_to_main_chain()
      {
        MERROR_VER("  *pmax_used_block_height: " << *pmax_used_block_height);
      }

      return false;
    }

    if (tx.version == 1)
    {
      if (threads > 1)
      {
        // ND: Speedup
        // 1. Thread ring signature verification if possible.
        tpool.submit(&waiter, boost::bind(&Blockchain::check_ring_signature, this, std::cref(tx_prefix_hash), std::cref(in_to_key.k_image), std::cref(pubkeys[sig_index]), std::cref(tx.signatures[sig_index]), std::ref(results[sig_index])), true);
      }
      else
      {
        check_ring_signature(tx_prefix_hash, in_to_key.k_image, pubkeys[sig_index], tx.signatures[sig_index], results[sig_index]);
        if (!results[sig_index])
        {
          MERROR_VER("Failed to check ring signature for tx " << get_transaction_hash(tx) << "  vin key with k_image: " << in_to_key.k_image << "  sig_index: " << sig_index);

          if (pmax_used_block_height)  // a default value of NULL is used when called from Blockchain::handle_block_to_main_chain()
          {
            MERROR_VER("*pmax_used_block_height: " << *pmax_used_block_height);
          }

          return false;
        }
      }
    }

    sig_index++;
  }
  if (tx.version == 1 && threads > 1)
    if (!waiter.wait())
      return false;

  // enforce min output age
  if (hf_version >= HF_VERSION_ENFORCE_MIN_AGE)
  {
    CHECK_AND_ASSERT_MES(*pmax_used_block_height + CRYPTONOTE_DEFAULT_TX_SPENDABLE_AGE <= m_db->height(),
        false, "Transaction spends at least one output which is too young");
  }

  // Warn that new RCT types are present, and thus the cache is not being used effectively
  static constexpr const std::uint8_t RCT_CACHE_TYPE = rct::RCTTypeBulletproofPlus;
  if (tx.rct_signatures.type > RCT_CACHE_TYPE)
  {
    MWARNING("RCT cache is not caching new verification results. Please update RCT_CACHE_TYPE!");
  }

  if (tx.version == 1)
  {
    if (threads > 1)
    {
      // save results to table, passed or otherwise
      bool failed = false;
      for (size_t i = 0; i < tx.vin.size(); i++)
      {
        if(!failed && !results[i])
          failed = true;
      }

      if (failed)
      {
        MERROR_VER("Failed to check ring signatures!");
        return false;
      }
    }
  }
  else
  {
    // from version 2, check ringct signatures
    // obviously, the original and simple rct APIs use a mixRing that's indexes
    // in opposite orders, because it'd be too simple otherwise...
    const rct::rctSig &rv = tx.rct_signatures;
    switch (rv.type)
    {
    case rct::RCTTypeNull: {
      // we only accept no signatures for coinbase txes
      MERROR_VER("Null rct signature on non-coinbase tx");
      return false;
    }
    case rct::RCTTypeSimple:
    case rct::RCTTypeBulletproof:
    case rct::RCTTypeBulletproof2:
    case rct::RCTTypeCLSAG:
    case rct::RCTTypeBulletproofPlus:
    {
      if (!ver_rct_non_semantics_simple_cached(tx, pubkeys, m_rct_ver_cache, RCT_CACHE_TYPE, tx_rules_version))
      {
        MERROR_VER("Failed to check ringct signatures!");
        return false;
      }
      break;
    }
    case rct::RCTTypeFull:
    {
      if (!expand_transaction_2(tx, tx_prefix_hash, pubkeys))
      {
        MERROR_VER("Failed to expand rct signatures!");
        return false;
      }

      // check all this, either reconstructed (so should really pass), or not
      {
        bool size_matches = true;
        for (size_t i = 0; i < pubkeys.size(); ++i)
          size_matches &= pubkeys[i].size() == rv.mixRing.size();
        for (size_t i = 0; i < rv.mixRing.size(); ++i)
          size_matches &= pubkeys.size() == rv.mixRing[i].size();
        if (!size_matches)
        {
          MERROR_VER("Failed to check ringct signatures: mismatched pubkeys/mixRing size");
          return false;
        }

        for (size_t n = 0; n < pubkeys.size(); ++n)
        {
          for (size_t m = 0; m < pubkeys[n].size(); ++m)
          {
            if (pubkeys[n][m].dest != rct::rct2pk(rv.mixRing[m][n].dest))
            {
              MERROR_VER("Failed to check ringct signatures: mismatched pubkey at vin " << n << ", index " << m);
              return false;
            }
            if (pubkeys[n][m].mask != rct::rct2pk(rv.mixRing[m][n].mask))
            {
              MERROR_VER("Failed to check ringct signatures: mismatched commitment at vin " << n << ", index " << m);
              return false;
            }
          }
        }
      }

      if (rv.p.MGs.size() != 1)
      {
        MERROR_VER("Failed to check ringct signatures: Bad MGs size");
        return false;
      }
      if (rv.p.MGs.empty() || rv.p.MGs[0].II.size() != tx.vin.size())
      {
        MERROR_VER("Failed to check ringct signatures: mismatched II/vin sizes");
        return false;
      }
      for (size_t n = 0; n < tx.vin.size(); ++n)
      {
        if (memcmp(&boost::get<txin_to_key>(tx.vin[n]).k_image, &rv.p.MGs[0].II[n], 32))
        {
          MERROR_VER("Failed to check ringct signatures: mismatched II/vin sizes");
          return false;
        }
      }

      if (!rct::verRct(rv, false))
      {
        MERROR_VER("Failed to check ringct signatures!");
        return false;
      }
      break;
    }
    default:
      MERROR_VER("Unsupported rct type: " << rv.type);
      return false;
    }

    // for bulletproofs, check they're only multi-output after v8
    if (rct::is_rct_bulletproof(rv.type))
    {
      if (hf_version < 8)
      {
        for (const rct::Bulletproof &proof: rv.p.bulletproofs)
        {
          if (proof.V.size() > 1)
          {
            MERROR_VER("Multi output bulletproofs are invalid before v8");
            return false;
          }
        }
      }
    }
  }
  return true;
}

//------------------------------------------------------------------
void Blockchain::check_ring_signature(const crypto::hash &tx_prefix_hash, const crypto::key_image &key_image, const std::vector<rct::ctkey> &pubkeys, const std::vector<crypto::signature>& sig, uint64_t &result) const
{
  std::vector<const crypto::public_key *> p_output_keys;
  p_output_keys.reserve(pubkeys.size());
  for (auto &key : pubkeys)
  {
    // rct::key and crypto::public_key have the same structure, avoid object ctor/memcpy
    p_output_keys.push_back(&(const crypto::public_key&)key.dest);
  }

  result = crypto::check_ring_signature(tx_prefix_hash, key_image, p_output_keys, sig.data()) ? 1 : 0;
}

//------------------------------------------------------------------
uint64_t Blockchain::get_dynamic_base_fee(uint64_t block_reward, size_t median_block_weight, uint8_t version)
{
  const uint64_t min_block_weight = get_min_block_weight(version);
  if (median_block_weight < min_block_weight)
    median_block_weight = min_block_weight;
  uint64_t hi, lo;

  if (version >= HF_VERSION_PER_BYTE_FEE)
  {
    lo = mul128(block_reward, DYNAMIC_FEE_REFERENCE_TRANSACTION_WEIGHT, &hi);
    div128_64(hi, lo, median_block_weight, &hi, &lo, NULL, NULL);
    if (version >= HF_VERSION_2021_SCALING)
    {
      // min_fee_per_byte = round_up( 0.95 * block_reward * ref_weight / (fee_median^2) )
      // note: since hardfork HF_VERSION_2021_SCALING, fee_median (a.k.a. median_block_weight) equals effective long term median
      div128_64(hi, lo, median_block_weight, &hi, &lo, NULL, NULL);
      assert(hi == 0);
      lo -= lo / 20;
      return lo == 0 ? 1 : lo;
    }
    else
    {
      // min_fee_per_byte = 0.2 * block_reward * ref_weight / (min_penalty_free_zone * fee_median)
      div128_64(hi, lo, min_block_weight, &hi, &lo, NULL, NULL);
      assert(hi == 0);
      lo /= 5;
      return lo;
    }
  }

  const uint64_t fee_base = version >= 5 ? DYNAMIC_FEE_PER_KB_BASE_FEE_V5 : DYNAMIC_FEE_PER_KB_BASE_FEE;

  uint64_t unscaled_fee_base = (fee_base * min_block_weight / median_block_weight);
  lo = mul128(unscaled_fee_base, block_reward, &hi);
  div128_64(hi, lo, DYNAMIC_FEE_PER_KB_BASE_BLOCK_REWARD, &hi, &lo, NULL, NULL);
  assert(hi == 0);

  // quantize fee up to 8 decimals
  uint64_t mask = get_fee_quantization_mask();
  uint64_t qlo = (lo + mask - 1) / mask * mask;
  MDEBUG("lo " << print_money(lo) << ", qlo " << print_money(qlo) << ", mask " << mask);

  return qlo;
}

//------------------------------------------------------------------
bool Blockchain::check_fee(size_t tx_weight, uint64_t fee) const
{
  const uint8_t version = get_current_hard_fork_version();

  uint64_t median = 0;
  uint64_t already_generated_coins = 0;
  uint64_t base_reward = 0;
  if (version >= HF_VERSION_DYNAMIC_FEE)
  {
    median = m_current_block_cumul_weight_limit / 2;
    const uint64_t blockchain_height = m_db->height();
    already_generated_coins = blockchain_height ? m_db->get_block_already_generated_coins(blockchain_height - 1) : 0;
    if (!get_block_reward(median, 1, already_generated_coins, base_reward, version))
      return false;
  }

  uint64_t needed_fee;
  if (version >= HF_VERSION_PER_BYTE_FEE)
  {
    const bool use_long_term_median_in_fee = version >= HF_VERSION_LONG_TERM_BLOCK_WEIGHT;
    uint64_t fee_per_byte = get_dynamic_base_fee(base_reward, use_long_term_median_in_fee ? std::min<uint64_t>(median, m_long_term_effective_median_block_weight) : median, version);
    MDEBUG("Using " << print_money(fee_per_byte) << "/byte fee");
    needed_fee = tx_weight * fee_per_byte;
    // quantize fee up to 8 decimals
    const uint64_t mask = get_fee_quantization_mask();
    needed_fee = (needed_fee + mask - 1) / mask * mask;
  }
  else
  {
    uint64_t fee_per_kb;
    if (version < HF_VERSION_DYNAMIC_FEE)
    {
      fee_per_kb = FEE_PER_KB;
    }
    else
    {
      fee_per_kb = get_dynamic_base_fee(base_reward, median, version);
    }
    MDEBUG("Using " << print_money(fee_per_kb) << "/kB fee");

    needed_fee = tx_weight / 1024;
    needed_fee += (tx_weight % 1024) ? 1 : 0;
    needed_fee *= fee_per_kb;
  }

  if (fee < needed_fee - needed_fee / 50) // keep a little 2% buffer on acceptance - no integer overflow
  {
    MERROR_VER("transaction fee is not enough: " << print_money(fee) << ", minimum fee: " << print_money(needed_fee));
    return false;
  }
  return true;
}

//------------------------------------------------------------------
void Blockchain::get_dynamic_base_fee_estimate_2021_scaling(uint64_t grace_blocks, uint64_t base_reward, uint64_t Mnw, uint64_t Mlw, std::vector<uint64_t> &fees) const
{
  // variable names and calculations as per https://github.com/ArticMine/Monero-Documents/blob/master/MoneroScaling2021-02.pdf
  // from (earlier than) this fork, the base fee is per byte
  const uint64_t Mfw = std::min(Mnw, Mlw);

  // 3 kB divided by something ? It's going to be either 0 or *very* quantized, so fold it into integer steps below
  //const uint64_t Brlw = DYNAMIC_FEE_REFERENCE_TRANSACTION_WEIGHT / Mfw;

  // constant.... equal to 0, unless floating point, so fold it into integer steps below
  //const uint64_t Br = DYNAMIC_FEE_REFERENCE_TRANSACTION_WEIGHT / CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5

  //const uint64_t Fl = base_reward * Brlw / Mfw; fold Brlw from above
  const uint64_t Fl = base_reward * /*Brlw*/ DYNAMIC_FEE_REFERENCE_TRANSACTION_WEIGHT / (Mfw * Mfw);

  // fold Fl into this for better precision (and to match the test cases in the PDF)
  // const uint64_t Fn = 4 * Fl;
  const uint64_t Fn = 4 * base_reward * /*Brlw*/ DYNAMIC_FEE_REFERENCE_TRANSACTION_WEIGHT / (Mfw * Mfw);

  // const uint64_t Fm = 16 * base_reward * Br / Mfw; fold Br from above
  const uint64_t Fm = 16 * base_reward * DYNAMIC_FEE_REFERENCE_TRANSACTION_WEIGHT / (CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5 * Mfw);

  // const uint64_t Fp = 2 * base_reward / Mnw;

  // fold Br from above, move 4Fm in the max to decrease quantization effect
  //const uint64_t Fh = 4 * Fm * std::max<uint64_t>(1, Mfw / (32 * DYNAMIC_FEE_REFERENCE_TRANSACTION_WEIGHT * Mnw / CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5));
  const uint64_t Fh = std::max<uint64_t>(4 * Fm, 4 * Fm * Mfw / (32 * DYNAMIC_FEE_REFERENCE_TRANSACTION_WEIGHT * Mnw / CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5));

  fees.resize(4);
  fees[0] = cryptonote::round_money_up(Fl, CRYPTONOTE_SCALING_2021_FEE_ROUNDING_PLACES);
  fees[1] = cryptonote::round_money_up(Fn, CRYPTONOTE_SCALING_2021_FEE_ROUNDING_PLACES);
  fees[2] = cryptonote::round_money_up(Fm, CRYPTONOTE_SCALING_2021_FEE_ROUNDING_PLACES);
  fees[3] = cryptonote::round_money_up(Fh, CRYPTONOTE_SCALING_2021_FEE_ROUNDING_PLACES);
}

void Blockchain::get_dynamic_base_fee_estimate_2021_scaling(uint64_t grace_blocks, std::vector<uint64_t> &fees) const
{
  const uint8_t version = get_current_hard_fork_version();
  const uint64_t db_height = m_db->height();

  CHECK_AND_ASSERT_THROW_MES(grace_blocks <= CRYPTONOTE_REWARD_BLOCKS_WINDOW, "Grace blocks invalid In 2021 fee scaling estimate.");

  // we want Mlw = median of max((min(Mbw, 1.7 * Ml), Zm), Ml / 1.7)
  // Mbw: block weight for the last 99990 blocks, 0 for the next 10
  // Ml: penalty free zone (dynamic), aka long_term_median, aka median of max((min(Mb, 1.7 * Ml), Zm), Ml / 1.7)
  // Zm: 300000 (minimum penalty free zone)
  //
  // So we copy the current rolling median state, add 10 (grace_blocks) zeroes to it, and get back Mlw

  epee::misc_utils::rolling_median_t<uint64_t> rm = m_long_term_block_weights_cache_rolling_median;
  for (size_t i = 0; i < grace_blocks; ++i)
    rm.insert(0);
  const uint64_t Mlw_penalty_free_zone_for_wallet = std::max<uint64_t>(rm.median(), CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5);

  // Msw: median over [100 - grace blocks] past + [grace blocks] future blocks
  std::vector<uint64_t> weights;
  get_last_n_blocks_weights(weights, 100 - grace_blocks);
  weights.reserve(100);
  for (size_t i = 0; i < grace_blocks; ++i)
    weights.push_back(0);
  const uint64_t Msw_effective_short_term_median = std::max(epee::misc_utils::median(weights), Mlw_penalty_free_zone_for_wallet);

  const uint64_t Mnw = std::min(Msw_effective_short_term_median, 50 * Mlw_penalty_free_zone_for_wallet);

  uint64_t already_generated_coins = db_height ? m_db->get_block_already_generated_coins(db_height - 1) : 0;
  uint64_t base_reward;
  if (!get_block_reward(m_current_block_cumul_weight_limit / 2, 1, already_generated_coins, base_reward, version))
  {
    MERROR("Failed to determine block reward, using placeholder " << print_money(BLOCK_REWARD_OVERESTIMATE) << " as a high bound");
    base_reward = BLOCK_REWARD_OVERESTIMATE;
  }

  get_dynamic_base_fee_estimate_2021_scaling(grace_blocks, base_reward, Mnw, Mlw_penalty_free_zone_for_wallet, fees);
}

//------------------------------------------------------------------
uint64_t Blockchain::get_dynamic_base_fee_estimate(uint64_t grace_blocks) const
{
  const uint8_t version = get_current_hard_fork_version();
  const uint64_t db_height = m_db->height();

  if (version < HF_VERSION_DYNAMIC_FEE)
    return FEE_PER_KB;

  if (grace_blocks >= CRYPTONOTE_REWARD_BLOCKS_WINDOW)
    grace_blocks = CRYPTONOTE_REWARD_BLOCKS_WINDOW - 1;

  if (version >= HF_VERSION_2021_SCALING)
  {
    std::vector<uint64_t> fees;
    get_dynamic_base_fee_estimate_2021_scaling(grace_blocks, fees);
    return fees[0];
  }

  const uint64_t min_block_weight = get_min_block_weight(version);
  std::vector<uint64_t> weights;
  get_last_n_blocks_weights(weights, CRYPTONOTE_REWARD_BLOCKS_WINDOW - grace_blocks);
  weights.reserve(grace_blocks);
  for (size_t i = 0; i < grace_blocks; ++i)
    weights.push_back(min_block_weight);

  uint64_t median = epee::misc_utils::median(weights);
  if(median <= min_block_weight)
    median = min_block_weight;

  uint64_t already_generated_coins = db_height ? m_db->get_block_already_generated_coins(db_height - 1) : 0;
  uint64_t base_reward;
  if (!get_block_reward(m_current_block_cumul_weight_limit / 2, 1, already_generated_coins, base_reward, version))
  {
    MERROR("Failed to determine block reward, using placeholder " << print_money(BLOCK_REWARD_OVERESTIMATE) << " as a high bound");
    base_reward = BLOCK_REWARD_OVERESTIMATE;
  }

  const bool use_long_term_median_in_fee = version >= HF_VERSION_LONG_TERM_BLOCK_WEIGHT;
  const uint64_t use_median_value = use_long_term_median_in_fee ? std::min<uint64_t>(median, m_long_term_effective_median_block_weight) : median;
  const uint64_t fee = get_dynamic_base_fee(base_reward, use_median_value, version);
  const bool per_byte = version < HF_VERSION_PER_BYTE_FEE;
  MDEBUG("Estimating " << grace_blocks << "-block fee at " << print_money(fee) << "/" << (per_byte ? "byte" : "kB"));
  return fee;
}

//------------------------------------------------------------------
// This function checks to see if a tx is unlocked.  unlock_time is either
// a block index or a unix time.
bool Blockchain::is_tx_spendtime_unlocked(uint64_t unlock_time, uint8_t hf_version) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  if(unlock_time < CRYPTONOTE_MAX_BLOCK_NUMBER)
  {
    // ND: Instead of calling get_current_blockchain_height(), call m_db->height()
    //    directly as get_current_blockchain_height() locks the recursive mutex.
    if(m_db->height()-1 + CRYPTONOTE_LOCKED_TX_ALLOWED_DELTA_BLOCKS >= unlock_time)
      return true;
    else
      return false;
  }
  else
  {
    //interpret as time
    const uint64_t current_time = hf_version >= HF_VERSION_DETERMINISTIC_UNLOCK_TIME ? get_adjusted_time(m_db->height()) : static_cast<uint64_t>(time(NULL));
    if(current_time + (locked_tx_allowed_delta_seconds(m_nettype, get_current_hard_fork_version())) >= unlock_time)
      return true;
    else
      return false;
  }
  return false;
}
//------------------------------------------------------------------
// This function locates all outputs associated with a given input (mixins)
// and validates that they exist and are usable.  It also checks the ring
// signature for each input.
bool Blockchain::check_tx_input(size_t tx_version, const txin_to_key& txin, const crypto::hash& tx_prefix_hash, const std::vector<crypto::signature>& sig, const rct::rctSig &rct_signatures, std::vector<rct::ctkey> &output_keys, uint64_t* pmax_related_block_height, uint8_t hf_version) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);

  // ND:
  // 1. Disable locking and make method private.
  //CRITICAL_REGION_LOCAL(m_blockchain_lock);

  struct outputs_visitor
  {
    std::vector<rct::ctkey >& m_output_keys;
    const Blockchain& m_bch;
    const uint8_t hf_version;
    outputs_visitor(std::vector<rct::ctkey>& output_keys, const Blockchain& bch, uint8_t hf_version) :
      m_output_keys(output_keys), m_bch(bch), hf_version(hf_version)
    {
    }
    bool handle_output(uint64_t unlock_time, const crypto::public_key &pubkey, const rct::key &commitment)
    {
      //check tx unlock time
      if (!m_bch.is_tx_spendtime_unlocked(unlock_time, hf_version))
      {
        MERROR_VER("One of outputs for one of inputs has wrong tx.unlock_time = " << unlock_time);
        return false;
      }

      // The original code includes a check for the output corresponding to this input
      // to be a txout_to_key. This is removed, as the database does not store this info.
      // Only txout_to_key (and since HF_VERSION_VIEW_TAGS, txout_to_tagged_key)
      // outputs are stored in the DB in the first place, done in Blockchain*::add_output.
      // Additional type checks on outputs were also added via cryptonote::check_output_types
      // and cryptonote::get_output_public_key (see Blockchain::check_tx_outputs).

      m_output_keys.push_back(rct::ctkey({rct::pk2rct(pubkey), commitment}));
      return true;
    }
  };

  output_keys.clear();

  // collect output keys
  outputs_visitor vi(output_keys, *this, hf_version);
  if (!scan_outputkeys_for_indexes(tx_version, txin, vi, tx_prefix_hash, pmax_related_block_height))
  {
    MERROR_VER("Failed to get output keys for tx with amount = " << print_money(txin.amount) << " and count indexes " << txin.key_offsets.size());
    return false;
  }

  if(txin.key_offsets.size() != output_keys.size())
  {
    MERROR_VER("Output keys for tx with amount = " << txin.amount << " and count indexes " << txin.key_offsets.size() << " returned wrong keys count " << output_keys.size());
    return false;
  }
  if (tx_version == 1) {
    CHECK_AND_ASSERT_MES(sig.size() == output_keys.size(), false, "internal error: tx signatures count=" << sig.size() << " mismatch with outputs keys count for inputs=" << output_keys.size());
  }
  // rct_signatures will be expanded after this
  return true;
}
//------------------------------------------------------------------
// only works on the main chain
uint64_t Blockchain::get_adjusted_time(uint64_t height) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);

  // if not enough blocks, no proper median yet, return current time
  if(height < BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW)
  {
      return static_cast<uint64_t>(time(NULL));
  }
  std::vector<uint64_t> timestamps;

  // need most recent 60 blocks, get index of first of those
  size_t offset = height - BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW;
  timestamps.reserve(height - offset);
  for(;offset < height; ++offset)
  {
    timestamps.push_back(m_db->get_block_timestamp(offset));
  }
  uint64_t median_ts = epee::misc_utils::median(timestamps);

  // project the median to match approximately when the block being validated will appear
  // the median is calculated from a chunk of past blocks, so we use +1 to offset onto the current block
  median_ts += (BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW + 1) * block_target(m_nettype) / 2;

  // project the current block's time based on the previous block's time
  // we don't use the current block's time directly to mitigate timestamp manipulation
  uint64_t adjusted_current_block_ts = timestamps.back() + block_target(m_nettype);

  // return minimum of ~current block time and adjusted median time
  // we do this since it's better to report a time in the past than a time in the future
  return (adjusted_current_block_ts < median_ts ? adjusted_current_block_ts : median_ts);
}
//------------------------------------------------------------------
//TODO: revisit, has changed a bit on upstream
bool Blockchain::check_block_timestamp(std::vector<uint64_t>& timestamps, const block& b, uint64_t& median_ts) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  median_ts = epee::misc_utils::median(timestamps);

  if(b.timestamp < median_ts)
  {
    MERROR_VER("Timestamp of block with id: " << get_block_hash(b) << ", " << b.timestamp << ", less than median of last " << BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW << " blocks, " << median_ts);
    return false;
  }

  return true;
}
//------------------------------------------------------------------
// This function grabs the timestamps from the most recent <n> blocks,
// where n = BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW.  If there are not those many
// blocks in the blockchain, the timestap is assumed to be valid.  If there
// are, this function returns:
//   true if the block's timestamp is not less than the timestamp of the
//       median of the selected blocks
//   false otherwise
bool Blockchain::check_block_timestamp(const block& b, uint64_t& median_ts) const
{
  LOG_PRINT_L3("Blockchain::" << __func__);
  if(b.timestamp > (uint64_t)time(NULL) + CRYPTONOTE_BLOCK_FUTURE_TIME_LIMIT)
  {
    MERROR_VER("Timestamp of block with id: " << get_block_hash(b) << ", " << b.timestamp << ", bigger than local time + 2 hours");
    return false;
  }

  const auto h = m_db->height();

  // if not enough blocks, no proper median yet, return true
  if(h < BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW)
  {
    return true;
  }

  std::vector<uint64_t> timestamps;

  // need most recent 60 blocks, get index of first of those
  size_t offset = h - BLOCKCHAIN_TIMESTAMP_CHECK_WINDOW;
  timestamps.reserve(h - offset);
  for(;offset < h; ++offset)
  {
    timestamps.push_back(m_db->get_block_timestamp(offset));
  }

  return check_block_timestamp(timestamps, b, median_ts);
}
//------------------------------------------------------------------
bool Blockchain::flush_txes_from_pool(const std::vector<crypto::hash> &txids)
{
  CRITICAL_REGION_LOCAL(m_tx_pool);

  bool res = true;
  for (const auto &txid: txids)
  {
    cryptonote::transaction tx;
    cryptonote::blobdata txblob;
    size_t tx_weight;
    uint64_t fee;
    bool relayed, do_not_relay, double_spend_seen, pruned;
    MINFO("Removing txid " << txid << " from the pool");
    if(m_tx_pool.have_tx(txid, relay_category::all) && !m_tx_pool.take_tx(txid, tx, txblob, tx_weight, fee, relayed, do_not_relay, double_spend_seen, pruned))
    {
      MERROR("Failed to remove txid " << txid << " from the pool");
      res = false;
    }
  }
  return res;
}
//------------------------------------------------------------------
//      Needs to validate the block and acquire each transaction from the
//      transaction mem_pool, then pass the block and transactions to
//      m_db->add_block()
bool Blockchain::handle_block_to_main_chain(const block& bl, const crypto::hash& id,
  block_verification_context& bvc, pool_supplement& extra_block_txs)
{
  LOG_PRINT_L3("Blockchain::" << __func__);

  TIME_MEASURE_START(block_processing_time);
  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  TIME_MEASURE_START(t1);

  static bool seen_future_version = false;

  if (m_vm_state_stale && !rebuild_vm_state())
  {
    MERROR_VER("Contract state could not be replayed, refusing block " << id);
    bvc.m_verifivation_failed = true;
    return false;
  }

  db_rtxn_guard rtxn_guard(m_db);
  uint64_t blockchain_height;
  const crypto::hash top_hash = get_tail_id(blockchain_height);
  ++blockchain_height; // block height to chain height
  if(bl.prev_id != top_hash)
  {
    MERROR_VER("Block with id: " << id << std::endl << "has wrong prev_id: " << bl.prev_id << std::endl << "expected: " << top_hash);
    bvc.m_verifivation_failed = true;
leave:
    return false;
  }

  // warn users if they're running an old version
  if (!seen_future_version && bl.major_version > m_hardfork->get_ideal_version())
  {
    seen_future_version = true;
    const el::Level level = el::Level::Warning;
    MCLOG_RED(level, "global", "**********************************************************************");
    MCLOG_RED(level, "global", "A block was seen on the network with a version higher than the last");
    MCLOG_RED(level, "global", "known one. This may be an old version of the daemon, and a software");
    MCLOG_RED(level, "global", "update may be required to sync further. Try running: update check");
    MCLOG_RED(level, "global", "**********************************************************************");
  }

  // this is a cheap test
  const uint8_t hf_version = get_current_hard_fork_version();
  if (!m_hardfork->check(bl))
  {
    MERROR_VER("Block with id: " << id << std::endl << "has old version: " << (unsigned)bl.major_version << std::endl << "current: " << (unsigned)hf_version);
    bvc.m_verifivation_failed = true;
    goto leave;
  }

  TIME_MEASURE_FINISH(t1);
  TIME_MEASURE_START(t2);

  // make sure block timestamp is not less than the median timestamp
  // of a set number of the most recent blocks.
  if(!check_block_timestamp(bl))
  {
    MERROR_VER("Block with id: " << id << std::endl << "has invalid timestamp: " << bl.timestamp);
    bvc.m_verifivation_failed = true;
    goto leave;
  }

  TIME_MEASURE_FINISH(t2);
  //check proof of work
  TIME_MEASURE_START(target_calculating_time);

  // get the target difficulty for the block.
  // the calculation can overflow, among other failure cases,
  // so we need to check the return type.
  // FIXME: get_difficulty_for_next_block can also assert, look into
  // changing this to throwing exceptions instead so we can clean up.
  difficulty_type current_diffic = get_difficulty_for_next_block();
  CHECK_AND_ASSERT_MES(current_diffic, false, "!!!!!!!!! difficulty overhead !!!!!!!!!");

  TIME_MEASURE_FINISH(target_calculating_time);

  TIME_MEASURE_START(longhash_calculating_time);

  crypto::hash proof_of_work;
  memset(proof_of_work.data, 0xff, sizeof(proof_of_work.data));

  // Formerly the code below contained an if loop with the following condition
  // !m_checkpoints.is_in_checkpoint_zone(get_current_blockchain_height())
  // however, this caused the daemon to not bother checking PoW for blocks
  // before checkpoints, which is very dangerous behaviour. We moved the PoW
  // validation out of the next chunk of code to make sure that we correctly
  // check PoW now.
  // FIXME: height parameter is not used...should it be used or should it not
  // be a parameter?
  // validate proof_of_work versus difficulty target
  bool precomputed = false;
  bool fast_check = false;
#if defined(PER_BLOCK_CHECKPOINT)
  if (blockchain_height < m_blocks_hash_check.size())
  {
    const auto &expected_hash = m_blocks_hash_check[blockchain_height].first;
    if (expected_hash != crypto::null_hash)
    {
      if (memcmp(&id, &expected_hash, sizeof(hash)) != 0)
      {
        MERROR_VER("Block with id is INVALID: " << id << ", expected " << expected_hash);
        bvc.m_verifivation_failed = true;
        goto leave;
      }
      fast_check = true;
    }
    else
    {
      MCINFO("verify", "No pre-validated hash at height " << blockchain_height << ", verifying fully");
    }
  }
#endif
  if (!fast_check)
  {
    auto it = m_blocks_longhash_table.find(id);
    if (it != m_blocks_longhash_table.end())
    {
      precomputed = true;
      proof_of_work = it->second;
    }
    else
      proof_of_work = get_block_longhash(this, bl, blockchain_height, 0);

    // validate proof_of_work versus difficulty target
    if(!check_hash(proof_of_work, current_diffic))
    {
      MERROR_VER("Block with id: " << id << std::endl << "does not have enough proof of work: " << proof_of_work << " at height " << blockchain_height << ", unexpected difficulty: " << current_diffic);
      bvc.m_verifivation_failed = true;
      bvc.m_bad_pow = true;
      goto leave;
    }
  }

  // If we're at a checkpoint, ensure that our hardcoded checkpoint hash
  // is correct.
  if(m_checkpoints.is_in_checkpoint_zone(blockchain_height))
  {
    if(!m_checkpoints.check_block(blockchain_height, id))
    {
      LOG_ERROR("CHECKPOINT VALIDATION FAILED");
      bvc.m_verifivation_failed = true;
      goto leave;
    }
  }

  TIME_MEASURE_FINISH(longhash_calculating_time);
  if (precomputed)
    longhash_calculating_time += m_fake_pow_calc_time;

  TIME_MEASURE_START(t3);

  // sanity check basic miner tx properties;
  if(!prevalidate_miner_transaction(bl, blockchain_height, bl.major_version))
  {
    MERROR_VER("Block with id: " << id << " failed to pass prevalidation");
    bvc.m_verifivation_failed = true;
    goto leave;
  }

  // verify all non-input consensus rules for txs inside the pool supplement (if not inside checkpoint zone)
#if defined(PER_BLOCK_CHECKPOINT)
  if (!fast_check)
#endif
  {
    tx_verification_context tvc{};
    // If fail non-input consensus rule checking...
    if (!ver_non_input_consensus(extra_block_txs, tvc, bl.major_version))
    {
      MERROR_VER("Pool supplement provided for block with id: " << id << " failed to pass validation");
      bvc.m_verifivation_failed = true;
      goto leave;
    }
  }

  size_t coinbase_weight = get_transaction_weight(bl.miner_tx);
  size_t cumulative_block_weight = coinbase_weight;

  std::vector<std::pair<transaction, blobdata>> txs;
  //                          txid     weight mempool?
  std::vector<std::tuple<crypto::hash, size_t, bool>> txs_meta;

  // This will be the data sent to the ZMQ pool listeners for txs which skipped the mempool
  std::vector<txpool_event> txpool_events;

  const uint8_t pool_rules_version = get_ideal_hard_fork_version(blockchain_height);
  auto return_txs_to_pool = [this, &txs, &txs_meta, &rtxn_guard, pool_rules_version]()
  {
    rtxn_guard.stop();
    if (txs_meta.size() != txs.size())
    {
      MERROR("BUG: txs_meta and txs not matching size!!!");
      return;
    }

    for (size_t i = 0; i < txs.size(); ++i)
    {
      const bool found_in_pool = std::get<2>(txs_meta[i]);
      if (!found_in_pool)
        continue;

      m_returned_pool_txs.push_back({std::move(txs[i].first), std::move(txs[i].second),
          std::get<0>(txs_meta[i]), std::get<1>(txs_meta[i]), pool_rules_version});
    }
  };
  const auto reject_block_as_invalid = [this, &bl, &id, &bvc, &return_txs_to_pool]()
  {
    add_block_as_invalid(bl, id);
    bvc.m_verifivation_failed = true;
    return_txs_to_pool();
    return false;
  };

  key_images_container keys;

  uint64_t fee_summary = 0;
  uint64_t t_checktx = 0;
  uint64_t t_exists = 0;
  uint64_t t_pool = 0;
  uint64_t t_dblspnd = 0;
  uint64_t n_pruned = 0;

  const bool contracts_active = is_qwc_contracts_hardfork(bl.major_version);
  contract_block_context contract_context{};
  if (contracts_active && !get_contract_block_context(blockchain_height, contract_context))
  {
    MERROR_VER("Block with id: " << id << " has no parent for contract execution");
    bvc.m_verifivation_failed = true;
    goto leave;
  }
  contract_block_executor contract_executor(m_vm_state, contract_context);

  TIME_MEASURE_FINISH(t3);

// XXX old code adds miner tx here

  // Iterate over the block's transaction hashes, grabbing each
  // from the tx_pool (or from extra_block_txs) and validating them.  Each is then added
  // to txs.  Keys spent in each are added to <keys> by the double spend check.
  txs.reserve(bl.tx_hashes.size());
  txs_meta.reserve(bl.tx_hashes.size());
  txpool_events.reserve(bl.tx_hashes.size());
  for (const crypto::hash& tx_id : bl.tx_hashes)
  {
    TIME_MEASURE_START(aa);

// XXX old code does not check whether tx exists
    if (m_db->tx_exists(tx_id))
    {
      MERROR("Block with id: " << id << " attempting to add transaction already in blockchain with id: " << tx_id);
      bvc.m_verifivation_failed = true;
      return_txs_to_pool();
      return false;
    }

    TIME_MEASURE_FINISH(aa);
    t_exists += aa;
    TIME_MEASURE_START(bb);

    // get transaction with hash <tx_id> from m_tx_pool or extra_block_txs
    // tx info we want:
    //   * tx as `cryptonote::transaction`
    //   * blob
    //   * weight
    //   * fee
    //   * is pruned?
    txs.emplace_back();
    transaction &tx = txs.back().first;
    blobdata &txblob = txs.back().second;
    size_t tx_weight{};
    uint64_t fee{};
    bool pruned{};

    /* 
     * Try pulling transaction data from the mempool proper first. If that fails, then try pulling
     * from the block supplement. We add txs pulled from the block to the txpool events for future
     * notifications, since if the tx skipped the mempool, then listeners have not yet received a
     * notification for this tx.
     */
    bool _unused1, _unused2, _unused3;
    const bool found_tx_in_pool{
        m_tx_pool.take_tx(tx_id, tx, txblob, tx_weight, fee,
          _unused1, _unused2, _unused3, pruned, /*suppress_missing_msgs=*/true)
      };
    bool find_tx_failure{!found_tx_in_pool};
    bool return_to_pool_on_failure{false};
    const auto measure_loose_tx = [&]()
    {
      tx_weight = tx.pruned ? get_pruned_transaction_weight(tx) : get_transaction_weight(tx, txblob.size());
      fee = get_tx_fee(tx);
      pruned = tx.pruned;
    };
    if (!found_tx_in_pool) // if not in mempool:
    {
      const auto extra_txs_it{extra_block_txs.txs_by_txid.find(tx_id)};
      if (extra_txs_it != extra_block_txs.txs_by_txid.end()) // if in block supplement:
      {
        tx = std::move(extra_txs_it->second.first);
        txblob = std::move(extra_txs_it->second.second);
        measure_loose_tx();
        extra_block_txs.txs_by_txid.erase(extra_txs_it);
        txpool_events.emplace_back(txpool_event{tx, tx_id, txblob.size(), tx_weight, true});
        find_tx_failure = false;
      }
      else if (m_tx_pool.take_expired_window_tx(tx_id, tx, txblob)) // QWC-3
      {
        measure_loose_tx();
        return_to_pool_on_failure = true;
        find_tx_failure = false;
      }
      else
      {
        const auto returned = std::find_if(m_returned_pool_txs.begin(), m_returned_pool_txs.end(),
            [&tx_id](const returned_pool_tx &entry) { return entry.id == tx_id; });
        if (returned != m_returned_pool_txs.end())
        {
          tx = std::move(returned->tx);
          txblob = std::move(returned->blob);
          tx_weight = returned->weight;
          fee = get_tx_fee(tx);
          pruned = tx.pruned;
          m_returned_pool_txs.erase(returned);
          return_to_pool_on_failure = true;
          find_tx_failure = false;
        }
      }
    }

    // @TODO: We should move this section (checking if the daemon has all txs from the block) to
    // right after the PoW check. Since it's now expected the node will sometimes not have all txs
    // in its pool at this point nor the txs included as fluffy txs (and will need to re-request
    // missing fluffy txs), then the node will sometimes waste cycles doing verification for some
    // txs twice.
    if (find_tx_failure) // did not find txid in mempool or provided extra block txs
    {
      const bool fully_supplemented_block = extra_block_txs.txs_by_txid.size() >= bl.tx_hashes.size();
      if (fully_supplemented_block)
        MERROR_VER("Block with id: " << id  << " has at least one unknown transaction with id: " << tx_id);
      else
        LOG_PRINT_L2("Block with id: " << id  << " has at least one unknown transaction with id: " << tx_id);
      txs.pop_back(); // We push to the back preemptively. On fail, we need txs & txs_meta to match size
      bvc.m_verifivation_failed = true;
      bvc.m_missing_txs = true;
      return_txs_to_pool();
      return false;
    }
    if (pruned)
      ++n_pruned;

    TIME_MEASURE_FINISH(bb);
    t_pool += bb;
    // add the transaction to the temp list of transactions, so we can either
    // store the list of transactions all at once or return the ones we've
    // taken from the tx_pool back to it if the block fails verification.
    txs_meta.emplace_back(tx_id, tx_weight, found_tx_in_pool || return_to_pool_on_failure);

    tx_extra_vm_call call{};
    if (contract_call_before_activation(get_tx_vm_call(tx.extra, call), bl.major_version)
        || !check_tx_contract_extra(tx, bl.major_version, contract_extra_scope::transaction))
    {
      MERROR_VER("Block with id: " << id << " has a transaction (id: " << tx_id << ") with contract fields invalid for its version");
      return reject_block_as_invalid();
    }

    tx_extra_height_window window{};
    const height_window_verdict window_verdict = check_block_tx_height_window(tx, bl.major_version, blockchain_height, window);
    if (window_verdict != height_window_verdict::absent && window_verdict != height_window_verdict::inside)
    {
      MERROR_VER("Block with id: " << id << " has a transaction (id: " << tx_id << ") outside its height window at height " << blockchain_height);
      return reject_block_as_invalid();
    }

    TIME_MEASURE_START(dd);

    // FIXME: the storage should not be responsible for validation.
    //        If it does any, it is merely a sanity check.
    //        Validation is the purview of the Blockchain class
    //        - TW
    //
    // ND: this is not needed, db->add_block() checks for duplicate k_images and fails accordingly.
    // if (!check_for_double_spend(tx, keys))
    // {
    //     LOG_PRINT_L0("Double spend detected in transaction (id: " << tx_id);
    //     bvc.m_verifivation_failed = true;
    //     break;
    // }

    TIME_MEASURE_FINISH(dd);
    t_dblspnd += dd;
    TIME_MEASURE_START(cc);

#if defined(PER_BLOCK_CHECKPOINT)
    if (!fast_check)
#endif
    {
      // validate that transaction inputs and the keys spending them are correct.
      tx_verification_context tvc;
      if(!check_tx_inputs(tx, tvc, nullptr, bl.major_version))
      {
        MERROR_VER("Block with id: " << id  << " has at least one transaction (id: " << tx_id << ") with wrong inputs.");

        //TODO: why is this done?  make sure that keeping invalid blocks makes sense.
        add_block_as_invalid(bl, id);
        MERROR_VER("Block with id " << id << " added as invalid because of wrong inputs in transactions");
        bvc.m_verifivation_failed = true;
        return_txs_to_pool();
        return false;
      }
    }

    TIME_MEASURE_FINISH(cc);
    t_checktx += cc;

    if (contracts_active)
    {
      contract_call_verdict verdict;
      try
      {
        verdict = contract_executor.execute(tx, fee);
      }
      catch (...)
      {
        MERROR_VER("Block with id: " << id << " could not execute contract call (tx id: " << tx_id << ")");
        bvc.m_verifivation_failed = true;
        return_txs_to_pool();
        return false;
      }
      if (!contract_call_accepted(verdict))
      {
        MERROR_VER("Block with id: " << id << " has a contract call (tx id: " << tx_id << ") that is " << contract_call_verdict_name(verdict));
        return reject_block_as_invalid();
      }
    }

    fee_summary += fee;
    cumulative_block_weight += tx_weight;
  }

  uint64_t contract_minted = 0;
  if (contracts_active)
  {
    fee_summary -= contract_executor.effects().deposited; // H-29
    contract_minted = contract_executor.effects().withdrawn;
    if (!check_contract_mints_in_miner_tx(bl.miner_tx, contract_executor.effects().mints))
    {
      MERROR_VER("Block with id: " << id << " does not pay its contract mints in the coinbase");
      bvc.m_verifivation_failed = true;
      return_txs_to_pool();
      return false;
    }
  }

  // if we were syncing pruned blocks
  if (n_pruned > 0)
  {
    if (blockchain_height >= m_blocks_hash_check.size() || m_blocks_hash_check[blockchain_height].second == 0)
    {
      MERROR("Block at " << blockchain_height << " is pruned, but we do not have a weight for it");
      goto leave;
    }
    cumulative_block_weight = m_blocks_hash_check[blockchain_height].second;
  }

  vm_block_update vm_update{};
  vm::write_set vm_undo;
  bool vm_mirror_applied = false;
  const auto revert_vm_mirror = epee::misc_utils::create_scope_leave_handler([&]() {
    if (vm_mirror_applied)
      m_vm_state.apply(vm_undo);
  });
  if (contracts_active)
  {
    crypto::hash claimed_root{};
    if (get_tx_vm_state_root(bl.miner_tx.extra, claimed_root) != 1)
    {
      MERROR_VER("Block with id: " << id << " does not carry exactly one contract state root");
      bvc.m_verifivation_failed = true;
      return_txs_to_pool();
      return false;
    }
    vm_update.writes = contract_executor.writes();
    vm_undo = vm::undo_for(m_vm_state, vm_update.writes);
    m_vm_state.apply(vm_update.writes);
    vm_mirror_applied = true;
    if (!make_vm_commitment(blockchain_height, contract_executor.effects(), vm_update.commitment) || vm_update.commitment.root != claimed_root)
    {
      MERROR_VER("Block with id: " << id << " has contract state root " << claimed_root << ", expected " << vm_update.commitment.root);
      bvc.m_verifivation_failed = true;
      return_txs_to_pool();
      return false;
    }
  }

  chainlock_record chainlock_carrier;
  bool has_chainlock_carrier = false;
  if (!validate_block_chainlock(bl, blockchain_height, !fast_check, chainlock_carrier, has_chainlock_carrier))
  {
    MERROR_VER("Block with id: " << id << " carries an invalid chainlock");
    bvc.m_verifivation_failed = true;
    return_txs_to_pool();
    return false;
  }

  TIME_MEASURE_START(vmt);
  uint64_t base_reward = 0;
  uint64_t already_generated_coins = blockchain_height ? m_db->get_block_already_generated_coins(blockchain_height - 1) : 0;
  if(!validate_miner_transaction(bl, cumulative_block_weight, fee_summary, contract_minted, base_reward, already_generated_coins, bvc.m_partial_block_reward, bl.major_version))
  {
    MERROR_VER("Block with id: " << id << " has incorrect miner transaction");
    bvc.m_verifivation_failed = true;
    return_txs_to_pool();
    return false;
  }

  epose_state_commitment_v2 epose_commitment{};
  std::vector<qwertycoin::epose::envelope_record_v2> confirmed_epose_records;
  if (!apply_epose_block(
          bl, txs, blockchain_height, cumulative_block_weight, base_reward,
          fee_summary, contract_minted,
          epose_commitment, confirmed_epose_records))
  {
    bvc.m_verifivation_failed = true;
    return_txs_to_pool();
    return false;
  }

  TIME_MEASURE_FINISH(vmt);
  size_t block_weight;
  difficulty_type cumulative_difficulty;

  // populate various metadata about the block to be stored alongside it.
  block_weight = cumulative_block_weight;
  cumulative_difficulty = current_diffic;
  // In the "tail" state when the minimum subsidy (implemented in get_block_reward) is in effect, the number of
  // coins will eventually exceed MONEY_SUPPLY and overflow a uint64. To prevent overflow, cap already_generated_coins
  // at MONEY_SUPPLY. already_generated_coins is only used to compute the block subsidy and MONEY_SUPPLY yields a
  // subsidy of 0 under the base formula and therefore the minimum subsidy >0 in the tail state.
  already_generated_coins = base_reward < (MONEY_SUPPLY-already_generated_coins) ? already_generated_coins + base_reward : MONEY_SUPPLY;
  if(blockchain_height)
    cumulative_difficulty += m_db->get_block_cumulative_difficulty(blockchain_height - 1);

  TIME_MEASURE_FINISH(block_processing_time);
  if(precomputed)
    block_processing_time += m_fake_pow_calc_time;

  std::string epose_snapshot_image;
  if (m_epose_v2 && qwertycoin::epose::requires_boundary_snapshot(
          epose_commitment.schema_version, blockchain_height,
          m_epose_v2_parameters.timing.epoch_length))
  {
    qwertycoin::epose::state_snapshot_v2 snapshot{};
    snapshot.state_schema = epose_commitment.schema_version;
    snapshot.parameter_set_hash = epose_commitment.parameter_set_hash;
    snapshot.height = blockchain_height;
    snapshot.block_hash = epose_commitment.block_hash;
    snapshot.state_root = epose_commitment.state_hash;
    snapshot.state_image = m_epose_v2->serialize_state();
    if (!snapshot.state_image.empty())
      epose_snapshot_image = snapshot.serialize();
    if (epose_snapshot_image.empty())
      MERROR("Failed to serialize EPoSE boundary snapshot at height " << blockchain_height);
  }

  rtxn_guard.stop();
  TIME_MEASURE_START(addblock);
  uint64_t new_height = 0;
  if (!bvc.m_verifivation_failed)
  {
    try
    {
      uint64_t long_term_block_weight = get_next_long_term_block_weight(block_weight);
      cryptonote::blobdata bd = cryptonote::block_to_blob(bl);
      const epose_state_commitment_v2 *commitment =
          m_epose_v2 ? &epose_commitment : nullptr;
      new_height = m_db->add_block(std::make_pair(std::move(bl), std::move(bd)), block_weight, long_term_block_weight, cumulative_difficulty, already_generated_coins, txs, commitment, contracts_active ? &vm_update : nullptr, has_chainlock_carrier ? &chainlock_carrier : nullptr, epose_snapshot_image.empty() ? nullptr : &epose_snapshot_image);
      vm_mirror_applied = false;
    }
    catch (const KEY_IMAGE_EXISTS& e)
    {
      LOG_ERROR("Error adding block with hash: " << id << " to blockchain, what = " << e.what());
      m_batch_success = false;
      bvc.m_verifivation_failed = true;
      rollback_epose_block(blockchain_height);
      return_txs_to_pool();
      return false;
    }
    catch (const std::exception& e)
    {
      //TODO: figure out the best way to deal with this failure
      LOG_ERROR("Error adding block with hash: " << id << " to blockchain, what = " << e.what());
      m_batch_success = false;
      bvc.m_verifivation_failed = true;
      rollback_epose_block(blockchain_height);
      return_txs_to_pool();
      return false;
    }
  }
  else
  {
    LOG_ERROR("Blocks that failed verification should not reach here");
  }

  TIME_MEASURE_FINISH(addblock);

  // do this after updating the hard fork state since the weight limit may change due to fork
  if (!update_next_cumulative_weight_limit())
  {
    MERROR("Failed to update next cumulative weight limit");
    pop_block_from_blockchain();
    return false;
  }
  if (m_epose_v2_relay_pool && !confirmed_epose_records.empty())
    m_epose_v2_relay_pool->erase_confirmed_records(
        confirmed_epose_records, new_height - 1);

  MINFO("+++++ BLOCK SUCCESSFULLY ADDED" << std::endl << "id:\t" << id << std::endl << "PoW:\t" << proof_of_work << std::endl << "HEIGHT " << new_height-1 << ", difficulty:\t" << current_diffic << std::endl << "block reward: " << print_money(fee_summary + base_reward) << "(" << print_money(base_reward) << " + " << print_money(fee_summary) << "), coinbase_weight: " << coinbase_weight << ", cumulative weight: " << cumulative_block_weight << ", " << block_processing_time << "(" << target_calculating_time << "/" << longhash_calculating_time << ")ms");
  if(m_show_time_stats)
  {
    MINFO("Height: " << new_height << " coinbase weight: " << coinbase_weight << " cumm: "
        << cumulative_block_weight << " p/t: " << block_processing_time << " ("
        << target_calculating_time << "/" << longhash_calculating_time << "/"
        << t1 << "/" << t2 << "/" << t3 << "/" << t_exists << "/" << t_pool
        << "/" << t_checktx << "/" << t_dblspnd << "/" << vmt << "/" << addblock << ")ms");
  }

  bvc.m_added_to_main_chain = true;
  ++m_sync_counter;

  // appears to be a NOP *and* is called elsewhere.  wat?
  m_tx_pool.on_blockchain_inc(new_height, id);
  get_difficulty_for_next_block(); // just to cache it
  invalidate_block_template_cache();

  const uint8_t new_hf_version = get_current_hard_fork_version();
  if (new_hf_version != hf_version)
  {
    // the genesis block is added before everything's setup, and the txpool is empty
    // when we start from scratch, so we skip this
    const bool is_genesis_block = new_height == 1;
    if (!is_genesis_block)
    {
      MGINFO("Validating txpool for v" << (unsigned)new_hf_version);
      m_tx_pool.validate(new_hf_version);
    }
  }

  const crypto::hash seedhash = get_block_id_by_height(crypto::rx_seedheight(new_height));

  // Make sure that txpool notifications happen BEFORE block and miner data notifications
  notify_txpool_event(std::move(txpool_events));

  // send miner notifications to switch as soon as possible
  send_miner_notifications(new_height, seedhash, id, already_generated_coins);

  // then send block notifications
  for (const auto& notifier: m_block_notifiers)
    notifier(new_height - 1, {std::addressof(bl), 1});

  if (m_hardfork->get_current_version() >= RX_BLOCK_VERSION)
    rx_set_main_seedhash(seedhash.data, tools::get_max_concurrency());

  return true;
}
//------------------------------------------------------------------
bool Blockchain::prune_blockchain(uint32_t pruning_seed)
{
  m_tx_pool.lock();
  epee::misc_utils::auto_scope_leave_caller unlocker = epee::misc_utils::create_scope_leave_handler([&](){m_tx_pool.unlock();});
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  return m_db->prune_blockchain(pruning_seed);
}
//------------------------------------------------------------------
bool Blockchain::update_blockchain_pruning()
{
  m_tx_pool.lock();
  epee::misc_utils::auto_scope_leave_caller unlocker = epee::misc_utils::create_scope_leave_handler([&](){m_tx_pool.unlock();});
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  return m_db->update_pruning();
}
//------------------------------------------------------------------
bool Blockchain::check_blockchain_pruning()
{
  m_tx_pool.lock();
  epee::misc_utils::auto_scope_leave_caller unlocker = epee::misc_utils::create_scope_leave_handler([&](){m_tx_pool.unlock();});
  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  return m_db->check_pruning();
}
//------------------------------------------------------------------
// returns min(Mb, 1.7*Ml) as per https://github.com/ArticMine/Monero-Documents/blob/master/MoneroScaling2021-02.pdf from HF_VERSION_LONG_TERM_BLOCK_WEIGHT
uint64_t Blockchain::get_next_long_term_block_weight(uint64_t block_weight) const
{
  PERF_TIMER(get_next_long_term_block_weight);

  const uint64_t db_height = m_db->height();
  const uint64_t nblocks = std::min<uint64_t>(m_long_term_block_weights_window, db_height);

  const uint8_t hf_version = get_current_hard_fork_version();
  if (hf_version < HF_VERSION_LONG_TERM_BLOCK_WEIGHT)
    return block_weight;

  if (nblocks == 0)
    return block_weight;

  return constrained_long_term_block_weight(block_weight,
      get_long_term_block_weight_median(db_height - nblocks, nblocks), hf_version);
}
//------------------------------------------------------------------
bool Blockchain::update_next_cumulative_weight_limit(uint64_t *long_term_effective_median_block_weight)
{
  PERF_TIMER(update_next_cumulative_weight_limit);

  LOG_PRINT_L3("Blockchain::" << __func__);

  // when we reach this, the last hf version is not yet written to the db
  const uint64_t db_height = m_db->height();
  const uint8_t hf_version = get_current_hard_fork_version();
  uint64_t full_reward_zone = get_min_block_weight(hf_version);

  if (hf_version < HF_VERSION_LONG_TERM_BLOCK_WEIGHT)
  {
    std::vector<uint64_t> weights;
    get_last_n_blocks_weights(weights, CRYPTONOTE_REWARD_BLOCKS_WINDOW);
    m_current_block_cumul_weight_median = epee::misc_utils::median(weights);
  }
  else
  {
    const uint64_t nblocks = std::min<uint64_t>(m_long_term_block_weights_window, db_height);
    const uint64_t long_term_median = nblocks == 0 ? 0 : get_long_term_block_weight_median(db_height - nblocks, nblocks);

    m_long_term_effective_median_block_weight = std::max<uint64_t>(CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5, long_term_median);

    std::vector<uint64_t> weights;
    get_last_n_blocks_weights(weights, CRYPTONOTE_REWARD_BLOCKS_WINDOW);

    uint64_t short_term_median = epee::misc_utils::median(weights);
    uint64_t effective_median_block_weight;
    if (hf_version >= HF_VERSION_2021_SCALING)
    {
      // effective median = short_term_median bounded to range [long_term_median, 50*long_term_median], but it can't be smaller than the
      // minimum penalty free zone (a.k.a. 'full reward zone')
      effective_median_block_weight = std::min<uint64_t>(std::max<uint64_t>(m_long_term_effective_median_block_weight, short_term_median), CRYPTONOTE_SHORT_TERM_BLOCK_WEIGHT_SURGE_FACTOR * m_long_term_effective_median_block_weight);
    }
    else
    {
      // effective median = short_term_median bounded to range [0, 50*long_term_median], but it can't be smaller than the
      // minimum penalty free zone (a.k.a. 'full reward zone')
      effective_median_block_weight = std::min<uint64_t>(std::max<uint64_t>(CRYPTONOTE_BLOCK_GRANTED_FULL_REWARD_ZONE_V5, short_term_median), CRYPTONOTE_SHORT_TERM_BLOCK_WEIGHT_SURGE_FACTOR * m_long_term_effective_median_block_weight);
    }

    m_current_block_cumul_weight_median = effective_median_block_weight;
  }

  if (m_current_block_cumul_weight_median <= full_reward_zone)
    m_current_block_cumul_weight_median = full_reward_zone;

  m_current_block_cumul_weight_limit = m_current_block_cumul_weight_median * 2;

  if (long_term_effective_median_block_weight)
    *long_term_effective_median_block_weight = m_long_term_effective_median_block_weight;

  if (!m_db->is_read_only())
    m_db->add_max_block_size(m_current_block_cumul_weight_limit);

  return true;
}
//------------------------------------------------------------------
bool Blockchain::add_new_block(const block& bl_, block_verification_context& bvc)
{
  pool_supplement ps{};
  return add_new_block(bl_, bvc, ps);
}
//------------------------------------------------------------------
bool Blockchain::add_new_block(const block& bl, block_verification_context& bvc,
  pool_supplement& extra_block_txs)
{
  try
  {

  LOG_PRINT_L3("Blockchain::" << __func__);
  crypto::hash id = get_block_hash(bl);
  CRITICAL_REGION_LOCAL(m_tx_pool);//to avoid deadlock lets lock tx_pool for whole add/reorganize process
  CRITICAL_REGION_LOCAL1(m_blockchain_lock);
  m_epose_switch_mutated = false;
  if (!epose_state_at_tip())
  {
    bvc.m_retryable_busy = true;
    schedule_epose_recovery();
    return false;
  }
  db_rtxn_guard rtxn_guard(m_db);
  const uint64_t previous_height = m_db->height();
  if(have_block(id))
  {
    LOG_PRINT_L3("block with id = " << id << " already exists");
    bvc.m_already_exists = true;
    return false;
  }

  //check that block refers to chain tail
  if(!(bl.prev_id == get_tail_id()))
  {
    //chain switching or wrong block
    bvc.m_added_to_main_chain = false;
    rtxn_guard.stop();
    return handle_alternative_block(bl, id, bvc, extra_block_txs);
    //never relay alternative blocks
  }

  rtxn_guard.stop();
  const bool added = handle_block_to_main_chain(bl, id, bvc, extra_block_txs);
  if (added && bvc.m_added_to_main_chain)
    prune_after_canonical_advance(previous_height);
  return added;

  }
  catch (const std::exception &e)
  {
    LOG_ERROR("Exception at [add_new_block], what=" << e.what());
    CRITICAL_REGION_LOCAL(m_tx_pool);
    CRITICAL_REGION_LOCAL1(m_blockchain_lock);
    if (m_epose_switch_mutated)
    {
      m_batch_success = false;
      m_epose_switch_busy = true;
      bvc.m_retryable_busy = true;
      bvc.m_verifivation_failed = false;
    }
    else
      bvc.m_verifivation_failed = true;
    return false;
  }
}
//------------------------------------------------------------------
//TODO: Refactor, consider returning a failure height and letting
//      caller decide course of action.
void Blockchain::check_against_checkpoints(const checkpoints& points, bool enforce)
{
  const auto& pts = points.get_points();
  bool stop_batch;

  CRITICAL_REGION_LOCAL(m_blockchain_lock);
  stop_batch = m_db->batch_start();
  const uint64_t blockchain_height = m_db->height();
  for (const auto& pt : pts)
  {
    // if the checkpoint is for a block we don't have yet, move on
    if (pt.first >= blockchain_height)
    {
      continue;
    }

    if (!points.check_block(pt.first, m_db->get_block_hash_from_height(pt.first)))
    {
      // if asked to enforce checkpoints, roll back to a couple of blocks before the checkpoint
      if (enforce)
      {
        LOG_ERROR("Local blockchain failed to pass a checkpoint, rolling back!");
        tombstone_main_chainlocks_from(pt.first); // H-5
        std::list<block> empty;
        rollback_blockchain_switching(empty, pt.first - 2);
      }
      else
      {
        LOG_ERROR("WARNING: local blockchain failed to pass a DNS checkpoint, and you could be on a fork. You should either sync up from scratch, OR download a fresh blockchain bootstrap, OR enable checkpoint enforcing with the --enforce-dns-checkpointing command-line option");
      }
    }
  }
  if (stop_batch)
    m_db->batch_stop();
}
//------------------------------------------------------------------
// returns false if any of the checkpoints loading returns false.
// That should happen only if a checkpoint is added that conflicts
// with an existing checkpoint.
bool Blockchain::update_checkpoints(const std::string& file_path, bool check_dns)
{
  if (!m_checkpoints.load_checkpoints_from_json(file_path))
  {
      return false;
  }

  // if we're checking both dns and json, load checkpoints from dns.
  // if we're not hard-enforcing dns checkpoints, handle accordingly
  if (m_enforce_dns_checkpoints && check_dns && !m_offline)
  {
    if (!m_checkpoints.load_checkpoints_from_dns())
    {
      return false;
    }
  }
  else if (check_dns && !m_offline)
  {
    checkpoints dns_points;
    dns_points.load_checkpoints_from_dns();
    if (m_checkpoints.check_for_conflicts(dns_points))
    {
      check_against_checkpoints(dns_points, false);
    }
    else
    {
      MERROR("One or more checkpoints fetched from DNS conflicted with existing checkpoints!");
    }
  }

  check_against_checkpoints(m_checkpoints, true);

  return true;
}
//------------------------------------------------------------------
void Blockchain::set_enforce_dns_checkpoints(bool enforce_checkpoints)
{
  m_enforce_dns_checkpoints = enforce_checkpoints;
}

//------------------------------------------------------------------
void Blockchain::block_longhash_worker(uint64_t height, const epee::span<const block> &blocks, std::unordered_map<crypto::hash, crypto::hash> &map) const
{
  TIME_MEASURE_START(t);
  slow_hash_allocate_state();

  for (const auto & block : blocks)
  {
    if (m_cancel)
       break;
    crypto::hash id = get_block_hash(block);
    crypto::hash pow = get_block_longhash(this, block, height++, 0);
    map.emplace(id, pow);
  }

  slow_hash_free_state();
  TIME_MEASURE_FINISH(t);
}

//------------------------------------------------------------------
bool Blockchain::cleanup_handle_incoming_blocks(bool force_sync)
{
  epee::misc_utils::auto_scope_leave_caller unlocker = epee::misc_utils::create_scope_leave_handler([&](){m_tx_pool.unlock();});
  bool success = false;
  bool batch_aborted = false;
  bool batch_closed = false;
  std::vector<returned_pool_tx> returned_pool_txs;

  MTRACE("Blockchain::" << __func__);
  CRITICAL_REGION_BEGIN(m_blockchain_lock);
  TIME_MEASURE_START(t1);

  try
  {
    if (m_batch_success)
    {
      m_db->batch_stop();
      if (m_reset_timestamps_and_difficulties_height)
      {
        m_timestamps_and_difficulties_height = 0;
        m_reset_timestamps_and_difficulties_height = false;
      }
    }
    else
    {
      m_db->batch_abort();
      batch_aborted = true;
    }
    batch_closed = true;
    m_epose_rpc_batch_open = false;
    if (m_batch_success && m_alternative_prune_pending)
      prune_after_canonical_advance(m_db->height());
    success = true;
  }
  catch (const std::exception &e)
  {
    MERROR("Exception in cleanup_handle_incoming_blocks: " << e.what());
  }

  // A database batch can contain several blocks. If a later block fails,
  // LMDB rolls the complete batch back while the in-memory coordinator has
  // already advanced through each earlier block. Rebuild only after the
  // abort completed so memory and the canonical database converge again.
  if (success && batch_aborted && !load_vm_state())
  {
    MERROR("Failed to reload contract state after database batch abort");
    success = false;
  }

  if (success && batch_aborted && m_epose_v2 && !try_rebuild_epose_state())
  {
    MERROR("Failed to replay EPoSE-v2 state after database batch abort");
    schedule_epose_recovery();
    success = false;
  }

  if (success && m_sync_counter > 0)
  {
    if (force_sync)
    {
      if(m_db_sync_mode != db_nosync)
        store_blockchain();
      m_sync_counter = 0;
    }
    else if (m_db_sync_threshold && ((m_db_sync_on_blocks && m_sync_counter >= m_db_sync_threshold) || (!m_db_sync_on_blocks && m_bytes_to_sync >= m_db_sync_threshold)))
    {
      MDEBUG("Sync threshold met, syncing");
      if(m_db_sync_mode == db_async)
      {
        m_sync_counter = 0;
        m_bytes_to_sync = 0;
        boost::asio::dispatch(m_async_service, boost::bind(&Blockchain::store_blockchain, this));
      }
      else if(m_db_sync_mode == db_sync)
      {
        store_blockchain();
      }
      else // db_nosync
      {
        // DO NOTHING, not required to call sync.
      }
    }
  }

  TIME_MEASURE_FINISH(t1);
  m_blocks_longhash_table.clear();
  m_scan_table.clear();

  // when we're well clear of the precomputed hashes, free the memory
  if (!m_blocks_hash_check.empty() && m_db->height() > m_blocks_hash_check.size() + 4096)
  {
    MINFO("Dumping block hashes, we're now 4k past " << m_blocks_hash_check.size());
    m_blocks_hash_check.clear();
    m_blocks_hash_check.shrink_to_fit();
  }

  if (batch_closed)
    returned_pool_txs.swap(m_returned_pool_txs);

  CRITICAL_REGION_END();
  for (returned_pool_tx &returned : returned_pool_txs)
  {
    tx_verification_context tvc{};
    try
    {
      if (!m_tx_pool.add_tx(returned.tx, returned.id, returned.blob, returned.weight, tvc,
          relay_method::block, true, returned.rules_version, returned.rules_version)
          && !tvc.m_window_expired)
        MERROR("Failed to return taken transaction with hash: " << returned.id << " to tx_pool");
    }
    catch (const std::exception &e)
    {
      MERROR("Failed to return taken transaction with hash: " << returned.id << " to tx_pool: " << e.what());
    }
  }

  update_blockchain_pruning();

  return success && !batch_aborted;
}

//------------------------------------------------------------------
void Blockchain::output_scan_worker(const uint64_t amount, const std::vector<uint64_t> &offsets, std::vector<output_data_t> &outputs) const
{
  try
  {
    m_db->get_output_key(epee::span<const uint64_t>(&amount, 1), offsets, outputs, true);
  }
  catch (const std::exception& e)
  {
    MERROR_VER("EXCEPTION: " << e.what());
  }
  catch (...)
  {

  }
}

uint64_t Blockchain::prevalidate_block_hashes(uint64_t height, const std::vector<crypto::hash> &hashes, const std::vector<uint64_t> &weights)
{
  // new: . . . . . X X X X X . . . . . .
  // pre: A A A A B B B B C C C C D D D D

  CHECK_AND_ASSERT_MES(weights.empty() || weights.size() == hashes.size(), 0, "Unexpected weights size");

  CRITICAL_REGION_LOCAL(m_blockchain_lock);

  // easy case: height >= hashes
  if (height >= m_blocks_hash_of_hashes.size() * HASH_OF_HASHES_STEP)
    return hashes.size();

  // if we're getting old blocks, we might have jettisoned the hashes already
  if (m_blocks_hash_check.empty())
    return hashes.size();

  // find hashes encompassing those block
  size_t first_index = height / HASH_OF_HASHES_STEP;
  size_t last_index = (height + hashes.size() - 1) / HASH_OF_HASHES_STEP;
  MDEBUG("Blocks " << height << " - " << (height + hashes.size() - 1) << " start at " << first_index << " and end at " << last_index);

  // case of not enough to calculate even a single hash
  if (first_index == last_index && hashes.size() < HASH_OF_HASHES_STEP && (height + hashes.size()) % HASH_OF_HASHES_STEP)
    return hashes.size();

  // build hashes vector to hash hashes together
  std::vector<crypto::hash> data_hashes;
  std::vector<uint64_t> data_weights;
  data_hashes.reserve(hashes.size() + HASH_OF_HASHES_STEP - 1); // may be a bit too much
  if (!weights.empty())
    data_weights.reserve(data_hashes.size());

  // we expect height to be either equal or a bit below db height
  bool disconnected = (height > m_db->height());
  size_t pop;
  if (disconnected && height % HASH_OF_HASHES_STEP)
  {
    ++first_index;
    pop = HASH_OF_HASHES_STEP - height % HASH_OF_HASHES_STEP;
  }
  else
  {
    // we might need some already in the chain for the first part of the first hash
    for (uint64_t h = first_index * HASH_OF_HASHES_STEP; h < height; ++h)
    {
      data_hashes.push_back(m_db->get_block_hash_from_height(h));
      if (!weights.empty())
        data_weights.push_back(m_db->get_block_weight(h));
    }
    pop = 0;
  }

  // push the data to check
  for (size_t i = 0; i < hashes.size(); ++i)
  {
    if (pop)
      --pop;
    else
    {
      data_hashes.push_back(hashes[i]);
      if (!weights.empty())
        data_weights.push_back(weights[i]);
    }
  }

  // hash and check
  uint64_t usable = first_index * HASH_OF_HASHES_STEP - height; // may start negative, but unsigned under/overflow is not UB
  for (size_t n = first_index; n <= last_index; ++n)
  {
    if (n < m_blocks_hash_of_hashes.size())
    {
      // if the last index isn't fully filled, we can't tell if valid
      if (data_hashes.size() < (n - first_index) * HASH_OF_HASHES_STEP + HASH_OF_HASHES_STEP)
        break;

      crypto::hash hash;
      cn_fast_hash(data_hashes.data() + (n - first_index) * HASH_OF_HASHES_STEP, HASH_OF_HASHES_STEP * sizeof(crypto::hash), hash);
      bool valid = hash == m_blocks_hash_of_hashes[n].first;
      if (valid && !weights.empty())
      {
        cn_fast_hash(data_weights.data() + (n - first_index) * HASH_OF_HASHES_STEP, HASH_OF_HASHES_STEP * sizeof(uint64_t), hash);
        valid &= hash == m_blocks_hash_of_hashes[n].second;
      }

      // add to the known hashes array
      if (!valid)
      {
        MDEBUG("invalid hash for blocks " << n * HASH_OF_HASHES_STEP << " - " << (n * HASH_OF_HASHES_STEP + HASH_OF_HASHES_STEP - 1));
        break;
      }

      size_t end = n * HASH_OF_HASHES_STEP + HASH_OF_HASHES_STEP;
      for (size_t i = n * HASH_OF_HASHES_STEP; i < end; ++i)
      {
        CHECK_AND_ASSERT_MES(m_blocks_hash_check[i].first == crypto::null_hash || m_blocks_hash_check[i].first == data_hashes[i - first_index * HASH_OF_HASHES_STEP],
            0, "Consistency failure in m_blocks_hash_check construction");
        m_blocks_hash_check[i].first = data_hashes[i - first_index * HASH_OF_HASHES_STEP];
        if (!weights.empty())
        {
          CHECK_AND_ASSERT_MES(m_blocks_hash_check[i].second == 0 || m_blocks_hash_check[i].second == data_weights[i - first_index * HASH_OF_HASHES_STEP],
              0, "Consistency failure in m_blocks_hash_check construction");
          m_blocks_hash_check[i].second = data_weights[i - first_index * HASH_OF_HASHES_STEP];
        }
      }
      usable += HASH_OF_HASHES_STEP;
    }
    else
    {
      // if after the end of the precomputed blocks, accept anything
      usable += HASH_OF_HASHES_STEP;
      if (usable > hashes.size())
        usable = hashes.size();
    }
  }
  MDEBUG("usable: " << usable << " / " << hashes.size());
  CHECK_AND_ASSERT_MES(usable < std::numeric_limits<uint64_t>::max() / 2, 0, "usable is negative");
  return usable;
}

bool Blockchain::has_block_weights(uint64_t height, uint64_t nblocks) const
{
  CHECK_AND_ASSERT_MES(nblocks > 0, false, "nblocks is 0");
  uint64_t last_block_height = height + nblocks - 1;
  if (last_block_height >= m_blocks_hash_check.size())
    return false;
  for (uint64_t h = height; h <= last_block_height; ++h)
    if (m_blocks_hash_check[h].second == 0)
      return false;
  return true;
}

//------------------------------------------------------------------
// ND: Speedups:
// 1. Thread long_hash computations if possible (m_max_prepare_blocks_threads = nthreads, default = 4)
// 2. Group all amounts (from txs) and related absolute offsets and form a table of tx_prefix_hash
//    vs [k_image, output_keys] (m_scan_table). This is faster because it takes advantage of bulk queries
//    and is threaded if possible. The table (m_scan_table) will be used later when querying output
//    keys.
bool Blockchain::prepare_handle_incoming_blocks(const std::vector<block_complete_entry> &blocks_entry, std::vector<block> &blocks)
{
  MTRACE("Blockchain::" << __func__);
  TIME_MEASURE_START(prepare);
  bool stop_batch;
  uint64_t bytes = 0;
  size_t total_txs = 0;
  blocks.clear();

  // Order of locking must be:
  //  m_incoming_tx_lock (optional)
  //  m_tx_pool lock
  //  blockchain lock
  //
  //  Something which takes the blockchain lock may never take the txpool lock
  //  if it has not provably taken the txpool lock earlier
  //
  //  The txpool lock is now taken in prepare_handle_incoming_blocks
  //  and released in cleanup_handle_incoming_blocks. This avoids issues
  //  when something uses the pool, which now uses the blockchain and
  //  needs a batch, since a batch could otherwise be active while the
  //  txpool and blockchain locks were not held

  m_tx_pool.lock();
  CRITICAL_REGION_LOCAL1(m_blockchain_lock);

  if(blocks_entry.size() == 0)
    return false;

  for (const auto &entry : blocks_entry)
  {
    bytes += entry.block.size();
    for (const auto &tx_blob : entry.txs)
    {
      bytes += tx_blob.blob.size();
    }
    total_txs += entry.txs.size();
  }
  m_bytes_to_sync += bytes;
  while (!(stop_batch = m_db->batch_start(blocks_entry.size(), bytes))) {
    m_blockchain_lock.unlock();
    m_tx_pool.unlock();
    epee::misc_utils::sleep_no_w(1000);
    m_tx_pool.lock();
    m_blockchain_lock.lock();
  }
  m_batch_success = true;
  m_epose_rpc_batch_open = true;

  const uint64_t height = m_db->height();
  if ((height + blocks_entry.size()) < m_blocks_hash_check.size())
    return true;

  bool blocks_exist = false;
  tools::threadpool& tpool = tools::threadpool::getInstanceForCompute();
  unsigned threads = tpool.get_max_concurrency();
  blocks.resize(blocks_entry.size());

  if (1)
  {
    // limit threads, default limit = 4
    if(threads > m_max_prepare_blocks_threads)
      threads = m_max_prepare_blocks_threads;

    unsigned int batches = blocks_entry.size() / threads;
    unsigned int extra = blocks_entry.size() % threads;
    MDEBUG("block_batches: " << batches);
    std::vector<std::unordered_map<crypto::hash, crypto::hash>> maps(threads);
    auto it = blocks_entry.begin();
    unsigned blockidx = 0;

    const crypto::hash tophash = m_db->top_block_hash();
    for (unsigned i = 0; i < threads; i++)
    {
      for (unsigned int j = 0; j < batches; j++, ++blockidx)
      {
        block &block = blocks[blockidx];
        crypto::hash block_hash;

        if (!parse_and_validate_block_from_blob(it->block, block, block_hash))
          return false;

        // check first block and skip all blocks if its not chained properly
        if (blockidx == 0)
        {
          if (block.prev_id != tophash)
          {
            MDEBUG("Skipping prepare blocks. New blocks don't belong to chain.");
            blocks.clear();
            return true;
          }
        }
        if (have_block(block_hash))
          blocks_exist = true;

        std::advance(it, 1);
      }
    }

    for (unsigned i = 0; i < extra && !blocks_exist; i++, blockidx++)
    {
      block &block = blocks[blockidx];
      crypto::hash block_hash;

      if (!parse_and_validate_block_from_blob(it->block, block, block_hash))
        return false;

      if (have_block(block_hash))
        blocks_exist = true;

      std::advance(it, 1);
    }

    if (!blocks_exist)
    {
      m_blocks_longhash_table.clear();
      uint64_t thread_height = height;
      tools::threadpool::waiter waiter(tpool);
      m_prepare_height = height;
      m_prepare_nblocks = blocks_entry.size();
      m_prepare_blocks = &blocks;
      for (unsigned int i = 0; i < threads; i++)
      {
        unsigned nblocks = batches;
        if (i < extra)
          ++nblocks;
        if (nblocks == 0)
          break;
        tpool.submit(&waiter, boost::bind(&Blockchain::block_longhash_worker, this, thread_height, epee::span<const block>(&blocks[thread_height - height], nblocks), std::ref(maps[i])), true);
        thread_height += nblocks;
      }

      if (!waiter.wait())
        return false;
      m_prepare_height = 0;

      if (m_cancel)
         return false;

      for (const auto & map : maps)
      {
        m_blocks_longhash_table.insert(map.begin(), map.end());
      }
    }
  }

  if (m_cancel)
    return false;

  if (blocks_exist)
  {
    MDEBUG("Skipping remainder of prepare blocks. Blocks exist.");
    return true;
  }

  m_fake_scan_time = 0;
  m_fake_pow_calc_time = 0;

  m_scan_table.clear();

  TIME_MEASURE_FINISH(prepare);
  m_fake_pow_calc_time = prepare / blocks_entry.size();

  if (blocks_entry.size() > 1 && threads > 1 && m_show_time_stats)
    MDEBUG("Prepare blocks took: " << prepare << " ms");

  TIME_MEASURE_START(scantable);

  // [input] stores all unique amounts found
  std::vector < uint64_t > amounts;
  // [input] stores all absolute_offsets for each amount
  std::map<uint64_t, std::vector<uint64_t>> offset_map;
  // [output] stores all output_data_t for each absolute_offset
  std::map<uint64_t, std::vector<output_data_t>> tx_map;
  std::vector<std::pair<cryptonote::transaction, crypto::hash>> txes(total_txs);

#define SCAN_TABLE_QUIT(m) \
        do { \
            MERROR_VER(m) ;\
            m_scan_table.clear(); \
            return false; \
        } while(0); \

  // generate sorted tables for all amounts and absolute offsets
  size_t tx_index = 0, block_index = 0;
  for (const auto &entry : blocks_entry)
  {
    if (m_cancel)
      return false;

    for (const auto &tx_blob : entry.txs)
    {
      if (tx_index >= txes.size())
        SCAN_TABLE_QUIT("tx_index is out of sync");
      transaction &tx = txes[tx_index].first;
      crypto::hash &tx_prefix_hash = txes[tx_index].second;
      ++tx_index;

      if (!parse_and_validate_tx_base_from_blob(tx_blob.blob, tx))
        SCAN_TABLE_QUIT("Could not parse tx from incoming blocks.");
      cryptonote::get_transaction_prefix_hash(tx, tx_prefix_hash);

      auto its = m_scan_table.find(tx_prefix_hash);
      if (its != m_scan_table.end())
        SCAN_TABLE_QUIT("Duplicate tx found from incoming blocks.");

      m_scan_table.emplace(tx_prefix_hash, std::unordered_map<crypto::key_image, std::vector<output_data_t>>());
      its = m_scan_table.find(tx_prefix_hash);
      assert(its != m_scan_table.end());

      // initialize amount buckets for tx.vin(s)
      for (const auto &txin : tx.vin)
      {
        const txin_to_key &in_to_key = boost::get < txin_to_key > (txin);

        // check for duplicate
        auto it = its->second.find(in_to_key.k_image);
        if (it != its->second.end())
          SCAN_TABLE_QUIT("Duplicate key_image found from incoming blocks.");

        offset_map.emplace(in_to_key.amount, std::vector<uint64_t>());
        tx_map.emplace(in_to_key.amount, std::vector<output_data_t>());
      }

      // add new absolute_offsets to offset_map
      for (const auto &txin : tx.vin)
      {
        const txin_to_key &in_to_key = boost::get < txin_to_key > (txin);
        // no need to check for duplicate here.
        auto absolute_offsets = relative_output_offsets_to_absolute(in_to_key.key_offsets);
        for (const auto & offset : absolute_offsets)
          offset_map[in_to_key.amount].push_back(offset);

      }
    }
    ++block_index;
  }

  amounts.reserve(offset_map.size());
  for (const auto &offsets : offset_map)
    amounts.push_back(offsets.first);

  // sort and remove duplicate absolute_offsets in offset_map
  for (auto &offsets : offset_map)
  {
    std::sort(offsets.second.begin(), offsets.second.end());
    auto last = std::unique(offsets.second.begin(), offsets.second.end());
    offsets.second.erase(last, offsets.second.end());
  }

  // gather all the output keys
  threads = tpool.get_max_concurrency();
  if (!m_db->can_thread_bulk_indices())
    threads = 1;

  if (threads > 1 && amounts.size() > 1)
  {
    tools::threadpool::waiter waiter(tpool);

    for (size_t i = 0; i < amounts.size(); i++)
    {
      uint64_t amount = amounts[i];
      tpool.submit(&waiter, boost::bind(&Blockchain::output_scan_worker, this, amount, std::cref(offset_map[amount]), std::ref(tx_map[amount])), true);
    }
    if (!waiter.wait())
      return false;
  }
  else
  {
    for (size_t i = 0; i < amounts.size(); i++)
    {
      uint64_t amount = amounts[i];
      output_scan_worker(amount, offset_map[amount], tx_map[amount]);
    }
  }

  // now generate a table for each tx_prefix and k_image hashes
  tx_index = 0;
  for (const auto &entry : blocks_entry)
  {
    if (m_cancel)
      return false;

    for (size_t i = 0; i < entry.txs.size(); ++i)
    {
      if (tx_index >= txes.size())
        SCAN_TABLE_QUIT("tx_index is out of sync");
      const transaction &tx = txes[tx_index].first;
      const crypto::hash &tx_prefix_hash = txes[tx_index].second;
      ++tx_index;

      auto its = m_scan_table.find(tx_prefix_hash);
      if (its == m_scan_table.end())
        SCAN_TABLE_QUIT("Tx not found on scan table from incoming blocks.");

      for (const auto &txin : tx.vin)
      {
        const txin_to_key &in_to_key = boost::get < txin_to_key > (txin);
        auto needed_offsets = relative_output_offsets_to_absolute(in_to_key.key_offsets);

        const auto offset_it = offset_map.find(in_to_key.amount);
        const auto tx_it = tx_map.find(in_to_key.amount);
        if (offset_it == offset_map.end() || tx_it == tx_map.end())
          SCAN_TABLE_QUIT("Amount not found on scan table from incoming blocks.");

        const std::vector<uint64_t> &offsets_found = offset_it->second;
        const std::vector<output_data_t> &outputs_found = tx_it->second;

        std::vector<output_data_t> outputs;
        for (const uint64_t & offset_needed : needed_offsets)
        {
          // offsets_found is sorted above before output_scan_worker populates outputs_found.
          const auto found_it = std::lower_bound(offsets_found.begin(), offsets_found.end(), offset_needed);
          const size_t pos = found_it - offsets_found.begin();
          const bool found = found_it != offsets_found.end() && *found_it == offset_needed;

          if (found && pos < outputs_found.size())
            outputs.push_back(outputs_found[pos]);
          else
            break;
        }

        its->second.emplace(in_to_key.k_image, outputs);
      }
    }
  }

  TIME_MEASURE_FINISH(scantable);
  if (total_txs > 0)
  {
    m_fake_scan_time = scantable / total_txs;
    if(m_show_time_stats)
      MDEBUG("Prepare scantable took: " << scantable << " ms");
  }

  return true;
}

void Blockchain::prepare_handle_incoming_block_no_preprocess(const size_t block_byte_estimate)
{
  m_tx_pool.lock();
  bool pool_locked = true;
  try
  {
    CRITICAL_REGION_LOCAL1(m_blockchain_lock);
    m_bytes_to_sync += block_byte_estimate;
    while (!m_db->batch_start(1, block_byte_estimate))
    {
      m_blockchain_lock.unlock();
      m_tx_pool.unlock();
      pool_locked = false;
      epee::misc_utils::sleep_no_w(1000);
      m_tx_pool.lock();
      pool_locked = true;
      m_blockchain_lock.lock();
    }
    m_batch_success = true;
    m_epose_rpc_batch_open = true;
  }
  catch (...)
  {
    if (pool_locked)
      m_tx_pool.unlock();
    throw;
  }
}

void Blockchain::add_txpool_tx(const crypto::hash &txid, const cryptonote::blobdata &blob, const txpool_tx_meta_t &meta)
{
  m_db->add_txpool_tx(txid, blob, meta);
}

void Blockchain::update_txpool_tx(const crypto::hash &txid, const txpool_tx_meta_t &meta)
{
  m_db->update_txpool_tx(txid, meta);
}

void Blockchain::remove_txpool_tx(const crypto::hash &txid)
{
  m_db->remove_txpool_tx(txid);
}

uint64_t Blockchain::get_txpool_tx_count(bool include_sensitive) const
{
  return m_db->get_txpool_tx_count(include_sensitive ? relay_category::all : relay_category::broadcasted);
}

bool Blockchain::get_txpool_tx_meta(const crypto::hash& txid, txpool_tx_meta_t &meta) const
{
  return m_db->get_txpool_tx_meta(txid, meta);
}

bool Blockchain::get_txpool_tx_blob(const crypto::hash& txid, cryptonote::blobdata &bd, relay_category tx_category) const
{
  return m_db->get_txpool_tx_blob(txid, bd, tx_category);
}

cryptonote::blobdata Blockchain::get_txpool_tx_blob(const crypto::hash& txid, relay_category tx_category) const
{
  return m_db->get_txpool_tx_blob(txid, tx_category);
}

bool Blockchain::for_all_txpool_txes(std::function<bool(const crypto::hash&, const txpool_tx_meta_t&, const cryptonote::blobdata_ref*)> f, bool include_blob, relay_category tx_category) const
{
  return m_db->for_all_txpool_txes(f, include_blob, tx_category);
}

bool Blockchain::txpool_tx_matches_category(const crypto::hash& tx_hash, relay_category category)
{
  return m_db->txpool_tx_matches_category(tx_hash, category);
}

void Blockchain::set_user_options(uint64_t maxthreads, bool sync_on_blocks, uint64_t sync_threshold, blockchain_db_sync_mode sync_mode, bool fast_sync)
{
  if (sync_mode == db_defaultsync)
  {
    m_db_default_sync = true;
    sync_mode = db_async;
  }
  m_db_sync_mode = sync_mode;
  m_fast_sync = fast_sync;
  m_db_sync_on_blocks = sync_on_blocks;
  m_db_sync_threshold = sync_threshold;
  m_max_prepare_blocks_threads = maxthreads;
}

void Blockchain::set_txpool_notify(TxpoolNotifyCallback&& notify)
{
  std::lock_guard<decltype(m_txpool_notifier_mutex)> lg(m_txpool_notifier_mutex);
  m_txpool_notifier = notify;
}

void Blockchain::add_block_notify(BlockNotifyCallback&& notify)
{
  if (notify)
  {
    CRITICAL_REGION_LOCAL(m_blockchain_lock);
    m_block_notifiers.push_back(std::move(notify));
  }
}

void Blockchain::add_miner_notify(MinerNotifyCallback&& notify)
{
  if (notify)
  {
    CRITICAL_REGION_LOCAL(m_blockchain_lock);
    m_miner_notifiers.push_back(std::move(notify));
  }
}

void Blockchain::notify_txpool_event(std::vector<txpool_event>&& event) const
{
  std::lock_guard<decltype(m_txpool_notifier_mutex)> lg(m_txpool_notifier_mutex);
  if (m_txpool_notifier)
  {
    try
    {
      m_txpool_notifier(std::move(event));
    }
    catch (const std::exception &e)
    {
      MDEBUG("During Blockchain::notify_txpool_event(), ignored exception: " << e.what());
    }
  }
}

void Blockchain::safesyncmode(const bool onoff)
{
  /* all of this is no-op'd if the user set a specific
   * --db-sync-mode at startup.
   */
  if (m_db_default_sync)
  {
    m_db->safesyncmode(onoff);
    m_db_sync_mode = onoff ? db_nosync : db_async;
  }
}

HardFork::State Blockchain::get_hard_fork_state() const
{
  return m_hardfork->get_state();
}

bool Blockchain::get_hard_fork_voting_info(uint8_t version, uint32_t &window, uint32_t &votes, uint32_t &threshold, uint64_t &earliest_height, uint8_t &voting) const
{
  return m_hardfork->get_voting_info(version, window, votes, threshold, earliest_height, voting);
}

uint64_t Blockchain::get_difficulty_target() const
{
  return block_target(m_nettype, get_current_hard_fork_version());
}

std::map<uint64_t, std::tuple<uint64_t, uint64_t, uint64_t>> Blockchain:: get_output_histogram(const std::vector<uint64_t> &amounts, bool unlocked, uint64_t recent_cutoff, uint64_t min_count) const
{
  return m_db->get_output_histogram(amounts, unlocked, recent_cutoff, min_count);
}

void Blockchain::load_alternative_blocks(blocks_ext_by_hash &blocks,
    alternative_child_index &children) const
{
  blocks.reserve(m_db->get_alt_block_count());
  m_db->for_all_alt_blocks([&blocks, &children](const crypto::hash &blkid, const cryptonote::alt_block_data_t &data, const cryptonote::blobdata_ref *blob) {
    if (!blob)
    {
      MERROR("No blob, but blobs were requested");
      return false;
    }
    block_extended_info bei;
    if (cryptonote::parse_and_validate_block_from_blob(*blob, bei.bl))
    {
      bei.height = data.height;
      bei.block_cumulative_weight = data.cumulative_weight;
      bei.cumulative_difficulty = data.cumulative_difficulty_high;
      bei.cumulative_difficulty = (bei.cumulative_difficulty << 64) + data.cumulative_difficulty_low;
      bei.already_generated_coins = data.already_generated_coins;
      children.add(bei.bl.prev_id, blkid);
      blocks.insert(std::make_pair(blkid, std::move(bei)));
    }
    else
      MERROR("Failed to parse block from blob");
    return true;
  }, true);
}

std::vector<std::pair<Blockchain::block_extended_info,std::vector<crypto::hash>>> Blockchain::get_alternative_chains() const
{
  std::vector<std::pair<Blockchain::block_extended_info,std::vector<crypto::hash>>> chains;
  blocks_ext_by_hash alt_blocks;
  alternative_child_index children(m_db->get_alt_block_count());
  load_alternative_blocks(alt_blocks, children);

  for (const auto &i: alt_blocks)
  {
    const crypto::hash top = cryptonote::get_block_hash(i.second.bl);
    if (!children.has_child(top))
    {
      std::vector<crypto::hash> chain;
      auto h = i.second.bl.prev_id;
      chain.push_back(top);
      blocks_ext_by_hash::const_iterator prev;
      while ((prev = alt_blocks.find(h)) != alt_blocks.end())
      {
        ++m_alternative_ancestor_walks;
        chain.push_back(h);
        h = prev->second.bl.prev_id;
      }
      chains.push_back(std::make_pair(i.second, chain));
    }
  }
  return chains;
}

std::vector<Blockchain::alternative_tip> Blockchain::get_alternative_tips(size_t *operation_count) const
{
  blocks_ext_by_hash alt_blocks;
  alternative_child_index children(m_db->get_alt_block_count());
  load_alternative_blocks(alt_blocks, children);
  std::unordered_set<crypto::hash> stored_locks;
  m_db->for_chainlocks_descending([&](const chainlock_record &lock) {
    if (!lock.tombstoned)
      stored_locks.insert(lock.block_hash);
    return true;
  });
  std::vector<blocks_ext_by_hash::const_iterator> ordered;
  ordered.reserve(alt_blocks.size());
  for (auto it = alt_blocks.cbegin(); it != alt_blocks.cend(); ++it)
    ordered.push_back(it);
  std::sort(ordered.begin(), ordered.end(), [](const auto &left, const auto &right) {
    return left->second.height < right->second.height;
  });
  struct branch_summary
  {
    uint64_t split_height;
    bool potential_lock;
    crypto::hash lock_source;
  };
  std::unordered_map<crypto::hash, branch_summary> summaries;
  summaries.reserve(alt_blocks.size());
  for (const auto &it : ordered)
  {
    const auto parent = summaries.find(it->second.bl.prev_id);
    blobdata carrier;
    const bool local_lock = stored_locks.count(it->first) != 0
        || get_tx_chainlock(it->second.bl.miner_tx.extra, carrier) == 1;
    summaries.emplace(it->first, branch_summary{
        parent == summaries.end() ? it->second.height : parent->second.split_height,
        local_lock || (parent != summaries.end() && parent->second.potential_lock),
        local_lock ? it->first
            : parent == summaries.end() ? crypto::null_hash : parent->second.lock_source});
  }
  std::vector<alternative_tip> tips;
  tips.reserve(alt_blocks.size());
  for (const auto &entry : alt_blocks)
    if (!children.has_child(entry.first))
    {
      const auto summary = summaries.find(entry.first);
      tips.push_back({entry.first, entry.second.cumulative_difficulty,
          summary->second.split_height, summary->second.potential_lock,
          summary->second.lock_source});
    }
  if (operation_count)
    *operation_count = children.operations();
  return tips;
}

crypto::hash Blockchain::best_alternative_descendant(const crypto::hash &ancestor,
    size_t *operation_count) const
{
  blocks_ext_by_hash blocks;
  alternative_child_index children(m_db->get_alt_block_count());
  load_alternative_blocks(blocks, children);
  std::unordered_map<crypto::hash, difficulty_type> scores;
  scores.reserve(blocks.size());
  for (const auto &entry : blocks)
    scores.emplace(entry.first, entry.second.cumulative_difficulty);
  const crypto::hash best = children.best_descendant(ancestor, scores);
  if (operation_count)
    *operation_count = children.operations();
  return best;
}

void Blockchain::cancel()
{
  m_cancel = true;
}

#if defined(PER_BLOCK_CHECKPOINT)
static const char expected_block_hashes_hash[] = "2aea941d43024422a63f223c84b9d88d1f58d31e1f508c2d6d43cd637ba32d16";
void Blockchain::load_compiled_in_block_hashes(const GetCheckpointsCallback& get_checkpoints)
{
  if (get_checkpoints == nullptr || !m_fast_sync)
  {
    return;
  }
  const epee::span<const unsigned char> &checkpoints = get_checkpoints(m_nettype);
  if (!checkpoints.empty())
  {
    MINFO("Loading precomputed blocks (" << checkpoints.size() << " bytes)");
    if (m_nettype == MAINNET)
    {
      // first check hash
      crypto::hash hash;
      if (!tools::sha256sum(checkpoints.data(), checkpoints.size(), hash))
      {
        MERROR("Failed to hash precomputed blocks data");
        return;
      }
      MINFO("precomputed blocks hash: " << hash << ", expected " << expected_block_hashes_hash);
      cryptonote::blobdata expected_hash_data;
      if (!epee::string_tools::parse_hexstr_to_binbuff(std::string(expected_block_hashes_hash), expected_hash_data) || expected_hash_data.size() != sizeof(crypto::hash))
      {
        MERROR("Failed to parse expected block hashes hash");
        return;
      }
      const crypto::hash expected_hash = *reinterpret_cast<const crypto::hash*>(expected_hash_data.data());
      if (hash != expected_hash)
      {
        MERROR("Block hash data does not match expected hash");
        return;
      }
    }

    if (checkpoints.size() > 4)
    {
      const unsigned char *p = checkpoints.data();
      const uint32_t nblocks = *p | ((*(p+1))<<8) | ((*(p+2))<<16) | ((*(p+3))<<24);
      if (nblocks > (std::numeric_limits<uint32_t>::max() - 4) / sizeof(hash))
      {
        MERROR("Block hash data is too large");
        return;
      }
      const size_t size_needed = 4 + nblocks * (sizeof(crypto::hash) * 2);
      if(checkpoints.size() != size_needed)
      {
        MERROR("Failed to load hashes - unexpected data size");
        return;
      }
      else if(nblocks > 0 && nblocks > (m_db->height() + HASH_OF_HASHES_STEP - 1) / HASH_OF_HASHES_STEP)
      {
        p += sizeof(uint32_t);
        m_blocks_hash_of_hashes.reserve(nblocks);
        for (uint32_t i = 0; i < nblocks; i++)
        {
          crypto::hash hash_hashes, hash_weights;
          memcpy(hash_hashes.data, p, sizeof(hash_hashes.data));
          p += sizeof(hash_hashes.data);
          memcpy(hash_weights.data, p, sizeof(hash_weights.data));
          p += sizeof(hash_weights.data);
          m_blocks_hash_of_hashes.push_back(std::make_pair(hash_hashes, hash_weights));
        }
        m_blocks_hash_check.resize(m_blocks_hash_of_hashes.size() * HASH_OF_HASHES_STEP, std::make_pair(crypto::null_hash, 0));
        MINFO(nblocks << " block hashes loaded");

        // FIXME: clear tx_pool because the process might have been
        // terminated and caused it to store txs kept by blocks.
        // The core will not call check_tx_inputs(..) for these
        // transactions in this case. Consequently, the sanity check
        // for tx hashes will fail in handle_block_to_main_chain(..)
        CRITICAL_REGION_LOCAL(m_tx_pool);

        std::vector<transaction> txs;
        m_tx_pool.get_transactions(txs, true);

        size_t tx_weight;
        uint64_t fee;
        bool relayed, do_not_relay, double_spend_seen, pruned;
        transaction pool_tx;
        blobdata txblob;
        for(const transaction &tx : txs)
        {
          crypto::hash tx_hash = get_transaction_hash(tx);
          m_tx_pool.take_tx(tx_hash, pool_tx, txblob, tx_weight, fee, relayed, do_not_relay, double_spend_seen, pruned);
        }
      }
    }
  }
}
#endif

bool Blockchain::is_within_compiled_block_hash_area(uint64_t height) const
{
#if defined(PER_BLOCK_CHECKPOINT)
  return height < m_blocks_hash_of_hashes.size() * HASH_OF_HASHES_STEP;
#else
  return false;
#endif
}

void Blockchain::lock()
{
  m_blockchain_lock.lock();
}

void Blockchain::unlock()
{
  m_blockchain_lock.unlock();
}

bool Blockchain::for_all_key_images(std::function<bool(const crypto::key_image&)> f) const
{
  return m_db->for_all_key_images(f);
}

bool Blockchain::for_blocks_range(const uint64_t& h1, const uint64_t& h2, std::function<bool(uint64_t, const crypto::hash&, const block&)> f) const
{
  return m_db->for_blocks_range(h1, h2, f);
}

bool Blockchain::for_all_transactions(std::function<bool(const crypto::hash&, const cryptonote::transaction&)> f, bool pruned) const
{
  return m_db->for_all_transactions(f, pruned);
}

bool Blockchain::for_all_outputs(std::function<bool(uint64_t amount, const crypto::hash &tx_hash, uint64_t height, size_t tx_idx)> f) const
{
  return m_db->for_all_outputs(f);
}

bool Blockchain::for_all_outputs(uint64_t amount, std::function<bool(uint64_t height)> f) const
{
  return m_db->for_all_outputs(amount, f);
}

void Blockchain::invalidate_block_template_cache()
{
  MDEBUG("Invalidating block template cache");
  m_btc_valid = false;
}

void Blockchain::cache_block_template(const block &b, const cryptonote::account_public_address &address, const blobdata &nonce, const difficulty_type &diff, uint64_t height, uint64_t expected_reward, uint64_t seed_height, const crypto::hash &seed_hash, uint64_t pool_cookie)
{
  MDEBUG("Setting block template cache");
  m_btc = b;
  m_btc_address = address;
  m_btc_nonce = nonce;
  m_btc_difficulty = diff;
  m_btc_height = height;
  m_btc_expected_reward = expected_reward;
  m_btc_seed_hash = seed_hash;
  m_btc_seed_height = seed_height;
  m_btc_pool_cookie = pool_cookie;
  m_btc_valid = true;
}

void Blockchain::send_miner_notifications(uint64_t height, const crypto::hash &seed_hash, const crypto::hash &prev_id, uint64_t already_generated_coins)
{
  if (m_miner_notifiers.empty())
    return;

  const uint8_t major_version = m_hardfork->get_ideal_version(height);
  const difficulty_type diff = get_difficulty_for_next_block();
  const uint64_t median_weight = m_current_block_cumul_weight_median;

  std::vector<tx_block_template_backlog_entry> tx_backlog;
  m_tx_pool.get_block_template_backlog(tx_backlog);

  for (const auto& notifier : m_miner_notifiers)
  {
    notifier(major_version, height, prev_id, seed_hash, diff, median_weight, already_generated_coins, tx_backlog);
  }
}

namespace cryptonote {
template bool Blockchain::get_transactions(const std::vector<crypto::hash>&, std::vector<transaction>&, std::vector<crypto::hash>&, bool) const;
template bool Blockchain::get_split_transactions_blobs(const std::vector<crypto::hash>&, std::vector<std::tuple<crypto::hash, cryptonote::blobdata, crypto::hash, cryptonote::blobdata>>&, std::vector<crypto::hash>&) const;
}
