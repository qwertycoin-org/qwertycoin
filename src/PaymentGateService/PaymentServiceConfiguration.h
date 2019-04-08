// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014 - 2017 XDN - project developers
// Copyright (c) 2018, The TurtleCoin Developers
// Copyright (c) 2016-2019 The Karbo developers
// Copyright (c) 2018-2019, The Qwertycoin developers
//
// This file is part of Qwertycoin.
//
// Qwertycoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Qwertycoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Qwertycoin.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <string>
#include <stdexcept>
#include <cstdint>

#include <boost/program_options.hpp>

#include "SimpleWallet/PasswordContainer.h"

namespace {
  Tools::PasswordContainer pwd_container;
}

namespace PaymentService {

class ConfigurationError : public std::runtime_error {
public:
  ConfigurationError(const char* desc) : std::runtime_error(desc) {}
};

struct Configuration {
  Configuration();

  void init(const boost::program_options::variables_map& options);
  static void initOptions(boost::program_options::options_description& desc);

  std::string bindAddress;
  uint16_t bindPort;
  std::string m_rpcUser;
  std::string m_rpcPassword;

  std::string containerFile;
  std::string containerPassword;
  std::string logFile;
  std::string serverRoot;
  std::string secretViewKey;
  std::string secretSpendKey;
  std::string mnemonicSeed;

  bool generateNewContainer;
  bool generateDeterministic;
  bool daemonize;
  bool registerService;
  bool unregisterService;
  bool testnet;
  bool printAddresses;

  size_t logLevel;
};

} //namespace PaymentService
