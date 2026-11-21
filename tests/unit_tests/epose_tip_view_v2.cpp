// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

// The per-tip EPoSE view. A fake chain with branches stands in for
// Blockchain: it adds, pops and reorganizes under checked_chain_lock_v2 and
// publishes at every final tip.

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "crypto/hash.h"
#include "epose/relay_pool_v2.h"
#include "epose/shared_chunk_vector_v2.h"
#include "epose/tip_view_v2.h"

namespace
{
  using namespace qwertycoin::epose;

  crypto::hash hash_text(const std::string &text)
  {
    return crypto::cn_fast_hash(text.data(), text.size());
  }

  constexpr size_t branch_count = 4;
  constexpr uint64_t max_height = 96;
  // Branch b > 0 shares the blocks of branch 0 below its fork height.
  constexpr uint64_t fork_heights[branch_count] = {0, 30, 41, 58};

  consensus_parameters_v2 test_parameters()
  {
    consensus_parameters_v2 parameters{};
    parameters.nettype = cryptonote::TESTNET;
    parameters.genesis_hash = hash_text("tip-view-genesis");
    parameters.parameter_set_hash = hash_text("tip-view-parameters");
    parameters.timing = {0, 20, 5};
    parameters.admission = {admission_work_algorithm_v2::randomx, 1};
    parameters.committee = {1, 1, 2, 1, 1, {0, 8}};
    return parameters;
  }

  crypto::hash block_id(size_t branch, uint64_t height)
  {
    if (height < fork_heights[branch])
      branch = 0;
    return hash_text("block:" + std::to_string(branch) + ":" + std::to_string(height));
  }

  // The tip hash stands in for the parameter set hash, so every tip has its
  // own state hash.
  std::shared_ptr<const semantic_state_v2> tip_state(const crypto::hash &tip)
  {
    const consensus_parameters_v2 parameters = test_parameters();
    return std::make_shared<const semantic_state_v2>(
        parameters.nettype, parameters.genesis_hash, tip,
        parameters.timing, parameters.admission, parameters.committee);
  }

  // Looks up heights in `blocks`, which must outlive the returned function.
  auto block_hash_in(const std::vector<crypto::hash> &blocks)
  {
    return [&blocks](uint64_t height, crypto::hash &hash) {
      hash = height < blocks.size() ? blocks[height] : crypto::null_hash;
      return hash != crypto::null_hash;
    };
  }

  struct expected_tip
  {
    size_t branch = 0;
    uint64_t top = 0;
    crypto::hash state_hash{};
  };

  // Every tip the writer can reach, by tip hash.
  std::map<crypto::hash, expected_tip> expected_tips()
  {
    std::map<crypto::hash, expected_tip> out;
    for (size_t branch = 0; branch < branch_count; ++branch)
      for (uint64_t top = 0; top < max_height; ++top)
      {
        const crypto::hash tip = block_id(branch, top);
        if (out.count(tip) != 0)
          continue;
        out[tip] = {branch, top, tip_state(tip)->state_hash()};
      }
    return out;
  }

  // Mirrors Blockchain: mutators take the chain lock and publish at the
  // final tip.
  class fake_chain
  {
  public:
    fake_chain()
    {
      CRITICAL_REGION_LOCAL(lock_);
      blocks_.push_back(block_id(0, 0));
      publish();
    }

    void add()
    {
      CRITICAL_REGION_LOCAL(lock_);
      if (blocks_.size() >= max_height)
        return;
      blocks_.push_back(block_id(branch_, blocks_.size()));
      publish();
    }

    void pop()
    {
      CRITICAL_REGION_LOCAL(lock_);
      if (blocks_.size() <= 1)
        return;
      blocks_.pop_back();
      publish();
    }

    // Each pop and each add of the reorg is one final tip, as in
    // switch_to_alternative_blockchain.
    void reorg(size_t branch, uint64_t target_height)
    {
      CRITICAL_REGION_LOCAL(lock_);
      uint64_t split = std::min<uint64_t>(blocks_.size(), max_height);
      for (uint64_t height = 1; height < split; ++height)
        if (blocks_[height] != block_id(branch, height))
        {
          split = height;
          break;
        }
      while (blocks_.size() > split)
      {
        blocks_.pop_back();
        publish();
      }
      branch_ = branch;
      while (blocks_.size() < std::min<uint64_t>(target_height, max_height))
      {
        blocks_.push_back(block_id(branch_, blocks_.size()));
        publish();
      }
    }

