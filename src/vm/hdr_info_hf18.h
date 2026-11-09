#pragma once

#include <map>
#include <utility>

#include <boost/optional.hpp>

#include "vm.h"

namespace vm
{
  class ancestor_header_cache
  {
  public:
    explicit ancestor_header_cache(const parent_header& parent) : m_parent(parent)
    {
      m_ancestors.emplace(parent.height, parent);
    }

    template <typename ReadMain, typename ReadHash>
    bool resolve(uint64_t height, parent_header& result, ReadMain read_main, ReadHash read_hash)
    {
      result = {};
      if (height == 0 || height > m_parent.height)
        return false;
      parent_header current = m_parent;
      while (current.height > height)
      {
        const parent_header* main_at_height = main_header(current.height, read_main);
        if (main_at_height && main_at_height->hash == current.hash)
        {
          if (const parent_header* target = main_header(height, read_main))
          {
            result = *target;
            return true;
          }
        }

        const auto previous = m_ancestors.find(current.height - 1);
        if (previous != m_ancestors.end())
          current = previous->second;
        else
        {
          parent_header found{};
          if (!read_hash(current.prev, current.height - 1, found) || found.hash != current.prev)
            return false;
          current = m_ancestors.emplace(current.height - 1, found).first->second;
        }
      }
      result = current;
      return true;
    }

  private:
    template <typename ReadMain>
    const parent_header* main_header(uint64_t height, ReadMain read_main)
    {
      auto found = m_main.find(height);
      if (found == m_main.end())
      {
        parent_header header{};
        boost::optional<parent_header> value;
        if (read_main(height, header))
          value = header;
        found = m_main.emplace(height, std::move(value)).first;
      }
      return found->second ? &*found->second : nullptr;
    }

    parent_header m_parent;
    std::map<uint64_t, parent_header> m_ancestors;
    std::map<uint64_t, boost::optional<parent_header>> m_main;
  };

  template <typename ReadMain, typename ReadHash>
  bool resolve_ancestor_header(const parent_header& parent, uint64_t height, parent_header& result,
      ReadMain read_main, ReadHash read_hash)
  {
    ancestor_header_cache cache(parent);
    return cache.resolve(height, result, read_main, read_hash);
  }
}
