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
//
// You should have received a copy of the GNU Lesser General Public License
// along with Qwertycoin.  If not, see <http://www.gnu.org/licenses/>.

#include <Http/HttpParser.h>
#include <System/Ipv4Resolver.h>
#include <System/Ipv4Address.h>
#include <System/TcpConnector.h>
#include <Rpc/HttpClient.h>

namespace CryptoNote {

HttpClient::HttpClient(System::Dispatcher &dispatcher, const std::string &address, uint16_t port)
    : m_dispatcher(dispatcher),
      m_address(address),
      m_port(port)
{
}

HttpClient::~HttpClient()
{
    if (m_connected) {
        disconnect();
    }
}

void HttpClient::request(const HttpRequest &req, HttpResponse &res)
{
    if (!m_connected) {
        connect();
    }

    try {
        std::iostream stream(m_streamBuf.get());
        HttpParser parser;
        stream << req;
        stream.flush();
        parser.receiveResponse(stream, res);
    } catch (const std::exception &) {
        disconnect();
        throw;
    }
}

void HttpClient::connect()
{
    try {
        auto ipAddr = System::Ipv4Resolver(m_dispatcher).resolve(m_address);
        m_connection = System::TcpConnector(m_dispatcher).connect(ipAddr, m_port);
        m_streamBuf.reset(new System::TcpStreambuf(m_connection));
        m_connected = true;
    } catch (const std::exception &e) {
        throw ConnectException(e.what());
    }
}

bool HttpClient::isConnected() const
{
    return m_connected;
}

void HttpClient::disconnect()
{
    m_streamBuf.reset();

    try {
        m_connection.write(nullptr, 0); // socket shutdown.
    } catch (std::exception &) {
        // ignoring possible exception.
    }

    try {
        m_connection = System::TcpConnection();
    } catch (std::exception &) {
        // ignoring possible exception.
    }

    m_connected = false;
}

ConnectException::ConnectException(const std::string &whatArg)
    : std::runtime_error(whatArg.c_str())
{
}

} // namespace CryptoNote
