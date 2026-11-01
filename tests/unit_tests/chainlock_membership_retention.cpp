#include <gtest/gtest.h>

#include <vector>

#include "epose/membership_v2.h"
#include "epose/block_transition_v2.h"
#include "epose/chainlock_v1.h"
#include "epose/state_image_v2.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "epose_state_retention_fixture.h"
#include "../epose_membership_test_access.h"

using namespace qwertycoin::epose;

TEST(chainlock_membership_retention, carrier_tracks_pruned_snapshot_and_previous_qualification)
{
  const crypto::hash genesis = crypto::cn_fast_hash("membership-genesis", 18);
  const crypto::hash parameters = crypto::cn_fast_hash("membership-parameters", 21);
  const crypto::hash anchor_one = crypto::cn_fast_hash("anchor-one", 10);
  const crypto::hash anchor_two = crypto::cn_fast_hash("anchor-two", 10);
  const epoch_timing_v2 timing{0, 20, 5};
  const admission_policy_v2 admission{admission_work_algorithm_v2::randomx, 1};
  const committee_policy_v2 committee{1, 1, 1, 1, 1, {0}};
  membership_pipeline_v2 membership{cryptonote::TESTNET, genesis, parameters,
      timing, admission, committee};
  ASSERT_TRUE(membership.valid());
  auto undo = membership.checkpoint();
  ASSERT_EQ(pipeline_status_v2::accepted,
      membership.freeze_membership(1, 15, anchor_one, &undo));
  ASSERT_EQ(pipeline_status_v2::accepted,
      membership.close_qualification(1, 34, &undo));
  ASSERT_EQ(pipeline_status_v2::accepted,
      membership.freeze_membership(2, 35, anchor_two, &undo));
  ASSERT_EQ(pipeline_status_v2::accepted,
      membership.close_qualification(2, 54, &undo));

  const crypto::hash state_hash = membership.state_hash();
  membership_pipeline_v2 without_carrier = membership;
  membership.prune_history(5);
  ASSERT_EQ(nullptr, membership.snapshot(2));
  ASSERT_EQ(nullptr, membership.qualification(1));
  ASSERT_EQ(nullptr, membership.qualification(2));
  membership_snapshot_v2 reconstructed_snapshot;
  qualification_set_v2 reconstructed_qualification;
  EXPECT_FALSE(membership.get_snapshot(2, reconstructed_snapshot));
  EXPECT_FALSE(membership.get_qualification(1, reconstructed_qualification));
  EXPECT_TRUE(membership.qualification_pruned(1));
  ASSERT_TRUE(membership.get_snapshot(2, reconstructed_snapshot, true));
  ASSERT_TRUE(membership.get_qualification(1, reconstructed_qualification, true));
  EXPECT_EQ(without_carrier.snapshot(2)->snapshot_hash,
      reconstructed_snapshot.snapshot_hash);
  EXPECT_EQ(without_carrier.qualification(1)->qualification_hash,
      reconstructed_qualification.qualification_hash);
  ASSERT_EQ(state_hash, membership.state_hash());

  ASSERT_TRUE(membership.retain_carrier(2));
  membership.prune_history(5);
  ASSERT_EQ(nullptr, membership.snapshot(2));
  ASSERT_EQ(nullptr, membership.qualification(1));
  ASSERT_EQ(nullptr, membership.qualification(2));
  ASSERT_TRUE(membership.get_snapshot(2, reconstructed_snapshot, true));
  ASSERT_TRUE(membership.get_qualification(1, reconstructed_qualification, true));
  ASSERT_EQ(state_hash, membership.state_hash());

  membership = without_carrier;
  membership.prune_history(5);
  EXPECT_EQ(nullptr, membership.snapshot(2));
  EXPECT_EQ(nullptr, membership.qualification(1));
  EXPECT_EQ(nullptr, membership.qualification(2));
  EXPECT_EQ(state_hash, membership.state_hash());

  crypto::hash digest{};
  ASSERT_TRUE(membership.fold_epoch(0, 1, digest, &undo));
  ASSERT_TRUE(membership.fold_epoch(1, 2, digest, &undo));
  ASSERT_TRUE(membership.fold_epoch(2, 3, digest, &undo));
  state_image_writer_v2 writer;
  ASSERT_TRUE(membership.write_image(writer));
  state_image_reader_v2 reader(writer.data());
  membership_pipeline_v2 restored{cryptonote::TESTNET, genesis, parameters,
      timing, admission, committee};
  ASSERT_TRUE(restored.read_image(reader, 3));
  ASSERT_TRUE(reader.finished());
  EXPECT_EQ(nullptr, restored.snapshot(2));
  EXPECT_EQ(nullptr, restored.qualification(1));
  ASSERT_TRUE(restored.get_snapshot(2, reconstructed_snapshot, true));
  ASSERT_TRUE(restored.get_qualification(1, reconstructed_qualification, true));
  EXPECT_EQ(membership.state_hash(), restored.state_hash());
}

