// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <string>

#include "bvm2/bvm2.h"
#include "gas.h"
#include "vm.h"

namespace vm
{
  inline beam::Blob to_blob(const std::string& s)
  {
    return beam::Blob(s.data(), static_cast<uint32_t>(s.size()));
  }

  inline std::string to_string(const beam::Blob& b)
  {
    return std::string(static_cast<const char*>(b.p), b.n);
  }

  inline status classify(const beam::Exc& e)
  {
    if (e.m_Type == beam::bvm2::ErrorSubType::NoCharge)
      return status::out_of_gas;
    return status::trap;
  }

  class metered_processor : public beam::bvm2::ProcessorContract
  {
  public:
    void charge(uint64_t units) { DischargeUnits(units); }

    void start(const crypto::hash& cid, uint32_t method, const std::string& args)
    {
      InitStackPlus(Stack::AlignUp(static_cast<uint32_t>(args.size())));
      m_Stack.PushAlias(to_blob(args));
      CallFar(cid, method, m_Stack.get_AlasSp(), static_cast<uint32_t>(args.size()), 0);
    }

    void run()
    {
      while (!IsDone())
      {
        uint8_t op = m_Instruction.m_p0 < m_Instruction.m_p1 ? *m_Instruction.m_p0 : 0;
        DischargeUnits(gas::opcode_cost(op));
        if (op == 0xf1)
        {
          auto instruction = m_Instruction;
          instruction.Read1();
          uint32_t words = 0;
          for (unsigned shift = 0; shift < 35; shift += 7)
          {
            const uint8_t byte = instruction.Read1();
            words |= uint32_t(byte & 0x7f) << shift;
            if (!(byte & 0x80))
              break;
          }
          DischargeUnits(gas::local_word * words);
        }
        RunOnce();
      }
    }
  };
}
