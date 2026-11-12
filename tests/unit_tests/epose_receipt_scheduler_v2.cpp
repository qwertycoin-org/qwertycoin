// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#include <gtest/gtest.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "crypto/hash.h"
#include "cryptonote_core/epose_http_client.h"
#include "cryptonote_core/epose_receipt_scheduler_v2.h"

namespace
{
  using namespace cryptonote;
  using steady = std::chrono::steady_clock;
  using ms = std::chrono::milliseconds;
  using admission = epose_receipt_admission_v2;

  // A job handle the test finishes by hand.
  struct fake_job
  {
    mutable std::atomic<bool> done{false};
    mutable std::atomic<bool> cancelled{false};
    mutable std::atomic<unsigned> waits{0};
    bool ready() const { return done.load(); }
    void cancel() const
    {
      cancelled = true;
      done = true;
    }
    int wait() const
    {
      ++waits;
      return 0;
    }
  };

  using fake_scheduler = epose_receipt_scheduler_v2<int, fake_job>;

  crypto::public_key key(unsigned i)
  {
    const crypto::hash h = crypto::cn_fast_hash(&i, sizeof(i));
    crypto::public_key out{};
    static_assert(sizeof(out) == sizeof(h), "key size");
    memcpy(&out, &h, sizeof(out));
    return out;
  }

  crypto::hash slot(unsigned i)
  {
    const unsigned salted[2] = {i, 0x510750u};
    return crypto::cn_fast_hash(salted, sizeof(salted));
  }

  const crypto::public_key local = key(1000000);
  constexpr uint64_t test_epoch = 46;
  constexpr uint64_t test_round = 1;

  std::vector<fake_scheduler::candidate> candidates(unsigned first, unsigned count,
      uint64_t deadline_height = 100)
  {
    std::vector<fake_scheduler::candidate> out;
    for (unsigned i = first; i < first + count; ++i)
      out.push_back({slot(i), key(i), deadline_height});
    return out;
  }

  // Runs one schedule pass with fake handles and records what happened.
  struct pass
  {
    std::vector<std::shared_ptr<fake_job>> launched;
    std::vector<std::pair<crypto::public_key, admission>> skipped;
    std::vector<admission> refused;
    fake_scheduler::schedule_result result;
    size_t max_owned = 0;

    pass(fake_scheduler &scheduler, const std::vector<fake_scheduler::candidate> &due,
        steady::time_point now, uint64_t window_ms = 0)
    {
      result = scheduler.schedule(due, local, test_epoch, test_round, window_ms, now,
          [](const fake_scheduler::candidate &, int &payload) {
            payload = 7;
            return true;
          },
          [this, &scheduler](fake_scheduler::job &entry) {
            EXPECT_EQ(7, entry.payload);
            auto handle = std::make_shared<fake_job>();
            launched.push_back(handle);
            // Called under the scheduler lock, before the job is added.
            max_owned = std::max(max_owned, launched.size());
            return handle;
          },
          [this](const fake_scheduler::candidate &next, admission why) {
            skipped.emplace_back(next.peer, why);
          },
          [this](const fake_scheduler::job &, admission why) { refused.push_back(why); });
    }

    size_t skipped_as(admission why) const
    {
      return std::count_if(skipped.begin(), skipped.end(),
          [why](const std::pair<crypto::public_key, admission> &s) { return s.second == why; });
    }
  };
}

TEST(epose_receipt_scheduler_v2, submits_every_due_peer_in_one_pass)
{
  fake_scheduler scheduler(9, {});
  const auto now = steady::now();
  scheduler.observe_round(test_epoch, test_round, crypto::null_hash, now);
  pass tick(scheduler, candidates(0, 5), now);
  EXPECT_EQ(5u, tick.result.submitted);
  EXPECT_FALSE(tick.result.aborted);
  EXPECT_EQ(5u, tick.launched.size());
  EXPECT_EQ(5u, scheduler.size());
  EXPECT_TRUE(tick.skipped.empty());
  for (unsigned i = 0; i < 5; ++i)
    EXPECT_TRUE(scheduler.in_flight(slot(i)));
}