TEST(chainlock_membership_retention, transition_replays_and_releases_late_carrier)
{
  const epoch_timing_v2 timing{0, 20, 5};
  auto transition = epose_state_retention_fixture::make_transition(timing);
  auto replayed = epose_state_retention_fixture::make_transition(timing);
  ASSERT_TRUE(transition.valid());
  ASSERT_TRUE(replayed.valid());

  chainlock_v1 lock;
  lock.height = 40;
  lock.block_hash = crypto::cn_fast_hash(&lock.height, sizeof(lock.height));
  lock.epoch = 2;
  lock.signers[0] = 1;
  lock.signatures.resize(1);
  const cryptonote::blobdata lock_blob = serialize_chainlock_v1(lock);
  epose_state_retention_fixture::empty_contexts contexts;
  crypto::hash before_carrier{};
  for (uint64_t height = 0; height <= 121; ++height)
  {
    cryptonote::transaction coinbase{};
    if (height == 121)
    {
      ASSERT_TRUE(cryptonote::add_chainlock_to_tx_extra(coinbase.extra, lock_blob));
    }
    const block_transaction_context_v2 transaction{&coinbase, true, nullptr};
    const crypto::hash block_hash = crypto::cn_fast_hash(&height, sizeof(height));
    block_apply_summary_v2 summary{};
    if (height == 121)
    {
      EXPECT_EQ(pipeline_status_v2::accepted,
          transition.state().membership().read_epoch(2));
      EXPECT_EQ(nullptr, transition.state().membership().snapshot(2));
      EXPECT_EQ(nullptr, transition.state().membership().qualification(1));
      EXPECT_EQ(nullptr, transition.state().membership().qualification(2));
      before_carrier = transition.state().state_hash();
    }
    ASSERT_EQ(block_transition_status_v2::accepted,
        transition.apply_block(HF_VERSION_QWC_CONTRACTS, height,
            block_hash, {transaction}, contexts, summary));
    ASSERT_EQ(block_transition_status_v2::accepted,
        replayed.apply_block(HF_VERSION_QWC_CONTRACTS, height,
            block_hash, {transaction}, contexts, summary));
  }
  EXPECT_TRUE(transition.state().carrier_retained(2));
  EXPECT_EQ(nullptr, transition.state().membership().snapshot(2));
  EXPECT_EQ(nullptr, transition.state().membership().qualification(1));
  EXPECT_EQ(nullptr, transition.state().membership().qualification(2));
  EXPECT_EQ(before_carrier, transition.state().state_hash());
  EXPECT_EQ(transition.state().state_hash(), replayed.state().state_hash());
  EXPECT_EQ(transition.state().membership().state_hash(),
      replayed.state().membership().state_hash());
  const semantic_state_v2 *retained_state = &transition.state();
  cryptonote::transaction another_coinbase{};
  ASSERT_TRUE(cryptonote::add_chainlock_to_tx_extra(another_coinbase.extra, lock_blob));
  const block_transaction_context_v2 another_transaction{&another_coinbase, true, nullptr};
  const uint64_t next_height = 122;
  const crypto::hash next_hash = crypto::cn_fast_hash(&next_height, sizeof(next_height));
  block_apply_summary_v2 next_summary{};
  ASSERT_EQ(block_transition_status_v2::accepted,
      transition.apply_block(HF_VERSION_QWC_CONTRACTS, next_height,
          next_hash, {another_transaction}, contexts, next_summary));
  EXPECT_EQ(retained_state, &transition.state());
  ASSERT_EQ(block_transition_status_v2::accepted, transition.disconnect_tip(next_height));
  ASSERT_EQ(block_transition_status_v2::accepted, transition.disconnect_tip(121));
  EXPECT_EQ(nullptr, transition.state().membership().snapshot(2));
  EXPECT_EQ(nullptr, transition.state().membership().qualification(1));
  EXPECT_EQ(nullptr, transition.state().membership().qualification(2));
}

