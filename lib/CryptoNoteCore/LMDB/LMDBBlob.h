// Copyright (c) 2018-2021, The Qwertycoin Group.
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

#include <cassert>
#include <deque>
#include <iostream>
#include <iterator>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include <boost/type_traits/integral_constant.hpp>
#include <boost/type_traits/is_integral.hpp>
#include <boost/type_traits/make_unsigned.hpp>

#include <Common/Varint.h>
#include <CryptoNote.h>
#include <Serialization/BinarySerializationTools.h>

/**
 * @struct FBinaryArchiveBase
 *
 * @brief Base for the binary archive type
 *
 * @details It isn't used outside of this file, which its only
 * purpose is to define the functions used for the binary_archive. Its
 * a header, basically. I think it was declared simply to save typing...
 */
template<class Stream, bool bIsSaving>
struct FBinaryArchiveBase
{
    typedef Stream streamType;
    typedef FBinaryArchiveBase<Stream, bIsSaving> FBaseType;
    typedef boost::mpl::bool_<bIsSaving> sIsSaving;
    typedef uint8_t uVariantTagType;

    explicit FBinaryArchiveBase(streamType &s) : sStream(s) {};

    void tag(const char *) {}

    void beginObject() {}

    void endObject() {}

    void beginVarInt() {}

    void endVarInt() {}

    streamType &stream() { return sStream; }

protected:
    streamType &sStream;
};

/**
 * @struct FBinaryArchive
 *
 * @brief The actually binary archive type
 *
 * @details The boolean template argument @a bIsWrite is
 * the bIsSaving parameter for FBinaryArchiveBase
 *
 * The bIsSaving parameter says whether the archive is being read from (false)
 * or is written to (true)
 */
template<bool bIsWrite>
struct FBinaryArchive;

template<>
struct FBinaryArchive<false> : public FBinaryArchiveBase<std::istream, false>
{
    explicit FBinaryArchive(streamType &s) : FBaseType(s)
    {
        streamType::pos_type pos = sStream.tellg();
        sStream.seekg(0, std::ios_base::end);
        sEoFPos = sStream.tellg();
        sStream.seekg(pos);
    }

    template<class T>
    void serializeInt(T &value)
    {
        serializeUInt(*(typename boost::make_unsigned<T>::type *) &value);
    }

    template<class T>
    void serializeUInt(T &value, size_t width = sizeof(T))
    {
        T ret = 0;
        unsigned uShift = 0;
        for (size_t i = 0; i < width; i++) {
            char cC;
            sStream.get(cC);
            T b = (unsigned char) cC;
            ret += (b << uShift);
            uShift += 8;
        }

        value = ret;
    }

    void serializeBlob(void *buffer, size_t uLength, const char *delimiter = "")
    {
        sStream.read((char *) buffer, uLength);
    }

    template<class T>
    void serializeVarInt(T &value)
    {
        serializeUVarInt(*(typename boost::make_unsigned<T>::type *) (&value));
    }

    template<class T>
    void serializeUVarInt(T &value)
    {
        typedef std::istreambuf_iterator<char> it;
        Tools::read_varint(it(sStream), it(), value);
    }

    void beginArray(size_t &uSize)
    {
        serializeVarInt(uSize);
    }

    void beginArray() {}

    void delimitArray() {}

    void endArray() {}

    void beginString(const char *delimiter /*="\""*/) {}

    void endString(const char *delimiter /*="\""*/) {}

    void readVariantTag(uVariantTagType &tagType)
    {
        serializeInt(tagType);
    }

    size_t uRemainingBytes()
    {
        if (!sStream.good()) {
            return 0;
        }

        assert(sStream.tellg() <= sEoFPos);

        return sEoFPos - sStream.tellg();
    }

protected:
    std::streamoff sEoFPos;
};

template<>
struct FBinaryArchive<true> : public FBinaryArchiveBase<std::ostream, true>
{
    explicit FBinaryArchive(streamType &stream) : FBaseType(stream) {}

    template<class T>
    void TSerializeInt(T value)
    {
        TSerializeUInt(static_cast<typename boost::make_unsigned<T>::type>(value));
    }

    template<class T>
    void TSerializeUInt(T value)
    {
        for (size_t i = 0; i < sizeof(T); i++) {
            sStream.put((char) (value & 0xFF));
            if (1 < sizeof(T)) {
                value >>= 8;
            }
        }
    }

