// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2020, The Qwertycoin Group.
//
// This file is part of Qwertycoin.
//
// Qwertycoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Qwertycoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Qwertycoin.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <atomic>

namespace unit_test
{
  class call_counter
  {
  public:
    call_counter()
      : m_counter(0)
    {
    }

    void inc() volatile
    {
      // memory_order_relaxed is enough for call counter
      m_counter.fetch_add(1, std::memory_order_relaxed);
    }

    size_t get() volatile const
    {
      return m_counter.load(std::memory_order_relaxed);
    }

    void reset() volatile
    {
      m_counter.store(0, std::memory_order_relaxed);
    }

  private:
    std::atomic<size_t> m_counter;
  };
}
