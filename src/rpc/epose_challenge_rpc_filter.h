#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <list>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <utility>

#include <boost/asio/ip/address_v6.hpp>

#include "cryptonote_config.h"

namespace cryptonote
{
  struct epose_bucket_limits
  {
    size_t capacity;
    uint64_t refill_ms;
    size_t quantum;
  };

  constexpr epose_bucket_limits epose_host_requests{QWC_EPOSE_CHALLENGE_RPC_BUCKET_CAPACITY,
      QWC_EPOSE_CHALLENGE_RPC_REFILL_MS, 1};
  constexpr epose_bucket_limits epose_prefix_requests{QWC_EPOSE_CHALLENGE_RPC_IPV6_48_CAPACITY,
      QWC_EPOSE_CHALLENGE_RPC_IPV6_48_REFILL_MS, 1};
  constexpr epose_bucket_limits epose_global_requests{QWC_EPOSE_CHALLENGE_RPC_GLOBAL_CAPACITY,
      QWC_EPOSE_CHALLENGE_RPC_GLOBAL_REFILL_MS, 1};
  constexpr epose_bucket_limits epose_host_bytes{QWC_EPOSE_CHALLENGE_RPC_HOST_BYTE_CAPACITY,
      QWC_EPOSE_CHALLENGE_RPC_BYTE_REFILL_MS, QWC_EPOSE_CHALLENGE_RPC_HOST_BYTE_REFILL};
  constexpr epose_bucket_limits epose_prefix_bytes{QWC_EPOSE_CHALLENGE_RPC_IPV6_48_BYTE_CAPACITY,
      QWC_EPOSE_CHALLENGE_RPC_BYTE_REFILL_MS, QWC_EPOSE_CHALLENGE_RPC_IPV6_48_BYTE_REFILL};
  constexpr epose_bucket_limits epose_global_bytes{QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_CAPACITY,
      QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_REFILL_MS, QWC_EPOSE_CHALLENGE_RPC_RESPONSE_BYTE_REFILL};

  constexpr bool valid_epose_bucket_limits(epose_bucket_limits limits)
  {
    return limits.capacity > 0 && limits.refill_ms > 0 && limits.quantum > 0
        && limits.quantum <= limits.capacity
        && limits.refill_ms <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
  }

  static_assert(valid_epose_bucket_limits(epose_host_requests), "invalid host request limits");
  static_assert(valid_epose_bucket_limits(epose_prefix_requests), "invalid prefix request limits");
  static_assert(valid_epose_bucket_limits(epose_global_requests), "invalid global request limits");
  static_assert(valid_epose_bucket_limits(epose_host_bytes), "invalid host byte limits");
  static_assert(valid_epose_bucket_limits(epose_prefix_bytes), "invalid prefix byte limits");
  static_assert(valid_epose_bucket_limits(epose_global_bytes), "invalid global byte limits");
  static_assert(QWC_EPOSE_CHALLENGE_RPC_BUCKETS_MAX > 0, "invalid entry limit");
  constexpr uint64_t epose_response_reservation_bytes =
      static_cast<uint64_t>(QWC_EPOSE_CHALLENGE_RPC_MAX_RESPONSE_BYTES);
  static_assert(epose_response_reservation_bytes <=
      std::numeric_limits<uint64_t>::max() / QWC_EPOSE_CHALLENGE_RPC_BUCKETS_MAX,
      "in-flight byte bound overflows");
  constexpr uint64_t epose_max_in_flight_bytes =
      epose_response_reservation_bytes * QWC_EPOSE_CHALLENGE_RPC_BUCKETS_MAX;

  class epose_challenge_rpc_filter
  {
  public:
    enum class decision { allowed, banned, rate_limited };

    decision check(const std::string &host, const std::string &uri,
        bool p2p_allowed,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now())
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (p2p_allowed)
        clear_ban_for_host_unlocked(host);
      if (uri != EPOSE_SERVICE_CHALLENGE_V2_URI)
        return p2p_allowed ? decision::allowed : decision::banned;

