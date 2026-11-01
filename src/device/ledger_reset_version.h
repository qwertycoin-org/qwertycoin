#pragma once

#include <cstddef>

namespace hw::ledger {

void append_reset_version(unsigned char* buffer, int& offset, size_t buffer_size);

}
