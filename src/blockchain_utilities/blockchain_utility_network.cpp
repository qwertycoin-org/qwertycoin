#include "blockchain_utility_network.h"

#include "common/command_line.h"
#include "cryptonote_core/cryptonote_core.h"

namespace blockchain_utilities {

void add_devnet_option(boost::program_options::options_description& options)
{
  command_line::add_arg(options, cryptonote::arg_devnet_on);
}

}
