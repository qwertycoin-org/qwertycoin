#pragma once

#include <cstddef>
#include <unordered_map>
#include <vector>

#include "crypto/hash.h"

namespace cryptonote
{
  class alternative_child_index
  {
  public:
    explicit alternative_child_index(size_t count = 0)
    {
      children_.reserve(count);
    }

    void add(const crypto::hash &parent, const crypto::hash &child)
    {
      ++operations_;
      children_[parent].push_back(child);
    }

    bool has_child(const crypto::hash &hash) const
    {
      ++operations_;
      return children_.find(hash) != children_.end();
    }

    template<class Difficulty>
    crypto::hash best_descendant(const crypto::hash &ancestor,
        const std::unordered_map<crypto::hash, Difficulty> &scores) const
    {
      crypto::hash best = ancestor;
      Difficulty best_score{};
      std::vector<crypto::hash> pending{ancestor};
      while (!pending.empty())
      {
        const crypto::hash current = pending.back();
        pending.pop_back();
        ++operations_;
        const auto score = scores.find(current);
        if (score != scores.end()
            && (score->second > best_score
                || (score->second == best_score && current < best)))
        {
          best = current;
          best_score = score->second;
        }
        const auto children = children_.find(current);
        if (children != children_.end())
          pending.insert(pending.end(), children->second.begin(), children->second.end());
      }
      return best;
    }

    size_t operations() const { return operations_; }

  private:
    std::unordered_map<crypto::hash, std::vector<crypto::hash>> children_;
    mutable size_t operations_ = 0;
  };
}
