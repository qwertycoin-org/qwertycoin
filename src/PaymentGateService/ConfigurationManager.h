// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2020, The Qwertycoin Group.
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

#include <CryptoNoteCore/CoreConfig.h>
#include <P2p/NetNodeConfig.h>
#include "PaymentServiceConfiguration.h"
#include "RpcNodeConfiguration.h"

namespace PaymentService {

class ConfigurationManager
{
public:
    ConfigurationManager();

    bool init(int argc, char **argv);

    bool startInprocess;
    Configuration gateConfiguration;
    CryptoNote::NetNodeConfig netNodeConfig;
    CryptoNote::CoreConfig coreConfig;
    RpcNodeConfiguration remoteNodeConfig;
};

} // namespace PaymentService
