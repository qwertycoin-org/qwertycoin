#pragma once

#include "wallet/wallet2.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

inline void make_two_of_two_multisig_wallets(std::array<tools::wallet2, 2>& wallets)
{
  const epee::wipeable_string password("");
  std::vector<std::string> messages;
  for (auto& wallet : wallets)
  {
    wallet.init("", boost::none, "", 0, true, epee::net_utils::ssl_support_t::e_ssl_support_disabled);
    wallet.set_subaddress_lookahead(1, 1);
    wallet.generate("", password, crypto::secret_key(), false, false);
    wallet.decrypt_keys(password);
    messages.push_back(wallet.get_multisig_first_kex_msg());
    wallet.encrypt_keys(password);
  }
  std::vector<std::string> next;
  for (auto& wallet : wallets)
    next.push_back(wallet.make_multisig(password, messages, 2));
  while (true)
  {
    bool ready = false;
    wallets.front().multisig(&ready);
    if (ready)
      break;
    messages = std::move(next);
    next.clear();
    for (auto& wallet : wallets)
      next.push_back(wallet.exchange_multisig_keys(password, messages));
  }
}
