

#pragma once

#include <deque>
#include <list>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include <boost/type_traits/integral_constant.hpp>
#include <boost/type_traits/is_integral.hpp>

#include <Common/StringTools.h>

#include <CryptoNoteCore/LMDB/BinaryArchive.h>

#include <Serialization/BinarySerializationTools.h>

namespace CryptoNote {
typedef std::string blobData; // BinaryArray usually is std::vector<uin8_t>

namespace Serial {
template <class T>
struct IsBlobType
{
    typedef boost::false_type type;
};

template <class T>
struct IsBasicType
{
    typedef boost::false_type type;
};

template <typename F, typename S>
struct IsBasicType<std::pair<F, S>>
{
typename boost::true_type type;
};

template <>
struct IsBasicType<std::string>
{
    typedef boost::true_type type;
};

template <class Archive, class T>
struct Serializer
{
    static bool serialize(Archive &ar, T &v)
    {
        return serialize(ar,
                         v,
                         typename boost::is_integral<T>::type(),
                         typename IsBlobType<T>::type(),
                         typename IsBasicType<T>::type());
    }

    template <typename A>
    static bool serialize(Archive &ar, T &v, boost::false_type, boost::true_type, A a)
    {
        ar.serializeBlob(&v, sizeof(v));
        return true;
    }

    template <typename A>
    static bool serialize(Archive &ar, T &v, boost::false_type, boost::false_type, A a)
    {
        ar.serializeBlob(&v, sizeof(v));
        return true;
    }

    template <typename A>
    static bool serialize(Archive &ar, T &v, boost::true_type, boost::false_type, A a)
    {
        ar.serializeInt(v);
        return true;
    }

    static bool serialize(Archive &ar, T &v, boost::false_type, boost::false_type, boost::true_type)
    {
        return doSerialize(ar, v);
    }

    static void serializeCustom(Archive &ar, T &v, boost::true_type) {}
};

template <class Archive, class T>
inline bool doSerialize(Archive &ar, T &v)
{
    return Serializer<Archive, T>::serialize(ar, v);
}

template <class Archive, class T>
inline bool doSerialize(Archive &ar, bool &v)
{
    ar.serializeBlob(&v, sizeof(v));
    return true;
}

namespace Detail {
/*!
 * @fn doCheckStreamState
 *
 * @brief self explanatory
 */
template <class Stream>
bool doCheckStreamState(Stream &s, boost::mpl::bool_<true>)
{
    return s.good();
}

/*!
 * @fn doCheckStreamState
 *
 * @brief self explanatory
 *
 * @details Also checks to make sure that the stream is not at EoF
 */
template <class Stream>
bool doCheckStreamState(Stream &s, boost::mpl::bool_<false>)
{
    bool result = false;
    if (s.good()) {
        std::ios_base::iostate state = s.rdstate();
        result = EOF == s.peek();
        s.clear(state);
    }
    return result;
}

template <typename Archive, class T>
bool serializeContainerElement(Archive &ar, T &e)
{
    return doSerialize(ar, e);
}

template <typename Archive>
bool serializeContainerElement(Archive &ar, uint32_t &e)
{
    ar.serializeVarint(e);
    return true;
}

template <typename Archive>
bool serializeContainerElement(Archive &ar, uint64_t &e)
{
    ar.serializeVarint(e);
    return true;
}

template <typename C>
void doReserve(C &c, size_t N) {}

template <typename T>
void doReserve(std::vector<T> &c, size_t N)
{
    c.reserve(N);
}

template <typename T>
void doAdd(std::vector<T> &c, T &&e)
{
    c.emplace_back(std::move(e));
}
} // namespace Detail

/*!
 * @fn checkStreamState
 *
 * @brief calls Detail::doCheckStreamState for ar
 */
template <class Archive>
bool checkStreamState(Archive &ar)
{
    return Detail::doCheckStreamState(ar.stream(), typename Archive::isSaving());
}

/*!
 * @fn serialize
 *
 * @brief serializes @a v into @a ar
 */
template <class Archive, class T>
inline bool serialize(Archive &ar, T &v)
{
    bool r = doSerialize(ar, v);
    return r && checkStreamState(ar);
}

template <template <bool> class Archive>
inline bool doSerialize(Archive<false> &ar, std::string &str)
{
    size_t size = 0;
    ar.serializeVarint(size);
    if (ar.remainingBytes() < size) {
        ar.stream().setstate(std::ios::failbit);
        return false;
    }

    std::unique_ptr<std::string::value_type[]> buf(new std::string::value_type[size]);
    ar.serializeBlob(buf.get(), size);
    str.erase();
    str.append(buf.get(), size);
    return true;
}

template <template <bool> class Archive>
inline bool doSerialize(Archive<true> &ar, std::string &str)
{
    size_t size = str.size();
    ar.serializeVarint(size);
    ar.serializeBlob(const_cast<std::string::value_type *>(str.c_str()), size);
    return true;
}

template <template <bool> class Archive, typename C>
bool doSerializeContainer(Archive<false> &ar, C &v)
{
    size_t container;
    ar.beginArray(container);
    if (!ar.stream().good()) {
        return false;
    }
    v.clear();

    // very basic sanity check
    if (ar.remainingBytes() < container) {
        ar.stream().setstate(std::ios::failbit);
        return false;
    }

    Detail::doReserve(v, container);

    for (size_t i = 0; i < container; i++) {
        if (i > 0) {
            ar.delimitArray();
        }

        typename C::value_type e;

        if (!Detail::serializeContainerElement(ar, e)) {
            return false;
        }

        Detail::doAdd(v, std::move(e));

        if (!ar.stream().good()) {
            return false;
        }
    }

    ar.endArray();
    return true;
}

template <template <bool> class Archive, typename C>
bool doSerializeContainer(Archive<true> &ar, C &v)
{
    size_t container = v.size();
    ar.beginArray(container);
    for (auto i = v.begin(); i != v.end(); ++i) {
        if (!ar.stream().good()) {
            return false;
        }

        if (i != v.begin()) {
            ar.delimitArray();
        }

        if (!Detail::serializeContainerElement(ar, const_cast<typename C::value_type &>(*i))) {
            return false;
        }

        if (!ar.stream().good()) {
            return false;
        }
    }

    ar.endArray();
    return true;
}

template <template <bool> class Archive, class T>
bool doSerialize(Archive<false> &ar, std::vector<T> &v)
{
    return doSerializeContainer(ar, v);
}

template <template <bool> class Archive, class T>
bool doSerialize(Archive<true> &ar, std::vector<T> &v)
{
    return doSerializeContainer(ar, v);
}

std::vector<uint8_t> fromHex(const std::string &string);
std::string toHex(const std::vector<uint8_t> &vec);

template <class TObject>
bool TSerializableObjectToBlob(const TObject &to, CryptoNote::blobData &bBlob)
{
    std::stringstream ss;
    BinaryArchive<false> bA(ss);
    bool r = serialize(bA, const_cast<TObject &>(to));
    bBlob = ss.str();
    return r;
}

template <class TObject>
CryptoNote::blobData TSerializableObjectToBlob(const TObject &to)
{
    CryptoNote::blobData b;
    BinaryArray bA;
    loadFromBinary(to, bA);
    b = toHex(bA);
    return b;
}
} // namespace Serial
} // namespace CryptoNote
