// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "crypto/crypto.h"
#include "crypto/hash.h"

namespace cryptonote
{
  class epose_receipt_job_v2;

  uint64_t epose_receipt_start_offset_ms_v2(
      const crypto::public_key &local, const crypto::public_key &peer,
      uint64_t epoch, uint64_t round, uint64_t window_ms);

  // Per-peer attempt gate: a peer is not started again until this long after
  // the last start or harvest of an attempt to it.
  constexpr uint64_t EPOSE_V2_RECEIPT_PEER_GATE_MS = 30000;

  enum class epose_receipt_admission_v2
  {
    admit,
    stopped,
    slot_in_flight,
    peer_gate,
    start_jitter,
    capacity,
    launch_failed,
  };

  // Owns the in-flight jobs, the per-peer gate and the round-start time under
  // one mutex (idle thread: harvest, schedule; signal thread: stop). A job
  // leaves exactly once, via harvest or stop. launch must not block.
  template<class Payload, class Handle = epose_receipt_job_v2>
  class epose_receipt_scheduler_v2
  {
  public:
    using clock = std::chrono::steady_clock;

    struct job
    {
      crypto::hash slot{};
      crypto::public_key peer{};
      uint64_t deadline_height = 0;
      std::shared_ptr<Handle> handle;
      Payload payload{};
    };

    struct candidate
    {
      crypto::hash slot{};
      crypto::public_key peer{};
      uint64_t deadline_height = 0;
    };

    struct harvested
    {
      std::vector<job> ready;   // finished; wait() does not block
      std::vector<job> expired; // chain passed deadline_height; cancelled
    };

    struct schedule_result
    {
      size_t submitted = 0;
      bool aborted = false;
    };

    epose_receipt_scheduler_v2(size_t capacity, std::function<size_t()> executor_in_flight)
      : capacity_(capacity), executor_in_flight_(std::move(executor_in_flight)) {}

    epose_receipt_scheduler_v2(const epose_receipt_scheduler_v2 &) = delete;
    epose_receipt_scheduler_v2 &operator=(const epose_receipt_scheduler_v2 &) = delete;

    bool stopped() const
    {
      std::lock_guard<std::mutex> lock(mutex_);
      return stopped_;
    }

    size_t size() const
    {
      std::lock_guard<std::mutex> lock(mutex_);
      return jobs_.size();
    }

    bool in_flight(const crypto::hash &slot) const
    {
      std::lock_guard<std::mutex> lock(mutex_);
      return has_slot(slot);
    }

    // When this node first saw (epoch, round, anchor) active: the jitter base.
    clock::time_point observe_round(uint64_t epoch, uint64_t round,
        const crypto::hash &anchor, clock::time_point now)
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (round_epoch_ != epoch || round_ != round || round_anchor_ != anchor)
      {
        round_epoch_ = epoch;
        round_ = round;
        round_anchor_ = anchor;
        round_seen_ = now;
      }
      return round_seen_;
    }

    // Takes out finished and expired jobs (cancelling the latter), refreshes
    // the gate of each finished peer and drops stale gate entries.
    harvested harvest(uint64_t chain_height, clock::time_point now)
    {
      harvested out;
      std::lock_guard<std::mutex> lock(mutex_);
      for (auto it = jobs_.begin(); it != jobs_.end();)
      {
        if (it->handle->ready())
        {
          gate_[it->peer] = now;
          out.ready.push_back(std::move(*it));
          it = jobs_.erase(it);
        }
        else if (chain_height > it->deadline_height)
        {
          it->handle->cancel();
          out.expired.push_back(std::move(*it));
          it = jobs_.erase(it);
        }
        else
          ++it;
      }
      for (auto gate = gate_.begin(); gate != gate_.end();)
      {
        if (now - gate->second >= gate_period())
          gate = gate_.erase(gate);
        else
          ++gate;
      }
      return out;
    }

    // In order: stopped, slot in flight, per-peer gate, start jitter and
    // capacity (scheduler or executor jobs at N).
    epose_receipt_admission_v2 admit(const crypto::hash &slot,
        const crypto::public_key &local, const crypto::public_key &peer,
        uint64_t epoch, uint64_t round, uint64_t window_ms, clock::time_point now) const
    {
      std::lock_guard<std::mutex> lock(mutex_);
      return admit_locked(slot, local, peer, epoch, round, window_ms, now);
    }

