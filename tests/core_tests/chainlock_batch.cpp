#include <chrono>
#include <future>

#include "chaingen.h"
#include "chainlock.h"
#include "cryptonote_core/blockchain.h"

using namespace cryptonote;

bool gen_chainlock_during_batch::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(start_chain(events, chain), false, "failed to start chain");
  block locked;
  CHECK_AND_ASSERT_MES(extend(events, chain, chain.tip, chain.miner, locked), false, "locked block");
  DO_CALLBACK(events, "check_concurrent_lock");
  return true;
}

std::vector<gen_chainlock_base::expectation> gen_chainlock_during_batch::expectations() const
{
  return {};
}

bool gen_chainlock_during_batch::check_concurrent_lock(
    core& c, size_t ev_index, const std::vector<test_event_entry>& events)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_during_batch::check_concurrent_lock");
  const block* locked = main_block_at(events, get_block_height(boost::get<block>(events[ev_index - 1])));
  CHECK_TEST_CONDITION(locked != nullptr);
  blobdata lock_blob;
  CHECK_TEST_CONDITION(make_lock_blob(events, full_lock(*locked), lock_blob));

  Blockchain& chain = c.get_blockchain_storage();
  chain.prepare_handle_incoming_block_no_preprocess(0);
  std::promise<void> entered;
  auto started = entered.get_future();
  auto ingress = std::async(std::launch::async, [&chain, lock_blob, &entered]() {
    entered.set_value();
    blobdata relay;
    return chain.handle_incoming_chainlock(lock_blob, relay);
  });
  started.wait();
  const bool waited_for_batch = ingress.wait_for(std::chrono::milliseconds(50))
      == std::future_status::timeout;
  const bool cleaned = chain.cleanup_handle_incoming_blocks();
  CHECK_TEST_CONDITION(cleaned);
  CHECK_TEST_CONDITION(waited_for_batch);
  CHECK_TEST_CONDITION(ingress.get() == Blockchain::chainlock_ingress::accepted);
  chainlock_record stored;
  CHECK_TEST_CONDITION(chain.get_db().get_chainlock(get_block_height(*locked), stored));
  return true;
}
