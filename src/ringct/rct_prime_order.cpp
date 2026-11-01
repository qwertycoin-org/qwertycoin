#include "rct_prime_order.h"

#include "rctTypes.h"
#include "crypto/crypto.h"
#include "crypto/ed25519_order.h"

#include <cstring>
#include <utility>

namespace rct {

namespace {
template<size_t... I>
constexpr key order_key(std::index_sequence<I...>) { return {{crypto::ed25519_order[I]...}}; }
}

constexpr key L = order_key(std::make_index_sequence<32>{});

bool is_in_main_subgroup(const key& A)
{
  crypto::public_key point;
  memcpy(&point, A.bytes, sizeof(point));
  return crypto::check_prime_order_key(point, false);
}

}