    void hold_lock(std::chrono::milliseconds duration)
    {
      CRITICAL_REGION_LOCAL(lock_);
      std::this_thread::sleep_for(duration);
      blocks_.push_back(block_id(branch_, blocks_.size()));
      publish();
    }

    checked_chain_lock_v2 &lock() { return lock_; }
    std::shared_ptr<const epose_tip_view> view() const { return published_.load(); }

  private:
    void publish()
    {
      const crypto::hash tip = blocks_.back();
      const consensus_parameters_v2 parameters = test_parameters();
      published_.publish(build_epose_tip_view(
          tip, blocks_.size(), true, &parameters, tip_state(tip), block_hash_in(blocks_)));
    }

    mutable checked_chain_lock_v2 lock_;
    std::vector<crypto::hash> blocks_;
    size_t branch_ = 0;
    epose_tip_view_publisher published_;
  };

  // Every field of the view must belong to the tip it names.
  bool view_is_consistent(
      const epose_tip_view &view,
      const std::map<crypto::hash, expected_tip> &tips)
  {
    const auto found = tips.find(view.tip_hash);
    if (found == tips.end())
      return false;
    const expected_tip &tip = found->second;
    const consensus_parameters_v2 parameters = test_parameters();
    const uint64_t epoch = tip.top / parameters.timing.epoch_length;
    if (!view.v2 || !view.state || view.height != tip.top + 1
        || view.epoch != epoch
        || view.state_hash != tip.state_hash
        || view.state->state_hash() != tip.state_hash
        || view.receipt_count != view.state->membership().receipt_count()
        || view.descriptors.size()
            != view.state->lifecycle().descriptors_for_epoch(epoch).size())
      return false;

    std::map<uint64_t, crypto::hash> anchors;
    bool round_started = false;
    uint64_t round = 0;
    for (uint64_t value = epoch == 0 ? 0 : epoch - 1; value <= epoch + 1; ++value)
      for (uint64_t index = 0; index < 2; ++index)
      {
        uint64_t anchor_height = 0;
        if (!view.round_anchor_height(value, index, anchor_height)
            || anchor_height > tip.top)
          continue;
        anchors[anchor_height] = block_id(tip.branch, anchor_height);
        if (value == epoch)
        {
          round_started = true;
          round = index;
        }
      }
    if (view.anchors != anchors || view.round_started != round_started
        || view.round != round)
      return false;
    for (const auto &anchor : view.anchors)
    {
      crypto::hash hash{};
      if (!view.anchor_hash(anchor.first, hash) || hash != anchor.second)
        return false;
    }
    return true;
  }

  uint64_t percentile(std::vector<uint64_t> values, double fraction)
  {
    if (values.empty())
      return 0;
    std::sort(values.begin(), values.end());
    const size_t index = std::min(values.size() - 1,
        static_cast<size_t>(fraction * static_cast<double>(values.size())));
    return values[index];
  }
}

