// Copyright (c) 2022-2026, The Qwertycoin Project

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <unordered_map>
#include <utility>

#include "crypto/hash.h"
#include "cryptonote_basic/blobdatatype.h"
#include "cryptonote_config.h"

namespace cryptonote
{
  class expired_window_tx_cache
  {
  public:
    expired_window_tx_cache(
        const uint64_t retention_blocks = QWC_WINDOW_EXPIRED_TX_RETENTION_BLOCKS,
        const size_t max_bytes = QWC_WINDOW_EXPIRED_TX_CACHE_MAX_BYTES)
      : m_retention_blocks(retention_blocks), m_max_bytes(max_bytes) {}

    void insert(const crypto::hash& txid, blobdata blob, const uint64_t dropped_at_height)
    {
      if (blob.size() > m_max_bytes)
        return;
      erase(txid);
      m_bytes += blob.size();
      m_by_drop_height.emplace(dropped_at_height, txid);
      m_entries.emplace(txid, entry{std::move(blob), dropped_at_height});
      while (m_bytes > m_max_bytes)
        erase(m_by_drop_height.begin()->second);
    }

    bool get(const crypto::hash& txid, blobdata& blob) const
    {
      const auto it = m_entries.find(txid);
      if (it == m_entries.end())
        return false;
      blob = it->second.blob;
      return true;
    }

    bool take(const crypto::hash& txid, blobdata& blob)
    {
      if (!get(txid, blob))
        return false;
      erase(txid);
      return true;
    }

    bool contains(const crypto::hash& txid) const
    {
      return m_entries.find(txid) != m_entries.end();
    }

    void evict_expired(const uint64_t current_height)
    {
      while (!m_by_drop_height.empty() && m_by_drop_height.begin()->first + m_retention_blocks < current_height)
        erase(m_by_drop_height.begin()->second);
    }

    void clear()
    {
      m_entries.clear();
      m_by_drop_height.clear();
      m_bytes = 0;
    }

    size_t size() const { return m_entries.size(); }
    size_t bytes() const { return m_bytes; }

  private:
    struct entry
    {
      blobdata blob;
      uint64_t dropped_at_height;
    };

    void erase(const crypto::hash& txid)
    {
      const auto it = m_entries.find(txid);
      if (it == m_entries.end())
        return;
      const auto range = m_by_drop_height.equal_range(it->second.dropped_at_height);
      for (auto drop = range.first; drop != range.second; ++drop)
      {
        if (drop->second == txid)
        {
          m_by_drop_height.erase(drop);
          break;
        }
      }
      m_bytes -= it->second.blob.size();
      m_entries.erase(it);
    }

    uint64_t m_retention_blocks;
    size_t m_max_bytes;
    size_t m_bytes = 0;
    std::unordered_map<crypto::hash, entry> m_entries;
    std::multimap<uint64_t, crypto::hash> m_by_drop_height;
  };
}
