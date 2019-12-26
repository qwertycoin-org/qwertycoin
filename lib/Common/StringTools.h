// Copyright (c) 2012-2017, The CryptoNote developers, The Bytecoin developers
// Copyright (c) 2016-2018, The Karbowanec developers
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

#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace Common {

std::string asString(const void *data, size_t size);
std::string asString(const std::vector<uint8_t> &data);

std::vector<uint8_t> asBinaryArray(const std::string &data);

// Returns value of hex 'character', throws on error
uint8_t fromHex(char character);

// Assigns value of hex 'character' to 'value', returns false on error, does not throw
bool fromHex(char character, uint8_t &value);

// Assigns values of hex 'text' to buffer 'data' up to 'bufferSize',
// returns actual data size, throws on error
size_t fromHex(const std::string &text, void *data, size_t bufferSize);

// Assigns values of hex 'text' to buffer 'data' up to 'bufferSize',
// assigns actual data size to 'size', returns false on error, does not throw
bool fromHex(const std::string &text, void *data, size_t bufferSize, size_t &size);

// Returns values of hex 'text', throws on error
std::vector<uint8_t> fromHex(const std::string &text);

// Appends values of hex 'text' to 'data', returns false on error, does not throw
bool fromHex(const std::string &text, std::vector<uint8_t> &data);

template <typename T>
bool podFromHex(const std::string &text, T &val)
{
    size_t outSize;

    return fromHex(text, &val, sizeof(val), outSize) && outSize == sizeof(val);
}

// Returns hex representation of ('data', 'size'), does not throw
std::string toHex(const void *data, size_t size);

// Appends hex representation of ('data', 'size') to 'text', does not throw
void toHex(const void *data, size_t size, std::string &text);

// Returns hex representation of 'data', does not throw
std::string toHex(const std::vector<uint8_t> &data);

// Appends hex representation of 'data' to 'text', does not throw
void toHex(const std::vector<uint8_t> &data, std::string &text);

template<class T>
std::string podToHex(const T &s)
{
    return toHex(&s, sizeof(s));
}

std::string extract(std::string &text, char delimiter);
std::string extract(const std::string &text, char delimiter, size_t &offset);

template<typename T>
T fromString(const std::string &text)
{
    T value;

    std::istringstream stream(text);
    stream >> value;

    if (stream.fail()) {
        throw std::runtime_error("fromString: unable to parse value");
    }

    return value;
}

template<typename T>
bool fromString(const std::string &text, T &value)
{
    std::istringstream stream(text);
    stream >> value;

    return !stream.fail();
}

template<typename T>
std::vector<T> fromDelimitedString(const std::string &source, char delimiter)
{
    std::vector<T> data;
    for (size_t offset = 0; offset != source.size();) {
        data.emplace_back(fromString<T>(extract(source, delimiter, offset)));
    }

    return data;
}

template<typename T>
bool fromDelimitedString(const std::string &source, char delimiter, std::vector<T> &data)
{
    for (size_t offset = 0; offset != source.size();) {
        T value;
        if (!fromString<T>(extract(source, delimiter, offset), value)) {
            return false;
        }

        data.emplace_back(value);
    }

    return true;
}

template<typename T>
std::string toString(const T &value)
{
    std::ostringstream stream;
    stream << value;

    return stream.str();
}

template<typename T>
void toString(const T &value, std::string &text)
{
    std::ostringstream stream;
    stream << value;
    text += stream.str();
}

bool loadFileToString(const std::string &filepath, std::string &buf);
bool saveStringToFile(const std::string &filepath, const std::string &buf);

std::string base64Decode(std::string const &encoded_string);

std::string ipAddressToString(uint32_t ip);
uint32_t stringToIpAddress(const std::string &addr);
bool parseIpAddressAndPort(uint32_t &ip, uint32_t &port, const std::string &addr);

std::string timeIntervalToString(uint64_t intervalInSeconds);

} // namespace Common
