#pragma once

#include "cryptonote_core/blockchain.h"
#include "cryptonote_core/tx_pool.h"

namespace cryptonote_test
{
  struct BlockchainAndPool
  {
    cryptonote::tx_memory_pool txpool;
    cryptonote::Blockchain bc;
    BlockchainAndPool(): txpool(bc), bc(txpool) {}
  };
}