TEST(epose_receipt_scheduler_v2, capacity_is_reached_in_one_pass_and_never_exceeded)
{
  constexpr size_t N = EPOSE_V2_RECEIPT_PARALLEL_JOBS;
  fake_scheduler scheduler(N, {});
  auto now = steady::now();
  scheduler.observe_round(test_epoch, test_round, crypto::null_hash, now);
  pass first(scheduler, candidates(0, 2 * N + 3), now);
  EXPECT_EQ(N, first.result.submitted);
  ASSERT_EQ(N, first.launched.size());
  EXPECT_EQ(N, first.max_owned);
  EXPECT_EQ(N, scheduler.size());
  // Every later candidate is skipped, not dropped from the pass.
  EXPECT_EQ(N + 3, first.skipped_as(admission::capacity));
  EXPECT_TRUE(first.refused.empty());

  // Full: a second pass starts nothing.
  pass full(scheduler, candidates(100, 4), now);
  EXPECT_EQ(0u, full.result.submitted);
  EXPECT_EQ(4u, full.skipped_as(admission::capacity));

  // Two finish; exactly two more start.
  first.launched[0]->done = true;
  first.launched[1]->done = true;
  now += ms(10);
  const auto harvested = scheduler.harvest(1, now);
  EXPECT_EQ(2u, harvested.ready.size());
  EXPECT_TRUE(harvested.expired.empty());
  pass refill(scheduler, candidates(200, 5), now);
  EXPECT_EQ(2u, refill.result.submitted);
  EXPECT_EQ(N, scheduler.size());

  // The executor's own in-flight count also bounds admission.
  std::atomic<size_t> executor{N};
  fake_scheduler busy(N, [&executor] { return executor.load(); });
  busy.observe_round(test_epoch, test_round, crypto::null_hash, now);
  pass blocked(busy, candidates(300, 3), now);
  EXPECT_EQ(0u, blocked.result.submitted);
  EXPECT_EQ(3u, blocked.skipped_as(admission::capacity));
  executor = N - 1;
  pass one(busy, candidates(300, 3), now);
  EXPECT_EQ(3u, one.result.submitted); // the probe is the executor's count, not ours
}

TEST(epose_receipt_scheduler_v2, per_peer_gate_holds_for_30_seconds_from_start_and_harvest)
{
  fake_scheduler scheduler(9, {});
  const auto t0 = steady::now();
  scheduler.observe_round(test_epoch, test_round, crypto::null_hash, t0);
  pass start(scheduler, {{slot(1), key(1), 100}}, t0);
  ASSERT_EQ(1u, start.result.submitted);

  // Another slot for the same peer (for example the next round).
  EXPECT_EQ(admission::peer_gate,
      scheduler.admit(slot(2), local, key(1), test_epoch, test_round, 0, t0 + ms(29999)));
  EXPECT_EQ(admission::admit,
      scheduler.admit(slot(2), local, key(1), test_epoch, test_round, 0, t0 + ms(30000)));
  EXPECT_EQ(admission::admit, scheduler.admit(slot(3), local, key(3), test_epoch, test_round, 0, t0));

  // Harvest restarts the gate at completion time.
  const auto t1 = t0 + ms(10000);
  start.launched[0]->done = true;
  ASSERT_EQ(1u, scheduler.harvest(1, t1).ready.size());
  EXPECT_EQ(admission::peer_gate,
      scheduler.admit(slot(2), local, key(1), test_epoch, test_round, 0, t0 + ms(30000)));
  EXPECT_EQ(admission::peer_gate,
      scheduler.admit(slot(2), local, key(1), test_epoch, test_round, 0, t1 + ms(29999)));
  EXPECT_EQ(admission::admit,
      scheduler.admit(slot(2), local, key(1), test_epoch, test_round, 0, t1 + ms(30000)));

  // The pass reports the gate skip under its own label.
  pass gated(scheduler, {{slot(2), key(1), 100}}, t1 + ms(1));
  EXPECT_EQ(0u, gated.result.submitted);
  EXPECT_EQ(1u, gated.skipped_as(admission::peer_gate));
  // Expired gate entries are pruned by harvest and do not block.
  scheduler.harvest(1, t1 + ms(30000));
  pass open(scheduler, {{slot(2), key(1), 100}}, t1 + ms(30000));
  EXPECT_EQ(1u, open.result.submitted);
}

