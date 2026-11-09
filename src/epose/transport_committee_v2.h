#pragma once

#include <algorithm>

#include "epose/membership_v2.h"

namespace qwertycoin
{
namespace epose
{
  inline uint8_t subject_address_families_v2(const frozen_member_v2 &subject)
  {
    if (subject.endpoint_transport == 3)
      return subject.dns_address_families;
    return subject.endpoint_transport == 2 ? 2 : 1;
  }

  inline bool transport_compatible_v2(
      const frozen_member_v2 &candidate,
      const frozen_member_v2 &subject)
  {
    return (candidate.dial_capabilities & subject_address_families_v2(subject)) != 0;
  }

  inline size_t compatible_committee_size_v2(
      const membership_snapshot_v2 &snapshot,
      const frozen_member_v2 &subject,
      size_t limit)
  {
    size_t count = 0;
    for (const frozen_member_v2 &candidate : snapshot.members)
      if (candidate.service_public_key != subject.service_public_key
          && (!snapshot.transport_aware
              || transport_compatible_v2(candidate, subject)))
        ++count;
    return std::min(count, limit);
  }

  inline bool compatible_committee_judged_v2(
      const membership_snapshot_v2 &snapshot,
      size_t compatible_committee_size,
      size_t minimum_compatible_committee)
  {
    return compatible_committee_size != 0
        && (!snapshot.transport_aware
            || compatible_committee_size >= minimum_compatible_committee);
  }
}
}
