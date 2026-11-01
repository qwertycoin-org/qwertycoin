#include <algorithm>
#include <cstring>
#include <fstream>
#include <string>
#include <tuple>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "chaingen.h"
#include "chainlock.h"
#include "checkpoints/checkpoints.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "cryptonote_core/blockchain.h"
#include "string_tools.h"

using namespace cryptonote;
using namespace qwertycoin::epose;

namespace
{
  const uint64_t fork_height = gen_height_window_base::mined_height;
  blobdata shared_message_main_lock;
  blobdata shared_message_rival_lock;

  const cryptonote::block* block_at(const std::vector<test_event_entry>& events, size_t end, uint64_t height, gen_chainlock_base::branch which)
  {
    const cryptonote::block* found = nullptr;
    for (size_t index = 0; index < end && index < events.size(); ++index)
    {
      if (events[index].type() != typeid(cryptonote::block))
        continue;
      const cryptonote::block& blk = boost::get<cryptonote::block>(events[index]);
      if (get_block_height(blk) != height)
        continue;
      found = &blk;
      if (which == gen_chainlock_base::branch::main)
        break;
    }
    return found;
  }

  std::vector<crypto::public_key> public_keys(const std::vector<gen_chainlock_base::signer_key>& signers)
  {
    std::vector<crypto::public_key> keys;
    for (const gen_chainlock_base::signer_key& signer : signers)
      keys.push_back(signer.public_key);
    return keys;
  }

  struct chainlock_outcome
  {
    crypto::hash tip{};
    uint64_t lock_height = 0;
    crypto::hash lock_hash{};
    std::vector<std::tuple<uint64_t, crypto::hash, bool, blobdata>> candidates;
  };

  struct replay_cleanup
  {
    core &node;
    boost::filesystem::path directory;
    bool init_attempted = false;
    bool closed = false;

    bool close()
    {
      if (closed)
        return true;
      closed = true;
      bool deinitialized = !init_attempted;
      if (init_attempted)
      {
        try { deinitialized = node.deinit(); }
        catch (...) { deinitialized = false; }
      }
      boost::system::error_code error;
      boost::filesystem::remove_all(directory, error);
      return deinitialized && !error;
    }

    ~replay_cleanup()
    {
      if (!closed)
        close();
    }
  };

  bool capture_chainlock_outcome(core &node, chainlock_outcome &outcome)
  {
    const auto &chain = node.get_blockchain_storage();
    outcome.tip = chain.get_tail_id();
    if (!chain.get_highest_main_chainlock(outcome.lock_height, outcome.lock_hash))
      return false;
    return chain.get_db().for_chainlocks_descending([&](const chainlock_record &candidate) {
      outcome.candidates.emplace_back(candidate.height, candidate.block_hash,
          candidate.tombstoned, candidate.lock);
      return true;
    });
  }

  template<class Validator>
  bool replay_chainlock_order(const std::vector<test_event_entry> &events,
      Validator &validator, chainlock_outcome &outcome,
      bool force_init_failure = false,
      boost::filesystem::path *created_directory = nullptr,
      bool *cleanup_succeeded = nullptr)
  {
    const boost::filesystem::path directory = boost::filesystem::temp_directory_path()
        / boost::filesystem::unique_path("chainlock-order-%%%%%%%%%%%%%%%%");
    core node(nullptr);
    replay_cleanup cleanup{node, directory};
    const auto close = [&] {
      const bool cleaned = cleanup.close();
      if (cleanup_succeeded)
        *cleanup_succeeded = cleaned;
      return cleaned;
    };
    if (created_directory)
      *created_directory = directory;
    boost::filesystem::create_directories(directory);
    const boost::filesystem::path data_directory = force_init_failure
        ? directory / "blocked-data-dir" : directory;
    if (force_init_failure)
      std::ofstream(data_directory.string()).put('x');
    boost::program_options::options_description options;
    core::init_options(options);
    boost::program_options::variables_map values;
    const std::vector<std::string> args{"--data-dir", data_directory.string()};
    boost::program_options::store(boost::program_options::command_line_parser(args)
        .options(options).run(), values);
    boost::program_options::notify(values);
    get_test_options<Validator> configured;
    v_hardforks_t hardforks;
    cryptonote::test_options event_options{nullptr, 0};
    const cryptonote::test_options *selected = &configured.test_options;
    if (extract_hard_forks(events, hardforks))
    {
      hardforks.emplace_back(0, 0);
      event_options.hard_forks = hardforks.data();
      selected = &event_options;
    }
    cleanup.init_attempted = true;
    bool initialized = false;
    try { initialized = node.init(values, selected); }
    catch (...) { close(); return false; }
    if (!initialized)
    {
      close();
      return false;
    }
    if (force_init_failure)
      return close();
    node.get_blockchain_storage().get_db().set_batch_transactions(true);
    const bool replayed = replay_events_through_core_plain(node, events, validator)
        && capture_chainlock_outcome(node, outcome);
    const bool cleaned = close();
    return replayed && cleaned;
  }

