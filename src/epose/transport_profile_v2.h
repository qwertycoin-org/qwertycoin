#pragma once

#include <cstdint>

namespace qwertycoin
{
namespace epose
{
  inline bool valid_transport_profile_v2(
      uint8_t transport, uint8_t dial_capabilities, uint8_t dns_address_families)
  {
    return transport >= 1 && transport <= 3
        && dial_capabilities != 0
        && (dial_capabilities & ~3) == 0
        && (dns_address_families & ~3) == 0
        && ((transport == 3) == (dns_address_families != 0));
  }

  template<typename Probe>
  uint8_t reachable_dial_capabilities_v2(uint8_t configured, Probe &&probe)
  {
    uint8_t reached = 0;
    if ((configured & 1) && probe(false))
      reached |= 1;
    if ((configured & 2) && probe(true))
      reached |= 2;
    return reached;
  }

  inline uint8_t dial_capabilities_or_configured_v2(uint8_t configured, uint8_t reached)
  {
    return reached != 0 ? reached : static_cast<uint8_t>(configured & 3);
  }
}
}
