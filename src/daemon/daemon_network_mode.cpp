#include "daemon_network_mode.h"

#include "misc_log_ex.h"

#include <iostream>

namespace daemon_args {

void report_network_mode_conflict()
{
  std::cerr << "Can't specify more than one of --testnet, --stagenet, --devnet and --regtest" << ENDL;
}

}
