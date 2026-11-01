// Copyright (c) 2026, Qwertycoin
//
// SPDX-License-Identifier: BSD-3-Clause

#include "gtest/gtest.h"

#include "vm/state_tree.h"

#include <cstring>
#include <iterator>
#include <map>
#include <random>

namespace vm
{
  struct state_tree_test_access
  {
    static uint64_t node_hashes(const state_tree& tree) { return tree.m_node_hashes; }
  };
}

namespace
{
  using reference_map = std::map<crypto::hash, std::string>;

  bool reference_bit(const crypto::hash& path, uint32_t bit)
  {
    return (static_cast<uint8_t>(path.data[bit / 8]) >> (7 - bit % 8)) & 1;
  }

  uint32_t reference_difference(const crypto::hash& left, const crypto::hash& right)
  {
    for (uint32_t bit = 0; bit < sizeof(crypto::hash) * 8; ++bit)
      if (reference_bit(left, bit) != reference_bit(right, bit))
        return bit;
    return sizeof(crypto::hash) * 8;
  }

  crypto::hash reference_hash(reference_map::const_iterator begin, reference_map::const_iterator end,
      const crypto::hash& query, vm::state_proof& proof, bool on_path)
  {
    auto last = std::prev(end);
    if (begin == last)
    {
      const crypto::hash value = crypto::cn_fast_hash(begin->second.data(), begin->second.size());
      if (on_path)
      {
        proof.leaf_path = begin->first;
        proof.leaf_value = value;
      }
      uint8_t data[1 + 2 * sizeof(crypto::hash)];
      data[0] = 0;
      memcpy(data + 1, &begin->first, sizeof(crypto::hash));
      memcpy(data + 1 + sizeof(crypto::hash), &value, sizeof(value));
      return crypto::cn_fast_hash(data, sizeof(data));
    }

    const uint32_t bit = reference_difference(begin->first, last->first);
    auto mid = begin;
    while (mid != end && !reference_bit(mid->first, bit))
      ++mid;
    const bool right = reference_bit(query, bit);
    const size_t step = proof.steps.size();
    if (on_path)
      proof.steps.push_back({static_cast<uint8_t>(bit), crypto::null_hash});
    const crypto::hash left = reference_hash(begin, mid, query, proof, on_path && !right);
    const crypto::hash right_hash = reference_hash(mid, end, query, proof, on_path && right);
    if (on_path)
      proof.steps[step].sibling = right ? left : right_hash;
    uint8_t data[2 + 2 * sizeof(crypto::hash)];
    data[0] = 1;
    data[1] = static_cast<uint8_t>(bit);
    memcpy(data + 2, &left, sizeof(left));
    memcpy(data + 2 + sizeof(left), &right_hash, sizeof(right_hash));
    return crypto::cn_fast_hash(data, sizeof(data));
  }

  void check_reference(const vm::state_tree& tree, const reference_map& values, const std::string& key)
  {
    vm::state_proof expected;
    crypto::hash root = crypto::null_hash;
    if (!values.empty())
      root = reference_hash(values.begin(), values.end(), vm::state_tree::key_path(key), expected, true);
    EXPECT_EQ(tree.size(), values.size());
    EXPECT_EQ(tree.get_root(), root);
    const vm::state_proof actual = tree.get_proof(key);
    EXPECT_EQ(static_cast<bool>(actual.leaf_path), static_cast<bool>(expected.leaf_path));
    if (actual.leaf_path && expected.leaf_path)
    {
      EXPECT_EQ(*actual.leaf_path, *expected.leaf_path);
    }
    EXPECT_EQ(actual.leaf_value, expected.leaf_value);
    ASSERT_EQ(actual.steps.size(), expected.steps.size());
    for (size_t i = 0; i < actual.steps.size(); ++i)
    {
      EXPECT_EQ(actual.steps[i].bit, expected.steps[i].bit);
      EXPECT_EQ(actual.steps[i].sibling, expected.steps[i].sibling);
    }
  }

  vm::state_tree sample(size_t n)
  {
    vm::state_tree t;
    for (size_t i = 0; i < n; ++i)
      t.set("key" + std::to_string(i), "value" + std::to_string(i));
    return t;
  }
}

TEST(vm_state_tree, incremental_hashes_match_full_rehash)
{
  std::mt19937 random(123456);
  vm::state_tree tree;
  reference_map values;
  for (size_t operation = 0; operation < 4000; ++operation)
  {
    const std::string key = "key" + std::to_string(random() % 256);
    const uint64_t before = vm::state_tree_test_access::node_hashes(tree);
    if (random() % 4 == 0)
    {
      tree.erase(key);
      values.erase(vm::state_tree::key_path(key));
    }
    else
    {
      const std::string value = "value" + std::to_string(random());
      tree.set(key, value);
      values[vm::state_tree::key_path(key)] = value;
    }
    EXPECT_LE(vm::state_tree_test_access::node_hashes(tree) - before, 40u);
    const uint64_t after = vm::state_tree_test_access::node_hashes(tree);
    check_reference(tree, values, key);
    EXPECT_EQ(vm::state_tree_test_access::node_hashes(tree), after);

    if (operation % 7 == 0)
    {
      vm::write_set writes;
      writes[key] = std::string("temporary");
      const vm::write_set undo = vm::undo_for(tree, writes);
      const reference_map saved = values;
      const vm::state_tree snapshot = tree;
      const uint64_t before_apply = vm::state_tree_test_access::node_hashes(tree);
      tree.apply(writes);
      EXPECT_LE(vm::state_tree_test_access::node_hashes(tree) - before_apply, 40u);
      values[vm::state_tree::key_path(key)] = "temporary";
      check_reference(tree, values, key);
      check_reference(snapshot, saved, key);
      const uint64_t before_undo = vm::state_tree_test_access::node_hashes(tree);
      tree.apply(undo);
      values = saved;
      EXPECT_LE(vm::state_tree_test_access::node_hashes(tree) - before_undo, 40u);
      check_reference(tree, values, key);
    }
  }
}

