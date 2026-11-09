#pragma once

#include <stddef.h>

struct hardfork_t;

hardfork_t contracts_hardfork();

extern const hardfork_t devnet_hard_forks[];
extern const size_t num_devnet_hard_forks;