  template<class First, class Second>
  bool check_chainlock_order_convergence(const std::string &first_callback,
      const std::string &second_callback)
  {
    DEFINE_TESTS_ERROR_CONTEXT("check_chainlock_order_convergence");
    First first;
    Second second;
    std::vector<test_event_entry> first_events;
    std::vector<test_event_entry> second_events;
    CHECK_TEST_CONDITION(first.generate(first_events));
    CHECK_TEST_CONDITION(second.generate(second_events));
    chainlock_outcome failed_outcome;
    boost::filesystem::path failed_directory;
    bool cleaned = false;
    CHECK_TEST_CONDITION(!replay_chainlock_order(first_events, first,
        failed_outcome, true, &failed_directory, &cleaned));
    CHECK_TEST_CONDITION(cleaned);
    CHECK_TEST_CONDITION(!boost::filesystem::exists(failed_directory));
    const auto first_order = std::find_if(first_events.begin(), first_events.end(),
        [&](const test_event_entry &event) {
          const auto *callback = boost::get<callback_entry>(&event);
          return callback && (callback->callback_name == first_callback
              || callback->callback_name == second_callback);
        });
    const auto second_order = std::find_if(second_events.begin(), second_events.end(),
        [&](const test_event_entry &event) {
          const auto *callback = boost::get<callback_entry>(&event);
          return callback && (callback->callback_name == first_callback
              || callback->callback_name == second_callback);
        });
    CHECK_TEST_CONDITION(first_order != first_events.end()
        && second_order != second_events.end());
    CHECK_EQ(boost::get<callback_entry>(*first_order).callback_name, first_callback);
    CHECK_EQ(boost::get<callback_entry>(*second_order).callback_name, second_callback);
    CHECK_TEST_CONDITION(first_callback != second_callback
        || std::distance(first_events.begin(), first_order)
            != std::distance(second_events.begin(), second_order));
    first_events.pop_back();
    second_events.pop_back();
    chainlock_outcome first_outcome;
    chainlock_outcome second_outcome;
    CHECK_TEST_CONDITION(replay_chainlock_order(first_events, first, first_outcome));
    CHECK_TEST_CONDITION(replay_chainlock_order(second_events, second, second_outcome));
    CHECK_EQ(first_outcome.tip, second_outcome.tip);
    CHECK_EQ(first_outcome.lock_height, second_outcome.lock_height);
    CHECK_EQ(first_outcome.lock_hash, second_outcome.lock_hash);
    CHECK_TEST_CONDITION(first_outcome.candidates == second_outcome.candidates);
    return true;
  }
}

epoch_timing_v2 gen_chainlock_base::timing()
{
  epoch_timing_v2 result;
  result.activation_height = 0;
  result.epoch_length = lock_epoch_length;
  result.anchor_depth = lock_anchor_depth;
  return result;
}

std::vector<gen_chainlock_base::signer_key> gen_chainlock_base::signers()
{
  std::vector<signer_key> keys(signer_count);
  for (size_t index = 0; index < keys.size(); ++index)
  {
    const std::string seed_text = "chainlock-core-test-signer-" + std::to_string(index);
    const crypto::hash seed_hash = crypto::cn_fast_hash(seed_text.data(), seed_text.size());
    crypto::secret_key seed;
    static_assert(sizeof(seed) == sizeof(seed_hash), "secret key size");
    std::memcpy(&seed, &seed_hash, sizeof(seed_hash));
    crypto::generate_keys(keys[index].public_key, keys[index].secret_key, seed, true);
  }
  return keys;
}

const cryptonote::block* gen_chainlock_base::main_block_at(const std::vector<test_event_entry>& events, uint64_t height)
{
  return block_at(events, events.size(), height, branch::main);
}

std::vector<gen_chainlock_base::expectation> gen_chainlock_base::rejected_expectations()
{
  return {{fork_height, branch::main, boost::none, boost::none, boost::none}};
}

bool gen_chainlock_base::start_chain(std::vector<test_event_entry>& events, ring_chain& chain) const
{
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "failed to generate ring chain");
  DO_CALLBACK(events, "install_quorum");
  return true;
}

bool gen_chainlock_base::extend(std::vector<test_event_entry>& events, ring_chain& chain, const cryptonote::block& prev,
    const cryptonote::account_base& miner, cryptonote::block& blk, const cryptonote::blobdata& lock, uint8_t version) const
{
  CHECK_AND_ASSERT_MES(construct_next_block(chain, prev, miner, version, {}, blk, lock), false, "failed to construct block");
  events.push_back(blk);
  return true;
}

