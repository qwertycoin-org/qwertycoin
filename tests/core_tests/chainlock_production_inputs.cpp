#include "blockchain_test_access.h"
#include "chainlock_production_inputs.h"
#include "../epose_membership_test_access.h"
#include "cryptonote_core/blockchain.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include <algorithm>

using namespace cryptonote;
using namespace qwertycoin::epose;

bool gen_chainlock_production_inputs::generate(std::vector<test_event_entry>& events) const
{
  ring_chain chain;
  CHECK_AND_ASSERT_MES(start_chain(events, chain), false, "chain");
  block locked, carrier;
  CHECK_AND_ASSERT_MES(extend(events, chain, chain.tip, chain.miner, locked), false, "locked");
  blobdata lock;
  CHECK_AND_ASSERT_MES(make_lock_blob(events, full_lock(locked), lock), false, "lock");
  CHECK_AND_ASSERT_MES(extend(events, chain, locked, chain.miner, carrier, lock), false, "carrier");
  DO_CALLBACK(events, "check_inputs");
  return true;
}

bool gen_chainlock_production_inputs::check_inputs(core& c, size_t, const std::vector<test_event_entry>&)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_production_inputs::check_inputs");
  Blockchain& chain = c.get_blockchain_storage();
  chainlock_proof carrier;
  CHECK_TEST_CONDITION(chain.get_chainlock_proof(0, carrier) == chainlock_proof_result::ok);
  chainlock_v1 lock;
  CHECK_TEST_CONDITION(parse_chainlock_v1(carrier.lock_blob, lock));
  CHECK_TEST_CONDITION(install_production_inputs(chain, lock));

  membership_snapshot_v2 independent_snapshot;
  qualification_set_v2 independent_qualification;
  CHECK_TEST_CONDITION(chain.get_epose_membership_snapshot_v2(lock.epoch, independent_snapshot)
      == Blockchain::epose_query_status::found);
  CHECK_TEST_CONDITION(chain.get_epose_qualification_v2(lock.epoch - 1, independent_qualification)
      == Blockchain::epose_query_status::found);
  chainlock_quorum_inputs inputs;
  CHECK_TEST_CONDITION(chain.get_chainlock_quorum_inputs(lock.epoch, inputs));
  CHECK_TEST_CONDITION(!inputs.test_override);
  CHECK_EQ(inputs.snapshot_total, independent_snapshot.members.size());
  CHECK_EQ(inputs.qualification_total, independent_qualification.qualified_nodes.size());
  CHECK_EQ(inputs.snapshot_height, independent_snapshot.anchor_height);
  CHECK_EQ(inputs.snapshot_block_id, independent_snapshot.anchor_hash);
  CHECK_EQ(inputs.qualification_height, independent_qualification.closed_height);
  CHECK_EQ(inputs.snapshot_members.size(), independent_snapshot.members.size());
  for (size_t index = 0; index < independent_snapshot.members.size(); ++index)
    CHECK_EQ(inputs.snapshot_members[index], independent_snapshot.members[index].service_public_key);
  CHECK_TEST_CONDITION(inputs.qualified_members == independent_qualification.qualified_nodes);
  const auto candidates = chainlock_candidates_v1(independent_snapshot, independent_qualification);
  CHECK_EQ(candidates.size(), signer_count);
  const auto independent_quorum = select_chainlock_quorum_v1(lock.epoch, independent_snapshot.anchor_hash, candidates, inputs.context);
  CHECK_TEST_CONDITION(independent_quorum.members == carrier.quorum.members);
  chainlock_proof production;
  CHECK_TEST_CONDITION(chain.get_chainlock_proof(0, production) == chainlock_proof_result::ok);
  CHECK_TEST_CONDITION(production.quorum.members == independent_quorum.members);
  CHECK_TEST_CONDITION(blockchain_test_access::verify_complete_chainlock(chain, lock, true));
  auto& stored_membership = const_cast<membership_pipeline_v2&>(blockchain_test_access::coordinator(chain)->state().membership());
  membership_pipeline_test_access::prune_snapshot(stored_membership, lock.epoch);
  CHECK_TEST_CONDITION(chain.chainlock_quorum_inputs_require_reconstruction(lock.epoch));
  chainlock_proof reconstructed;
  CHECK_TEST_CONDITION(chain.get_chainlock_proof(0, reconstructed)
      == chainlock_proof_result::height_not_locked);
  CHECK_TEST_CONDITION(chain.get_chainlock_proof(0, reconstructed, true)
      == chainlock_proof_result::ok);
  CHECK_TEST_CONDITION(reconstructed.quorum.members == production.quorum.members);

  consensus_parameters_v2 parameters = blockchain_test_access::parameters(chain);
  parameters.parameter_set_hash.data[0] = 1;
  blockchain_test_access::parameters(chain) = parameters;
  uint64_t previous_anchor = 0, previous_deadline = 0, anchor = 0;
  CHECK_TEST_CONDITION(timing().committee_anchor(lock.epoch - 1, previous_anchor));
  CHECK_TEST_CONDITION(timing().evidence_deadline(lock.epoch - 1, previous_deadline));
  CHECK_TEST_CONDITION(timing().committee_anchor(lock.epoch, anchor));
  chainlock_quorum_v1 large_tip_quorum;
  const auto install_pruned_membership = [&](const crypto::hash &anchor_hash) {
    blockchain_test_access::coordinator(chain).reset(new consensus_coordinator_v2(parameters));
    auto& membership = const_cast<membership_pipeline_v2&>(blockchain_test_access::coordinator(chain)->state().membership());
    CHECK_TEST_CONDITION(membership.valid());
    auto undo = membership.checkpoint();
    for (uint8_t index = 1; index <= 62; ++index)
    {
      frozen_member_v2 member{};
      member.service_public_key.data[0] = index;
      member.identity_id.data[0] = index;
      const crypto::hash lease_hash = crypto::cn_fast_hash(&member, sizeof(member));
      if (index <= 60)
        membership_pipeline_test_access::add_member(membership, undo,
            lock.epoch - 1, member, lease_hash);
      if (index >= 2)
        membership_pipeline_test_access::add_member(membership, undo,
            lock.epoch, member, lease_hash);
      if (index <= 60 && index != 7)
        membership_pipeline_test_access::qualify(membership, undo,
            lock.epoch - 1, member.service_public_key);
    }
    CHECK_TEST_CONDITION(membership.freeze_membership(lock.epoch - 1, previous_anchor,
        chain.get_db().get_block_hash_from_height(previous_anchor), &undo) == pipeline_status_v2::accepted);
    CHECK_TEST_CONDITION(membership.close_qualification(lock.epoch - 1, previous_deadline, &undo)
        == pipeline_status_v2::accepted);
    CHECK_TEST_CONDITION(membership.freeze_membership(lock.epoch, anchor, anchor_hash, &undo)
        == pipeline_status_v2::accepted);
    const auto candidates = chainlock_candidates_v1(*membership.snapshot(lock.epoch),
        *membership.qualification(lock.epoch - 1));
    CHECK_TEST_CONDITION(candidates.size() > QWC_CHAINLOCK_QUORUM_MAX);
    const chainlock_context_v1 context{FAKECHAIN,
        chain.get_db().get_block_hash_from_height(0), parameters.parameter_set_hash};
    large_tip_quorum = select_chainlock_quorum_v1(lock.epoch, anchor_hash,
        candidates, context);
    CHECK_TEST_CONDITION(large_tip_quorum.can_lock());
    membership.prune_history(lock.epoch + EPOSE_STATE_RETENTION_EPOCHS + 1);
    mark_test_state_at_tip(chain);
    return true;
  };
  const crypto::hash main_anchor = chain.get_db().get_block_hash_from_height(anchor);
  CHECK_TEST_CONDITION(install_pruned_membership(main_anchor));
  CHECK_TEST_CONDITION(chain.chainlock_quorum_inputs_require_reconstruction(lock.epoch));
  CHECK_TEST_CONDITION(!chain.get_chainlock_quorum_inputs(lock.epoch, inputs));
  CHECK_TEST_CONDITION(chain.get_chainlock_quorum_inputs(lock.epoch, inputs, 0, 256, true));
  CHECK_EQ(inputs.snapshot_height, anchor);
  CHECK_EQ(inputs.snapshot_block_id, main_anchor);
  CHECK_EQ(inputs.qualification_height, previous_deadline);
  const auto reconstructed_quorum = select_chainlock_quorum_v1(lock.epoch,
      inputs.snapshot_block_id,
      chainlock_candidates_v1(inputs.snapshot_members, inputs.qualified_members),
      inputs.context);
  CHECK_TEST_CONDITION(reconstructed_quorum.members == large_tip_quorum.members);
  CHECK_TEST_CONDITION(!inputs.test_override);
  CHECK_TEST_CONDITION(chain.get_chainlock_proof(0, production)
      == chainlock_proof_result::height_not_locked);
  const uint64_t selections_before = chain.chainlock_quorum_selection_count();
  CHECK_TEST_CONDITION(chain.get_chainlock_proof(0, production, true)
      == chainlock_proof_result::height_not_locked);
  CHECK_EQ(chain.chainlock_quorum_selection_count(), selections_before + 1);
  crypto::hash wrong_anchor = main_anchor;
  wrong_anchor.data[0] ^= 1;
  CHECK_TEST_CONDITION(install_pruned_membership(wrong_anchor));
  CHECK_TEST_CONDITION(!chain.get_chainlock_quorum_inputs(lock.epoch, inputs, 0, 256, true));

  CHECK_TEST_CONDITION(install_production_inputs(chain, lock));
  auto& rpc_membership = const_cast<membership_pipeline_v2&>(
      blockchain_test_access::coordinator(chain)->state().membership());
  auto rpc_undo = rpc_membership.checkpoint();
  for (const auto& signer : signers())
    membership_pipeline_test_access::qualify(rpc_membership, rpc_undo,
        lock.epoch, signer.public_key);
  uint64_t deadline = 0;
  CHECK_TEST_CONDITION(timing().evidence_deadline(lock.epoch, deadline));
  CHECK_TEST_CONDITION(rpc_membership.close_qualification(lock.epoch, deadline,
      &rpc_undo) == pipeline_status_v2::accepted);
  crypto::hash digest{};
  for (uint64_t epoch = 0; epoch <= lock.epoch; ++epoch)
  {
    auto fold_undo = rpc_membership.checkpoint();
    CHECK_TEST_CONDITION(rpc_membership.fold_epoch(epoch, epoch + 1, digest, &fold_undo));
  }
  mark_test_state_at_tip(chain);
  membership_snapshot_v2 folded_snapshot;
  qualification_set_v2 folded_qualification;
  CHECK_TEST_CONDITION(chain.get_epose_membership_snapshot_v2(lock.epoch, folded_snapshot)
      == Blockchain::epose_query_status::found);
  CHECK_TEST_CONDITION(chain.get_epose_qualification_v2(lock.epoch, folded_qualification)
      == Blockchain::epose_query_status::found);
  std::vector<crypto::public_key> qualified_nodes;
  CHECK_TEST_CONDITION(chain.get_epose_qualified_service_nodes(lock.epoch, qualified_nodes)
      == Blockchain::epose_query_status::found);
  CHECK_TEST_CONDITION(qualified_nodes == folded_qualification.qualified_nodes);
  CHECK_TEST_CONDITION(!folded_qualification.qualified_nodes.empty());
  CHECK_TEST_CONDITION(!chain.get_epose_committee_v2(lock.epoch, 0,
      signers().front().public_key, folded_snapshot.anchor_hash).empty());
  rpc_membership.prune_history(lock.epoch + EPOSE_STATE_RETENTION_EPOCHS + 1);
  mark_test_state_at_tip(chain);
  CHECK_TEST_CONDITION(chain.get_epose_membership_snapshot_v2(lock.epoch, folded_snapshot)
      == Blockchain::epose_query_status::absent);
  CHECK_TEST_CONDITION(chain.get_epose_qualification_v2(lock.epoch, folded_qualification)
      == Blockchain::epose_query_status::absent);
  CHECK_TEST_CONDITION(chain.get_epose_qualified_service_nodes(lock.epoch, qualified_nodes)
      == Blockchain::epose_query_status::absent);
  CHECK_TEST_CONDITION(qualified_nodes.empty());
  CHECK_TEST_CONDITION(chain.get_epose_committee_v2(lock.epoch, 0,
      signers().front().public_key, folded_snapshot.anchor_hash).empty());
  CHECK_TEST_CONDITION(chain.epose_qualification_pruned_v2(lock.epoch));
  return true;
}

