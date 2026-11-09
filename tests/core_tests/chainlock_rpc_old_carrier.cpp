#include "chainlock_rpc.h"
#include "chainlock_production_inputs.h"

#include "cryptonote_core/blockchain.h"

using namespace cryptonote;

bool gen_chainlock_rpc_old_carrier::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(start_chain(events, chain), false, "chain");
  block locked, carrier, next;
  CHECK_AND_ASSERT_MES(extend(events, chain, chain.tip, chain.miner, locked), false, "locked");
  blobdata lock;
  CHECK_AND_ASSERT_MES(make_lock_blob(events, full_lock(locked), lock), false, "lock");
  CHECK_AND_ASSERT_MES(extend(events, chain, locked, chain.miner, carrier, lock), false, "carrier");

  block main_tip = carrier;
  for (uint64_t count = 0; count < 2 * lock_epoch_length + 1; ++count)
  {
    CHECK_AND_ASSERT_MES(extend(events, chain, main_tip, chain.miner, next), false, "main block");
    main_tip = next;
  }
  DO_CALLBACK(events, "check_proof");

  account_base rival;
  rival.generate();
  block rival_tip = locked;
  for (uint64_t count = 0; count < 2 * lock_epoch_length + 3; ++count)
  {
    CHECK_AND_ASSERT_MES(extend(events, chain, rival_tip, rival, next), false, "rival block");
    rival_tip = next;
  }
  DO_CALLBACK(events, "check_reorg");
  return true;
}

bool gen_chainlock_rpc_old_carrier::check_proof(core& c, size_t, const std::vector<test_event_entry>&)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_rpc_old_carrier::check_proof");
  Blockchain& chain = c.get_blockchain_storage();
  chainlock_proof proof;
  CHECK_TEST_CONDITION(chain.get_chainlock_proof(0, proof) == chainlock_proof_result::ok);
  CHECK_TEST_CONDITION(chain.get_current_blockchain_height() - proof.lock_carrier_height
      > 2 * lock_epoch_length);
  qwertycoin::epose::chainlock_v1 lock;
  qwertycoin::epose::chainlock_context_v1 context;
  CHECK_TEST_CONDITION(qwertycoin::epose::parse_chainlock_v1(proof.lock_blob, lock));
  CHECK_TEST_CONDITION(gen_chainlock_production_inputs::install_production_inputs(chain, lock));
  CHECK_TEST_CONDITION(chain.get_chainlock_proof(lock.height, proof) == chainlock_proof_result::ok);
  CHECK_TEST_CONDITION(chain.get_chainlock_context(context));
  CHECK_TEST_CONDITION(qwertycoin::epose::verify_chainlock_v1(lock, proof.quorum, context, true));
  CHECK_TEST_CONDITION(chain.get_chainlock_proof(proof.height, proof) == chainlock_proof_result::ok);
  CHECK_TEST_CONDITION(gen_chainlock_production_inputs::restore_test_quorum(chain));
  return true;
}

bool gen_chainlock_rpc_old_carrier::check_reorg(core& c, size_t, const std::vector<test_event_entry>&)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_rpc_old_carrier::check_reorg");
  const Blockchain& chain = c.get_blockchain_storage();
  uint64_t carrier_height = 0;
  chainlock_record carrier;
  CHECK_TEST_CONDITION(!chain.get_db().get_last_chainlock_carrier(carrier_height, carrier));
  chainlock_proof proof;
  CHECK_TEST_CONDITION(chain.get_chainlock_proof(0, proof) == chainlock_proof_result::no_lock);
  return true;
}
