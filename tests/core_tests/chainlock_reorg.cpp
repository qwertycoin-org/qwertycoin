#include "chainlock_reorg.h"

using namespace cryptonote;

bool gen_chainlock_reorg_below_lock_then_above::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(start_chain(events, chain), false, "failed to start chain");
  cryptonote::account_base rival;
  rival.generate();
  cryptonote::block a0, a1, a2, b0, b1, b2, b3, c1, c2, c3;
  CHECK_AND_ASSERT_MES(extend(events, chain, chain.tip, chain.miner, a0), false, "a0");
  cryptonote::blobdata lock;
  CHECK_AND_ASSERT_MES(make_lock_blob(events, full_lock(a0), lock), false, "lock");
  CHECK_AND_ASSERT_MES(extend(events, chain, a0, chain.miner, a1, lock), false, "a1");
  CHECK_AND_ASSERT_MES(extend(events, chain, a1, chain.miner, a2), false, "a2");

  CHECK_AND_ASSERT_MES(extend(events, chain, chain.tip, rival, b0), false, "b0");
  CHECK_AND_ASSERT_MES(extend(events, chain, b0, rival, b1), false, "b1");
  CHECK_AND_ASSERT_MES(extend(events, chain, b1, rival, b2), false, "b2");
  CHECK_AND_ASSERT_MES(extend(events, chain, b2, rival, b3), false, "b3");
  DO_CALLBACK(events, "check_expectation");

  CHECK_AND_ASSERT_MES(extend(events, chain, a0, rival, c1), false, "c1");
  CHECK_AND_ASSERT_MES(extend(events, chain, c1, rival, c2), false, "c2");
  CHECK_AND_ASSERT_MES(extend(events, chain, c2, rival, c3), false, "c3");
  DO_CALLBACK(events, "check_expectation");
  return true;
}

std::vector<gen_chainlock_base::expectation> gen_chainlock_reorg_below_lock_then_above::expectations() const
{
  return {
      {mined_height + 2, branch::main, mined_height, mined_height, boost::none},
      {mined_height + 3, branch::rival, mined_height, boost::none, boost::none}};
}
