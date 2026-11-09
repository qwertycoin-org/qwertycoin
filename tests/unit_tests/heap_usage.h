#pragma once

#include <cstddef>

#if defined(__APPLE__)
#include <malloc/malloc.h>
#elif defined(__GLIBC__)
#include <malloc.h>
#endif

namespace test_heap_usage
{
  inline size_t in_use()
  {
#if defined(__APPLE__)
    malloc_statistics_t statistics{};
    malloc_zone_statistics(malloc_default_zone(), &statistics);
    return statistics.size_in_use;
#elif defined(__GLIBC__)
#if __GLIBC_PREREQ(2, 33)
    return mallinfo2().uordblks;
#else
    return static_cast<size_t>(mallinfo().uordblks);
#endif
#else
    return 0;
#endif
  }
}
