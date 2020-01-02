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

#include <limits>
#include <Common/ArrayView.h>

namespace Common {

/*!
    \class ArrayRef
    \inmodule Common
    \brief This is a pair of pointer to object of parametrized type and size.

    Supports 'EMPTY' and 'NIL' representations as follows:
    - 'data' == 'nullptr' && 'size' == 0 - EMPTY NIL
    - 'data' != 'nullptr' && 'size' == 0 - EMPTY NOTNIL
    - 'data' == 'nullptr' && 'size' > 0 - Undefined
    - 'data' != 'nullptr' && 'size' > 0 - NOTEMPTY NOTNIL
    For signed integer 'Size', 'ArrayRef' with 'size' < 0 is undefined.

    \note It is recommended to pass 'ArrayRef' to procedures by value.
*/
template<class ObjectType = uint8_t, class SizeType = size_t>
class ArrayRef
{
public:
    typedef ObjectType Object;
    typedef SizeType Size;

    const static Size INVALID;
    const static ArrayRef EMPTY;
    const static ArrayRef NIL;

    /*!
        Default constructor.
        Leaves object uninitialized. Any usage before initializing it is undefined.
    */
    ArrayRef()
#ifndef NDEBUG // In debug mode, fill in object with invalid state (undefined).
        : m_data(nullptr),
          m_size(INVALID)
#endif
    {
    }

    /*!
        Direct constructor.
        The behavior is undefined unless 'arrayData' != 'nullptr' || 'arraySize' == 0
    */
    ArrayRef(Object *arrayData, Size arraySize)
        : m_data(arrayData),
          m_size(arraySize)
    {
        assert(m_data != nullptr || m_size == 0);
    }

    /*!
        Constructor from C array.
        The behavior is undefined unless 'arrayData' != 'nullptr' || 'arraySize' == 0.
        Input state can be malformed using poiner conversions.
    */
    template<Size arraySize>
    explicit ArrayRef(Object(&arrayData)[arraySize])
        : m_data(arrayData),
          m_size(arraySize)
    {
        assert(m_data != nullptr || m_size == 0);
    }

    /*!
        Copy constructor.
        Performs default action - bitwise copying of source object.
        The behavior is undefined unless 'other' 'ArrayRef' is in defined state,
        that is 'data' != 'nullptr' || 'size' == 0
    */
    ArrayRef(const ArrayRef &other)
        : m_data(other.m_data),
          m_size(other.m_size)
    {
        assert(m_data != nullptr || m_size == 0);
    }

    /*!
        Destructor.
        Does nothing.
    */
    ~ArrayRef() = default;

    Object *getData() const
    {
        assert(m_data != nullptr || m_size == 0);

        return m_data;
    }

    Size getSize() const
    {
        assert(m_data != nullptr || m_size == 0);

        return m_size;
    }

    /*!
        Return false if 'ArrayRef' is not EMPTY.
        The behavior is undefined unless 'ArrayRef' was initialized.
    */
    bool isEmpty() const
    {
        assert(m_data != nullptr || m_size == 0);

        return m_size == 0;
    }

    /*!
        Return false if 'ArrayRef' is not NIL.
        The behavior is undefined unless 'ArrayRef' was initialized.
    */
    bool isNil() const
    {
        assert(m_data != nullptr || m_size == 0);

        return m_data == nullptr;
    }

    /*!
        Get first element.
        The behavior is undefined unless 'ArrayRef' was initialized and 'size' > 0
    */
    Object &first() const
    {
        assert(m_data != nullptr || m_size == 0);

        assert(m_size > 0);

        return *m_data;
    }

    /*!
        Get last element.
        The behavior is undefined unless 'ArrayRef' was initialized and 'size' > 0
    */
    Object &last() const
    {
        assert(m_data != nullptr || m_size == 0);

        assert(m_size > 0);

        return *(m_data + (m_size - 1));
    }

    /*!
        Return a pointer to the first element.
        The behavior is undefined unless 'ArrayRef' was initialized.
    */
    Object *begin() const
    {
        assert(m_data != nullptr || m_size == 0);

        return m_data;
    }

    /*!
        Return a pointer after the last element.
        The behavior is undefined unless 'ArrayRef' was initialized.
    */
    Object *end() const
    {
        assert(m_data != nullptr || m_size == 0);

        return m_data + m_size;
    }

    /*!
        Return false if 'ArrayRef' does not contain 'object' at the beginning.
        The behavior is undefined unless 'ArrayRef' was initialized.
    */
    bool beginsWith(const Object &object) const
    {
        assert(m_data != nullptr || m_size == 0);

        if (m_size == 0) {
            return false;
        }

        return *m_data == object;
    }

    /*!
        Return false if 'ArrayRef' does not contain 'other' at the beginning.
        The behavior is undefined unless both arrays were initialized.
    */
    bool beginsWith(ArrayView<Object, Size> other) const
    {
        assert(m_data != nullptr || m_size == 0);

        if (m_size >= other.getSize()) {
            for (Size i = 0;; ++i) {
                if (i == other.getSize()) {
                    return true;
                }

                if (!(*(m_data + i) == *(other.getData() + i))) {
                    break;
                }
            }
        }

        return false;
    }

