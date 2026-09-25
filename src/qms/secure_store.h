#pragma once

#include <cstdint>
#include <string>
#include <boost/utility/string_ref.hpp>

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
  bytes encrypt_store(const bytes& plaintext, boost::string_ref password,
                      const bytes& context);
  bytes decrypt_store(const bytes& encoded, boost::string_ref password,
                      const bytes& expected_context);
  // Rewrap only the random store key.  The authenticated ciphertext and its
  // nonce remain unchanged, so a wallet-password change cannot rewind or
  // otherwise mutate ratchet state.
  bytes rewrap_store(const bytes& encoded, boost::string_ref old_password,
                     boost::string_ref new_password);
}
}