TEST(epose_tip_view_v2, view_fields_and_queries)
{
  const consensus_parameters_v2 parameters = test_parameters();
  std::vector<crypto::hash> blocks;
  for (uint64_t height = 0; height < 47; ++height)
    blocks.push_back(block_id(0, height));
  const auto view = build_epose_tip_view(
      blocks.back(), blocks.size(), true, &parameters, tip_state(blocks.back()),
      block_hash_in(blocks));

  // Height 47, top 46: epoch 2 = [40, 59]. Anchors at or below 46: epoch 1
  // (15, 28), epoch 2 (35; 48 is above the tip). Epoch 3 starts at 55.
  ASSERT_TRUE(view->v2);
  EXPECT_EQ(view->epoch, 2u);
  EXPECT_EQ(view->epoch_start_height(2), 40u);
  EXPECT_EQ(view->epoch_end_height(2), 59u);
  EXPECT_TRUE(view->round_started);
  EXPECT_EQ(view->round, 0u);
  EXPECT_EQ(view->anchors.size(), 3u);
  crypto::hash hash{};
  EXPECT_TRUE(view->anchor_hash(15, hash));
  EXPECT_EQ(hash, blocks[15]);
  EXPECT_TRUE(view->anchor_hash(35, hash));
  EXPECT_EQ(hash, blocks[35]);
  EXPECT_FALSE(view->anchor_hash(48, hash));
  EXPECT_EQ(hash, crypto::null_hash);
  uint64_t source = 0;
  EXPECT_TRUE(view->reward_source_epoch(47, source));
  EXPECT_EQ(source, 1u);
  EXPECT_TRUE(view->reward_source_epoch(10, source));
  EXPECT_EQ(source, 0u);
  EXPECT_TRUE(view->qualified_service_nodes(2).empty());
  EXPECT_EQ(view->snapshot(2), nullptr);
  std::vector<size_t> coverage;
  EXPECT_TRUE(view->receipt_coverage(2, crypto::public_key{}, coverage));

  // No v2 coordinator: the legacy epoch formula and empty v2 queries, as the
  // Blockchain getters return without m_epose_v2.
  const auto legacy = build_epose_tip_view(
      blocks.back(), blocks.size(), false, nullptr, nullptr,
      [](uint64_t, crypto::hash &) { return false; });
  EXPECT_FALSE(legacy->v2);
  EXPECT_EQ(legacy->state, nullptr);
  EXPECT_TRUE(legacy->descriptors_for_epoch(legacy->epoch).empty());
  EXPECT_FALSE(legacy->reward_source_epoch(47, source));
  EXPECT_FALSE(legacy->receipt_coverage(0, crypto::public_key{}, coverage));
}

TEST(epose_tip_view_v2, concurrent_readers_see_consistent_tips)
{
  const std::map<crypto::hash, expected_tip> tips = expected_tips();
  fake_chain chain;
  const uint64_t violations_before = epose_view_reader_lock_violations();

  std::atomic<bool> stop{false};
  std::atomic<uint64_t> reads{0};
  std::atomic<uint64_t> inconsistent{0};
  std::atomic<uint64_t> tips_seen{0};
  std::vector<std::thread> readers;
  for (size_t index = 0; index < 4; ++index)
    readers.emplace_back([&]() {
      const epose_view_reader_scope reader;
      crypto::hash last{};
      while (!stop.load(std::memory_order_acquire))
      {
        const auto view = chain.view();
        if (!view || !view_is_consistent(*view, tips))
          inconsistent.fetch_add(1, std::memory_order_relaxed);
        else if (view->tip_hash != last)
        {
          last = view->tip_hash;
          tips_seen.fetch_add(1, std::memory_order_relaxed);
        }
        reads.fetch_add(1, std::memory_order_relaxed);
      }
    });

  std::mt19937 random(7);
  uint64_t adds = 0, pops = 0, reorgs = 0;
  for (size_t step = 0; step < 3000; ++step)
  {
    const uint32_t action = random() % 10;
    if (action < 6)
    {
      chain.add();
      ++adds;
    }
    else if (action < 9)
    {
      chain.pop();
      ++pops;
    }
    else
    {
      chain.reorg(random() % branch_count, 20 + random() % (max_height - 20));
      ++reorgs;
    }
  }
  while (reads.load() < 1000)
    std::this_thread::yield();
  stop.store(true, std::memory_order_release);
  for (std::thread &reader : readers)
    reader.join();

  std::cout << "[tip-view] adds=" << adds << " pops=" << pops
            << " reorgs=" << reorgs << " reads=" << reads.load()
            << " tips_seen=" << tips_seen.load() << std::endl;
  EXPECT_EQ(inconsistent.load(), 0u);
  EXPECT_GT(tips_seen.load(), 1u);
  EXPECT_GT(reorgs, 0u);
  EXPECT_EQ(epose_view_reader_lock_violations(), violations_before);
}