TEST(vm_state_tree, empty_root_and_absence)
{
  vm::state_tree t;
  EXPECT_EQ(t.get_root(), crypto::null_hash);
  vm::state_proof p = t.get_proof("missing");
  EXPECT_TRUE(vm::state_tree::verify_proof(t.get_root(), "missing", boost::none, p));
  EXPECT_FALSE(vm::state_tree::verify_proof(t.get_root(), "missing", std::string("x"), p));
}

TEST(vm_state_tree, single_leaf)
{
  vm::state_tree t;
  t.set("a", "1");
  EXPECT_NE(t.get_root(), crypto::null_hash);
  vm::state_proof p = t.get_proof("a");
  EXPECT_TRUE(p.steps.empty());
  EXPECT_TRUE(vm::state_tree::verify_proof(t.get_root(), "a", std::string("1"), p));
  EXPECT_TRUE(vm::state_tree::verify_proof(t.get_root(), "b", boost::none, t.get_proof("b")));
}

TEST(vm_state_tree, inclusion_proofs)
{
  vm::state_tree t = sample(64);
  const crypto::hash root = t.get_root();
  for (size_t i = 0; i < 64; ++i)
  {
    std::string key = "key" + std::to_string(i);
    vm::state_proof p = t.get_proof(key);
    EXPECT_TRUE(vm::state_tree::verify_proof(root, key, std::string("value" + std::to_string(i)), p)) << key;
    EXPECT_FALSE(vm::state_tree::verify_proof(root, key, std::string("other"), p)) << key;
    EXPECT_FALSE(vm::state_tree::verify_proof(root, key, boost::none, p)) << key;
  }
}

TEST(vm_state_tree, absence_proofs)
{
  vm::state_tree t = sample(64);
  const crypto::hash root = t.get_root();
  for (size_t i = 64; i < 128; ++i)
  {
    std::string key = "key" + std::to_string(i);
    vm::state_proof p = t.get_proof(key);
    EXPECT_TRUE(vm::state_tree::verify_proof(root, key, boost::none, p)) << key;
    EXPECT_FALSE(vm::state_tree::verify_proof(root, key, std::string("value" + std::to_string(i)), p)) << key;
  }
}

TEST(vm_state_tree, proofs_do_not_transfer)
{
  vm::state_tree t = sample(16);
  const crypto::hash root = t.get_root();
  vm::state_proof p = t.get_proof("key3");
  EXPECT_FALSE(vm::state_tree::verify_proof(root, "key4", std::string("value3"), p));
  EXPECT_FALSE(vm::state_tree::verify_proof(root, "key4", boost::none, p));

  vm::state_proof tampered = p;
  ASSERT_FALSE(tampered.steps.empty());
  tampered.steps.front().sibling.data[0] ^= 1;
  EXPECT_FALSE(vm::state_tree::verify_proof(root, "key3", std::string("value3"), tampered));

  vm::state_proof reordered = p;
  if (reordered.steps.size() > 1)
  {
    std::swap(reordered.steps[0], reordered.steps[1]);
    EXPECT_FALSE(vm::state_tree::verify_proof(root, "key3", std::string("value3"), reordered));
  }

  crypto::hash other_root = root;
  other_root.data[31] ^= 1;
  EXPECT_FALSE(vm::state_tree::verify_proof(other_root, "key3", std::string("value3"), p));
}

TEST(vm_state_tree, root_is_order_independent)
{
  vm::state_tree a;
  vm::state_tree b;
  for (int i = 0; i < 32; ++i)
    a.set("k" + std::to_string(i), std::to_string(i));
  for (int i = 31; i >= 0; --i)
    b.set("k" + std::to_string(i), std::to_string(i));
  EXPECT_EQ(a.get_root(), b.get_root());
}

TEST(vm_state_tree, updates_change_root_and_erase_restores)
{
  vm::state_tree t = sample(8);
  const crypto::hash before = t.get_root();
  t.set("extra", "x");
  EXPECT_NE(t.get_root(), before);
  t.erase("extra");
  EXPECT_EQ(t.get_root(), before);

  t.set("key0", "changed");
  EXPECT_NE(t.get_root(), before);
  t.set("key0", "value0");
  EXPECT_EQ(t.get_root(), before);
}

TEST(vm_state_tree, apply_write_set)
{
  vm::state_tree t = sample(4);
  vm::write_set w;
  w["key0"] = boost::none;
  w["key1"] = std::string("new");
  w["fresh"] = std::string("f");
  t.apply(w);

  std::string v;
  EXPECT_FALSE(t.get("key0", v));
  ASSERT_TRUE(t.get("key1", v));
  EXPECT_EQ(v, "new");
  ASSERT_TRUE(t.get("fresh", v));
  EXPECT_EQ(v, "f");
  EXPECT_EQ(t.size(), 4u);

  vm::state_tree expected;
  expected.set("key1", "new");
  expected.set("key2", "value2");
  expected.set("key3", "value3");
  expected.set("fresh", "f");
  EXPECT_EQ(t.get_root(), expected.get_root());
}
