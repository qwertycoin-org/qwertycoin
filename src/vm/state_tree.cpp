// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "state_tree.h"

#include <cstring>
#include <utility>

namespace vm
{
  std::atomic<uint64_t> state_tree::s_copy_count{0};

  state_tree::state_tree(const state_tree& other) : m_root(other.m_root), m_size(other.m_size), m_node_hashes(other.m_node_hashes)
  {
    ++s_copy_count;
  }

  state_tree& state_tree::operator=(const state_tree& other)
  {
    if (this != &other)
    {
      m_root = other.m_root;
      m_size = other.m_size;
      m_node_hashes = other.m_node_hashes;
      ++s_copy_count;
    }
    return *this;
  }

  state_tree::state_tree(state_tree&& other) noexcept
    : m_root(std::move(other.m_root)), m_size(other.m_size), m_node_hashes(other.m_node_hashes)
  {
    other.m_size = 0;
  }

  state_tree& state_tree::operator=(state_tree&& other) noexcept
  {
    if (this != &other)
    {
      m_root = std::move(other.m_root);
      m_size = other.m_size;
      m_node_hashes = other.m_node_hashes;
      other.m_size = 0;
    }
    return *this;
  }

  uint64_t state_tree::copy_count()
  {
    return s_copy_count.load();
  }

  namespace
  {
    constexpr uint8_t leaf_tag = 0;
    constexpr uint8_t node_tag = 1;
    constexpr uint32_t path_bits = sizeof(crypto::hash) * 8;

    bool path_bit(const crypto::hash& path, uint32_t bit)
    {
      const auto* bytes = reinterpret_cast<const uint8_t*>(&path);
      return (bytes[bit / 8] >> (7 - bit % 8)) & 1;
    }

    uint32_t first_diff_bit(const crypto::hash& a, const crypto::hash& b)
    {
      for (uint32_t bit = 0; bit < path_bits; ++bit)
        if (path_bit(a, bit) != path_bit(b, bit))
          return bit;
      return path_bits;
    }

    crypto::hash value_hash(const std::string& value)
    {
      return crypto::cn_fast_hash(value.data(), value.size());
    }

    crypto::hash leaf_hash(const crypto::hash& path, const crypto::hash& value)
    {
      uint8_t buf[1 + 2 * sizeof(crypto::hash)];
      buf[0] = leaf_tag;
      memcpy(buf + 1, &path, sizeof(path));
      memcpy(buf + 1 + sizeof(path), &value, sizeof(value));
      return crypto::cn_fast_hash(buf, sizeof(buf));
    }

    crypto::hash node_hash(uint8_t bit, const crypto::hash& left, const crypto::hash& right)
    {
      uint8_t buf[2 + 2 * sizeof(crypto::hash)];
      buf[0] = node_tag;
      buf[1] = bit;
      memcpy(buf + 2, &left, sizeof(left));
      memcpy(buf + 2 + sizeof(left), &right, sizeof(right));
      return crypto::cn_fast_hash(buf, sizeof(buf));
    }
  }

  crypto::hash state_tree::key_path(const std::string& key)
  {
    return crypto::cn_fast_hash(key.data(), key.size());
  }

  bool state_tree::get(const std::string& key, std::string& value) const
  {
    const crypto::hash path = key_path(key);
    const node* current = m_root.get();
    while (current && current->left)
      current = path_bit(path, current->bit) ? current->right.get() : current->left.get();
    if (!current || current->path != path)
      return false;
    value = current->value;
    return true;
  }

  void state_tree::set(const std::string& key, const std::string& value)
  {
    bool added = false;
    m_root = insert(m_root, key_path(key), value, added);
    m_size += added;
  }

  void state_tree::erase(const std::string& key)
  {
    bool removed = false;
    m_root = remove(m_root, key_path(key), removed);
    m_size -= removed;
  }

  void state_tree::apply(const write_set& writes)
  {
    for (const auto& [key, value] : writes)
    {
      if (value)
        set(key, *value);
      else
        erase(key);
    }
  }

  state_tree::node_ptr state_tree::make_leaf(const crypto::hash& path, const std::string& value)
  {
    auto result = std::make_shared<node>();
    result->path = path;
    result->value = value;
    result->value_hash = value_hash(value);
    result->hash = leaf_hash(path, result->value_hash);
    ++m_node_hashes;
    return result;
  }