TEST(epose_tip_view_v2, reader_scope_flags_chain_lock)
{
  checked_chain_lock_v2 lock;
  const uint64_t before = epose_view_reader_lock_violations();
  EXPECT_FALSE(epose_view_reader_scope::active());
  {
    CRITICAL_REGION_LOCAL(lock);
  }
  EXPECT_EQ(epose_view_reader_lock_violations(), before);
  {
    const epose_view_reader_scope reader;
    EXPECT_TRUE(epose_view_reader_scope::active());
    {
      const epose_view_reader_scope nested;
      EXPECT_TRUE(epose_view_reader_scope::active());
    }
    EXPECT_TRUE(epose_view_reader_scope::active());
#ifdef NDEBUG
    {
      CRITICAL_REGION_LOCAL(lock);
    }
    EXPECT_TRUE(lock.tryLock());
    lock.unlock();
    EXPECT_EQ(epose_view_reader_lock_violations(), before + 2);
#else
    EXPECT_DEATH({ CRITICAL_REGION_LOCAL(lock); }, "");
#endif
  }
  EXPECT_FALSE(epose_view_reader_scope::active());

  // The scope is per thread: another thread may take the lock.
  const uint64_t after_scope = epose_view_reader_lock_violations();
  {
    const epose_view_reader_scope reader;
    std::thread other([&lock]() { CRITICAL_REGION_LOCAL(lock); });
    other.join();
  }
  EXPECT_EQ(epose_view_reader_lock_violations(), after_scope);
}

TEST(epose_tip_view_v2, reader_latency_while_chain_locked)
{
  fake_chain chain;
  for (size_t index = 0; index < 50; ++index)
    chain.add();
  const uint64_t violations_before = epose_view_reader_lock_violations();

  std::atomic<bool> holding{false};
  std::atomic<bool> done{false};
  std::thread writer([&]() {
    chain.lock().lock();
    holding.store(true, std::memory_order_release);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    holding.store(false, std::memory_order_release);
    chain.lock().unlock();
    done.store(true, std::memory_order_release);
  });
  while (!holding.load(std::memory_order_acquire))
    std::this_thread::yield();

  // What get_epose_info, get_service_nodes and get_epose_epoch read.
  std::vector<std::vector<uint64_t>> samples(4);
  std::vector<std::thread> readers;
  for (size_t index = 0; index < samples.size(); ++index)
    readers.emplace_back([&chain, &holding, &samples, index]() {
      const epose_view_reader_scope reader;
      while (holding.load(std::memory_order_acquire))
      {
        const auto start = std::chrono::steady_clock::now();
        const auto view = chain.view();
        uint64_t sink = view->epoch_start_height(view->epoch)
            + view->epoch_end_height(view->epoch)
            + view->descriptors_for_epoch(view->epoch).size()
            + view->qualified_service_nodes(view->epoch).size()
            + view->receipt_count;
        crypto::hash anchor{};
        uint64_t anchor_height = 0;
        if (view->round_anchor_height(view->epoch, view->round, anchor_height))
          sink += view->anchor_hash(anchor_height, anchor) ? 1 : 0;
        const auto elapsed = std::chrono::steady_clock::now() - start;
        samples[index].push_back(static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count())
            + (sink & 0));
        std::this_thread::sleep_for(std::chrono::microseconds(100));
      }
    });
  for (std::thread &reader : readers)
    reader.join();
  writer.join();

  std::vector<uint64_t> all;
  for (const auto &values : samples)
    all.insert(all.end(), values.begin(), values.end());
  const uint64_t p50 = percentile(all, 0.50);
  const uint64_t p99 = percentile(all, 0.99);
  const uint64_t max = all.empty() ? 0 : *std::max_element(all.begin(), all.end());
  std::cout << "[tip-view-latency] lock_hold_ms=500 reads=" << all.size()
            << " p50_us=" << p50 / 1000.0 << " p99_us=" << p99 / 1000.0
            << " max_us=" << max / 1000.0 << std::endl;
  EXPECT_TRUE(done.load());
  EXPECT_GT(all.size(), 100u);
  EXPECT_LT(p99, 1000000u);
  EXPECT_EQ(epose_view_reader_lock_violations(), violations_before);
}