      const auto keys = bucket_keys(host);
      const auto host_entry = buckets_.find(keys.first);
      const auto prefix_entry = keys.second.empty() ? buckets_.end() : buckets_.find(keys.second);
      const size_t missing = static_cast<size_t>(host_entry == buckets_.end())
          + static_cast<size_t>(!keys.second.empty() && prefix_entry == buckets_.end());
      const size_t available = QWC_EPOSE_CHALLENGE_RPC_BUCKETS_MAX - buckets_.size();
      const size_t needed_evictions = missing > available ? missing - available : 0;
      const size_t required_idle = static_cast<size_t>(host_entry != buckets_.end()
          && host_entry->second.in_flight_bytes == 0)
          + static_cast<size_t>(prefix_entry != buckets_.end()
              && prefix_entry->second.in_flight_bytes == 0);
      if (needed_evictions > idle_.size() - required_idle)
        return decision::rate_limited;

      if (host_entry != buckets_.end())
        touch(host_entry->second);
      if (prefix_entry != buckets_.end())
        touch(prefix_entry->second);
      for (size_t i = 0; i < needed_evictions; ++i)
      {
        buckets_.erase(idle_.back());
        idle_.pop_back();
      }

      auto &current = host_bucket(keys.first, epose_host_requests, epose_host_bytes, now);
      bucket *prefix = keys.second.empty() ? nullptr
          : &host_bucket(keys.second, epose_prefix_requests, epose_prefix_bytes, now);
      current.requests.refill(epose_host_requests, now);
      if (prefix)
        prefix->requests.refill(epose_prefix_requests, now);
      global_requests_.refill(epose_global_requests, now);
      current.bytes.refill(epose_host_bytes, now);
      if (prefix)
        prefix->bytes.refill(epose_prefix_bytes, now);
      global_bytes_.refill(epose_global_bytes, now);
      if (!current.requests.can_take(1) || !global_requests_.can_take(1)
          || (prefix && !prefix->requests.can_take(1))
          || !current.bytes.can_take(epose_response_reservation_bytes)
          || (prefix && !prefix->bytes.can_take(epose_response_reservation_bytes))
          || !global_bytes_.can_take(epose_response_reservation_bytes)
          || current.in_flight_bytes > epose_max_in_flight_bytes - epose_response_reservation_bytes
          || (prefix && prefix->in_flight_bytes > epose_max_in_flight_bytes - epose_response_reservation_bytes)
          || global_in_flight_bytes_ > epose_max_in_flight_bytes - epose_response_reservation_bytes)
        return decision::rate_limited;

      current.requests.take(1);
      global_requests_.take(1);
      current.bytes.take(epose_response_reservation_bytes);
      global_bytes_.take(epose_response_reservation_bytes);
      if (prefix)
      {
        prefix->requests.take(1);
        prefix->bytes.take(epose_response_reservation_bytes);
      }
      pin(current);
      current.in_flight_bytes += epose_response_reservation_bytes;
      if (prefix)
      {
        pin(*prefix);
        prefix->in_flight_bytes += epose_response_reservation_bytes;
      }
      global_in_flight_bytes_ += epose_response_reservation_bytes;
      return decision::allowed;
    }

    void settle_response_bytes(const std::string &host, size_t used_bytes)
    {
      std::lock_guard<std::mutex> lock(mutex_);
      const auto keys = bucket_keys(host);
      const auto current = buckets_.find(keys.first);
      if (current == buckets_.end())
        return;
      const auto prefix = keys.second.empty() ? buckets_.end() : buckets_.find(keys.second);
      if (current->second.in_flight_bytes < epose_response_reservation_bytes
          || (!keys.second.empty() && (prefix == buckets_.end()
              || prefix->second.in_flight_bytes < epose_response_reservation_bytes))
          || global_in_flight_bytes_ < epose_response_reservation_bytes)
        return;
      const uint64_t unused = epose_response_reservation_bytes
          - std::min(static_cast<uint64_t>(used_bytes), epose_response_reservation_bytes);
      settle(current->second, epose_host_bytes, unused);
      if (prefix != buckets_.end())
        settle(prefix->second, epose_prefix_bytes, unused);
      global_in_flight_bytes_ -= epose_response_reservation_bytes;
      global_bytes_.refund(unused, epose_global_bytes);
    }

