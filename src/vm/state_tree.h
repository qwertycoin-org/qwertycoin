// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cstdint>
#include <atomic>
#include <memory>
#include <boost/optional.hpp>
#include <string>
#include <vector>

#include "crypto/hash.h"
#include "state_view.h"

namespace vm
{
  struct state_proof
  {
    struct step
    {
      uint8_t bit;
      crypto::hash sibling;
    };

    std::vector<step> steps;
    boost::optional<crypto::hash> leaf_path;
    crypto::hash leaf_value = crypto::null_hash;
  };

  class state_tree : public state_view
  {
  public:
    state_tree() = default;
    state_tree(const state_tree& other);
    state_tree& operator=(const state_tree& other);
    state_tree(state_tree&& other) noexcept;
    state_tree& operator=(state_tree&& other) noexcept;
    static uint64_t copy_count();
    bool get(const std::string& key, std::string& value) const override;

    void set(const std::string& key, const std::string& value);
    void erase(const std::string& key);
    void apply(const write_set& writes);

    size_t size() const { return m_size; }

    crypto::hash get_root() const;
    state_proof get_proof(const std::string& key) const;

    static crypto::hash key_path(const std::string& key);
    static bool verify_proof(const crypto::hash& root, const std::string& key, const boost::optional<std::string>& value, const state_proof& proof);

  private:
    struct node
    {
      crypto::hash hash;
      crypto::hash path;
      crypto::hash value_hash;
      std::string value;
      uint8_t bit = 0;
      std::shared_ptr<const node> left;
      std::shared_ptr<const node> right;
    };

    using node_ptr = std::shared_ptr<const node>;

    node_ptr make_leaf(const crypto::hash& path, const std::string& value);
    node_ptr make_branch(uint8_t bit, node_ptr left, node_ptr right);
    node_ptr insert(node_ptr current, const crypto::hash& path, const std::string& value, bool& added);
    node_ptr remove(node_ptr current, const crypto::hash& path, bool& removed);

    node_ptr m_root;
    size_t m_size = 0;
    uint64_t m_node_hashes = 0;
    static std::atomic<uint64_t> s_copy_count;
    friend struct state_tree_test_access;
  };
}