bool gen_chainlock_production_inputs::install_production_inputs(Blockchain& chain, const chainlock_v1& lock)
{
  DEFINE_TESTS_ERROR_CONTEXT("gen_chainlock_production_inputs::install_production_inputs");
  uint64_t anchor_height = 0, qualification_height = 0;
  CHECK_TEST_CONDITION(timing().committee_anchor(lock.epoch, anchor_height));
  CHECK_TEST_CONDITION(timing().evidence_deadline(lock.epoch - 1, qualification_height));

  consensus_parameters_v2 parameters{};
  parameters.nettype = FAKECHAIN;
  parameters.genesis_hash = chain.get_db().get_block_hash_from_height(0);
  parameters.parameter_set_hash.data[0] = 1;
  parameters.timing = timing();
  parameters.admission = {admission_work_algorithm_v2::randomx, 1};
  parameters.committee = {1, 1, 1, 1, 1, {0}};
  blockchain_test_access::parameters(chain) = parameters;
  blockchain_test_access::coordinator(chain).reset(new consensus_coordinator_v2(parameters));
  blockchain_test_access::parameters(chain).parameter_set_hash = crypto::null_hash;
  auto undo = blockchain_test_access::coordinator(chain)->preview_state().membership().checkpoint();
  blockchain_test_access::test_quorum(chain) = boost::none;
  auto& membership = const_cast<membership_pipeline_v2&>(blockchain_test_access::coordinator(chain)->state().membership());
  qualification_set_v2 qualification{};
  qualification.epoch = lock.epoch - 1;
  qualification.closed_height = qualification_height;
  const auto keys = signers();
  uint8_t member_id = 1;
  const auto add_member = [&](const crypto::public_key &key) {
    frozen_member_v2 member{};
    member.service_public_key = key;
    member.identity_id.data[0] = member_id;
    admission_lease_v2 lease{};
    lease.member = member;
    lease.target_epoch = lock.epoch;
    lease.lease_hash.data[0] = member_id++;
    membership_pipeline_test_access::add_member(membership, undo, lock.epoch,
        member, lease.lease_hash);
  };
  for (const auto& key : keys)
  {
    add_member(key.public_key);
    qualification.qualified_nodes.push_back(key.public_key);
  }
  crypto::public_key snapshot_only{};
  snapshot_only.data[0] = 1;
  add_member(snapshot_only);
  crypto::public_key qualified_only{};
  qualified_only.data[0] = 2;
  CHECK_TEST_CONDITION(std::none_of(keys.begin(), keys.end(), [&](const gen_chainlock_base::signer_key& key) {
    return key.public_key == snapshot_only || key.public_key == qualified_only;
  }));
  qualification.qualified_nodes.push_back(qualified_only);
  CHECK_TEST_CONDITION(membership.freeze_membership(lock.epoch, anchor_height,
      chain.get_db().get_block_hash_from_height(anchor_height), &undo) == pipeline_status_v2::accepted);
  membership_pipeline_test_access::set_qualification(membership, undo,
      lock.epoch - 1, qualification);
  mark_test_state_at_tip(chain);

  return true;
}

void gen_chainlock_production_inputs::mark_test_state_at_tip(Blockchain& chain)
{
  blockchain_test_access::validation_height(chain) = chain.get_db().height();
  blockchain_test_access::validation_tip(chain) = chain.get_db().top_block_hash();
  blockchain_test_access::validation_checked_generation(chain) = blockchain_test_access::validation_generation(chain);
  blockchain_test_access::validation_coordinator_generation(chain) = blockchain_test_access::coordinator(chain)->generation();
  blockchain_test_access::validation_result(chain) = true;
}

bool gen_chainlock_production_inputs::restore_test_quorum(Blockchain& chain)
{
  blockchain_test_access::coordinator(chain).reset();
  blockchain_test_access::parameters(chain) = {};
  std::vector<crypto::public_key> keys;
  for (const signer_key &signer : signers())
    keys.push_back(signer.public_key);
  return chain.set_chainlock_test_quorum(timing(), std::move(keys));
}
