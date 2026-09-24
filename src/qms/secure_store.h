#pragma once

#include <cstdint>
#include <string>

#include "protocol.h"

namespace qwertycoin
{
namespace qms
{
  // Authenticated QMS2 store envelope. Messenger identity, prekeys, sessions,
  // outer secrets, reassembly state, history policy and complete send plans are
  // serialized by the caller into `plaintext` before this layer is invoked.
  // A random key is derived from the wallet password with Argon2id and a fresh
  // salt; no wallet spend/view key participates in this derivation.
  bytes encrypt_store(const bytes& plaintext, const std::string& password,
                      const bytes& context);
  bytes decrypt_store(const bytes& encoded, const std::string& password,
                      const bytes& expected_context);
}
}
