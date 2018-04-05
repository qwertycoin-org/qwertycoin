#pragma once

#include <vector>
#include <iterator>
#include <algorithm>

namespace Common {
namespace VectorUtils {
  template<typename T, class UnaryPredicate>
  std::vector<T> filter(const std::vector<T> & original, UnaryPredicate pred) {

    std::vector<T> filtered;

    std::copy_if(begin(original), end(original),
      std::back_inserter(filtered),
      pred);

    return filtered;
  }

  template<typename T2, typename T1, class UnaryOperation>
  std::vector<T2> map(const std::vector<T1> & original, UnaryOperation mappingFunction) {

    std::vector<T2> mapped;

    std::transform(begin(original), end(original),
      std::back_inserter(mapped),
      mappingFunction);

    return mapped;
  }

  template<typename T>
  void append(std::vector<T> & appendedTo, const std::vector<T> & appended) {
    appendedTo.insert(end(appendedTo), begin(appended), end(appended));
  }
}

}