bool gen_chainlock_base::quorum_for(const std::vector<test_event_entry>& events, const lock_spec& spec,
    chainlock_quorum_v1& quorum, chainlock_context_v1& context) const
{
  uint64_t anchor_height = 0;
  CHECK_AND_ASSERT_MES(timing().committee_anchor(spec.epoch, anchor_height), false, "no committee anchor");
  const cryptonote::block* anchor = main_block_at(events, anchor_height);
  const cryptonote::block* genesis = main_block_at(events, 0);
  CHECK_AND_ASSERT_MES((spec.anchor_hash || anchor != nullptr) && genesis != nullptr, false, "anchor block missing");
  context = {FAKECHAIN, get_block_hash(*genesis), crypto::null_hash};
  const std::vector<signer_key> keys = signers();
  quorum = select_chainlock_quorum_v1(spec.epoch, spec.anchor_hash ? *spec.anchor_hash : get_block_hash(*anchor), public_keys(keys), context);
  CHECK_AND_ASSERT_MES(quorum.can_lock(), false, "quorum cannot lock");
  return true;
}

bool gen_chainlock_base::make_lock(const std::vector<test_event_entry>& events, const lock_spec& spec, chainlock_v1& lock) const
{
  chainlock_quorum_v1 quorum;
  chainlock_context_v1 context{};
  CHECK_AND_ASSERT_MES(quorum_for(events, spec, quorum, context), false, "quorum unavailable");
  const std::vector<signer_key> keys = signers();
  lock = chainlock_v1{};
  lock.height = spec.height;
  lock.block_hash = spec.block_hash;
  lock.epoch = spec.epoch;
  for (size_t index = 0; index < quorum.members.size() && lock.signer_count() < spec.signatures; ++index)
  {
    const auto signer = std::find_if(keys.begin(), keys.end(),
        [&](const signer_key& key) { return key.public_key == quorum.members[index]; });
    CHECK_AND_ASSERT_MES(signer != keys.end(), false, "unknown quorum member");
    chainlock_v1 share;
    CHECK_AND_ASSERT_MES(sign_chainlock_share_v1(spec.height, spec.block_hash, quorum, signer->public_key, signer->secret_key, context, share),
        false, "failed to sign share");
    CHECK_AND_ASSERT_MES(merge_chainlock_share_v1(lock, share, quorum.threshold()), false, "failed to merge share");
  }
  return true;
}

bool gen_chainlock_base::make_lock_blob(const std::vector<test_event_entry>& events, const lock_spec& spec, cryptonote::blobdata& blob) const
{
  chainlock_v1 lock;
  CHECK_AND_ASSERT_MES(make_lock(events, spec, lock), false, "failed to make lock");
  blob = serialize_chainlock_v1(lock);
  return true;
}

gen_chainlock_base::lock_spec gen_chainlock_base::full_lock(const cryptonote::block& locked) const
{
  const uint64_t height = get_block_height(locked);
  uint64_t epoch = 0;
  timing().epoch_of(height, epoch);
  return {height, get_block_hash(locked), epoch, qwc_chainlock_threshold(signer_count)};
}

bool gen_chainlock_base::generate_rejected_lock(std::vector<test_event_entry>& events, const blob_maker& make_blob) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(start_chain(events, chain), false, "failed to start chain");
  cryptonote::block a0, rejected;
  CHECK_AND_ASSERT_MES(extend(events, chain, chain.tip, chain.miner, a0), false, "a0");
  cryptonote::blobdata blob;
  CHECK_AND_ASSERT_MES(make_blob(events, a0, blob), false, "failed to make rejected lock");
  CHECK_AND_ASSERT_MES(construct_next_block(chain, a0, chain.miner, fork_version, {}, rejected, blob), false, "rejected block");
  DO_CALLBACK(events, "mark_invalid_block");
  events.push_back(rejected);
  DO_CALLBACK(events, "check_expectation");
  return true;
}

bool gen_chainlock_base::install_quorum(cryptonote::core& c, size_t, const std::vector<test_event_entry>&)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_base::install_quorum");
  CHECK_TEST_CONDITION(c.get_blockchain_storage().set_chainlock_test_quorum(timing(), public_keys(signers())));
  return true;
}

bool gen_chainlock_base::install_checkpoint(cryptonote::core& c, size_t, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_base::install_checkpoint");
  const cryptonote::block* target = nullptr;
  for (const test_event_entry& event : events)
    if (event.type() == typeid(cryptonote::block)
        && (target == nullptr || get_block_height(boost::get<cryptonote::block>(event)) > get_block_height(*target)))
      target = &boost::get<cryptonote::block>(event);
  CHECK_TEST_CONDITION(target != nullptr);
  Blockchain& chain = c.get_blockchain_storage();
  cryptonote::checkpoints points = chain.get_checkpoints();
  CHECK_TEST_CONDITION(points.add_checkpoint(get_block_height(*target), epee::string_tools::pod_to_hex(get_block_hash(*target))));
  chain.set_checkpoints(std::move(points));
  return true;
}

