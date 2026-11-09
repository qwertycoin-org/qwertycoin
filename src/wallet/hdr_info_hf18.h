#pragma once

#include "cryptonote_basic/cryptonote_format_utils.h"
#include "include_base_utils.h"
#include "rpc/core_rpc_server_commands_defs.h"
#include "vm/hdr_info_hf18.h"

namespace tools
{
  template <typename GetBlock, typename MainChain>
  void bind_wallet_hdr_info_hf18(vm::call& request, GetBlock get_block, const MainChain& main_chain)
  {
    request.hdr_info_active = true;
    const vm::parent_header parent = request.parent;
    request.headers = [parent, get_block, &main_chain](uint64_t height, vm::parent_header& result) {
      const auto read_hash = [&get_block](const crypto::hash& hash, uint64_t at, vm::parent_header& header) {
        cryptonote::COMMAND_RPC_GET_BLOCK::request req{};
        cryptonote::COMMAND_RPC_GET_BLOCK::response response{};
        req.hash = epee::string_tools::pod_to_hex(hash);
        if (!get_block(req, response))
          return false;
        cryptonote::blobdata blob;
        cryptonote::block found;
        if (!epee::string_tools::parse_hexstr_to_binbuff(response.blob, blob)
            || !cryptonote::parse_and_validate_block_from_blob(blob, found)
            || cryptonote::get_block_hash(found) != hash)
          return false;
        header = {at, hash, found.prev_id, found.timestamp};
        return true;
      };
      const auto read_main = [&main_chain, &read_hash](uint64_t at, vm::parent_header& header) {
        if (at < main_chain.offset() || at >= main_chain.size())
          return false;
        return read_hash(main_chain[at], at, header);
      };
      return vm::resolve_ancestor_header(parent, height, result, read_main, read_hash);
    };
  }
}
