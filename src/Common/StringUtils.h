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