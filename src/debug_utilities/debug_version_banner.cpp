#include "debug_version_banner.h"

#include "misc_log_ex.h"
#include "version.h"

#include <iostream>

namespace debug_utilities {

void print_version_banner()
{
  std::cout << "Qwertycoin '" << QWERTYCOIN_RELEASE_NAME << "' (v" << QWERTYCOIN_VERSION_FULL << ")" << ENDL << ENDL;
}

}