    // Rechecks admission, then launches and takes the job under the lock so
    // stop() cannot miss it. No handle or a throw is launch_failed.
    template<class Launch>
    epose_receipt_admission_v2 submit(job &entry, Launch &&launch, clock::time_point now)
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (stopped_)
        return epose_receipt_admission_v2::stopped;
      if (has_slot(entry.slot))
        return epose_receipt_admission_v2::slot_in_flight;
      if (full())
        return epose_receipt_admission_v2::capacity;
      try
      {
        entry.handle = launch(entry);
      }
      catch (...)
      {
        entry.handle.reset();
      }
      if (!entry.handle)
        return epose_receipt_admission_v2::launch_failed;
      gate_[entry.peer] = now;
      jobs_.push_back(entry);
      return epose_receipt_admission_v2::admit;
    }

    // Admits or skips every candidate. prepare returning false aborts the
    // pass; skipped reports gate, jitter, slot and capacity skips; refused gets
    // a prepared job submit refused, and the pass ends after stopped or
    // launch_failed.
    template<class Prepare, class Launch, class Skipped, class Refused>
    schedule_result schedule(const std::vector<candidate> &candidates,
        const crypto::public_key &local, uint64_t epoch, uint64_t round,
        uint64_t window_ms, clock::time_point now, Prepare &&prepare,
        Launch &&launch, Skipped &&skipped, Refused &&refused)
    {
      schedule_result result;
      for (const auto &next : candidates)
      {
        const auto admission =
            admit(next.slot, local, next.peer, epoch, round, window_ms, now);
        if (admission == epose_receipt_admission_v2::stopped)
          break;
        if (admission != epose_receipt_admission_v2::admit)
        {
          skipped(next, admission);
          continue;
        }
        job entry{};
        entry.slot = next.slot;
        entry.peer = next.peer;
        entry.deadline_height = next.deadline_height;
        if (!prepare(next, entry.payload))
        {
          result.aborted = true;
          break;
        }
        const auto submitted = submit(entry, launch, now);
        if (submitted == epose_receipt_admission_v2::admit)
        {
          ++result.submitted;
          continue;
        }
        refused(entry, submitted);
        if (submitted == epose_receipt_admission_v2::stopped
            || submitted == epose_receipt_admission_v2::launch_failed)
        {
          result.aborted = submitted == epose_receipt_admission_v2::launch_failed;
          break;
        }
      }
      return result;
    }

    // finished were never cancelled and keep their real result; a cancelled
    // job may still have finished on its own first.
    struct stopped_jobs
    {
      std::vector<job> finished;
      std::vector<job> cancelled;

      size_t size() const { return finished.size() + cancelled.size(); }
      bool empty() const { return finished.empty() && cancelled.empty(); }
    };

    // Final. Cancels and joins its unfinished jobs and returns all it owns.
    stopped_jobs stop()
    {
      stopped_jobs taken;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
        for (auto &entry : jobs_)
          (entry.handle->ready() ? taken.finished : taken.cancelled)
              .push_back(std::move(entry));
        jobs_.clear();
      }
      for (const auto &entry : taken.cancelled)
        entry.handle->cancel();
      for (const auto &entry : taken.cancelled)
        entry.handle->wait();
      return taken;
    }

  private:
    static clock::duration gate_period()
    {
      return std::chrono::milliseconds(EPOSE_V2_RECEIPT_PEER_GATE_MS);
    }

    bool has_slot(const crypto::hash &slot) const
    {
      for (const auto &entry : jobs_)
        if (entry.slot == slot)
          return true;
      return false;
    }

    bool full() const
    {
      return jobs_.size() >= capacity_
          || (executor_in_flight_ && executor_in_flight_() >= capacity_);
    }

    epose_receipt_admission_v2 admit_locked(const crypto::hash &slot,
        const crypto::public_key &local, const crypto::public_key &peer,
        uint64_t epoch, uint64_t round, uint64_t window_ms, clock::time_point now) const
    {
      if (stopped_)
        return epose_receipt_admission_v2::stopped;
      if (has_slot(slot))
        return epose_receipt_admission_v2::slot_in_flight;
      const auto gate = gate_.find(peer);
      if (gate != gate_.end() && now - gate->second < gate_period())
        return epose_receipt_admission_v2::peer_gate;
      const uint64_t offset_ms =
          epose_receipt_start_offset_ms_v2(local, peer, epoch, round, window_ms);
      if (now < round_seen_ + std::chrono::milliseconds(offset_ms))
        return epose_receipt_admission_v2::start_jitter;
      if (full())
        return epose_receipt_admission_v2::capacity;
      return epose_receipt_admission_v2::admit;
    }

    const size_t capacity_;
    const std::function<size_t()> executor_in_flight_;
    mutable std::mutex mutex_;
    bool stopped_ = false;
    std::vector<job> jobs_;
    std::map<crypto::public_key, clock::time_point> gate_;
    uint64_t round_epoch_ = std::numeric_limits<uint64_t>::max();
    uint64_t round_ = std::numeric_limits<uint64_t>::max();
    crypto::hash round_anchor_{};
    clock::time_point round_seen_{};
  };
}
