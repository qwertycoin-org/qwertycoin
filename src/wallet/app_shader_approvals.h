#pragma once

#include <string>
#include <vector>

#include <boost/optional.hpp>

#include "crypto/crypto.h"
#include "string_tools.h"

namespace tools
{
  class app_shader_approvals
  {
  public:
    bool add(const crypto::hash& contract_id, const crypto::hash& key_id, const crypto::public_key& public_key, std::string& error)
    {
      for (const entry& approval : m_entries)
        if (approval.contract_id == contract_id && approval.key_id == key_id)
        {
          error = "Duplicate approval for contract " + epee::string_tools::pod_to_hex(contract_id)
            + " key id " + epee::string_tools::pod_to_hex(key_id);
          return false;
        }
      m_entries.push_back({contract_id, key_id, public_key, false});
      return true;
    }

    bool preauthorize(const crypto::hash& contract_id, const crypto::hash& key_id, boost::optional<crypto::public_key>& expected_key) const
    {
      for (const entry& approval : m_entries)
        if (!approval.used && approval.contract_id == contract_id && approval.key_id == key_id)
        {
          expected_key = approval.public_key;
          return true;
        }
      return false;
    }

    bool confirm(const crypto::hash& contract_id, const crypto::hash& key_id, const crypto::public_key& public_key, std::string& error)
    {
      for (entry& approval : m_entries)
        if (!approval.used && approval.contract_id == contract_id && approval.key_id == key_id)
        {
          if (approval.public_key != public_key)
          {
            error = "Approval for contract " + epee::string_tools::pod_to_hex(contract_id)
              + " key id " + epee::string_tools::pod_to_hex(key_id) + " has a different public key";
            return false;
          }
          approval.used = true;
          return true;
        }
      return false;
    }

    bool all_used() const
    {
      for (const entry& approval : m_entries)
        if (!approval.used)
          return false;
      return true;
    }

    std::string result_error(const std::string& approval_error) const
    {
      if (!approval_error.empty())
        return approval_error;
      return all_used() ? std::string() : "Approval list contains an unrequested key";
    }

  private:
    struct entry
    {
      crypto::hash contract_id;
      crypto::hash key_id;
      crypto::public_key public_key;
      bool used;
    };
    std::vector<entry> m_entries;
  };
}
