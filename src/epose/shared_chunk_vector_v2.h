// Copyright (c) 2026, Qwertycoin
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cassert>
#include <cstddef>
#include <iterator>
#include <memory>
#include <type_traits>
#include <vector>

namespace qwertycoin
{
namespace epose
{
  // An append-mostly sequence whose copies share every full chunk.
  //
  // Elements [0, size / N * N) live in sealed chunks of N elements, held as
  // shared_ptr<const> and never written again; the last size % N live in an
  // owned tail. A copy costs one reference per sealed chunk plus at most
  // N - 1 element copies; replace() clones a sealed chunk before writing it.
  // The layout depends on size alone, which keeps truncate() exact and
  // allocation-free. Sealed chunks are immutable, so copies may be used on
  // other threads; one object is not thread-safe for mutation.
  template <class T, std::size_t N>
  class shared_chunk_vector_v2
  {
    static_assert(N > 1, "chunk size");
    static_assert(std::is_nothrow_copy_constructible<T>::value
        && std::is_nothrow_copy_assignable<T>::value,
        "truncate() and restore() copy elements without throwing");

  public:
    using chunk = std::vector<T>;
    using chunk_ptr = std::shared_ptr<const chunk>;

    shared_chunk_vector_v2()
    {
      tail_.reserve(N);
    }

    shared_chunk_vector_v2(const shared_chunk_vector_v2 &other)
      : sealed_(other.sealed_)
    {
      tail_.reserve(N);
      tail_.assign(other.tail_.begin(), other.tail_.end());
    }

    shared_chunk_vector_v2 &operator=(const shared_chunk_vector_v2 &other)
    {
      if (this != &other)
      {
        std::vector<chunk_ptr> sealed(other.sealed_);
        tail_.assign(other.tail_.begin(), other.tail_.end());
        sealed_.swap(sealed);
      }
      return *this;
    }

    std::size_t size() const noexcept
    {
      return sealed_.size() * N + tail_.size();
    }

    const T &operator[](std::size_t index) const noexcept
    {
      const std::size_t chunk_index = index / N;
      return chunk_index < sealed_.size()
          ? (*sealed_[chunk_index])[index % N]
          : tail_[index - sealed_.size() * N];
    }

    // Bytes owned or shared by this sequence.
    std::size_t payload_bytes() const noexcept
    {
      return (sealed_.size() * N + tail_.capacity()) * sizeof(T);
    }

    void push_back(const T &value)
    {
      if (tail_.size() + 1 < N)
      {
        tail_.push_back(value);
        return;
      }
      // Allocate before sealing, so a throw leaves the sequence unchanged.
      sealed_.reserve(sealed_.size() + 1);
      chunk fresh;
      fresh.reserve(N);
      std::shared_ptr<chunk> full = std::make_shared<chunk>();
      tail_.push_back(value);
      full->swap(tail_);
      tail_.swap(fresh);
      sealed_.push_back(std::move(full));
    }

    // Drops elements [size, size()).
    void truncate(std::size_t size) noexcept
    {
      assert(size <= this->size());
      const std::size_t keep_sealed = size / N;
      if (keep_sealed == sealed_.size())
      {
        tail_.erase(tail_.begin() + (size - keep_sealed * N), tail_.end());
        return;
      }
      const chunk &partial = *sealed_[keep_sealed];
      tail_.assign(partial.begin(), partial.begin() + (size % N));
      sealed_.erase(sealed_.begin() + keep_sealed, sealed_.end());
    }

    // Overwrites one element. Returns the chunk the element lived in when it
    // was sealed, null when it lived in the tail; restore() takes it back.
    chunk_ptr replace(std::size_t index, const T &value)
    {
      const std::size_t chunk_index = index / N;
      if (chunk_index >= sealed_.size())
      {
        tail_[index - sealed_.size() * N] = value;
        return nullptr;
      }
      std::shared_ptr<chunk> copy = std::make_shared<chunk>(*sealed_[chunk_index]);
      (*copy)[index % N] = value;
      chunk_ptr previous = std::move(sealed_[chunk_index]);
      sealed_[chunk_index] = std::move(copy);
      return previous;
    }

    // Undoes replace() in strict LIFO order, after the sequence has been
    // truncated to its length at the checkpoint that preceded the replace.
    void restore(std::size_t index, const T &value, const chunk_ptr &previous) noexcept
    {
      if (index >= size())
        return;
      const std::size_t chunk_index = index / N;
      if (chunk_index < sealed_.size())
      {
        // Sealed now and at the replace, so the replace saved the chunk.
        assert(previous);
        sealed_[chunk_index] = previous;
      }
      else
        tail_[index - sealed_.size() * N] = value;
    }

    class const_iterator
    {
    public:
      using iterator_category = std::forward_iterator_tag;
      using value_type = T;
      using difference_type = std::ptrdiff_t;
      using pointer = const T *;
      using reference = const T &;

      const_iterator() = default;
      const_iterator(const shared_chunk_vector_v2 *owner, std::size_t index)
        : owner_(owner), index_(index) {}

      reference operator*() const { return (*owner_)[index_]; }
      pointer operator->() const { return &(*owner_)[index_]; }
      const_iterator &operator++() { ++index_; return *this; }
      const_iterator operator++(int) { const_iterator out = *this; ++index_; return out; }
      bool operator==(const const_iterator &other) const { return index_ == other.index_; }
      bool operator!=(const const_iterator &other) const { return index_ != other.index_; }
      std::size_t index() const { return index_; }

    private:
      const shared_chunk_vector_v2 *owner_ = nullptr;
      std::size_t index_ = 0;
    };

    const_iterator begin() const { return const_iterator(this, 0); }
    const_iterator end() const { return const_iterator(this, size()); }

  private:
    std::vector<chunk_ptr> sealed_;
    chunk tail_;
  };
} // namespace epose
} // namespace qwertycoin
