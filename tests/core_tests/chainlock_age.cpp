#include "chainlock_age.h"

using namespace cryptonote;

bool gen_chainlock_age_limit::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(start_chain(events, chain), false, "failed to start chain");
  cryptonote::block locked_stale, locked_edge, filler, stale, edge;
  CHECK_AND_ASSERT_MES(extend(events, chain, chain.tip, chain.miner, locked_stale), false, "locked_stale");
  CHECK_AND_ASSERT_MES(extend(events, chain, locked_stale, chain.miner, locked_edge), false, "locked_edge");
  const cryptonote::block* prev = &locked_edge;
  for (uint64_t height = mined_height + 2; height < mined_height + 1 + QWC_LOCK_MAX_AGE_BLOCKS; ++height)
  {
    CHECK_AND_ASSERT_MES(extend(events, chain, *prev, chain.miner, filler), false, "filler");
    prev = &filler;
  }

  cryptonote::blobdata stale_lock, edge_lock;
  CHECK_AND_ASSERT_MES(make_lock_blob(events, full_lock(locked_stale), stale_lock), false, "stale lock");
  CHECK_AND_ASSERT_MES(make_lock_blob(events, full_lock(locked_edge), edge_lock), false, "edge lock");
  CHECK_AND_ASSERT_MES(construct_next_block(chain, filler, chain.miner, fork_version, {}, stale, stale_lock), false, "stale");
  DO_CALLBACK(events, "mark_invalid_block");
  events.push_back(stale);
  DO_CALLBACK(events, "check_expectation");
  CHECK_AND_ASSERT_MES(extend(events, chain, filler, chain.miner, edge, edge_lock), false, "edge");
  DO_CALLBACK(events, "check_expectation");
  return true;
}

std::vector<gen_chainlock_base::expectation> gen_chainlock_age_limit::expectations() const
{
  const uint64_t edge_tip = mined_height + 1 + QWC_LOCK_MAX_AGE_BLOCKS;
  return {
      {edge_tip - 1, branch::main, boost::none, boost::none, boost::none},
      {edge_tip, branch::rival, mined_height + 1, mined_height + 1, boost::none}};
}
