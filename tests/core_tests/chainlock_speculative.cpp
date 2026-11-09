#include "chainlock_speculative.h"
#include "blockchain_test_access.h"

using namespace cryptonote;

namespace
{
  const uint64_t main_lock_height = gen_chainlock_base::mined_height;
  const uint64_t alt_lock_height = gen_chainlock_base::mined_height + 3;
  const uint8_t prior_version = 2;
  cryptonote::blobdata wrong_anchor_lock;

  bool generate_speculative_fork(const gen_chainlock_base& test, std::vector<test_event_entry>& events, bool anchor_on_alt_chain)
  {
    gen_chainlock_base::ring_chain chain;
    CHECK_AND_ASSERT_MES(test.start_chain(events, chain), false, "failed to start chain");
    cryptonote::account_base rival;
    rival.generate();
    cryptonote::block a0, a1;
    CHECK_AND_ASSERT_MES(test.extend(events, chain, chain.tip, chain.miner, a0), false, "a0");
    cryptonote::blobdata main_lock;
    CHECK_AND_ASSERT_MES(test.make_lock_blob(events, test.full_lock(a0), main_lock), false, "main lock");
    CHECK_AND_ASSERT_MES(test.extend(events, chain, a0, chain.miner, a1, main_lock), false, "a1");

    uint64_t alt_epoch = 0, anchor_height = 0;
    CHECK_AND_ASSERT_MES(test.timing().epoch_of(alt_lock_height, alt_epoch) && test.timing().committee_anchor(alt_epoch, anchor_height), false, "alt anchor");
    const cryptonote::block* parent = gen_chainlock_base::main_block_at(events, anchor_height - 1);
    CHECK_AND_ASSERT_MES(parent != nullptr, false, "fork parent");

    std::vector<cryptonote::block> alt(alt_lock_height - anchor_height + 1);
    for (size_t index = 0; index < alt.size(); ++index)
    {
      const uint8_t version = anchor_height + index < gen_chainlock_base::mined_height ? prior_version : gen_chainlock_base::fork_version;
      CHECK_AND_ASSERT_MES(test.construct_next_block(chain,
          index == 0 ? *parent : alt[index - 1], rival, version, {}, alt[index]),
          false, "alt block");
    }

    gen_chainlock_base::lock_spec spec = test.full_lock(alt.back());
    if (anchor_on_alt_chain)
      spec.anchor_hash = get_block_hash(alt.front());
    cryptonote::blobdata alt_lock;
    CHECK_AND_ASSERT_MES(test.make_lock_blob(events, spec, alt_lock), false, "alt lock");
    if (!anchor_on_alt_chain)
    {
      wrong_anchor_lock = alt_lock;
      DO_CALLBACK(events, "store_wrong_anchor_lock");
    }
    events.insert(events.end(), alt.begin(), alt.end());
    DO_CALLBACK(events, "check_expectation");
    cryptonote::block carrier;
    CHECK_AND_ASSERT_MES(test.construct_next_block(chain, alt.back(), rival, gen_chainlock_base::fork_version, {}, carrier, alt_lock), false, "carrier");
    events.push_back(carrier);
    DO_CALLBACK(events, "check_expectation");
    return true;
  }
}

bool gen_chainlock_speculative_switch_verified::generate(std::vector<test_event_entry>& events) const
{
  return generate_speculative_fork(*this, events, true);
}

bool gen_chainlock_speculative_switch_rolled_back::generate(std::vector<test_event_entry>& events) const
{
  return generate_speculative_fork(*this, events, false);
}

bool gen_chainlock_speculative_switch_rolled_back::store_wrong_anchor_lock(
    core& c, size_t, const std::vector<test_event_entry>&)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_speculative_switch_rolled_back::store_wrong_anchor_lock");
  qwertycoin::epose::chainlock_v1 lock{};
  CHECK_TEST_CONDITION(qwertycoin::epose::parse_chainlock_v1(wrong_anchor_lock, lock));
  CHECK_TEST_CONDITION(blockchain_test_access::verify_complete_chainlock(
      c.get_blockchain_storage(), lock, true));
  c.get_blockchain_storage().get_db().put_chainlock(
      {lock.height, lock.block_hash, false, wrong_anchor_lock});
  return true;
}

std::vector<gen_chainlock_base::expectation> gen_chainlock_speculative_switch_verified::expectations() const
{
  return {
      {main_lock_height + 1, branch::main, main_lock_height, main_lock_height, boost::none},
      {alt_lock_height + 1, branch::rival, alt_lock_height, alt_lock_height, boost::none}};
}

std::vector<gen_chainlock_base::expectation> gen_chainlock_speculative_switch_rolled_back::expectations() const
{
  const expectation unchanged{main_lock_height + 1, branch::main, main_lock_height, main_lock_height, boost::none};
  return {unchanged, unchanged};
}