TEST(epose_receipt_scheduler_v2, jitter_offsets_are_applied_from_first_observation)
{
  constexpr uint64_t window_ms = 1000;
  fake_scheduler scheduler(64, {});
  const auto seen = steady::now();
  EXPECT_EQ(seen, scheduler.observe_round(test_epoch, test_round, crypto::null_hash, seen));
  // Re-observing the same round keeps the first time; a new anchor resets it.
  EXPECT_EQ(seen, scheduler.observe_round(test_epoch, test_round, crypto::null_hash, seen + ms(500)));

  std::vector<uint64_t> offsets;
  unsigned positive = 0;
  for (unsigned i = 0; i < 32; ++i)
  {
    const uint64_t offset =
        epose_receipt_start_offset_ms_v2(local, key(i), test_epoch, test_round, window_ms);
    ASSERT_LT(offset, window_ms);
    offsets.push_back(offset);
    if (offset == 0)
      continue;
    ++positive;
    EXPECT_EQ(admission::start_jitter, scheduler.admit(slot(i), local, key(i), test_epoch, test_round,
        window_ms, seen + ms(offset - 1))) << i;
    EXPECT_EQ(admission::admit, scheduler.admit(slot(i), local, key(i), test_epoch, test_round,
        window_ms, seen + ms(offset))) << i;
  }
  ASSERT_GT(positive, 20u);

  // One pass at seen + 500 ms starts exactly the peers whose offset has
  // elapsed and skips the rest as start_jitter.
  const auto at = seen + ms(500);
  pass tick(scheduler, candidates(0, 32), at, window_ms);
  const size_t due = std::count_if(offsets.begin(), offsets.end(),
      [](uint64_t offset) { return offset <= 500; });
  ASSERT_GT(due, 0u);
  ASSERT_LT(due, offsets.size());
  EXPECT_EQ(due, tick.result.submitted);
  EXPECT_EQ(offsets.size() - due, tick.skipped_as(admission::start_jitter));
  for (const auto &skip : tick.skipped)
  {
    unsigned index = 0;
    while (index < 32 && !(key(index) == skip.first))
      ++index;
    ASSERT_LT(index, 32u);
    EXPECT_GT(offsets[index], 500u);
  }

  // A new anchor is a new round start.
  const auto later = seen + ms(5000);
  EXPECT_EQ(later, scheduler.observe_round(test_epoch, test_round, slot(999), later));
  unsigned i = 0;
  while (offsets[i] == 0)
    ++i;
  EXPECT_EQ(admission::start_jitter, scheduler.admit(slot(500 + i), local, key(i), test_epoch, test_round,
      window_ms, later + ms(offsets[i] - 1)));
}

