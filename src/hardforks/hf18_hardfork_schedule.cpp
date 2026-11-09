#include "hf18_hardfork_schedule.h"

#include "hardforks.h"
#include "cryptonote_config.h"

hardfork_t contracts_hardfork()
{
  return { HF_VERSION_QWC_CONTRACTS, QWC_DEVNET_CONTRACTS_ACTIVATION_HEIGHT, 0, 1787788801 };
}

const hardfork_t devnet_hard_forks[] = {
  { HF_VERSION_QWC_EPOSE, 0, 0, 1787788800 },
  contracts_hardfork(),
};
const size_t num_devnet_hard_forks = sizeof(devnet_hard_forks) / sizeof(devnet_hard_forks[0]);
