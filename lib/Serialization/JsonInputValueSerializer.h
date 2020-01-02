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

#include <Common/JsonValue.h>
#include <Serialization/ISerializer.h>

namespace CryptoNote {

class JsonInputValueSerializer : public ISerializer
{
public:
    explicit JsonInputValueSerializer(const Common::JsonValue &value);
    explicit JsonInputValueSerializer(Common::JsonValue &&value);
    ~JsonInputValueSerializer() override = default;

    SerializerType type() const override;

    bool beginObject(Common::StringView name) override;
    void endObject() override;

    bool beginArray(size_t &size, Common::StringView name) override;
    void endArray() override;

    bool operator()(uint8_t &value, Common::StringView name) override;
    bool operator()(int16_t &value, Common::StringView name) override;
    bool operator()(uint16_t &value, Common::StringView name) override;
    bool operator()(int32_t &value, Common::StringView name) override;
    bool operator()(uint32_t &value, Common::StringView name) override;
    bool operator()(int64_t &value, Common::StringView name) override;
    bool operator()(uint64_t &value, Common::StringView name) override;
    bool operator()(double &value, Common::StringView name) override;
    bool operator()(bool &value, Common::StringView name) override;
    bool operator()(std::string &value, Common::StringView name) override;

    bool binary(void *value, size_t size, Common::StringView name) override;
    bool binary(std::string &value, Common::StringView name) override;

    template<typename T>
    bool operator()(T &value, Common::StringView name)
    {
        return ISerializer::operator()(value, name);
    }

private:
    const Common::JsonValue *getValue(Common::StringView name);

    template <typename T>
    bool getNumber(Common::StringView name, T &v)
    {
        auto ptr = getValue(name);

        if (!ptr) {
            return false;
        }

        v = static_cast<T>(ptr->getInteger());

        return true;
    }

private:
    Common::JsonValue m_value;
    std::vector<const Common::JsonValue *> m_chain;
    std::vector<size_t> m_idxs;
};

} // namespace CryptoNote