TEST(epose_receipt_scheduler_v2, chain_deadline_expires_running_jobs)
{
  fake_scheduler scheduler(9, {});
  const auto now = steady::now();
  scheduler.observe_round(test_epoch, test_round, crypto::null_hash, now);
  pass tick(scheduler, {{slot(1), key(1), 10}, {slot(2), key(2), 12}}, now);
  ASSERT_EQ(2u, tick.result.submitted);
  auto harvested = scheduler.harvest(10, now);
  EXPECT_TRUE(harvested.ready.empty());
  EXPECT_TRUE(harvested.expired.empty());
  EXPECT_EQ(2u, scheduler.size());
  harvested = scheduler.harvest(11, now);
  ASSERT_EQ(1u, harvested.expired.size());
  EXPECT_EQ(slot(1), harvested.expired[0].slot);
  EXPECT_TRUE(tick.launched[0]->cancelled.load());
  EXPECT_FALSE(tick.launched[1]->cancelled.load());
  EXPECT_FALSE(scheduler.in_flight(slot(1)));
  EXPECT_TRUE(scheduler.in_flight(slot(2)));
  // A finished job is harvested as ready even past its deadline height.
  tick.launched[1]->done = true;
  harvested = scheduler.harvest(50, now);
  EXPECT_EQ(1u, harvested.ready.size());
  EXPECT_TRUE(harvested.expired.empty());
  EXPECT_FALSE(tick.launched[1]->cancelled.load());
  EXPECT_EQ(0u, scheduler.size());
}

TEST(epose_receipt_scheduler_v2, slot_in_flight_prepare_abort_and_launch_failure)
{
  fake_scheduler scheduler(9, {});
  const auto now = steady::now();
  scheduler.observe_round(test_epoch, test_round, crypto::null_hash, now);
  pass first(scheduler, {{slot(1), key(1), 100}}, now);
  ASSERT_EQ(1u, first.result.submitted);
  pass again(scheduler, {{slot(1), key(2), 100}}, now);
  EXPECT_EQ(1u, again.skipped_as(admission::slot_in_flight));

  unsigned prepared = 0, launched = 0;
  auto aborted = scheduler.schedule(candidates(10, 3), local, test_epoch, test_round, 0, now,
      [&prepared](const fake_scheduler::candidate &, int &) { return ++prepared < 2; },
      [&launched](fake_scheduler::job &) { ++launched; return std::make_shared<fake_job>(); },
      [](const fake_scheduler::candidate &, admission) {},
      [](const fake_scheduler::job &, admission) { ADD_FAILURE(); });
  EXPECT_TRUE(aborted.aborted);
  EXPECT_EQ(1u, aborted.submitted);
  EXPECT_EQ(2u, prepared);
  EXPECT_EQ(1u, launched);

  std::vector<admission> refused;
  auto failed = scheduler.schedule(candidates(20, 3), local, test_epoch, test_round, 0, now,
      [](const fake_scheduler::candidate &, int &) { return true; },
      [](fake_scheduler::job &) -> std::shared_ptr<fake_job> { throw std::runtime_error("launch"); },
      [](const fake_scheduler::candidate &, admission) {},
      [&refused](const fake_scheduler::job &, admission why) { refused.push_back(why); });
  EXPECT_TRUE(failed.aborted);
  EXPECT_EQ(0u, failed.submitted);
  ASSERT_EQ(1u, refused.size());
  EXPECT_EQ(admission::launch_failed, refused[0]);
  EXPECT_FALSE(scheduler.in_flight(slot(20)));
}

TEST(epose_receipt_scheduler_v2, stop_cancels_and_joins_running_jobs_and_is_final)
{
  constexpr size_t N = EPOSE_V2_RECEIPT_PARALLEL_JOBS;
  fake_scheduler scheduler(N, {});
  const auto now = steady::now();
  scheduler.observe_round(test_epoch, test_round, crypto::null_hash, now);
  pass tick(scheduler, candidates(0, N), now);
  ASSERT_EQ(N, tick.result.submitted);
  const auto stopped = scheduler.stop();
  ASSERT_EQ(N, stopped.size());
  for (const auto &job : tick.launched)
  {
    EXPECT_TRUE(job->cancelled.load());
    EXPECT_EQ(1u, job->waits.load());
  }
  EXPECT_TRUE(scheduler.stopped());
  EXPECT_EQ(0u, scheduler.size());
  EXPECT_TRUE(scheduler.harvest(1000, now).expired.empty());

  pass after(scheduler, candidates(100, 3), now);
  EXPECT_EQ(0u, after.result.submitted);
  EXPECT_TRUE(after.launched.empty());
  EXPECT_EQ(admission::stopped,
      scheduler.admit(slot(100), local, key(100), test_epoch, test_round, 0, now));
  fake_scheduler::job late{};
  late.slot = slot(101);
  late.peer = key(101);
  bool called = false;
  EXPECT_EQ(admission::stopped, scheduler.submit(late, [&called](fake_scheduler::job &) {
    called = true;
    return std::make_shared<fake_job>();
  }, now));
  EXPECT_FALSE(called);
  EXPECT_TRUE(scheduler.stop().empty());
}