    /*!
        Return false if 'ArrayRef' does not contain 'object'.
        The behavior is undefined unless 'ArrayRef' was initialized.
    */
    bool contains(const Object &object) const
    {
        assert(m_data != nullptr || m_size == 0);

        for (Size i = 0; i < m_size; ++i) {
            if (*(m_data + i) == object) {
                return true;
            }
        }

        return false;
    }

    /*!
        Return false if 'ArrayRef' does not contain 'other'.
        The behavior is undefined unless both arrays were initialized.
    */
    bool contains(ArrayView<Object, Size> other) const
    {
        assert(m_data != nullptr || m_size == 0);

        if (m_size >= other.getSize()) {
            Size i = m_size - other.getSize();
            for (Size j = 0; !(i < j); ++j) {
                for (Size k = 0;; ++k) {
                    if (k == other.getSize()) {
                        return true;
                    }

                    if (!(*(m_data + j + k) == *(other.getData() + k))) {
                        break;
                    }
                }
            }
        }

        return false;
    }

    /*!
        Return false if 'ArrayRef' does not contain 'object' at the end.
        The behavior is undefined unless 'ArrayRef' was initialized.
    */
    bool endsWith(const Object &object) const
    {
        assert(m_data != nullptr || m_size == 0);

        if (m_size == 0) {
            return false;
        }

        return *(m_data + (m_size - 1)) == object;
    }

    /*!
        Return false if 'ArrayRef' does not contain 'other' at the end.
        The behavior is undefined unless both arrays were initialized.
    */
    bool endsWith(ArrayView<Object, Size> other) const
    {
        assert(m_data != nullptr || m_size == 0);

        if (m_size >= other.getSize()) {
            Size i = m_size - other.getSize();
            for (Size j = 0;; ++j) {
                if (j == other.getSize()) {
                    return true;
                }

                if (!(*(m_data + i + j) == *(other.getData() + j))) {
                    break;
                }
            }
        }

        return false;
    }

    /*!
        Looks for the first occurence of 'object' in 'ArrayRef',
        returns index or INVALID if there are no occurences.
        The behavior is undefined unless 'ArrayRef' was initialized.
    */
    Size find(const Object &object) const
    {
        assert(m_data != nullptr || m_size == 0);

        for (Size i = 0; i < m_size; ++i) {
            if (*(m_data + i) == object) {
                return i;
            }
        }

        return INVALID;
    }

    /*!
        Looks for the first occurence of 'other' in 'ArrayRef',
        returns index or INVALID if there are no occurences.
        The behavior is undefined unless both arrays were initialized.
    */
    Size find(ArrayView<Object, Size> other) const
    {
        assert(m_data != nullptr || m_size == 0);

        if (m_size >= other.getSize()) {
            Size i = m_size - other.getSize();
            for (Size j = 0; !(i < j); ++j) {
                for (Size k = 0;; ++k) {
                    if (k == other.getSize()) {
                        return j;
                    }

                    if (!(*(m_data + j + k) == *(other.getData() + k))) {
                        break;
                    }
                }
            }
        }

        return INVALID;
    }

    /*!
        Looks for the last occurence of 'object' in 'ArrayRef',
        returns index or INVALID if there are no occurences.
        The behavior is undefined unless 'ArrayRef' was initialized.
    */
    Size findLast(const Object &object) const
    {
        assert(m_data != nullptr || m_size == 0);

        for (Size i = 0; i < m_size; ++i) {
            if (*(m_data + (m_size - 1 - i)) == object) {
                return m_size - 1 - i;
            }
        }

        return INVALID;
    }

    /*!
        Looks for the first occurence of 'other' in 'ArrayRef',
        returns index or INVALID if there are no occurences.
        The behavior is undefined unless both arrays were initialized.
    */
    Size findLast(ArrayView<Object, Size> other) const
    {
        assert(m_data != nullptr || m_size == 0);

        if (m_size >= other.getSize()) {
            Size i = m_size - other.getSize();
            for (Size j = 0; !(i < j); ++j) {
                for (Size k = 0;; ++k) {
                    if (k == other.getSize()) {
                        return i - j;
                    }

                    if (!(*(m_data + (i - j + k)) == *(other.getData() + k))) {
                        break;
                    }
                }
            }
        }

        return INVALID;
    }

    /*!
        Returns subarray of 'headSize' first elements.
        The behavior is undefined unless 'ArrayRef' was initialized and 'headSize' <= 'size'.
    */
    ArrayRef head(Size headSize) const
    {
        assert(m_data != nullptr || m_size == 0);

        assert(headSize <= m_size);

        return ArrayRef(m_data, headSize);
    }

    /*!
        Returns subarray of 'tailSize' last elements.
        The behavior is undefined unless 'ArrayRef' was initialized and 'tailSize' <= 'size'.
    */
    ArrayRef tail(Size tailSize) const
    {
        assert(m_data != nullptr || m_size == 0);

        assert(tailSize <= m_size);

        return ArrayRef(m_data + (m_size - tailSize), tailSize);
    }

