// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <map>
#include <boost/optional.hpp>
#include <string>

namespace vm
{
  using write_set = std::map<std::string, boost::optional<std::string>>;

  struct state_view
  {
    virtual ~state_view() = default;
    virtual bool get(const std::string& key, std::string& value) const = 0;
  };

  inline write_set undo_for(const state_view& state, const write_set& writes)
  {
    write_set undo;
    for (const auto& write : writes)
    {
      std::string previous;
      if (state.get(write.first, previous))
        undo.emplace(write.first, std::move(previous));
      else
        undo.emplace(write.first, boost::none);
    }
    return undo;
  }
}
