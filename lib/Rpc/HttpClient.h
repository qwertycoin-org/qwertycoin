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

#include <memory>
#include <Common/StringTools.h>
#include <Http/HttpRequest.h>
#include <Http/HttpResponse.h>
#include <Rpc/JsonRpc.h>
#include <Serialization/SerializationTools.h>
#include <System/TcpConnection.h>
#include <System/TcpStream.h>

namespace CryptoNote {

class ConnectException : public std::runtime_error
{
public:
    explicit ConnectException(const std::string &whatArg);
};

class HttpClient
{
public:
    HttpClient(System::Dispatcher &dispatcher, const std::string &address, uint16_t port);
    ~HttpClient();

    void request(const HttpRequest &req, HttpResponse &res);

    bool isConnected() const;

private:
    void connect();
    void disconnect();

private:
    const std::string m_address;
    const uint16_t m_port;

    bool m_connected = false;
    System::Dispatcher& m_dispatcher;
    System::TcpConnection m_connection;
    std::unique_ptr<System::TcpStreambuf> m_streamBuf;
};

template <typename Request, typename Response>
void invokeJsonCommand(HttpClient &cli,
                       const std::string &url,
                       const Request &req,
                       Response &res,
                       const std::string &user = "",
                       const std::string &password = "")
{
    HttpRequest hreq;
    HttpResponse hres;

    hreq.addHeader("Content-Type", "application/json");
    if (!user.empty() || !password.empty()) {
        hreq.addHeader("Authorization", "Basic " + Common::base64Decode(user + ":" + password));
    }
    hreq.setUrl(url);
    hreq.setBody(storeToJson(req));
    cli.request(hreq, hres);

    if (hres.getStatus() != HttpResponse::STATUS_200) {
        throw std::runtime_error("HTTP status: " + std::to_string(hres.getStatus()));
    }

    if (!loadFromJson(res, hres.getBody())) {
        throw std::runtime_error("Failed to parse JSON response");
    }
}

template <typename Request, typename Response>
void invokeJsonRpcCommand(HttpClient &cli,
                          const std::string &method,
                          const Request &req,
                          Response &res,
                          const std::string &user = "",
                          const std::string &password = "")
{
    try {
        JsonRpc::JsonRpcRequest jsReq;

        jsReq.setMethod(method);
        jsReq.setParams(req);

        HttpRequest httpReq;
        HttpResponse httpRes;

        httpReq.addHeader("Content-Type", "application/json");
        if (!user.empty() || !password.empty()) {
            httpReq.addHeader("Authorization", "Basic " + Common::base64Decode(user + ":" + password));
        }
        httpReq.setUrl("/json_rpc");
        httpReq.setBody(jsReq.getBody());

        cli.request(httpReq, httpRes);

        JsonRpc::JsonRpcResponse jsRes;

        //if (httpRes.getStatus() == HttpResponse::STATUS_200) {
        jsRes.parse(httpRes.getBody());
        if (!jsRes.getResult(res)) {
            throw std::runtime_error("HTTP status: " + std::to_string(httpRes.getStatus()));
        }
        //}

    } catch (const ConnectException&) {
        throw std::runtime_error("HTTP status: CONNECT_ERROR");
    } catch (const std::exception&) {
        throw std::runtime_error("HTTP status: NETWORK_ERROR");
    }
}

template <typename Request, typename Response>
void invokeBinaryCommand(HttpClient &cli,
                         const std::string &url,
                         const Request &req,
                         Response &res,
                         const std::string &user = "",
                         const std::string &password = "")
{
    HttpRequest hreq;
    HttpResponse hres;

    if (!user.empty() || !password.empty()) {
        hreq.addHeader("Authorization", "Basic " + Common::base64Decode(user + ":" + password));
    }
    hreq.setUrl(url);
    hreq.setBody(storeToBinaryKeyValue(req));
    cli.request(hreq, hres);

    if (!loadFromBinaryKeyValue(res, hres.getBody())) {
        throw std::runtime_error("Failed to parse binary response");
    }
}

} // namespace CryptoNote
