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

#include <vector>
#include <Common/IOutputStream.h>
#include <Serialization/ISerializer.h>
#include <Serialization/MemoryStream.h>

namespace CryptoNote {

class KVBinaryOutputStreamSerializer : public ISerializer
{
    enum class State
    {
        Root,
        Object,
        ArrayPrefix,
        Array
    };

    struct Level
    {
        State state;
        std::string name;
        size_t count;

        explicit Level(Common::StringView nm)
            : name(nm),
              state(State::Object),
              count(0)
        {
        }

        Level(Common::StringView nm, size_t arraySize)
            : name(nm),
              state(State::ArrayPrefix),
              count(arraySize)
        {
        }

        Level(Level &&rv) noexcept
        {
            state = rv.state;
            name = std::move(rv.name);
            count = rv.count;
        }
    };

public:
    KVBinaryOutputStreamSerializer();
    ~KVBinaryOutputStreamSerializer() override = default;

    void dump(Common::IOutputStream &target);

    ISerializer::SerializerType type() const override;

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
    void writeElementPrefix(uint8_t type, Common::StringView name);
    void checkArrayPreamble(uint8_t type);
    MemoryStream &stream();

private:
    std::vector<MemoryStream> m_objectsStack;
    std::vector<Level> m_stack;
};

} // namespace CryptoNote
