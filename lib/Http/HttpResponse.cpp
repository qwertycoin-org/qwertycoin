// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2014-2016 XDN developers
// Copyright (c) 2018-2023, The Qwertycoin Group.
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

#include <stdexcept>
#include <Http/HttpResponse.h>

namespace {

const char *getStatusString(CryptoNote::HttpResponse::HTTP_STATUS status)
{
    switch (status) {
    case CryptoNote::HttpResponse::STATUS_200:
        return "200 OK";
    case CryptoNote::HttpResponse::STATUS_401:
        return "401 Unauthorized";
    case CryptoNote::HttpResponse::STATUS_404:
        return "404 Not Found";
    case CryptoNote::HttpResponse::STATUS_500:
        return "500 Internal Server Error";
    default:
        throw std::runtime_error("Unknown HTTP status code is given");
    }
}

const char *getErrorBody(CryptoNote::HttpResponse::HTTP_STATUS status)
{
    switch (status) {
    case CryptoNote::HttpResponse::STATUS_401:
        return "Authorization required\n";
    case CryptoNote::HttpResponse::STATUS_404:
        return "Requested url is not found\n";
    case CryptoNote::HttpResponse::STATUS_500:
        return "Internal server error is occurred\n";
    default:
        throw std::runtime_error("Error body for given status is not available");
    }
}

} // namespace

namespace CryptoNote {

HttpResponse::HttpResponse()
{
    status = STATUS_200;
    headers["Server"] = "CryptoNote-based HTTP server";
    headers["Access-Control-Allow-Origin"] = "*";
}

void HttpResponse::setStatus(HTTP_STATUS s)
{
    status = s;
    if (status != HttpResponse::STATUS_200) {
        setBody(getErrorBody(status));
    }
}

void HttpResponse::addHeader(const std::string &name, const std::string &value)
{
    headers[name] = value;
}

void HttpResponse::setBody(const std::string &b)
{
    body = b;
    if (!body.empty()) {
        headers["Content-Length"] = std::to_string(body.size());
    } else {
        headers.erase("Content-Length");
    }
}

std::ostream &HttpResponse::printHttpResponse(std::ostream &os) const
{
    os << "HTTP/1.1 " << getStatusString(status) << "\r\n";

    for (const auto &pair: headers) {
        os << pair.first << ": " << pair.second << "\r\n";
    }
    os << "\r\n";

    if (!body.empty()) {
        os << body;
    }

    return os;
}

} // namespace CryptoNote