bool gen_chainlock_base::check_expectation(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_base::check_expectation");
  const std::vector<expectation> expected_list = expectations();
  CHECK_TEST_CONDITION(m_checked < expected_list.size());
  const expectation& expected = expected_list[m_checked++];
  const cryptonote::block* tip = block_at(events, ev_index, expected.tip_height, expected.tip_branch);
  CHECK_TEST_CONDITION(tip != nullptr);
  const Blockchain& chain = c.get_blockchain_storage();
  const BlockchainDB& db = chain.get_db();
  CHECK_EQ(get_block_hash(*tip), chain.get_tail_id());

  uint64_t highest = 0;
  const bool has_highest = chain.get_highest_main_chainlock(highest);
  CHECK_EQ(static_cast<bool>(expected.highest_lock), has_highest);
  if (has_highest)
    CHECK_EQ(*expected.highest_lock, highest);

  uint64_t carrier_height = 0;
  chainlock_record carrier;
  const bool has_carrier = db.get_last_chainlock_carrier(carrier_height, carrier);
  CHECK_EQ(static_cast<bool>(expected.carrier_lock), has_carrier);
  if (has_carrier)
  {
    CHECK_EQ(*expected.carrier_lock, carrier.height);
    CHECK_EQ(db.get_block_hash_from_height(carrier.height), carrier.block_hash);
  }

  if (expected.tombstoned)
  {
    chainlock_record stored;
    CHECK_TEST_CONDITION(db.get_chainlock(*expected.tombstoned, stored));
    CHECK_TEST_CONDITION(stored.tombstoned);
  }
  return true;
}

bool gen_chainlock_base::check_last_block_invalid(cryptonote::core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_base::check_last_block_invalid");
  const cryptonote::block* last = nullptr;
  for (size_t index = 0; index < ev_index && index < events.size(); ++index)
    if (events[index].type() == typeid(cryptonote::block))
      last = &boost::get<cryptonote::block>(events[index]);
  CHECK_TEST_CONDITION(last != nullptr);
  int where = HAVE_BLOCK_MAIN_CHAIN;
  CHECK_TEST_CONDITION(c.get_blockchain_storage().have_block(get_block_hash(*last), &where));
  CHECK_EQ(HAVE_BLOCK_INVALID, where);
  return true;
}

bool gen_chainlock_carried_lock_accepted::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(start_chain(events, chain), false, "failed to start chain");
  cryptonote::block a0, a1, a2;
  CHECK_AND_ASSERT_MES(extend(events, chain, chain.tip, chain.miner, a0), false, "a0");
  cryptonote::blobdata lock;
  CHECK_AND_ASSERT_MES(make_lock_blob(events, full_lock(a0), lock), false, "lock");
  CHECK_AND_ASSERT_MES(extend(events, chain, a0, chain.miner, a1, lock), false, "a1");
  DO_CALLBACK(events, "check_expectation");
  CHECK_AND_ASSERT_MES(extend(events, chain, a1, chain.miner, a2), false, "a2");
  DO_CALLBACK(events, "check_expectation");
  return true;
}

bool gen_chainlock_below_threshold_rejected::generate(std::vector<test_event_entry>& events) const
{
  return generate_rejected_lock(events, [this](const std::vector<test_event_entry>& evs, const cryptonote::block& locked, cryptonote::blobdata& blob) {
    lock_spec spec = full_lock(locked);
    --spec.signatures;
    return make_lock_blob(evs, spec, blob);
  });
}

bool gen_chainlock_wrong_epoch_rejected::generate(std::vector<test_event_entry>& events) const
{
  return generate_rejected_lock(events, [this](const std::vector<test_event_entry>& evs, const cryptonote::block& locked, cryptonote::blobdata& blob) {
    lock_spec spec = full_lock(locked);
    --spec.epoch;
    return make_lock_blob(evs, spec, blob);
  });
}

bool gen_chainlock_non_ancestor_rejected::generate(std::vector<test_event_entry>& events) const
{
  return generate_rejected_lock(events, [this](const std::vector<test_event_entry>& evs, const cryptonote::block& locked, cryptonote::blobdata& blob) {
    lock_spec spec = full_lock(locked);
    const std::string foreign = "chainlock-non-ancestor";
    spec.block_hash = crypto::cn_fast_hash(foreign.data(), foreign.size());
    return make_lock_blob(evs, spec, blob);
  });
}