TEST(chainlock_membership_retention, archived_epochs_follow_carrier_retention)
{
  const crypto::hash genesis = crypto::cn_fast_hash("membership-genesis", 18);
  const crypto::hash parameters = crypto::cn_fast_hash("membership-parameters", 21);
  const epoch_timing_v2 timing{0, 720, 60};
  const admission_policy_v2 admission{admission_work_algorithm_v2::randomx, 1};
  const committee_policy_v2 committee{1, 1, 1, 1, 1, {0}};
  membership_pipeline_v2 membership{cryptonote::TESTNET, genesis, parameters,
      timing, admission, committee};
  auto setup = membership.checkpoint();
  for (uint64_t epoch = 1; epoch <= 5; ++epoch)
  {
    if (epoch == 1)
    {
      frozen_member_v2 member{};
      member.service_public_key.data[0] = 1;
      member.identity_id.data[0] = 1;
      const crypto::hash lease_hash = crypto::cn_fast_hash(&member, sizeof(member));
      membership_pipeline_test_access::add_member(membership, setup, epoch,
          member, lease_hash);
    }
    const crypto::hash anchor = crypto::cn_fast_hash(&epoch, sizeof(epoch));
    ASSERT_EQ(pipeline_status_v2::accepted,
        membership.freeze_membership(epoch, epoch * 720 - 60, anchor, &setup));
    ASSERT_EQ(pipeline_status_v2::accepted,
        membership.close_qualification(epoch, (epoch + 1) * 720 - 61, &setup));
  }
  crypto::hash digest{};
  for (uint64_t epoch = 0; epoch <= 4; ++epoch)
  {
    auto undo = membership.checkpoint();
    ASSERT_TRUE(membership.fold_epoch(epoch, epoch + 1, digest, &undo));
  }
  ASSERT_NE(nullptr, membership.snapshot(1));
  auto marker = membership.checkpoint();
  ASSERT_TRUE(membership.retain_carrier(1, &marker));
  ASSERT_TRUE(membership.retain_carrier(2, &marker));
  ASSERT_TRUE(membership.carrier_retained(1));
  ASSERT_TRUE(membership.carrier_retained(2));
  const auto before = membership.memory_usage();
  auto eviction = membership.checkpoint();
  ASSERT_TRUE(membership.fold_epoch(5, 6, digest, &eviction));
  EXPECT_EQ(nullptr, membership.snapshot(1));
  EXPECT_FALSE(membership.carrier_retained(1));
  EXPECT_TRUE(membership.carrier_retained(2));
  EXPECT_NE(nullptr, membership.snapshot(2));
  membership_snapshot_v2 pruned_snapshot;
  EXPECT_FALSE(membership.get_snapshot(1, pruned_snapshot, true));
  EXPECT_LT(membership.memory_usage().snapshots, before.snapshots);
  EXPECT_LT(membership.memory_usage().leases, before.leases);
  EXPECT_LT(membership.memory_usage().vector_payload_bytes,
      before.vector_payload_bytes);
  EXPECT_LT(membership.memory_usage().map_value_bytes, before.map_value_bytes);

  state_image_writer_v2 writer;
  ASSERT_TRUE(membership.write_image(writer));
  state_image_reader_v2 reader(writer.data());
  membership_pipeline_v2 restored{cryptonote::TESTNET, genesis, parameters,
      timing, admission, committee};
  ASSERT_TRUE(restored.read_image(reader, 6));
  EXPECT_TRUE(reader.finished());
  EXPECT_EQ(nullptr, restored.snapshot(1));
  EXPECT_FALSE(restored.carrier_retained(1));
  EXPECT_TRUE(restored.carrier_retained(2));
  EXPECT_NE(nullptr, restored.snapshot(2));
  EXPECT_EQ(membership.memory_usage().snapshots, restored.memory_usage().snapshots);
  EXPECT_EQ(membership.state_hash(), restored.state_hash());

  membership.rollback(eviction);
  EXPECT_NE(nullptr, membership.snapshot(1));
  EXPECT_TRUE(membership.carrier_retained(1));
  EXPECT_TRUE(membership.carrier_retained(2));
  EXPECT_EQ(before.snapshots, membership.memory_usage().snapshots);
  EXPECT_EQ(before.map_value_bytes, membership.memory_usage().map_value_bytes);
}