    bool first_ban_for_host(const std::string &host)
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (!logged_bans_.insert(host).second)
        return false;
      if (ban_order_.size() >= QWC_EPOSE_CHALLENGE_RPC_BUCKETS_MAX)
      {
        logged_bans_.erase(ban_order_.front());
        ban_order_.pop_front();
      }
      ban_order_.push_back(host);
      return true;
    }

    void clear_ban_for_host(const std::string &host)
    {
      std::lock_guard<std::mutex> lock(mutex_);
      clear_ban_for_host_unlocked(host);
    }

  private:
    struct token_bucket
    {
      size_t tokens;
      std::chrono::steady_clock::time_point last;

      bool can_take(uint64_t count) const { return count <= tokens; }
      void take(uint64_t count)
      {
        if (can_take(count))
          tokens -= static_cast<size_t>(count);
      }

      void refund(uint64_t count, epose_bucket_limits limits)
      {
        if (tokens >= limits.capacity)
        {
          tokens = limits.capacity;
          return;
        }
        tokens += static_cast<size_t>(std::min(count, static_cast<uint64_t>(limits.capacity - tokens)));
      }

      void refill(epose_bucket_limits limits, std::chrono::steady_clock::time_point now)
      {
        if (now <= last)
          return;
        const auto last_ticks = last.time_since_epoch().count();
        const auto now_ticks = now.time_since_epoch().count();
        if (last_ticks < 0 && now_ticks >
            std::numeric_limits<std::chrono::steady_clock::duration::rep>::max() + last_ticks)
        {
          tokens = limits.capacity;
          last = now;
          return;
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count();
        if (elapsed <= 0)
          return;
        const auto interval = static_cast<int64_t>(limits.refill_ms);
        const auto steps = static_cast<uint64_t>(elapsed / interval);
        const size_t missing = tokens >= limits.capacity ? 0 : limits.capacity - tokens;
        const size_t saturation_steps = missing / limits.quantum + (missing % limits.quantum != 0);
        tokens = steps >= saturation_steps ? limits.capacity
            : tokens + static_cast<size_t>(steps) * limits.quantum;
        last += std::chrono::milliseconds(elapsed - elapsed % interval);
      }
    };

    struct bucket
    {
      token_bucket requests;
      token_bucket bytes;
      uint64_t in_flight_bytes;
      std::list<std::string>::iterator recency;
    };

    void clear_ban_for_host_unlocked(const std::string &host)
    {
      if (logged_bans_.erase(host) != 0)
        ban_order_.remove(host);
    }

    void touch(bucket &entry)
    {
      auto &recency = entry.in_flight_bytes == 0 ? idle_ : pinned_;
      recency.splice(recency.begin(), recency, entry.recency);
    }

    void pin(bucket &entry)
    {
      if (entry.in_flight_bytes == 0)
        pinned_.splice(pinned_.begin(), idle_, entry.recency);
      else
        pinned_.splice(pinned_.begin(), pinned_, entry.recency);
    }

    void settle(bucket &entry, epose_bucket_limits limits, uint64_t unused)
    {
      entry.in_flight_bytes -= epose_response_reservation_bytes;
      entry.bytes.refund(unused, limits);
      if (entry.in_flight_bytes == 0)
        idle_.splice(idle_.begin(), pinned_, entry.recency);
    }

    bucket &host_bucket(const std::string &key, epose_bucket_limits request_limits,
        epose_bucket_limits byte_limits, std::chrono::steady_clock::time_point now)
    {
      auto entry = buckets_.find(key);
      if (entry == buckets_.end())
      {
        idle_.push_front(key);
        entry = buckets_.emplace(key, bucket{{request_limits.capacity, now},
            {byte_limits.capacity, now}, 0, idle_.begin()}).first;
      }
      return entry->second;
    }

    static std::pair<std::string, std::string> bucket_keys(const std::string &host)
    {
      const std::string host_key = bucket_key(host, 64);
      return {host_key, host_key == host ? std::string{} : bucket_key(host, 48)};
    }

    static std::string bucket_key(const std::string &host, size_t prefix_bits)
    {
      boost::system::error_code error;
      const auto address = boost::asio::ip::make_address_v6(host, error);
      if (error)
        return host;
      const auto bytes = address.to_bytes();
      return std::string("ipv6:") + std::to_string(prefix_bits) + ":"
          + std::string(bytes.begin(), bytes.begin() + prefix_bits / 8);
    }

    std::mutex mutex_;
    std::map<std::string, bucket> buckets_;
    std::list<std::string> idle_;
    std::list<std::string> pinned_;
    token_bucket global_requests_{epose_global_requests.capacity, {}};
    token_bucket global_bytes_{epose_global_bytes.capacity, {}};
    uint64_t global_in_flight_bytes_ = 0;
    std::set<std::string> logged_bans_;
    std::list<std::string> ban_order_;
  };
}
