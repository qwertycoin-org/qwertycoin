#include "multisig_contract_guard.h"

#include "cryptonote_basic/cryptonote_basic.h"
#include "cryptonote_basic/cryptonote_format_utils.h"
#include "misc_log_ex.h"

#undef MONERO_DEFAULT_LOG_CATEGORY
#define MONERO_DEFAULT_LOG_CATEGORY "multisig"

namespace multisig::signing {

bool accepts_contract_extra(const cryptonote::transaction& tx)
{
  cryptonote::tx_extra_vm_call contract_call{};
  if (cryptonote::get_tx_vm_call(tx.extra, contract_call) != 0)
  {
    MERROR("Contract calls are not supported by multisig signing");
    return false;
  }
  return true;
}

}