TEST(chainlock_membership_retention, block_reorg_restores_evicted_history)
{
  auto transition = epose_state_retention_fixture::make_transition({0, 20, 5}, 2300);
  ASSERT_TRUE(transition.valid());
  epose_state_retention_fixture::empty_contexts contexts;
  cryptonote::transaction coinbase{};
  const block_transaction_context_v2 transaction{&coinbase, true, nullptr};
  const auto same_memory = [](const state_memory_usage_v2 &expected,
      const state_memory_usage_v2 &actual) {
    EXPECT_EQ(expected.identities, actual.identities);
    EXPECT_EQ(expected.lifecycle_records, actual.lifecycle_records);
    EXPECT_EQ(expected.leases, actual.leases);
    EXPECT_EQ(expected.receipts, actual.receipts);
    EXPECT_EQ(expected.snapshots, actual.snapshots);
    EXPECT_EQ(expected.qualifications, actual.qualifications);
    EXPECT_EQ(expected.vector_payload_bytes, actual.vector_payload_bytes);
    EXPECT_EQ(expected.map_value_bytes, actual.map_value_bytes);
  };
  const auto connect = [&](uint64_t height) {
    const crypto::hash block_hash = crypto::cn_fast_hash(&height, sizeof(height));
    block_apply_summary_v2 summary{};
    return transition.apply_block(HF_VERSION_QWC_CONTRACTS, height,
        block_hash, {transaction}, contexts, summary);
  };
  for (uint64_t height = 0; height < 2220; ++height)
  {
    ASSERT_EQ(block_transition_status_v2::accepted, connect(height));
    if (height == 40)
    {
      ASSERT_TRUE(transition.preview_state().preview_membership().retain_carrier(1));
    }
  }
  const auto& before = transition.state().membership();
  ASSERT_NE(nullptr, before.snapshot(1));
  ASSERT_TRUE(before.carrier_retained(1));
  const crypto::hash before_hash = transition.state().state_hash();
  const std::string before_image = transition.serialize_state();
  const state_memory_usage_v2 before_memory = before.memory_usage();
  ASSERT_EQ(block_transition_status_v2::accepted, connect(2220));
  const auto& after = transition.state().membership();
  EXPECT_EQ(nullptr, after.snapshot(1));
  EXPECT_FALSE(after.carrier_retained(1));
  const crypto::hash after_hash = transition.state().state_hash();
  const std::string after_image = transition.serialize_state();
  const state_memory_usage_v2 after_memory = after.memory_usage();
  ASSERT_EQ(block_transition_status_v2::accepted, transition.disconnect_tip(2220));
  EXPECT_NE(nullptr, transition.state().membership().snapshot(1));
  EXPECT_TRUE(transition.state().membership().carrier_retained(1));
  EXPECT_EQ(before_hash, transition.state().state_hash());
  EXPECT_EQ(before_image, transition.serialize_state());
  same_memory(before_memory, transition.state().membership().memory_usage());
  ASSERT_EQ(block_transition_status_v2::accepted, connect(2220));
  EXPECT_EQ(after_hash, transition.state().state_hash());
  EXPECT_EQ(after_image, transition.serialize_state());
  same_memory(after_memory, transition.state().membership().memory_usage());
}

TEST(chainlock_membership_retention, reconstruction_rejects_tampered_commitment)
{
  const crypto::hash genesis = crypto::cn_fast_hash("membership-genesis", 18);
  const crypto::hash parameters = crypto::cn_fast_hash("membership-parameters", 21);
  const crypto::hash anchor = crypto::cn_fast_hash("anchor-two", 10);
  const epoch_timing_v2 timing{0, 20, 5};
  const admission_policy_v2 admission{admission_work_algorithm_v2::randomx, 1};
  const committee_policy_v2 committee{1, 1, 1, 1, 1, {0}};
  membership_pipeline_v2 membership{cryptonote::TESTNET, genesis, parameters,
      timing, admission, committee};
  auto undo = membership.checkpoint();
  ASSERT_EQ(pipeline_status_v2::accepted,
      membership.freeze_membership(2, 35, anchor, &undo));
  auto *snapshot = const_cast<membership_snapshot_v2 *>(membership.snapshot(2));
  ASSERT_NE(nullptr, snapshot);
  snapshot->snapshot_hash.data[0] ^= 1;
  membership.prune_history(5);
  membership_snapshot_v2 restored;
  EXPECT_FALSE(membership.reconstruct_epoch(2, restored, nullptr));
  EXPECT_FALSE(membership.get_snapshot(2, restored, true));
}