// A job that already finished when stop() took it is never cancelled and
// keeps its real result.
TEST(epose_receipt_scheduler_v2, stop_keeps_finished_jobs_apart)
{
  fake_scheduler scheduler(9, {});
  const auto now = steady::now();
  scheduler.observe_round(test_epoch, test_round, crypto::null_hash, now);
  pass tick(scheduler, candidates(0, 4), now);
  ASSERT_EQ(4u, tick.result.submitted);
  tick.launched[1]->done = true;
  tick.launched[3]->done = true;
  const auto stopped = scheduler.stop();
  ASSERT_EQ(2u, stopped.finished.size());
  ASSERT_EQ(2u, stopped.cancelled.size());
  EXPECT_EQ(4u, stopped.size());
  EXPECT_EQ(slot(1), stopped.finished[0].slot);
  EXPECT_EQ(slot(3), stopped.finished[1].slot);
  EXPECT_EQ(slot(0), stopped.cancelled[0].slot);
  EXPECT_EQ(slot(2), stopped.cancelled[1].slot);
  for (unsigned i : {1u, 3u})
  {
    EXPECT_FALSE(tick.launched[i]->cancelled.load());
    EXPECT_EQ(0u, tick.launched[i]->waits.load());
  }
  for (unsigned i : {0u, 2u})
  {
    EXPECT_TRUE(tick.launched[i]->cancelled.load());
    EXPECT_EQ(1u, tick.launched[i]->waits.load());
  }
  EXPECT_EQ(0u, scheduler.size());
}

namespace
{
  epose_receipt_request_v2 literal_request(unsigned i, steady::time_point round_deadline)
  {
    epose_receipt_request_v2 request{};
    request.challenge.epoch = test_epoch;
    request.challenge.round = test_round;
    request.challenge.nonce = slot(i);
    request.endpoint_known = true;
    request.bind_to_challenge = false;
    // Public literals: no resolver involved.
    request.endpoint.host = "8.8." + std::to_string((i / 250) % 250) + "." + std::to_string(i % 250 + 1);
    request.endpoint.port = 8198;
    request.deadlines.round_deadline = round_deadline;
    return request;
  }
}

