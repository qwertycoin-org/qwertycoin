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

#include <Breakpad/Breakpad.h>
#include <Common/SignalHandler.h>
#include <Logging/ConsoleLogger.h>
#include <Logging/LoggerGroup.h>
#include <Logging/LoggerRef.h>
#include <System/Dispatcher.h>
#include "MinerManager.h"

int main(int argc, char **argv)
{
    Qwertycoin::Breakpad::ExceptionHandler exceptionHandler;

    try {
        CryptoNote::MiningConfig config;
        config.parse(argc, argv);

        if (config.help) {
            config.printHelp();
            return 0;
        }

        Logging::LoggerGroup loggerGroup;
        Logging::ConsoleLogger consoleLogger(static_cast<Logging::Level>(config.logLevel));
        loggerGroup.addLogger(consoleLogger);

        System::Dispatcher dispatcher;
        Miner::MinerManager app(dispatcher, config, loggerGroup);

        app.start();
    } catch (std::exception &e) {
        std::cerr << "Fatal: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
