// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2018-2019, The Qwertycoin developers
// Copyright (c) 2014-2016 XDN developers
// Copyright (c) 2016-2018 Karbowanec developers
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

#include <boost/scope_exit.hpp>
#include <Http/HttpParser.h>
#include <Rpc/HttpServer.h>
#include <System/InterruptedException.h>
#include <System/TcpStream.h>
#include <System/Ipv4Address.h>

using namespace Logging;

namespace {

std::string base64Encode(const std::string &data)
{
    static const char *et = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const size_t resultSize = 4 * ((data.size() + 2) / 3);
    std::string result;
    result.reserve(resultSize);

    for (size_t i = 0; i < data.size(); i += 3) {
        size_t a = static_cast<size_t>(data[i]);
        size_t b = i + 1 < data.size() ? static_cast<size_t>(data[i + 1]) : 0;
        size_t c = i + 2 < data.size() ? static_cast<size_t>(data[i + 2]) : 0;

        result.push_back(et[a >> 2]);
        result.push_back(et[((a & 0x3) << 4) | (b >> 4)]);
        if (i + 1 < data.size()) {
            result.push_back(et[((b & 0xF) << 2) | (c >> 6)]);
            if (i + 2 < data.size()) {
                result.push_back(et[c & 0x3F]);
            }
        }
    }

    while (result.size() != resultSize) {
        result.push_back('=');
    }

    return result;
}

void fillUnauthorizedResponse(CryptoNote::HttpResponse &response)
{
    response.setStatus(CryptoNote::HttpResponse::STATUS_401);
    response.addHeader("WWW-Authenticate", "Basic realm=\"RPC\"");
    response.addHeader("Content-Type", "text/plain");
    response.setBody("Authorization required");
}

} // namespace

namespace CryptoNote {

HttpServer::HttpServer(System::Dispatcher &dispatcher, Logging::ILogger &log)
    : m_dispatcher(dispatcher),
      workingContextGroup(dispatcher),
      logger(log, "HttpServer")
{
}

void HttpServer::start(const std::string &address,
                       uint16_t port,
                       const std::string &user,
                       const std::string &password)
{
    m_listener = System::TcpListener(m_dispatcher, System::Ipv4Address(address), port);
    workingContextGroup.spawn(std::bind(&HttpServer::acceptLoop, this));

    if (!user.empty() || !password.empty()) {
        m_credentials = base64Encode(user + ":" + password);
    }
}

void HttpServer::stop()
{
    workingContextGroup.interrupt();
    workingContextGroup.wait();
}

void HttpServer::acceptLoop()
{
    try {
        System::TcpConnection connection;
        bool accepted = false;

        while (!accepted) {
            try {
                connection = m_listener.accept();
                accepted = true;
            } catch (System::InterruptedException &) {
                throw;
            } catch (std::exception &) {
                // try again
            }
        }

        m_connections.insert(&connection);
        BOOST_SCOPE_EXIT_ALL(this, &connection) {
        m_connections.erase(&connection); };

        workingContextGroup.spawn(std::bind(&HttpServer::acceptLoop, this));

        // auto addr = connection.getPeerAddressAndPort();
        auto addr = std::pair<System::Ipv4Address, uint16_t>(static_cast<System::Ipv4Address>(0),0);
        try {
            addr = connection.getPeerAddressAndPort();
        } catch (std::runtime_error &) {
            logger(WARNING) << "Could not get IP of connection";
        }

        logger(DEBUGGING)
            << "Incoming connection from "
            << addr.first.toDottedDecimal()
            << ":"
            << addr.second;

        System::TcpStreambuf streambuf(connection);
        std::iostream stream(&streambuf);
        HttpParser parser;

        for (;;) {
            HttpRequest req;
            HttpResponse resp;
            resp.addHeader("Access-Control-Allow-Origin", "*");
            resp.addHeader("content-type", "application/json");

            parser.receiveRequest(stream, req);
            if (authenticate(req)) {
                processRequest(req, resp);
            } else {
                logger(WARNING)
                    << "Authorization required "
                    << addr.first.toDottedDecimal()
                    << ":"
                    << addr.second;
                fillUnauthorizedResponse(resp);
            }

            stream << resp;
            stream.flush();

            if (stream.peek() == std::iostream::traits_type::eof()) {
                break;
            }
        }

        logger(DEBUGGING)
            << "Closing connection from "
            << addr.first.toDottedDecimal()
            << ":"
            << addr.second
            << " total="
            << m_connections.size();
    } catch (System::InterruptedException &) {
        // do nothing
    } catch (std::exception &e) {
        logger(DEBUGGING) << "Connection error: " << e.what();
    }
}

bool HttpServer::authenticate(const HttpRequest &request) const
{
    if (!m_credentials.empty()) {
        auto headerIt = request.getHeaders().find("authorization");
        if (headerIt == request.getHeaders().end()) {
            return false;
        }

        if (headerIt->second.substr(0, 6) != "Basic ") {
            return false;
        }

        if (headerIt->second.substr(6) != m_credentials) {
            return false;
        }
    }

    return true;
}

size_t HttpServer::get_connections_count() const
{
    return m_connections.size();
}

} // namespace CryptoNote