TEST(chainlock_membership_retention, retention_failure_does_not_reject_transition)
{
  auto transition = epose_state_retention_fixture::make_transition();
  ASSERT_TRUE(transition.valid());
  chainlock_v1 lock;
  lock.height = 0;
  lock.block_hash.data[0] = 1;
  lock.epoch = 999;
  lock.signers[0] = 1;
  lock.signatures.resize(1);
  cryptonote::transaction coinbase{};
  ASSERT_TRUE(cryptonote::add_chainlock_to_tx_extra(coinbase.extra,
      serialize_chainlock_v1(lock)));
  const block_transaction_context_v2 transaction{&coinbase, true, nullptr};
  epose_state_retention_fixture::empty_contexts contexts;
  block_apply_summary_v2 summary{};
  crypto::hash block_hash{};
  block_hash.data[0] = 2;
  EXPECT_EQ(block_transition_status_v2::accepted,
      transition.apply_block(HF_VERSION_QWC_CONTRACTS, 0, block_hash,
          {transaction}, contexts, summary));
  EXPECT_FALSE(transition.state().carrier_retained(lock.epoch));
}

TEST(chainlock_membership_retention, deep_replay_drops_carrier_retention)
{
  const epoch_timing_v2 timing{0, 20, 5};
  auto original = epose_state_retention_fixture::make_transition(timing, 128);
  auto replayed = epose_state_retention_fixture::make_transition(timing, 128);
  ASSERT_TRUE(original.valid());
  ASSERT_TRUE(replayed.valid());
  chainlock_v1 lock;
  lock.height = 20;
  lock.block_hash.data[0] = 1;
  lock.epoch = 2;
  lock.signers[0] = 1;
  lock.signatures.resize(1);
  const cryptonote::blobdata lock_blob = serialize_chainlock_v1(lock);
  epose_state_retention_fixture::empty_contexts contexts;
  for (uint64_t height = 0; height <= 200; ++height)
  {
    cryptonote::transaction coinbase{};
    if (height == 40)
    {
      ASSERT_TRUE(cryptonote::add_chainlock_to_tx_extra(coinbase.extra, lock_blob));
    }
    const block_transaction_context_v2 transaction{&coinbase, true, nullptr};
    const crypto::hash hash = crypto::cn_fast_hash(&height, sizeof(height));
    block_apply_summary_v2 summary{};
    ASSERT_EQ(block_transition_status_v2::accepted,
        original.apply_block(HF_VERSION_QWC_CONTRACTS, height, hash,
            {transaction}, contexts, summary));
    coinbase.extra.clear();
    ASSERT_EQ(block_transition_status_v2::accepted,
        replayed.apply_block(HF_VERSION_QWC_CONTRACTS, height, hash,
            {transaction}, contexts, summary));
  }
  ASSERT_TRUE(original.state().carrier_retained(2));
  ASSERT_FALSE(replayed.state().carrier_retained(2));
  ASSERT_NE(nullptr, original.state().membership().snapshot(2));
  ASSERT_EQ(nullptr, replayed.state().membership().snapshot(2));
  membership_snapshot_v2 alt_snapshot;
  qualification_set_v2 alt_qualification;
  ASSERT_TRUE(replayed.state().reconstruct_chainlock_membership(2,
      alt_snapshot, alt_qualification));
  EXPECT_EQ(original.state().membership().snapshot(2)->snapshot_hash,
      alt_snapshot.snapshot_hash);
  EXPECT_EQ(original.state().state_hash(), replayed.state().state_hash());
  for (uint64_t height = 200; height >= 73; --height)
    ASSERT_EQ(block_transition_status_v2::accepted, original.disconnect_tip(height));
  EXPECT_EQ(block_transition_status_v2::deep_replay_required,
      original.disconnect_tip(72));
}