    void serializeBlob(void *buffer, size_t uLength, const char *delimiter = "")
    {
        sStream.write((char *) buffer, uLength);
    }

    template<class T>
    void serializeVarInt(T &value)
    {
        serializeUVarInt(*(typename boost::make_unsigned<T>::type *) (&value));
    }

    template<class T>
    void serializeUVarInt(T &value)
    {
        typedef std::ostreambuf_iterator<char> it;
        Tools::write_varint(it(sStream), value);
    }

    void beginArray(size_t uSize)
    {
        serializeVarInt(uSize);
    }

    void beginArray() {}

    void delimitArray() {}

    void endArray() {}

    void beginString(const char *delimiter = "\"") {}

    void endString(const char *delimiter = "\"") {}

    void writeVariantTag(uVariantTagType tagType)
    {
        TSerializeInt(tagType);
    }
};

namespace CryptoNote {
    typedef std::string blobData;

    namespace Serial {
        template<class T>
        struct FIsBlobType
        {
            typedef boost::false_type sType;
        };

        template<class T>
        struct FIsBasicType
        {
            typedef boost::false_type sType;
        };

        template<typename F, typename S>
        struct FIsBasicType<std::pair<F, S>>
        {
            typedef boost::true_type sType;
        };

        template<>
        struct FIsBasicType<std::string>
        {
            typedef boost::true_type sType;
        };

        template<class Archive, class T>
        struct FSerializer
        {
            static bool serialize(Archive &sArchive,
                                  T &value)
            {
                return serialize(sArchive,
                                 value,
                                 typename boost::is_integral<T>::type(),
                                 typename FIsBlobType<T>::sType(),
                                 typename FIsBasicType<T>::sType());
            }

            template<typename A>
            static bool serialize(Archive &sArchive,
                                  T &value,
                                  boost::false_type,
                                  boost::true_type,
                                  A sA)
            {
                sArchive.serializeBlob(&value, sizeof(value));

                return true;
            }

            template<typename A>
            static bool serialize(Archive &sArchive,
                                  T &value,
                                  boost::false_type,
                                  boost::false_type,
                                  A sA)
            {
                sArchive.serializeBlob(&value, sizeof(value));

                return true;
            }

            template<typename A>
            static bool serialize(Archive &sArchive,
                                  T &value,
                                  boost::true_type,
                                  boost::false_type,
                                  A sA)
            {
                sArchive.serializeInt(value);

                return true;
            }

            static bool serialize(Archive &sArchive, T &value, boost::false_type, boost::false_type, boost::true_type)
            {
                return doSerialize(sArchive, value);
            }

            static void serializeCustom(Archive &sArchive, T &value, boost::true_type)
            {
            }
        };

        template<class Archive, class T>
        inline bool doSerialize(Archive &sArchive, T &value)
        {
            return FSerializer<Archive, T>::serialize(sArchive, value);
        }

        template<class Archive>
        inline bool doSerialize(Archive &sArchive, bool &value)
        {
            sArchive.serializeBlob(&value, sizeof(value));

            return true;
        }

        namespace Detail {
            /**
             * @fn doCheckStreamState
             *
             * @brief self explanatory
             */
            template<class QStream>
            bool doCheckStreamState(QStream &sStream,
                                    boost::mpl::bool_<true>)
            {
                return sStream.good();
            }

            /**
             * @fn doCheckStreamState
             *
             * @details Also checks to make sure that the stream is not at EOF
             */
            template<class QStream>
            bool doCheckStreamState(QStream &sStream,
                                    boost::mpl::bool_<false>)
            {
                bool result = false;

                if (sStream.good()) {
                    std::ios_base::iostate state = sStream.rdstate();
                    result = EOF == sStream.peek();
                    sStream.clear(state);
                }

                return result;
            }

            template<typename Archive, class T>
            bool serializeContainerElement(Archive &sArchive, T &sElement)
            {
                return doSerialize(sArchive, sElement);
            }

            template<typename Archive>
            bool serializeContainerElement(Archive &sArchive, uint32_t &uElement)
            {
                sArchive.serializeVarInt(uElement);

                return true;
            }

            template<typename Archive>
            bool serializeContainerElement(Archive &sArchive, uint64_t &sElement)
            {
                sArchive.serializeVarInt(sElement);

                return true;
            }

            template<typename C>
            void doReserve(C &sC, size_t uN) {}

            template<typename T>
            void doReserve(std::vector<T> &vC, size_t uN)
            {
                vC.reserve(uN);
            }

