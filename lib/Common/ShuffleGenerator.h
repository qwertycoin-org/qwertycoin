// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The Qwertycoin developers
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

#include <unordered_map>
#include <random>

template <typename T, typename Gen>
class ShuffleGenerator {
public:

  ShuffleGenerator(T n, const Gen& gen = Gen()) :
    N(n), generator(gen), count(n) {}

  T operator()() {

    if (count == 0) {
      throw std::runtime_error("shuffle sequence ended");
    }

    typedef typename std::uniform_int_distribution<T> distr_t;
    typedef typename distr_t::param_type param_t;

    distr_t distr;

    T value = distr(generator, param_t(0, --count));

    auto rvalIt = selected.find(count);
    auto rval = rvalIt != selected.end() ? rvalIt->second : count;

    auto lvalIt = selected.find(value);

    if (lvalIt != selected.end()) {
      value = lvalIt->second;
      lvalIt->second = rval;
    } else {
      selected[value] = rval;
    }

    return value;
  }

  bool empty() const {
    return count == 0;
  }

  void reset() {
    count = N;
    selected.clear();
  }

private:

  std::unordered_map<T, T> selected;
  T count;
  const T N;
  Gen generator;
};
