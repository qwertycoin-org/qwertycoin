#include "hdr_info_hf18.h"

#include "blockchain.h"
#include "blockchain_db/blockchain_db.h"
#include "vm/hdr_info_hf18.h"

#include <memory>

namespace cryptonote
{
  void bind_hdr_info_hf18(const Blockchain& chain, contract_block_context& context)
  {
    context.hdr_info_active = is_qwc_contracts_hardfork(chain.get_ideal_hard_fork_version(context.height));
    const vm::parent_header parent = context.parent;
    const auto cache = std::make_shared<vm::ancestor_header_cache>(parent);
    context.headers = [&chain, cache](uint64_t height, vm::parent_header& result) {
      const auto read_main = [&chain](uint64_t at, vm::parent_header& header) {
        const BlockchainDB& db = chain.get_db();
        if (at >= db.height())
          return false;
        const block found = db.get_block_from_height(at);
        header = {at, get_block_hash(found), found.prev_id, found.timestamp};
        return true;
      };
      const auto read_hash = [&chain](const crypto::hash& hash, uint64_t at, vm::parent_header& header) {
        block found;
        if (!chain.get_block_by_hash(hash, found) || get_block_hash(found) != hash)
          return false;
        header = {at, hash, found.prev_id, found.timestamp};
        return true;
      };
      return cache->resolve(height, result, read_main, read_hash);
    };
  }
}