bool gen_chainlock_bad_signature_rejected::generate(std::vector<test_event_entry>& events) const
{
  return generate_rejected_lock(events, [this](const std::vector<test_event_entry>& evs, const cryptonote::block& locked, cryptonote::blobdata& blob) {
    chainlock_v1 lock;
    if (!make_lock(evs, full_lock(locked), lock) || lock.signatures.empty())
      return false;
    lock.signatures.front().c.data[0] ^= 1;
    blob = serialize_chainlock_v1(lock);
    return true;
  });
}

bool gen_chainlock_not_newer_rejected::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(start_chain(events, chain), false, "failed to start chain");
  cryptonote::block a0, a1, rejected;
  CHECK_AND_ASSERT_MES(extend(events, chain, chain.tip, chain.miner, a0), false, "a0");
  cryptonote::blobdata lock;
  CHECK_AND_ASSERT_MES(make_lock_blob(events, full_lock(a0), lock), false, "lock");
  CHECK_AND_ASSERT_MES(extend(events, chain, a0, chain.miner, a1, lock), false, "a1");
  CHECK_AND_ASSERT_MES(construct_next_block(chain, a1, chain.miner, fork_version, {}, rejected, lock), false, "rejected block");
  DO_CALLBACK(events, "mark_invalid_block");
  events.push_back(rejected);
  DO_CALLBACK(events, "check_expectation");
  return true;
}

bool gen_chainlock_fork_blocked::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(start_chain(events, chain), false, "failed to start chain");
  cryptonote::account_base rival;
  rival.generate();
  cryptonote::block a0, a1, b0, b1, b2;
  CHECK_AND_ASSERT_MES(extend(events, chain, chain.tip, chain.miner, a0), false, "a0");
  cryptonote::blobdata lock;
  CHECK_AND_ASSERT_MES(make_lock_blob(events, full_lock(a0), lock), false, "lock");
  CHECK_AND_ASSERT_MES(extend(events, chain, a0, chain.miner, a1, lock), false, "a1");
  CHECK_AND_ASSERT_MES(extend(events, chain, chain.tip, rival, b0), false, "b0");
  CHECK_AND_ASSERT_MES(extend(events, chain, b0, rival, b1), false, "b1");
  CHECK_AND_ASSERT_MES(extend(events, chain, b1, rival, b2), false, "b2");
  DO_CALLBACK(events, "check_expectation");
  return true;
}

bool gen_chainlock_higher_lock_wins::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(start_chain(events, chain), false, "failed to start chain");
  cryptonote::account_base rival;
  rival.generate();
  cryptonote::block a0, a1, a2, a3, b1, b2;
  CHECK_AND_ASSERT_MES(extend(events, chain, chain.tip, chain.miner, a0), false, "a0");
  cryptonote::blobdata main_lock;
  CHECK_AND_ASSERT_MES(make_lock_blob(events, full_lock(a0), main_lock), false, "main lock");
  CHECK_AND_ASSERT_MES(extend(events, chain, a0, chain.miner, a1, main_lock), false, "a1");
  CHECK_AND_ASSERT_MES(extend(events, chain, a1, chain.miner, a2), false, "a2");
  CHECK_AND_ASSERT_MES(extend(events, chain, a2, chain.miner, a3), false, "a3");
  CHECK_AND_ASSERT_MES(extend(events, chain, a0, rival, b1), false, "b1");
  cryptonote::blobdata alt_lock;
  CHECK_AND_ASSERT_MES(make_lock_blob(events, full_lock(b1), alt_lock), false, "alt lock");
  CHECK_AND_ASSERT_MES(extend(events, chain, b1, rival, b2, alt_lock), false, "b2");
  DO_CALLBACK(events, "check_expectation");
  return true;
}