TEST(epose_tip_view_v2, relay_pool_diagnostics_published)
{
  envelope_limits_v2 envelope_limits{};
  envelope_limits.max_envelope_bytes = 2048;
  envelope_limits.max_records = 4;
  envelope_limits.max_record_payload_bytes = 512;
  envelope_limits.max_signature_verifications = 8;
  envelope_limits.max_admission_verifications = 4;
  envelope_limits.supported_record_versions = {0, 1, 1, 1, 1, 1};
  const consensus_parameters_v2 parameters = test_parameters();
  relay_record_pool_v2 pool(parameters.timing, parameters.committee,
      envelope_limits,
      relay_queue_limits_v2{1024, 1 << 20, 64, 896, 1 << 16, 917504},
      relay_template_limits_v2{32, 32768, 8, 24, 8192, 24576});
  ASSERT_TRUE(pool.valid());
  EXPECT_EQ(pool.published_diagnostics().expired_round_receipts, 0u);

  constexpr uint64_t rejections = 20000;
  std::atomic<bool> stop{false};
  std::atomic<uint64_t> regressions{0};
  std::thread reader([&]() {
    uint64_t last = 0;
    while (!stop.load(std::memory_order_acquire))
    {
      const relay_pool_diagnostics_v2 published = pool.published_diagnostics();
      if (published.expired_round_receipts < last
          || (published.expired_round_receipts != published.capacity_rejections
              && published.expired_round_receipts != published.capacity_rejections + 1))
        regressions.fetch_add(1, std::memory_order_relaxed);
      last = published.expired_round_receipts;
    }
  });
  for (uint64_t index = 0; index < rejections; ++index)
  {
    pool.record_ingress_rejection(relay_record_status_v2::expired);
    pool.record_ingress_rejection(relay_record_status_v2::full);
  }
  stop.store(true, std::memory_order_release);
  reader.join();

  const relay_pool_diagnostics_v2 published = pool.published_diagnostics();
  const relay_pool_diagnostics_v2 locked = pool.diagnostics();
  EXPECT_EQ(regressions.load(), 0u);
  EXPECT_EQ(published.expired_round_receipts, rejections);
  EXPECT_EQ(published.capacity_rejections, rejections);
  EXPECT_EQ(locked.expired_round_receipts, rejections);
  EXPECT_EQ(published.queue_items, locked.queue_items);
  EXPECT_EQ(published.queue_bytes, locked.queue_bytes);

  const relay_record_pool_v2 copy(pool);
  EXPECT_EQ(copy.published_diagnostics().capacity_rejections, rejections);
}

// Against a std::vector model, with copies that must keep their contents
// however the original changes afterwards.
TEST(epose_shared_chunk_vector_v2, matches_vector_model)
{
  using sequence = shared_chunk_vector_v2<uint64_t, 4>;
  std::mt19937_64 rng(7);
  sequence value;
  std::vector<uint64_t> model;
  std::vector<std::pair<sequence, std::vector<uint64_t>>> copies;
  const auto same = [](const sequence &got, const std::vector<uint64_t> &want)
  {
    ASSERT_EQ(want.size(), got.size());
    size_t index = 0;
    for (auto it = got.begin(); it != got.end(); ++it, ++index)
    {
      ASSERT_EQ(index, it.index());
      ASSERT_EQ(want[index], *it);
      ASSERT_EQ(want[index], got[index]);
    }
  };
  struct change { size_t index; uint64_t old; sequence::chunk_ptr previous; };
  for (int round = 0; round < 400; ++round)
  {
    // A checkpoint, then pushes and replaces, then roll back or keep.
    const size_t checkpoint = model.size();
    const std::vector<uint64_t> saved = model;
    std::vector<change> undo;
    const int steps = 1 + rng() % 12;
    for (int step = 0; step < steps; ++step)
    {
      if (model.empty() || rng() % 3 != 0)
      {
        const uint64_t v = rng();
        value.push_back(v);
        model.push_back(v);
      }
      else
      {
        const size_t index = rng() % model.size();
        const uint64_t v = rng();
        undo.push_back({index, model[index], value.replace(index, v)});
        model[index] = v;
      }
      if (rng() % 4 == 0)
        copies.emplace_back(value, model);
    }
    same(value, model);
    if (rng() % 2 == 0)
    {
      value.truncate(checkpoint);
      for (auto it = undo.rbegin(); it != undo.rend(); ++it)
        value.restore(it->index, it->old, it->previous);
      model = saved;
      same(value, model);
    }
    if (rng() % 16 == 0 && !model.empty())
    {
      const size_t keep = rng() % model.size();
      value.truncate(keep);
      model.resize(keep);
    }
    sequence assigned;
    assigned.push_back(1);
    assigned = value;
    same(assigned, model);
  }
  for (const auto &copy : copies)
    same(copy.first, copy.second);
  EXPECT_GT(model.size(), 8u);
}
