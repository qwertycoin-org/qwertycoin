#include <gtest/gtest.h>

#include "epose_coordinator_fixture.h"
#include "epose_state_retention_fixture.h"
#include "heap_usage.h"

TEST(epose_memory_soak, many_epoch_transitions_have_bounded_heap_growth)
{
  using namespace qwertycoin::epose;
  const auto configuration = epose_coordinator_fixture::parameters(
      empty_qualification_policy_v2::miner_fallback);
  block_transition_v2 transition(configuration.nettype,
      configuration.genesis_hash, configuration.parameter_set_hash,
      configuration.timing, configuration.admission,
      configuration.committee, configuration.limits);
  ASSERT_TRUE(transition.valid());
  epose_state_retention_fixture::empty_contexts contexts;
  cryptonote::transaction coinbase{};
  size_t warmed_heap = 0;
  for (uint64_t height = 0; height < 720 * 8; ++height)
  {
    std::vector<block_transaction_context_v2> transactions{
        {&coinbase, true, nullptr}};
    cryptonote::transaction registration;
    if (height % 720 == 0)
    {
      auto member = epose_coordinator_fixture::make_enrollment(
          configuration, "soak-" + std::to_string(height));
      member.descriptor.effective_epoch = height / 720 + 1;
      member.descriptor.expiry_epoch = height / 720 + 3;
      registration = epose_coordinator_fixture::transaction_with(configuration,
          {epose_coordinator_fixture::lifecycle_record(configuration, member)});
      transactions.push_back({&registration, false, nullptr});
    }
    const crypto::hash block_hash = crypto::cn_fast_hash(&height, sizeof(height));
    block_apply_summary_v2 summary{};
    ASSERT_EQ(block_transition_status_v2::accepted,
        transition.apply_block(HF_VERSION_QWC_CONTRACTS, height,
            block_hash, transactions, contexts, summary));
    if (height == 720 * 2 - 1)
      warmed_heap = test_heap_usage::in_use();
  }
#if defined(__APPLE__) || defined(__GLIBC__)
  EXPECT_LE(test_heap_usage::in_use(), warmed_heap + 1024 * 1024);
#endif
}
