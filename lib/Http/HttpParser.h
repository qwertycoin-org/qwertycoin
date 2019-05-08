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

#pragma once

#include <iostream>
#include <map>
#include <string>
#include <Http/HttpRequest.h>
#include <Http/HttpResponse.h>

namespace CryptoNote {

class HttpParser
{
public:
    HttpParser() = default;

    void receiveRequest(std::istream &stream, HttpRequest &request);
    void receiveResponse(std::istream &stream, HttpResponse &response);
    static HttpResponse::HTTP_STATUS parseResponseStatusFromString(const std::string &status);

private:
    static void readWord(std::istream &stream, std::string &word);
    static void readHeaders(std::istream &stream, HttpRequest::Headers &headers);
    static bool readHeader(std::istream &stream, std::string &name, std::string &value);
    static void readBody(std::istream &stream, std::string &body, const size_t bodyLen);
    static size_t getBodyLen(const HttpRequest::Headers &headers);
};

} // namespace CryptoNote