  state_tree::node_ptr state_tree::make_branch(uint8_t bit, node_ptr left, node_ptr right)
  {
    auto result = std::make_shared<node>();
    result->path = left->path;
    result->bit = bit;
    result->left = std::move(left);
    result->right = std::move(right);
    result->hash = node_hash(bit, result->left->hash, result->right->hash);
    ++m_node_hashes;
    return result;
  }

  state_tree::node_ptr state_tree::insert(node_ptr current, const crypto::hash& path, const std::string& value, bool& added)
  {
    if (!current)
    {
      added = true;
      return make_leaf(path, value);
    }

    const uint32_t difference = first_diff_bit(path, current->path);
    if (!current->left && difference == path_bits)
      return current->value == value ? current : make_leaf(path, value);
    if (difference < (current->left ? current->bit : path_bits))
    {
      added = true;
      node_ptr leaf = make_leaf(path, value);
      return path_bit(path, difference)
        ? make_branch(static_cast<uint8_t>(difference), current, leaf)
        : make_branch(static_cast<uint8_t>(difference), leaf, current);
    }

    if (path_bit(path, current->bit))
    {
      node_ptr right = insert(current->right, path, value, added);
      return right == current->right ? current : make_branch(current->bit, current->left, right);
    }
    node_ptr left = insert(current->left, path, value, added);
    return left == current->left ? current : make_branch(current->bit, left, current->right);
  }

  state_tree::node_ptr state_tree::remove(node_ptr current, const crypto::hash& path, bool& removed)
  {
    if (!current)
      return current;
    if (!current->left)
    {
      if (current->path != path)
        return current;
      removed = true;
      return {};
    }
    if (first_diff_bit(path, current->path) < current->bit)
      return current;
    if (path_bit(path, current->bit))
    {
      node_ptr right = remove(current->right, path, removed);
      if (!removed)
        return current;
      return right ? make_branch(current->bit, current->left, right) : current->left;
    }
    node_ptr left = remove(current->left, path, removed);
    if (!removed)
      return current;
    return left ? make_branch(current->bit, left, current->right) : current->right;
  }

  crypto::hash state_tree::get_root() const
  {
    return m_root ? m_root->hash : crypto::null_hash;
  }

  state_proof state_tree::get_proof(const std::string& key) const
  {
    state_proof proof;
    if (!m_root)
      return proof;

    const crypto::hash path = key_path(key);
    const node* current = m_root.get();
    while (current->left)
    {
      if (path_bit(path, current->bit))
      {
        proof.steps.push_back({current->bit, current->left->hash});
        current = current->right.get();
      }
      else
      {
        proof.steps.push_back({current->bit, current->right->hash});
        current = current->left.get();
      }
    }

    proof.leaf_path = current->path;
    proof.leaf_value = current->value_hash;
    return proof;
  }

  bool state_tree::verify_proof(const crypto::hash& root, const std::string& key, const boost::optional<std::string>& value, const state_proof& proof)
  {
    if (!proof.leaf_path)
      return proof.steps.empty() && !value && root == crypto::null_hash;

    const crypto::hash path = key_path(key);
    const crypto::hash& leaf_path = *proof.leaf_path;

    if (value)
    {
      if (leaf_path != path || proof.leaf_value != value_hash(*value))
        return false;
    }
    else if (leaf_path == path)
      return false;

    for (size_t i = 0; i < proof.steps.size(); ++i)
    {
      uint8_t bit = proof.steps[i].bit;
      if (i > 0 && bit <= proof.steps[i - 1].bit)
        return false;
      if (path_bit(leaf_path, bit) != path_bit(path, bit))
        return false;
    }

    crypto::hash acc = leaf_hash(leaf_path, proof.leaf_value);
    for (auto it = proof.steps.rbegin(); it != proof.steps.rend(); ++it)
    {
      if (path_bit(path, it->bit))
        acc = node_hash(it->bit, it->sibling, acc);
      else
        acc = node_hash(it->bit, acc, it->sibling);
    }

    return acc == root;
  }
}
