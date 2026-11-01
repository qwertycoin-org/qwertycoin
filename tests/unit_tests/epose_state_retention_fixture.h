#pragma once

#include "epose/block_transition_v2.h"

namespace epose_state_retention_fixture
{
  using namespace qwertycoin::epose;

  class empty_contexts final : public canonical_context_source_v2
  {
  public:
    bool block_hash(uint64_t, crypto::hash &) const override { return false; }
    bool round_anchor(uint64_t, uint64_t, crypto::hash &) const override { return false; }
  };

  inline block_transition_v2 make_transition(
      const epoch_timing_v2 &timing = epoch_timing_v2{0, 720, 60},
      size_t undo_limit = 2160)
  {
    const crypto::hash genesis = crypto::cn_fast_hash("retention-genesis", 17);
    const crypto::hash parameters = crypto::cn_fast_hash("retention-parameters", 20);
    admission_policy_v2 admission{admission_work_algorithm_v2::randomx, 1};
    committee_policy_v2 committee{1, 1, 1, 1, 1, {0}};
    block_transition_limits_v2 limits{};
    limits.max_envelopes_per_transaction = 1;
    limits.envelope.max_envelope_bytes = 4096;
    limits.envelope.max_records = 8;
    limits.envelope.max_record_payload_bytes = 2048;
    limits.envelope.max_signature_verifications = 16;
    limits.envelope.max_admission_verifications = 4;
    limits.envelope.supported_record_versions = {0, 1, 1, 1, 1, 1};
    limits.block = {16384, 32, 32, 8};
    limits.max_recent_undo_blocks = undo_limit;
    return {cryptonote::TESTNET, genesis, parameters,
        timing, admission, committee, limits};
  }
}
