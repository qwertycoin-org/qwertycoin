// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
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
// Copyright (c) 2014-2016 XDN developers
//
// You should have received a copy of the GNU Lesser General Public License
// along with Qwertycoin.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <unordered_set>
#include <Http/HttpRequest.h>
#include <Http/HttpResponse.h>
#include <Logging/LoggerRef.h>
#include <System/ContextGroup.h>
#include <System/Dispatcher.h>
#include <System/Event.h>
#include <System/TcpConnection.h>
#include <System/TcpListener.h>

namespace CryptoNote {

class HttpServer
{
public:
    HttpServer(System::Dispatcher &dispatcher, Logging::ILogger &log);

    void start(const std::string &address,
               uint16_t port,
               const std::string &user = "",
               const std::string &password = "");
    void stop();

    virtual void processRequest(const HttpRequest& request, HttpResponse& response) = 0;
    virtual size_t get_connections_count() const;

protected:
    System::Dispatcher &m_dispatcher;

private:
    bool authenticate(const HttpRequest& request) const;
    void acceptLoop();

private:
    System::ContextGroup workingContextGroup;
    Logging::LoggerRef logger;
    System::TcpListener m_listener;
    std::unordered_set<System::TcpConnection *> m_connections;
    std::string m_credentials;
};

} // namespace CryptoNote
