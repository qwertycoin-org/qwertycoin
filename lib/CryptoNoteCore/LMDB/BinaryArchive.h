

#pragma once

#include <cassert>
#include <iostream>
#include <iterator>

#include <boost/type_traits/make_unsigned.hpp>

#include <Common/Varint.h>

/*!
 * @struct BinaryArchiveBase
 *
 * @brief base for the binary archive type
 * @details It isnt used outside of this file, which its only
 *          purpose is to define the functions used for the BinaryArchive.
 *          Its a header, basically. I think it was declared simply to save typing.
 */
template<class Stream, bool IsSaving>
struct BinaryArchiveBase
{
    typedef Stream streamType;
    typedef BinaryArchiveBase<Stream, IsSaving> baseType;
    typedef boost::mpl::bool_<IsSaving> isSaving;

    typedef uint8_t variantTagType;

    explicit BinaryArchiveBase(streamType &s)
            : mStream(s)
    {}

    /*!
     * definition of standard API functions
     */
    void tag(const char *)
    {}
    void beginObject()
    {}
    void endObject()
    {}
    void beginVariant()
    {}
    void endVariant()
    {}

    /*!
     * I just want to leave a comment saying how this line really shows
     * flaws in the ownership model of many OOP languages, thats all.
     */
    streamType &stream()
    { return mStream; }

protected:
    streamType &mStream;
};

/*!
 * @struct BinaryArchive
 *
 * @brief the actually binary archive type
 * @details The boolean template argument /a W is the isSaving parameter
 *          for BinaryArchiveBase.
 *
 * The isSaving parameter says whether the archive is being read from (false)
 * or written to (true)
 */
template<bool W>
struct BinaryArchive;

template<>
struct BinaryArchive<false>: public BinaryArchiveBase<std::istream, false>
{
    explicit BinaryArchive(streamType &s) : baseType(s)
    {
        std::streampos pos = mStream.tellg();
        mStream.seekg(0, std::ios_base::end);
        mEofPos = mStream.tellg();
        mStream.seekg(pos);
    }

    template<class T>
    void serializeInt(T &v)
    {
        serializeUint(*(typename boost::make_unsigned<T>::type *) &v);
    }

    /*!
     * @fn serializeUint
     *
     * @brief serializes an unsigned integer
     */
    template<class T>
    void serializeUint(T &v, size_t width = sizeof(T))
    {
        T ret = 0;
        unsigned shift = 0;
        for (size_t i = 0; i < width; i++) {
            char c;
            mStream.get(c);
            T b = (unsigned char) c;
            ret += (b << shift);
            shift += 8;
        }
        v = ret;
    }

    void serializeBlob(void *buf, size_t len, const char *delimiter = "")
    {
        mStream.read((char *)buf, len);
    }

    template<class T>
    void serializeVarint(T &v)
    {
        serializeUVarint(*(typename boost::make_unsigned<T>::type *)(&v));
    }

    template<class T>
    void serializeUVarint(T &v)
    {
        typedef std::istreambuf_iterator<char> it;
        Tools::read_varint(it(mStream), it(), v); // XXX handle failure
    }

    void beginArray(size_t &s)
    {
        serializeVarint(s);
    }

    void beginArray() {}
    void delimitArray() {}
    void endArray() {}

    void beginString(const char *delimiter  /*="\""*/) {}
    void endString(const char *delimiter    /*="\""*/) {}

    void readVariantTag(variantTagType &t)
    {
        serializeInt(t);
    }

    size_t remainingBytes()
    {
        if (!mStream.good()) {
            return 0;
        }
        assert(mStream.tellg() <= mEofPos);
        return mEofPos - mStream.tellg();
    }
protected:
    std::streamoff mEofPos;
};

template<>
struct BinaryArchive<true> : public BinaryArchiveBase<std::ostream, true>
{
    explicit BinaryArchive(streamType &s) : baseType(s) {}

    template <class T>
    void serializeInt(T v)
    {
        serializeUInt(static_cast<typename boost::make_unsigned<T>::type>(v));
    }

    template <class T>
    void serializeUInt(T v)
    {
        for (size_t i = 0; i < sizeof(T); i++) {
            mStream.put((char)(v & 0xff));
            if (1 < sizeof(T)) {
                v >>= 8;
            }
        }
    }

    void serializeBlob(void *buf, size_t len, const char *delimiter="")
    {
        mStream.write((char *)buf, len);
    }

    template <class T>
    void serializeVarint(T &v)
    {
        serializeUVarint(*(typename boost::make_unsigned<T>::type *)(&v));
    }

    template <class T>
    void serializeUVarint(T &v)
    {
        typedef std::ostreambuf_iterator<char> it;
        Tools::write_varint(it(mStream), v);
    }

    void beginArray() {}
    void delimitArray() {}
    void endArray() {}

    void beginString(const char *delimiter="\"") {}
    void endString(const char *delimiter="\"") {}

    void writeVariantTag(variantTagType t)
    {
        serializeInt(t);
    }
};
