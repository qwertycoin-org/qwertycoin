#include "contracts.h"
#include "../unit_tests/vm_test_utils.h"

using namespace cryptonote;

bool gen_contracts_forged_input_skips_vm::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(generate_ring_chain(events, chain), false, "ring chain");

  transaction deployment;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, deploy_call(), deployment, 0), false, "deployment");
  push_kept_txs(events, {deployment});
  block deployed;
  CHECK_AND_ASSERT_MES(construct_next_block(chain, chain.tip, chain.miner, fork_version, {deployment}, deployed), false, "deployment block");
  events.push_back(deployed);

  transaction forged;
  CHECK_AND_ASSERT_MES(construct_call_tx(chain, invoke_call(vm_test::m_unreachable, {}), forged, 1), false, "call");
  CHECK_AND_ASSERT_MES(!forged.rct_signatures.p.CLSAGs.empty(), false, "ring signature");
  forged.rct_signatures.p.CLSAGs.front().s.front().bytes[0] ^= 1;
  forged.invalidate_hashes();
  DO_CALLBACK(events, "mark_invalid_tx");
  events.push_back(std::vector<transaction>{forged});
  DO_CALLBACK(events, "check_pool_rejects_forged_input");
  return true;
}
