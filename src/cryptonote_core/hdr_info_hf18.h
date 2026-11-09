#pragma once

#include "contract_state.h"

namespace cryptonote
{
  class Blockchain;
  void bind_hdr_info_hf18(const Blockchain& chain, contract_block_context& context);
  inline void bind_hdr_info_hf18(const contract_block_context& context, vm::call& request)
  {
    request.headers = context.headers;
    request.hdr_info_active = context.hdr_info_active;
  }
}