    /*!
        Returns 'ArrayRef' without 'headSize' first elements.
        The behavior is undefined unless 'ArrayRef' was initialized and 'headSize' <= 'size'.
    */
    ArrayRef unhead(Size headSize) const
    {
        assert(m_data != nullptr || m_size == 0);

        assert(headSize <= m_size);

        return ArrayRef(m_data + headSize, m_size - headSize);
    }

    /*!
        Returns 'ArrayRef' without 'tailSize' last elements.
        The behavior is undefined unless 'ArrayRef' was initialized and 'tailSize' <= 'size'.
    */
    ArrayRef untail(Size tailSize) const
    {
        assert(m_data != nullptr || m_size == 0);

        assert(tailSize <= m_size);

        return ArrayRef(m_data, m_size - tailSize);
    }

    /*!
        Returns subarray starting at 'startIndex' and contaning 'endIndex' - 'startIndex' elements.
        The behavior is undefined unless 'ArrayRef' was initialized and 'startIndex' <= 'endIndex'
        and 'endIndex' <= 'size'.
    */
    ArrayRef range(Size startIndex, Size endIndex) const
    {
        assert(m_data != nullptr || m_size == 0);

        assert(startIndex <= endIndex && endIndex <= m_size);

        return ArrayRef(m_data + startIndex, endIndex - startIndex);
    }

    /*!
        Returns subarray starting at 'startIndex' and contaning 'sliceSize' elements.
        The behavior is undefined unless 'ArrayRef' was initialized and 'startIndex' <= 'size'
        and 'startIndex' + 'sliceSize' <= 'size'.
    */
    ArrayRef slice(Size startIndex, Size sliceSize) const
    {
        assert(m_data != nullptr || m_size == 0);

        assert(startIndex <= m_size && startIndex + sliceSize <= m_size);

        return ArrayRef(m_data + startIndex, sliceSize);
    }

    /*!
        Copy 'object' to each element of 'ArrayRef'.
        The behavior is undefined unless 'ArrayRef' was initialized.
    */
    const ArrayRef &fill(const Object &object) const
    {
        assert(m_data != nullptr || m_size == 0);

        for (Size i = 0; i < m_size; ++i) {
            *(m_data + i) = object;
        }

        return *this;
    }

    /*!
        Reverse 'ArrayRef' elements.
        The behavior is undefined unless 'ArrayRef' was initialized.
    */
    const ArrayRef &reverse() const
    {
        assert(m_data != nullptr || m_size == 0);

        for (Size i = 0; i < m_size / 2; ++i) {
            Object object = *(m_data + i);
            *(m_data + i) = *(m_data + (m_size - 1 - i));
            *(m_data + (m_size - 1 - i)) = object;
        }

        return *this;
    }

    /*!
        Copy assignment operator.
        The behavior is undefined unless 'other' 'ArrayRef' is in defined state,
        that is 'data' != 'nullptr' || 'size' == 0
    */
    ArrayRef &operator=(ArrayRef other)
    {
        assert(other.m_data != nullptr || other.m_size == 0);

        m_data = other.m_data;
        m_size = other.m_size;

        return *this;
    }

    /*!
        Compare elements of two arrays, return false if there is a difference.
        EMPTY and NIL arrays are considered equal.
        The behavior is undefined unless both arrays were initialized.
    */
    bool operator==(ArrayView<Object, Size> other) const
    {
        assert(m_data != nullptr || m_size == 0);

        if (m_size == other.getSize()) {
            for (Size i = 0;; ++i) {
                if (i == m_size) {
                    return true;
                }

                if (!(*(m_data + i) == *(other.getData() + i))) {
                    break;
                }
            }
        }

        return false;
    }

    /*!
        Compare elements two arrays, return false if there is no difference.
        EMPTY and NIL arrays are considered equal.
        The behavior is undefined unless both arrays were initialized.
    */
    bool operator!=(ArrayView<Object, Size> other) const
    {
        assert(m_data != nullptr || m_size == 0);

        if (m_size == other.getSize()) {
            for (Size i = 0;; ++i) {
                if (i == m_size) {
                    return false;
                }

                if (*(m_data + i) != *(other.getData() + i)) {
                    break;
                }
            }
        }

        return true;
    }

    /*!
        Get 'ArrayRef' element by index.
        The behavior is undefined unless 'ArrayRef' was initialized and 'index' < 'size'.
    */
    Object &operator[](Size index) const
    {
        assert(m_data != nullptr || m_size == 0);

        assert(index < m_size);

        return *(m_data + index);
    }

    operator ArrayView<Object, Size>() const
    {
        return ArrayView<Object, Size>(m_data, m_size);
    }

protected:
    Object *m_data;
    Size m_size;
};

template<class Object, class Size>
const Size ArrayRef<Object, Size>::INVALID = std::numeric_limits<Size>::max();

template<class Object, class Size>
const ArrayRef<Object, Size> ArrayRef<Object, Size>::EMPTY(reinterpret_cast<Object *>(1), 0);

template<class Object, class Size>
const ArrayRef<Object, Size> ArrayRef<Object, Size>::NIL(nullptr, 0);

} // namespace Common
