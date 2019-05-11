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
#include <android.h>

namespace CryptoNote {

class HttpRequest
{
public:
    typedef std::map<std::string, std::string> Headers;

    const std::string &getMethod() const;
    const std::string &getUrl() const;
    const Headers &getHeaders() const;
    const std::string &getBody() const;

    void addHeader(const std::string &name, const std::string &value);
    void setBody(const std::string &b);
    void setUrl(const std::string &uri);

private:
    std::string method;
    std::string url;
    Headers headers;
    std::string body;

    std::ostream &printHttpRequest(std::ostream &os) const;

    friend std::ostream &operator<<(std::ostream &os, const HttpRequest &resp);

    friend class HttpParser;
};

inline std::ostream &operator<<(std::ostream &os, const HttpRequest &resp)
{
    return resp.printHttpRequest(os);
}

} // namespace CryptoNote