bool gen_chainlock_message_order_base::generate_case(std::vector<test_event_entry>& events, bool lock_first, bool competing) const
{
  static std::vector<test_event_entry> shared;
  static size_t main_end = 0;
  static size_t fork_end = 0;
  if (shared.empty())
  {
    ring_chain chain;
    CHECK_AND_ASSERT_MES(start_chain(shared, chain), false, "chain");
    cryptonote::account_base rival;
    rival.generate();
    cryptonote::block a0, a1, b0, b1, b2, b3;
    CHECK_AND_ASSERT_MES(extend(shared, chain, chain.tip, chain.miner, a0), false, "a0");
    CHECK_AND_ASSERT_MES(extend(shared, chain, a0, chain.miner, a1), false, "a1");
    main_end = shared.size();
    CHECK_AND_ASSERT_MES(extend(shared, chain, chain.tip, rival, b0), false, "b0");
    CHECK_AND_ASSERT_MES(extend(shared, chain, b0, rival, b1), false, "b1");
    CHECK_AND_ASSERT_MES(extend(shared, chain, b1, rival, b2), false, "b2");
    fork_end = shared.size();
    CHECK_AND_ASSERT_MES(extend(shared, chain, b2, rival, b3), false, "b3");
    CHECK_AND_ASSERT_MES(make_lock_blob(shared, full_lock(a1), shared_message_main_lock),
        false, "main lock");
    CHECK_AND_ASSERT_MES(make_lock_blob(shared, full_lock(b2), shared_message_rival_lock),
        false, "rival lock");
  }
  lock_first_ = lock_first;
  competing_ = competing;
  events.assign(shared.begin(), shared.begin() + main_end);
  if (lock_first)
    DO_CALLBACK(events, "deliver_lock");
  events.insert(events.end(), shared.begin() + main_end, shared.begin() + fork_end);
  if (competing)
  {
    events.push_back(shared[fork_end]);
    DO_CALLBACK(events, "deliver_rival_lock");
  }
  if (!lock_first)
    DO_CALLBACK(events, "deliver_lock");
  DO_CALLBACK(events, "check_expectation");
  DO_CALLBACK(events, "check_convergence");
  return true;
}

bool gen_chainlock_message_order_base::check_convergence(core& c, size_t,
    const std::vector<test_event_entry>&)
{
  (void)c;
  if (competing_)
    return check_chainlock_order_convergence<gen_chainlock_competing_main_first,
        gen_chainlock_competing_rival_first>("deliver_lock", "deliver_rival_lock");
  return check_chainlock_order_convergence<gen_chainlock_message_before_reorg,
      gen_chainlock_message_after_reorg>("deliver_lock", "deliver_lock");
}

bool gen_chainlock_message_order_base::deliver_lock(core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_message_order_base::deliver_lock");
  const cryptonote::block* locked = block_at(events, ev_index, fork_height + 1, branch::main);
  CHECK_TEST_CONDITION(locked != nullptr);
  blobdata relay;
  CHECK_TEST_CONDITION(c.get_blockchain_storage().handle_incoming_chainlock(shared_message_main_lock, relay)
      == Blockchain::chainlock_ingress::accepted);
  return true;
}

bool gen_chainlock_message_order_base::deliver_rival_lock(core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_message_order_base::deliver_rival_lock");
  const cryptonote::block* locked = block_at(events, ev_index, fork_height + 2, branch::rival);
  CHECK_TEST_CONDITION(locked != nullptr);
  blobdata relay;
  CHECK_TEST_CONDITION(c.get_blockchain_storage().handle_incoming_chainlock(shared_message_rival_lock, relay)
      == Blockchain::chainlock_ingress::accepted);
  return true;
}

std::vector<gen_chainlock_base::expectation> gen_chainlock_message_order_base::expectations() const
{
  return {{fork_height + 1, branch::main, fork_height + 1, boost::none, boost::none}};
}

std::vector<gen_chainlock_base::expectation> gen_chainlock_competing_base::expectations() const
{
  return {{fork_height + 3, branch::rival, fork_height + 2, boost::none, boost::none}};
}

namespace
{
  struct divergent_message_fixture
  {
    std::vector<test_event_entry> blocks;
    cryptonote::block main_tip{};
    cryptonote::block rival_tip{};
    blobdata main_lock;
    blobdata rival_lock;
    uint64_t lock_height = fork_height + 3;
  };

  divergent_message_fixture &shared_divergent_messages()
  {
    static divergent_message_fixture fixture;
    return fixture;
  }
}

