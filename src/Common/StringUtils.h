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

#include <string>
#include <vector>

namespace Common {
namespace StringUtils
{
  std::vector<std::string> split(const std::string & str,
    const std::vector<std::string> & delimiters);

  std::vector<std::string> split(const std::string & str,
    const std::string & delimiter);

  std::string join(const std::vector<std::string> & tokens,
    const std::string & delimiter);

  std::string escapeChar(char character);

  std::string escapeString(const std::string & str);

  std::vector<std::string> escapeStrings(
    const std::vector<std::string> & strs);

  bool isAnInteger(const std::string & token);

  std::string extractRegion(const std::string & str,
    int from, int to);

  int convertToInt(const std::string & str);
};

}