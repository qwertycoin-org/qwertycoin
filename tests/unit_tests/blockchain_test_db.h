#pragma once

#include <algorithm>
#include <map>
#include <stdexcept>
#include <vector>

#include "blockchain_db/testdb.h"
#include "epose_coordinator_fixture.h"

namespace cryptonote
{
  class BlockchainTestDB : public BaseTestDB
  {
  public:
    uint64_t chain_height = 1;
    bool has_tip = false;
    bool throw_on_sync = false;
    bool has_commitment = false;
    epose_state_commitment_v2 commitment{};
    size_t chainlock_writes = 0;
    size_t alternative_reads = 0;
    std::vector<crypto::hash> pool_txs;
    std::map<crypto::hash, std::pair<alt_block_data_t, blobdata>> alternatives;
    std::map<crypto::hash, std::pair<alt_block_data_t, blobdata>> batch_alternatives;
    bool test_batch_active = false;
    std::vector<chainlock_record> stored_chainlocks;

    uint64_t height() const override { return chain_height; }
    bool batch_start(uint64_t = 0, uint64_t = 0) override
    {
      if (test_batch_active)
        return false;
      batch_alternatives = alternatives;
      test_batch_active = true;
      return true;
    }
    bool batch_active() const override { return test_batch_active; }
    void batch_stop() override
    {
      batch_alternatives.clear();
      test_batch_active = false;
    }
    void batch_abort() override
    {
      alternatives = std::move(batch_alternatives);
      test_batch_active = false;
    }
    void sync() override
    {
      if (throw_on_sync)
        throw std::runtime_error("Test database sync failure");
    }
    void add_txpool_tx(const crypto::hash &id, const blobdata_ref &,
        const txpool_tx_meta_t &) override
    {
      pool_txs.push_back(id);
    }
    bool txpool_has_tx(const crypto::hash &id, relay_category) const override
    {
      return std::find(pool_txs.begin(), pool_txs.end(), id) != pool_txs.end();
    }
    bool block_exists(const crypto::hash &, uint64_t *) const override { return has_tip; }
    crypto::hash top_block_hash(uint64_t *height = nullptr) const override
    {
      if (height)
        *height = 0;
      return has_tip ? get_block_hash_from_height(0) : crypto::hash();
    }
    crypto::hash get_block_hash_from_height(const uint64_t &) const override
    {
      return has_tip ? epose_coordinator_fixture::hash_text("blockchain-recovery-tip") : crypto::hash();
    }
    bool get_epose_state_commitment_v2(uint64_t, epose_state_commitment_v2 &out) const override
    {
      if (!has_commitment)
        return false;
      out = commitment;
      return true;
    }
    void put_chainlock(const chainlock_record &record) override
    {
      ++chainlock_writes;
      stored_chainlocks.push_back(record);
    }
    bool for_chainlocks_descending(std::function<bool(const chainlock_record &)> f) const override
    {
      for (auto it = stored_chainlocks.rbegin(); it != stored_chainlocks.rend(); ++it)
        if (!f(*it))
          return false;
      return true;
    }
    void add_alt_block(const crypto::hash &id, const alt_block_data_t &data,
        const blobdata_ref &blob) override
    {
      alternatives[id] = {data, blobdata(blob.data(), blob.size())};
    }
    bool get_alt_block(const crypto::hash &id, alt_block_data_t *data,
        blobdata *blob) override
    {
      ++alternative_reads;
      const auto found = alternatives.find(id);
      if (found == alternatives.end())
        return false;
      if (data)
        *data = found->second.first;
      if (blob)
        *blob = found->second.second;
      return true;
    }
    void remove_alt_block(const crypto::hash &id) override { alternatives.erase(id); }
    uint64_t get_alt_block_count() override { return alternatives.size(); }
    bool for_all_alt_blocks(std::function<bool(const crypto::hash &,
        const alt_block_data_t &, const blobdata_ref *)> f,
        bool include_blob = false) const override
    {
      for (const auto &entry : alternatives)
      {
        const blobdata_ref blob{entry.second.second.data(), entry.second.second.size()};
        if (!f(entry.first, entry.second.first, include_blob ? &blob : nullptr))
          return false;
      }
      return true;
    }
  };
}