bool gen_chainlock_divergent_order_base::generate_case(
    std::vector<test_event_entry>& events, bool main_first) const
{
  auto &shared = shared_divergent_messages();
  if (shared.blocks.empty())
  {
    ring_chain chain;
    CHECK_AND_ASSERT_MES(start_chain(shared.blocks, chain), false, "chain");
    block previous = chain.tip;
    for (uint64_t height = fork_height; height <= shared.lock_height; ++height)
    {
      block next{};
      CHECK_AND_ASSERT_MES(extend(shared.blocks, chain, previous, chain.miner, next), false, "main block");
      previous = next;
    }
    shared.main_tip = previous;
    uint64_t epoch = 0;
    uint64_t anchor_height = 0;
    CHECK_AND_ASSERT_MES(timing().epoch_of(shared.lock_height, epoch)
        && timing().committee_anchor(epoch, anchor_height), false, "anchor");
    const block *parent = main_block_at(shared.blocks, anchor_height - 1);
    CHECK_AND_ASSERT_MES(parent != nullptr, false, "fork parent");
    cryptonote::account_base rival;
    rival.generate();
    previous = *parent;
    crypto::hash rival_anchor{};
    for (uint64_t height = anchor_height; height <= shared.lock_height; ++height)
    {
      block next{};
      const uint8_t version = height < fork_height ? 2 : fork_version;
      CHECK_AND_ASSERT_MES(extend(shared.blocks, chain, previous, rival, next, {}, version), false, "rival block");
      if (height == anchor_height)
        rival_anchor = get_block_hash(next);
      previous = next;
    }
    shared.rival_tip = previous;
    CHECK_AND_ASSERT_MES(make_lock_blob(shared.blocks,
        full_lock(shared.main_tip), shared.main_lock), false, "main lock");
    lock_spec rival_spec = full_lock(shared.rival_tip);
    rival_spec.anchor_hash = rival_anchor;
    CHECK_AND_ASSERT_MES(make_lock_blob(shared.blocks,
        rival_spec, shared.rival_lock), false, "rival lock");
  }
  main_first_ = main_first;
  events = shared.blocks;
  DO_CALLBACK(events, main_first ? "deliver_main_lock" : "deliver_rival_lock");
  DO_CALLBACK(events, main_first ? "deliver_rival_lock" : "deliver_main_lock");
  DO_CALLBACK(events, "check_expectation");
  DO_CALLBACK(events, "check_convergence");
  return true;
}

std::vector<gen_chainlock_base::expectation>
gen_chainlock_divergent_order_base::expectations() const
{
  const auto &shared = shared_divergent_messages();
  const branch winner = get_block_hash(shared.main_tip) < get_block_hash(shared.rival_tip)
      ? branch::main : branch::rival;
  return {{shared.lock_height, winner, shared.lock_height, boost::none, boost::none}};
}

bool gen_chainlock_divergent_order_base::deliver_main_lock(
    core& c, size_t, const std::vector<test_event_entry>&)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_divergent_order_base::deliver_main_lock");
  blobdata relay;
  CHECK_TEST_CONDITION(c.get_blockchain_storage().handle_incoming_chainlock(
      shared_divergent_messages().main_lock, relay)
      == Blockchain::chainlock_ingress::accepted);
  return true;
}

bool gen_chainlock_divergent_order_base::deliver_rival_lock(
    core& c, size_t, const std::vector<test_event_entry>&)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_divergent_order_base::deliver_rival_lock");
  blobdata relay;
  CHECK_TEST_CONDITION(c.get_blockchain_storage().handle_incoming_chainlock(
      shared_divergent_messages().rival_lock, relay)
      == Blockchain::chainlock_ingress::accepted);
  return true;
}

bool gen_chainlock_divergent_order_base::check_convergence(
    core& c, size_t, const std::vector<test_event_entry>&)
{
  (void)c;
  return check_chainlock_order_convergence<gen_chainlock_divergent_main_first,
      gen_chainlock_divergent_rival_first>("deliver_main_lock", "deliver_rival_lock");
}

bool gen_chainlock_divergent_higher::generate(std::vector<test_event_entry>& events) const
{
  std::vector<test_event_entry> prepared;
  gen_chainlock_divergent_main_first source;
  CHECK_AND_ASSERT_MES(source.generate(prepared), false, "divergent fixture");
  const auto &shared = shared_divergent_messages();
  const auto main_end = std::find_if(shared.blocks.begin(), shared.blocks.end(),
      [&shared](const test_event_entry &event) {
        const block *candidate = boost::get<block>(&event);
        return candidate && get_block_hash(*candidate) == get_block_hash(shared.main_tip);
      });
  CHECK_AND_ASSERT_MES(main_end != shared.blocks.end(), false, "main tip");
  events.assign(shared.blocks.begin(), std::next(main_end));
  DO_CALLBACK(events, "deliver_older_main_lock");
  events.insert(events.end(), std::next(main_end), shared.blocks.end());
  DO_CALLBACK(events, "deliver_rival_lock");
  DO_CALLBACK(events, "check_expectation");
  return true;
}

std::vector<gen_chainlock_base::expectation>
gen_chainlock_divergent_higher::expectations() const
{
  const auto &shared = shared_divergent_messages();
  return {{shared.lock_height, branch::rival, shared.lock_height, boost::none, boost::none}};
}

bool gen_chainlock_divergent_higher::deliver_older_main_lock(
    core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_divergent_higher::deliver_older_main_lock");
  const auto &shared = shared_divergent_messages();
  const block *locked = main_block_at(events, shared.lock_height - 1);
  CHECK_TEST_CONDITION(locked != nullptr);
  blobdata blob, relay;
  CHECK_TEST_CONDITION(make_lock_blob(events, full_lock(*locked), blob));
  CHECK_TEST_CONDITION(c.get_blockchain_storage().handle_incoming_chainlock(blob, relay)
      == Blockchain::chainlock_ingress::accepted);
  return true;
}

