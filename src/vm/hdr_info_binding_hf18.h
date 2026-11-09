#pragma once

#include "bvm2/bvm2.h"
#include "vm.h"

namespace vm
{
  inline void bind_hdr_info_hf18(beam::bvm2::ProcessorContract& processor, const call& request)
  {
    processor.m_HdrInfoActive = request.hdr_info_active;
    processor.m_HdrInfoProvider = [&request](uint64_t height, beam::bvm2::ParentHeader& header) {
      parent_header found{};
      if (!request.headers || !request.headers(height, found))
        return false;
      header.m_Height = found.height;
      header.m_Hash = found.hash;
      header.m_Prev = found.prev;
      header.m_Timestamp = found.timestamp;
      return true;
    };
  }
}