            template<typename T>
            void doAdd(std::vector<T> &vC, T &&sE)
            {
                vC.emplace_back(std::move(sE));
            }
        }

        /**
         * @fn checkStreamState
         *
         * @brief calls Detail::doCheckStreamState for Archive
         */
        template<class Archive>
        bool checkStreamState(Archive &sArchive)
        {
            return Detail::doCheckStreamState(sArchive.stream(), typename Archive::sIsSaving());
        }

        /**
         * @fn serialize
         *
         * @brief serializes @a sValue into @a sArchive
         */
        template<class Archive, class T>
        inline bool serialize(Archive &sArchive, T &sValue)
        {
            bool res = doSerialize(sArchive, sValue);

            return res && checkStreamState(sArchive);
        }

        template<template<bool> class Archive>
        inline bool doSerialize(Archive<false> &sArchive, std::string &cString)
        {
            size_t uSize = 0;
            sArchive.serializeVarInt(uSize);
            if (sArchive.uRemainingBytes() < uSize) {
                sArchive.stream().setstate(std::ios::failbit);

                return false;
            }

            std::unique_ptr<std::string::value_type[]> buffer(new std::string::value_type[uSize]);
            sArchive.serializeBlob(buffer.get(), uSize);
            cString.erase();
            cString.append(buffer.get(), uSize);

            return true;
        }

        template<template<bool> class Archive>
        inline bool doSerialize(Archive<true> &sArchive, std::string &cString)
        {
            size_t uSize = cString.size();
            sArchive.serializeVarInt(uSize);
            sArchive.serializeBlob(const_cast<std::string::value_type *>(cString.c_str()), uSize);

            return true;
        }

        template<template<bool> class Archive, typename C>
        bool doSerializeContainer(Archive<false> &sArchive, C &vC)
        {
            size_t uCount;
            sArchive.beginArray(uCount);

            if (!sArchive.stream().good()) {
                return false;
            }

            vC.clear();

            // Very basic sanity check
            if (sArchive.uRemainingBytes() < uCount) {
                sArchive.stream().setstate(std::ios::failbit);

                return false;
            }

            Detail::doReserve(vC, uCount);

            for (size_t i = 0; i < uCount; i++) {
                if (i > 0) {
                    sArchive.delimitArray();
                }

                typename C::value_type sElem;
                if (!Detail::serializeContainerElement(sArchive, sElem)) {
                    return false;
                }

                Detail::doAdd(vC, std::move(sElem));
                if (!sArchive.stream().good()) {
                    return false;
                }
            }

            sArchive.endArray();

            return true;
        }

        template<template<bool> class Archive, typename C>
        bool doSerializeContainer(Archive<true> &sArchive, C &vC)
        {
            size_t uCount = vC.size();
            sArchive.beginArray(uCount);

            for (auto i = vC.begin(); i != vC.end(); ++i) {
                if (!sArchive.stream().good()) {
                    return false;
                }

                if (i != vC.begin()) {
                    sArchive.delimitArray();
                }

                if (!Detail::serializeContainerElement(sArchive, const_cast<typename C::value_type &>(*i))) {
                    return false;
                }

                if (!sArchive.stream().good()) {
                    return false;
                }

                sArchive.endArray();

                return true;
            }
        }

        template<template<bool> class Archive, class T>
        bool doSerialize(Archive<false> &sArchive, std::vector<T> &vT)
        {
            return doSerializeContainer(sArchive, vT);
        }

        template<template<bool> class Archive, class T>
        bool doSerialize(Archive<true> &sArchive, std::vector<T> &vT)
        {
            return doSerializeContainer(sArchive, vT);
        }

        std::vector<uint8_t> fromHex(const std::string &cHex);

        std::string toHex(const std::vector<uint8_t> &vBinaryArray);

        template<class T>
        bool serializableObjectToBlob(const T &sTo, CryptoNote::blobData &sBlob)
        {
            std::stringstream cSs;
            FBinaryArchive<false> fBinaryArchive(cSs);

            bool r = serialize(fBinaryArchive, const_cast<T &>(sTo));
            sBlob = cSs.str();

            return r;
        }

        template<class T>
        CryptoNote::blobData serializableObjectToBlob(const T &sTo)
        {
            CryptoNote::blobData sBlob;
            BinaryArray sBinArr;

            loadFromBinary(sTo, sBinArr);
            sBlob = toHex(sBinArr);

            return sBlob;
        }
    } // namespace Serial
} // namespace CryptoNote