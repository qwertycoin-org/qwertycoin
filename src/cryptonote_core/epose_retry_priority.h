#pragma once

#include "crypto/hash.h"
#include "cryptonote_basic/difficulty.h"

namespace cryptonote
{
  struct epose_retry_priority
  {
    bool verified = false;
    uint64_t lock_height = 0;
    crypto::hash lock_hash{};
    difficulty_type cumulative_difficulty{};
    crypto::hash tip{};
  };

  inline bool better_epose_retry_priority(const epose_retry_priority &candidate,
      const epose_retry_priority &current)
  {
    if (candidate.verified != current.verified)
      return candidate.verified;
    if (candidate.verified)
    {
      if (candidate.lock_height != current.lock_height)
        return candidate.lock_height > current.lock_height;
      if (candidate.lock_hash != current.lock_hash)
        return candidate.lock_hash < current.lock_hash;
    }
    if (candidate.cumulative_difficulty != current.cumulative_difficulty)
      return candidate.cumulative_difficulty > current.cumulative_difficulty;
    return candidate.tip < current.tip;
  }
}
