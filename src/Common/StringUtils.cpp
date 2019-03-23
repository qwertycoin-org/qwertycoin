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

#include "StringUtils.h"

#include <unordered_map>
#include <regex>
#include <sstream>

#include "VectorUtils.h"

namespace Common {

std::string StringUtils::join(const std::vector<std::string> & tokens,
  const std::string & delimiter) {
  std::stringstream stream;
  stream << tokens.front();
  std::for_each(
    begin(tokens) + 1,
    end(tokens),
    [&](const std::string &elem) { stream << delimiter << elem; }
  );
  return stream.str();
}

std::vector<std::string> StringUtils::split(const std::string & str,
  const std::vector<std::string> & delimiters) {

  std::regex rgx(join(escapeStrings(delimiters), "|"));

  std::sregex_token_iterator
    first{begin(str), end(str), rgx, -1},
    last;

  return{first, last};
}

std::vector<std::string> StringUtils::split(const std::string & str,
  const std::string & delimiter) {
  std::vector<std::string> delimiters = {delimiter};
  return split(str, delimiters);
}

std::string StringUtils::escapeChar(char character) {
  const std::unordered_map<char, std::string> ScapedSpecialCharacters = {
    {'.', "\\."}, {'|', "\\|"}, {'*', "\\*"}, {'?', "\\?"},
    {'+', "\\+"}, {'(', "\\("}, {')', "\\)"}, {'{', "\\{"},
    {'}', "\\}"}, {'[', "\\["}, {']', "\\]"}, {'^', "\\^"},
    {'$', "\\$"}, {'\\', "\\\\"}
  };

  auto it = ScapedSpecialCharacters.find(character);

  if (it == ScapedSpecialCharacters.end())
    return std::string(1, character);

  return it->second;
}

std::string StringUtils::escapeString(const std::string & str) {
  std::stringstream stream;
  std::for_each(begin(str), end(str),
    [&stream](const char character) { stream << escapeChar(character); }
  );
  return stream.str();
}

std::vector<std::string> StringUtils::escapeStrings(
  const std::vector<std::string> & delimiters) {
  return VectorUtils::map<std::string>(delimiters, escapeString);
}

bool StringUtils::isAnInteger(const std::string & token) {
  const std::regex e("\\s*[+-]?([1-9][0-9]*|0[0-7]*|0[xX][0-9a-fA-F]+)");
  return std::regex_match(token, e);
}

std::string StringUtils::extractRegion(const std::string & str,
  int from, int to) {
  std::string region = "";
  int regionSize = to - from;
  return str.substr(from, regionSize);
}

int StringUtils::convertToInt(const std::string & str) {
  std::string::size_type sz;
  return std::stoi(str, &sz);
}

}