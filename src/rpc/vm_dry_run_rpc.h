#pragma once

#include <algorithm>
#include <cstdint>
#include <string>

#include "cryptonote_config.h"

namespace cryptonote
{
  constexpr const char* RESTRICTED_VM_DRY_RUN_GAS_STATUS = "Gas exceeds restricted VM dry run cap";

  inline uint64_t vm_dry_run_cost(uint64_t gas_limit)
  {
    return std::max<uint64_t>(1, gas_limit / 10000 + (gas_limit % 10000 != 0));
  }

  inline bool vm_dry_run_gas_allowed(uint64_t gas_limit, bool restricted)
  {
    return !restricted || gas_limit <= QWC_VM_RESTRICTED_DRY_RUN_GAS_CAP;
  }

  inline bool vm_dry_run_retry_at_restricted_cap(bool estimating, uint64_t gas_limit, const std::string& status)
  {
    return estimating && gas_limit > QWC_VM_RESTRICTED_DRY_RUN_GAS_CAP && status == RESTRICTED_VM_DRY_RUN_GAS_STATUS;
  }
}
