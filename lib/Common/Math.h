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

#include <algorithm>
#include <numeric>
#include <vector>

namespace Common {

template <class T>
T meanValue(const std::vector<T>& v)
{
    if (v.empty()) {
        return T();
    }

    T sum = std::accumulate(v.begin(), v.end(), T());
    return sum / v.size();
}

template <class T>
T stddevValue(const std::vector<T>& v)
{
    if (v.size() < 2) {
        return T();
    }

    T mean = meanValue(v);
    std::vector<T> diff(v.size());
    std::transform(v.begin(), v.end(), diff.begin(), [mean](T x) { return x - mean; });
    T sq_sum = std::inner_product(diff.begin(), diff.end(), diff.begin(), T());
    return std::sqrt(sq_sum / v.size());
}

template <class T>
T medianValue(std::vector<T> &v)
{
    if (v.empty()) {
        return T();
    }

    if (v.size() == 1) {
        return v[0];
    }

    auto n = (v.size()) / 2;
    std::sort(v.begin(), v.end());
    if (v.size() % 2) { // 1, 3, 5...
        return v[n];
    } else { // 2, 4, 6...
        return (v[n - 1] + v[n]) / 2;
    }
}

} // namespace Common
