#include "ledger_reset_version.h"

#include "misc_log_ex.h"
#include "version.h"

#include <cstring>

namespace hw::ledger {

void append_reset_version(unsigned char* buffer, int& offset, size_t buffer_size)
{
  const size_t verlen = strlen(QWERTYCOIN_COMPATIBLE_VERSION);
  CHECK_AND_ASSERT_THROW_MES(offset + verlen <= buffer_size, "QWERTYCOIN_COMPATIBLE_VERSION is too long");
  memmove(buffer + offset, QWERTYCOIN_COMPATIBLE_VERSION, verlen);
  offset += strlen(QWERTYCOIN_COMPATIBLE_VERSION);
}

}