bool gen_chainlock_reorg_above_lock::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(start_chain(events, chain), false, "failed to start chain");
  cryptonote::account_base rival;
  rival.generate();
  cryptonote::block a0, a1, a2, b1, b2, b3;
  CHECK_AND_ASSERT_MES(extend(events, chain, chain.tip, chain.miner, a0), false, "a0");
  cryptonote::blobdata lock;
  CHECK_AND_ASSERT_MES(make_lock_blob(events, full_lock(a0), lock), false, "lock");
  CHECK_AND_ASSERT_MES(extend(events, chain, a0, chain.miner, a1, lock), false, "a1");
  CHECK_AND_ASSERT_MES(extend(events, chain, a1, chain.miner, a2), false, "a2");
  CHECK_AND_ASSERT_MES(extend(events, chain, a0, rival, b1), false, "b1");
  CHECK_AND_ASSERT_MES(extend(events, chain, b1, rival, b2), false, "b2");
  CHECK_AND_ASSERT_MES(extend(events, chain, b2, rival, b3), false, "b3");
  DO_CALLBACK(events, "check_expectation");
  return true;
}

bool gen_chainlock_checkpoint_overrides_lock::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(start_chain(events, chain), false, "failed to start chain");
  cryptonote::account_base rival;
  rival.generate();
  cryptonote::block a0, a1, a2, a3;
  CHECK_AND_ASSERT_MES(extend(events, chain, chain.tip, chain.miner, a0), false, "a0");
  cryptonote::blobdata lock;
  CHECK_AND_ASSERT_MES(make_lock_blob(events, full_lock(a0), lock), false, "lock");
  CHECK_AND_ASSERT_MES(extend(events, chain, a0, chain.miner, a1, lock), false, "a1");
  CHECK_AND_ASSERT_MES(extend(events, chain, a1, chain.miner, a2), false, "a2");
  CHECK_AND_ASSERT_MES(extend(events, chain, a2, chain.miner, a3), false, "a3");

  std::vector<cryptonote::block> rival_chain(6);
  const cryptonote::block* prev = &chain.tip;
  for (cryptonote::block& blk : rival_chain)
  {
    CHECK_AND_ASSERT_MES(construct_next_block(chain, *prev, rival, fork_version, {}, blk), false, "rival block");
    prev = &blk;
  }
  DO_CALLBACK(events, "install_checkpoint");

  for (size_t index = 0; index + 1 < rival_chain.size(); ++index)
  {
    events.push_back(rival_chain[index]);
    DO_CALLBACK(events, "check_expectation");
  }
  events.push_back(rival_chain.back());
  DO_CALLBACK(events, "check_expectation");
  return true;
}

std::vector<gen_chainlock_base::expectation> gen_chainlock_below_threshold_rejected::expectations() const
{
  return rejected_expectations();
}

std::vector<gen_chainlock_base::expectation> gen_chainlock_wrong_epoch_rejected::expectations() const
{
  return rejected_expectations();
}

std::vector<gen_chainlock_base::expectation> gen_chainlock_non_ancestor_rejected::expectations() const
{
  return rejected_expectations();
}

std::vector<gen_chainlock_base::expectation> gen_chainlock_bad_signature_rejected::expectations() const
{
  return rejected_expectations();
}

std::vector<gen_chainlock_base::expectation> gen_chainlock_carried_lock_accepted::expectations() const
{
  return {{fork_height + 1, branch::main, fork_height, fork_height, boost::none}, {fork_height + 2, branch::main, fork_height, fork_height, boost::none}};
}

std::vector<gen_chainlock_base::expectation> gen_chainlock_not_newer_rejected::expectations() const
{
  return {{fork_height + 1, branch::main, fork_height, fork_height, boost::none}};
}

std::vector<gen_chainlock_base::expectation> gen_chainlock_fork_blocked::expectations() const
{
  return {{fork_height + 1, branch::main, fork_height, fork_height, boost::none}};
}

std::vector<gen_chainlock_base::expectation> gen_chainlock_higher_lock_wins::expectations() const
{
  return {{fork_height + 2, branch::rival, fork_height + 1, fork_height + 1, boost::none}};
}

std::vector<gen_chainlock_base::expectation> gen_chainlock_reorg_above_lock::expectations() const
{
  return {{fork_height + 3, branch::rival, fork_height, boost::none, boost::none}};
}

std::vector<gen_chainlock_base::expectation> gen_chainlock_checkpoint_overrides_lock::expectations() const
{
  std::vector<expectation> blocked(5, {fork_height + 3, branch::main, fork_height, fork_height, boost::none});
  blocked.push_back({fork_height + 5, branch::rival, boost::none, boost::none, fork_height});
  return blocked;
}