TEST(chainlock_membership_retention, pruned_quorum_matches_tip_quorum)
{
  const crypto::hash genesis = crypto::cn_fast_hash("membership-genesis", 18);
  const crypto::hash parameters = crypto::cn_fast_hash("membership-parameters", 21);
  const crypto::hash previous_anchor = crypto::cn_fast_hash("anchor-one", 10);
  const crypto::hash anchor = crypto::cn_fast_hash("anchor-two", 10);
  const crypto::hash next_anchor = crypto::cn_fast_hash("anchor-three", 12);
  const epoch_timing_v2 timing{0, 20, 5};
  const admission_policy_v2 admission{admission_work_algorithm_v2::randomx, 1};
  const committee_policy_v2 committee{1, 1, 1, 1, 1, {0}};
  membership_pipeline_v2 membership{cryptonote::TESTNET, genesis, parameters,
      timing, admission, committee};
  auto undo = membership.checkpoint();
  for (uint8_t index = 1; index <= 76; ++index)
  {
    frozen_member_v2 member{};
    member.service_public_key.data[0] = index;
    member.identity_id.data[0] = index;
    for (uint64_t epoch = 1; epoch <= 4; ++epoch)
    {
      if (index >= epoch && index < epoch + 60)
        membership_pipeline_test_access::add_member(membership, undo, epoch,
            member, crypto::cn_fast_hash(&member, sizeof(member)));
      if (epoch <= 3 && index >= epoch && index < epoch + 60 && index != epoch + 5)
        membership_pipeline_test_access::qualify(membership, undo, epoch,
            member.service_public_key);
    }
  }
  ASSERT_EQ(pipeline_status_v2::accepted,
      membership.freeze_membership(1, 15, previous_anchor, &undo));
  ASSERT_EQ(pipeline_status_v2::accepted,
      membership.close_qualification(1, 34, &undo));
  ASSERT_EQ(pipeline_status_v2::accepted,
      membership.freeze_membership(2, 35, anchor, &undo));
  ASSERT_EQ(pipeline_status_v2::accepted,
      membership.close_qualification(2, 54, &undo));
  ASSERT_EQ(pipeline_status_v2::accepted,
      membership.freeze_membership(3, 55, next_anchor, &undo));
  ASSERT_EQ(pipeline_status_v2::accepted,
      membership.close_qualification(3, 74, &undo));
  const crypto::hash last_anchor = crypto::cn_fast_hash("anchor-four", 11);
  ASSERT_EQ(pipeline_status_v2::accepted,
      membership.freeze_membership(4, 75, last_anchor, &undo));
  const chainlock_context_v1 context{cryptonote::TESTNET, genesis, parameters};
  std::vector<chainlock_quorum_v1> at_tip;
  for (uint64_t epoch : {2, 3, 4})
  {
    ASSERT_NE(nullptr, membership.snapshot(epoch));
    ASSERT_NE(nullptr, membership.qualification(epoch - 1));
    at_tip.push_back(select_chainlock_quorum_v1(epoch,
        membership.snapshot(epoch)->anchor_hash,
        chainlock_candidates_v1(*membership.snapshot(epoch),
            *membership.qualification(epoch - 1)), context));
    ASSERT_TRUE(at_tip.back().can_lock());
    EXPECT_GT(chainlock_candidates_v1(*membership.snapshot(epoch),
        *membership.qualification(epoch - 1)).size(), QWC_CHAINLOCK_QUORUM_MAX);
  }
  auto prune_undo = membership.checkpoint();
  membership.prune_history(7, &prune_undo);
  ASSERT_EQ(nullptr, membership.snapshot(2));
  ASSERT_EQ(nullptr, membership.snapshot(3));
  ASSERT_EQ(nullptr, membership.snapshot(4));
  membership.rollback(prune_undo);
  ASSERT_NE(nullptr, membership.snapshot(2));
  membership.prune_history(7);
  for (uint64_t epoch : {2, 3, 4})
  {
    ASSERT_EQ(nullptr, membership.snapshot(epoch));
    ASSERT_EQ(nullptr, membership.qualification(epoch - 1));
    membership_snapshot_v2 restored_snapshot;
    qualification_set_v2 restored_qualification;
    ASSERT_TRUE(membership.get_snapshot(epoch, restored_snapshot, true));
    ASSERT_TRUE(membership.get_qualification(epoch - 1, restored_qualification, true));
    const auto reconstructed = select_chainlock_quorum_v1(epoch, restored_snapshot.anchor_hash,
        chainlock_candidates_v1(restored_snapshot, restored_qualification), context);
    EXPECT_EQ(at_tip[epoch - 2].members, reconstructed.members);
    EXPECT_EQ(at_tip[epoch - 2].threshold(), reconstructed.threshold());
    if (epoch == 3)
    {
      EXPECT_NE(at_tip[0].members, reconstructed.members);
      EXPECT_NE(at_tip[2].members, reconstructed.members);
    }
  }
}