// stop() on one thread while another thread harvests and submits, with
// real jobs in flight on the real client. Run under TSan.
TEST(epose_receipt_scheduler_v2, stop_races_submitting_scheduler)
{
  size_t stopped_with_jobs = 0;
  for (unsigned iteration = 0; iteration < 20; ++iteration)
  {
    SCOPED_TRACE(iteration);
    std::atomic<unsigned> connects{0};
    epose_network_v2 network;
    // Half the peers hang until cancelled, half refuse at once.
    network.connect = [&connects](const boost::asio::ip::tcp::endpoint &endpoint,
        boost::asio::io_context &, steady::time_point,
        epose_network_v2::connect_handler complete) -> epose_network_v2::cancel_operation {
      ++connects;
      if (endpoint.address().to_v4().to_bytes()[3] % 2 == 0)
      {
        const auto held = std::make_shared<epose_network_v2::connect_handler>(std::move(complete));
        return [held] { *held = {}; };
      }
      throw boost::system::system_error(boost::asio::error::connection_refused);
    };
    epose_receipt_client_v2 client(network);
    using scheduler_t = epose_receipt_scheduler_v2<unsigned>;
    scheduler_t scheduler(client.capacity(), [&client] { return client.in_flight(); });

    std::atomic<bool> stop_returned{false};
    std::atomic<unsigned> launched{0}, launched_after_stop{0}, harvested{0}, expired{0};
    std::atomic<unsigned> passes{0};
    unsigned request_index = iteration * 100000; // producer thread only
    std::thread producer([&] {
      unsigned next = iteration * 100000;
      uint64_t height = 0;
      while (!scheduler.stopped())
      {
        const auto now = steady::now();
        scheduler.observe_round(test_epoch, test_round, crypto::null_hash, now);
        // Each pass advances the height: every 4th job expires after 2 passes.
        const auto taken = scheduler.harvest(++height, now);
        for (const auto &job : taken.ready)
        {
          job.handle->wait();
          ++harvested;
        }
        expired += static_cast<unsigned>(taken.expired.size());
        std::vector<scheduler_t::candidate> due;
        for (unsigned i = 0; i < 12; ++i, ++next)
          due.push_back({slot(next), key(next), height + (i % 4 == 0 ? 2 : 1000000)});
        scheduler.schedule(due, local, test_epoch, test_round, 0, now,
            [](const scheduler_t::candidate &, unsigned &payload) {
              payload = 1;
              return true;
            },
            [&](scheduler_t::job &entry) {
              if (stop_returned.load())
                ++launched_after_stop;
              ++launched;
              EXPECT_EQ(1u, entry.payload);
              return std::make_shared<epose_receipt_job_v2>(client.submit(
                  literal_request(request_index++, now + std::chrono::seconds(30))));
            },
            [](const scheduler_t::candidate &, admission) {},
            [](const scheduler_t::job &, admission) {});
        ++passes;
      }
    });
    // Let the producer fill the executor and churn, then stop mid-pass.
    const auto spin_until = steady::now() + std::chrono::seconds(2);
    while (passes.load() < 3 + iteration % 5 && steady::now() < spin_until)
      std::this_thread::yield();
    const auto stopped = scheduler.stop();
    stop_returned = true;
    producer.join();

    EXPECT_EQ(0u, launched_after_stop.load());
    if (!stopped.empty())
      ++stopped_with_jobs;
    for (const auto &job : stopped.finished)
    {
      // Finished before stop() took it: never cancelled, real result kept.
      ASSERT_TRUE(job.handle->ready());
      EXPECT_EQ(qwertycoin::epose::receipt_failure_reason_v2::connection_refused,
          job.handle->wait().result.reason);
    }
    for (const auto &job : stopped.cancelled)
    {
      // stop() joined every job it cancelled: each is finished, and cancelled
      // unless it had completed on its own before the cancel landed.
      ASSERT_TRUE(job.handle->ready());
      const auto reason = job.handle->wait().result.reason;
      EXPECT_TRUE(reason == qwertycoin::epose::receipt_failure_reason_v2::cancelled
          || reason == qwertycoin::epose::receipt_failure_reason_v2::connection_refused)
          << qwertycoin::epose::to_string(reason);
    }
    // Every launched job was taken exactly once: by harvest, expiry or stop.
    EXPECT_EQ(launched.load(), harvested.load() + expired.load() + stopped.size());
    EXPECT_EQ(0u, scheduler.size());
    EXPECT_GT(launched.load(), 0u);
    // Expired jobs were cancelled asynchronously; the executor drains.
    const auto drain_until = steady::now() + std::chrono::seconds(5);
    while (client.in_flight() != 0 && steady::now() < drain_until)
      std::this_thread::sleep_for(ms(1));
    EXPECT_EQ(0u, client.in_flight());
  }
  // Hanging peers keep jobs in flight, so stop() has work to cancel.
  EXPECT_GE(stopped_with_jobs, 15u);
}
