#include <gtest/gtest.h>

#include "epose_state_retention_fixture.h"
#include "epose/state_index_v2.h"

TEST(epose_state_retention, empty_blocks_share_one_undo_state)
{
  using namespace qwertycoin::epose;
  auto transition = epose_state_retention_fixture::make_transition();
  ASSERT_TRUE(transition.valid());

  epose_state_retention_fixture::empty_contexts contexts;
  cryptonote::transaction coinbase{};
  const block_transaction_context_v2 transaction{&coinbase, true, nullptr};
  const semantic_state_v2 *initial_state = nullptr;
  crypto::hash initial_hash{};
  for (uint64_t height = 0; height < 300; ++height)
  {
    const crypto::hash block_hash = crypto::cn_fast_hash(&height, sizeof(height));
    block_apply_summary_v2 summary{};
    ASSERT_EQ(block_transition_status_v2::accepted,
        transition.apply_block(HF_VERSION_QWC_CONTRACTS, height,
            block_hash, {transaction}, contexts, summary));
    if (height == 0)
    {
      initial_state = &transition.state();
      initial_hash = initial_state->state_hash();
    }
    EXPECT_EQ(initial_state, &transition.state());
    EXPECT_EQ(initial_hash, transition.state().state_hash());
  }
  EXPECT_EQ(300u, transition.recent_undo_count());
  for (uint64_t height = 300; height > 0; --height)
  {
    ASSERT_EQ(block_transition_status_v2::accepted, transition.disconnect_tip(height - 1));
    EXPECT_EQ(initial_state, &transition.state());
  }
  EXPECT_EQ(0u, transition.recent_undo_count());
  EXPECT_EQ(block_transition_status_v2::nonsequential_height,
      transition.disconnect_tip(0));
}

TEST(epose_state_retention, schema_one_snapshot_restart_matches_replay)
{
  using namespace qwertycoin::epose;
  auto uninterrupted = epose_state_retention_fixture::make_transition();
  auto restarted = epose_state_retention_fixture::make_transition();
  epose_state_retention_fixture::empty_contexts contexts;
  cryptonote::transaction coinbase{};
  const block_transaction_context_v2 transaction{&coinbase, true, nullptr};
  for (uint64_t height = 0; height <= 10; ++height)
  {
    const crypto::hash block_hash = crypto::cn_fast_hash(&height, sizeof(height));
    block_apply_summary_v2 summary{};
    ASSERT_EQ(block_transition_status_v2::accepted,
        uninterrupted.apply_block(HF_VERSION_QWC_EPOSE, height, block_hash,
            {transaction}, contexts, summary));
  }
  const std::string image = uninterrupted.serialize_state();
  ASSERT_FALSE(image.empty());
  ASSERT_TRUE(restarted.restore_state(10, image));
  EXPECT_EQ(1u, restarted.state().state_commitment_schema());
  EXPECT_EQ(uninterrupted.state().state_hash(), restarted.state().state_hash());
  const uint64_t height = 11;
  const crypto::hash block_hash = crypto::cn_fast_hash(&height, sizeof(height));
  block_apply_summary_v2 first_summary{};
  block_apply_summary_v2 second_summary{};
  ASSERT_EQ(block_transition_status_v2::accepted,
      uninterrupted.apply_block(HF_VERSION_QWC_EPOSE, height, block_hash,
          {transaction}, contexts, first_summary));
  ASSERT_EQ(block_transition_status_v2::accepted,
      restarted.apply_block(HF_VERSION_QWC_EPOSE, height, block_hash,
          {transaction}, contexts, second_summary));
  EXPECT_EQ(uninterrupted.state().state_hash(), restarted.state().state_hash());
}

TEST(epose_state_retention, schema_one_boundaries_do_not_store_images)
{
  using namespace qwertycoin::epose;
  auto transition = epose_state_retention_fixture::make_transition();
  epose_state_retention_fixture::empty_contexts contexts;
  cryptonote::transaction coinbase{};
  const block_transaction_context_v2 transaction{&coinbase, true, nullptr};
  for (uint64_t height = 0; height <= 721; ++height)
  {
    const crypto::hash block_hash = crypto::cn_fast_hash(&height, sizeof(height));
    block_apply_summary_v2 summary{};
    ASSERT_EQ(block_transition_status_v2::accepted,
        transition.apply_block(HF_VERSION_QWC_EPOSE, height,
            block_hash, {transaction}, contexts, summary));
    EXPECT_FALSE(requires_boundary_snapshot(
        transition.state().state_commitment_schema(), height, 720));
    if (height == 720)
    {
      state_snapshot_v2 failed{};
      failed.state_schema = QWC_EPOSE_ROLLING_STATE_SCHEMA;
      failed.parameter_set_hash = transition.state().parameter_set_hash();
      failed.height = height;
      failed.block_hash = block_hash;
      failed.state_root = transition.state().state_hash();
      EXPECT_TRUE(failed.serialize().empty());
    }
  }
  EXPECT_EQ(722u, transition.state_diagnostics().undo_entries);
}
