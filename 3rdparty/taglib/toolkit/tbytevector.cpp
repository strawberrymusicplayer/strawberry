/***************************************************************************
    copyright            : (C) 2002 - 2008 by Scott Wheeler
    email                : wheeler@kde.org
 ***************************************************************************/

/***************************************************************************
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License version   *
 *   2.1 as published by the Free Software Foundation.                     *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful, but   *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA         *
 *   02110-1301  USA                                                       *
 *                                                                         *
 *   Alternatively, this file is available under the Mozilla Public        *
 *   License Version 1.1.  You may obtain a copy of the License at         *
 *   http://www.mozilla.org/MPL/                                           *
 ***************************************************************************/

#include <memory>
#include <algorithm>
#include <iostream>
#include <limits>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "tstring.h"
#include "tdebug.h"
#include "tutils.h"

#include "tbytevector.h"

// This is a bit ugly to keep writing over and over again.

// A rather obscure feature of the C++ spec that I hadn't thought of that makes working with C libs much more efficient.
// There's more here:
// http://www.informit.com/isapi/product_id~{9C84DAB4-FE6E-49C5-BB0A-FB50331233EA}/content/index.asp

namespace Strawberry_TagLib {
namespace TagLib {

template<class TIterator>
size_t findChar(const TIterator dataBegin, const TIterator dataEnd, char c, size_t offset, size_t byteAlign) {

  const size_t dataSize = dataEnd - dataBegin;
  if (offset + 1 > dataSize)
    return ByteVector::npos();

  // n % 0 is invalid

  if (byteAlign == 0)
    return ByteVector::npos();

  for (TIterator it = dataBegin + offset; it < dataEnd; it += byteAlign) {
    if (*it == c)
      return (it - dataBegin);
  }

  return ByteVector::npos();

}

template<class TIterator>
size_t findVector(const TIterator dataBegin, const TIterator dataEnd, const TIterator patternBegin, const TIterator patternEnd, size_t offset, size_t byteAlign) {

  const size_t dataSize = dataEnd - dataBegin;
  const size_t patternSize = patternEnd - patternBegin;
  if (patternSize == 0 || offset + patternSize > dataSize)
    return ByteVector::npos();

  // Special case that pattern contains just single char.

  if (patternSize == 1)
    return findChar(dataBegin, dataEnd, *patternBegin, offset, byteAlign);

  // n % 0 is invalid

  if (byteAlign == 0)
    return ByteVector::npos();

  // We don't use sophisticated algorithms like Knuth-Morris-Pratt here.

  // In the current implementation of TagLib, data and patterns are too small
  // for such algorithms to work effectively.

  for (TIterator it = dataBegin + offset; it < dataEnd - patternSize + 1; it += byteAlign) {

    TIterator itData = it;
    TIterator itPattern = patternBegin;

    while (*itData == *itPattern) {
      ++itData;
      ++itPattern;

      if (itPattern == patternEnd)
        return (it - dataBegin);
    }
  }

  return ByteVector::npos();

}

template<typename T, size_t LENGTH, ByteOrder ENDIAN>
inline T toNumber(const ByteVector &v, size_t offset) {
  if (LENGTH == sizeof(T) && offset + LENGTH <= v.size()) {
    // Uses memcpy instead of reinterpret_cast to avoid an alignment exception.
    T tmp;
    ::memcpy(&tmp, v.data() + offset, sizeof(T));

    if (ENDIAN != Utils::systemByteOrder())
      return Utils::byteSwap(tmp);
    else
      return tmp;
  }
  else if (offset < v.size()) {
    const size_t length = std::min(LENGTH, v.size() - offset);
    T sum = 0;
    for (size_t i = 0; i < length; ++i) {
      const size_t shift = (ENDIAN == BigEndian ? length - 1 - i : i) * 8;
      sum |= static_cast<T>(static_cast<unsigned char>(v[offset + i])) << shift;
    }

    return sum;
  }
  else {
    debug("toNumber<T>() - offset is out of range. Returning 0.");
    return 0;
  }
}

template<typename T, ByteOrder ENDIAN>
inline ByteVector fromNumber(T value) {

  if (ENDIAN != Utils::systemByteOrder())
    value = Utils::byteSwap(value);

  return ByteVector(reinterpret_cast<const char *>(&value), sizeof(T));

}

template<typename TFloat, typename TInt, ByteOrder ENDIAN>
TFloat toFloat(const ByteVector &v, size_t offset) {

  if (offset > v.size() - sizeof(TInt)) {
    debug("toFloat() - offset is out of range. Returning 0.");
    return 0.0;
  }

  union {
    TInt i;
    TFloat f;
  } tmp;
  ::memcpy(&tmp, v.data() + offset, sizeof(TInt));

  if (ENDIAN != Utils::systemByteOrder())
    tmp.i = Utils::byteSwap(tmp.i);

  return tmp.f;

}

template<typename TFloat, typename TInt, ByteOrder ENDIAN>
ByteVector fromFloat(TFloat value) {

  union {
    TInt i;
    TFloat f;
  } tmp;
  tmp.f = value;

  if (ENDIAN != Utils::systemByteOrder())
    tmp.i = Utils::byteSwap(tmp.i);

  return ByteVector(reinterpret_cast<char *>(&tmp), sizeof(TInt));

}

template<ByteOrder ENDIAN>
long double toFloat80(const ByteVector &v, size_t offset) {

  using std::swap;

  if (offset > v.size() - 10) {
    debug("toFloat80() - offset is out of range. Returning 0.");
    return 0.0;
  }

  unsigned char bytes[10];
  ::memcpy(bytes, v.data() + offset, 10);

  if (ENDIAN == LittleEndian) {
    swap(bytes[0], bytes[9]);
    swap(bytes[1], bytes[8]);
    swap(bytes[2], bytes[7]);
    swap(bytes[3], bytes[6]);
    swap(bytes[4], bytes[5]);
  }

  // 1-bit sign
  const bool negative = ((bytes[0] & 0x80) != 0);

  // 15-bit exponent
  const int exponent = ((bytes[0] & 0x7F) << 8) | bytes[1];

  // 64-bit fraction. Leading 1 is explicit.
  const unsigned long long fraction = (static_cast<unsigned long long>(bytes[2]) << 56) | (static_cast<unsigned long long>(bytes[3]) << 48) | (static_cast<unsigned long long>(bytes[4]) << 40) | (static_cast<unsigned long long>(bytes[5]) << 32) | (static_cast<unsigned long long>(bytes[6]) << 24) | (static_cast<unsigned long long>(bytes[7]) << 16) | (static_cast<unsigned long long>(bytes[8]) << 8) | (static_cast<unsigned long long>(bytes[9]));

  long double val;
  if (exponent == 0 && fraction == 0)
    val = 0;
  else {
    if (exponent == 0x7FFF) {
      debug("toFloat80() - can't handle the infinity or NaN. Returning 0.");
      return 0.0;
    }
    else
      val = ::ldexp(static_cast<long double>(fraction), exponent - 16383 - 63);
  }

  if (negative)
    return -val;
  else
    return val;

}

class ByteVector::ByteVectorPrivate {
 public:
  ByteVectorPrivate() : data(new std::vector<char>()),
                        offset(0),
                        length(0) {}

  ByteVectorPrivate(ByteVectorPrivate *d, size_t o, size_t l) : data(d->data),
                                                                offset(d->offset + o),
                                                                length(l) {}

  ByteVectorPrivate(size_t l, char c) : data(new std::vector<char>(l, c)),
                                        offset(0),
                                        length(l) {}

  ByteVectorPrivate(const char *s, size_t l) : data(new std::vector<char>(s, s + l)),
                                               offset(0),
                                               length(l) {}

  std::shared_ptr<std::vector<char>> data;
  size_t offset;
  size_t length;
};

////////////////////////////////////////////////////////////////////////////////
// static members
////////////////////////////////////////////////////////////////////////////////

size_t ByteVector::npos() {
  return static_cast<size_t>(-1);
}

ByteVector ByteVector::fromCString(const char *s, size_t length) {
  if (length == npos())
    return ByteVector(s);
  else
    return ByteVector(s, length);
}

ByteVector ByteVector::fromUInt16LE(size_t value) {
  return fromNumber<unsigned short, LittleEndian>(static_cast<unsigned short>(value));
}

ByteVector ByteVector::fromUInt16BE(size_t value) {
  return fromNumber<unsigned short, BigEndian>(static_cast<unsigned short>(value));
}

ByteVector ByteVector::fromUInt32LE(size_t value) {
  return fromNumber<unsigned int, LittleEndian>(static_cast<unsigned int>(value));
}

ByteVector ByteVector::fromUInt32BE(size_t value) {
  return fromNumber<unsigned int, BigEndian>(static_cast<unsigned int>(value));
}

ByteVector ByteVector::fromUInt64LE(unsigned long long value) {
  return fromNumber<unsigned long long, LittleEndian>(value);
}

ByteVector ByteVector::fromUInt64BE(unsigned long long value) {
  return fromNumber<unsigned long long, BigEndian>(value);
}

ByteVector ByteVector::fromFloat32LE(float value) {
  return fromFloat<float, unsigned int, LittleEndian>(value);
}

ByteVector ByteVector::fromFloat32BE(float value) {
  return fromFloat<float, unsigned int, BigEndian>(value);
}

ByteVector ByteVector::fromFloat64LE(double value) {
  return fromFloat<double, unsigned long long, LittleEndian>(value);
}

ByteVector ByteVector::fromFloat64BE(double value) {
  return fromFloat<double, unsigned long long, BigEndian>(value);
}

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

ByteVector::ByteVector() : d(new ByteVectorPrivate()) {}

ByteVector::ByteVector(size_t size, char value) : d(new ByteVectorPrivate(size, value)) {}

ByteVector::ByteVector(const ByteVector &v) : d(new ByteVectorPrivate(*v.d)) {}

ByteVector::ByteVector(const ByteVector &v, size_t offset, size_t length) : d(new ByteVectorPrivate(v.d, offset, length)) {}

ByteVector::ByteVector(char c) : d(new ByteVectorPrivate(1, c)) {}

ByteVector::ByteVector(const char *data, size_t length) : d(new ByteVectorPrivate(data, length)) {}

ByteVector::ByteVector(const char *data) : d(new ByteVectorPrivate(data, ::strlen(data))) {}

ByteVector::~ByteVector() {
  delete d;
}

ByteVector &ByteVector::setData(const char *data, size_t length) {
  ByteVector(data, length).swap(*this);
  return *this;
}

ByteVector &ByteVector::setData(const char *data) {
  ByteVector(data).swap(*this);
  return *this;
}

char *ByteVector::data() {
  detach();
  return (size() > 0) ? (&(*d->data)[d->offset]) : nullptr;
}

const char *ByteVector::data() const {
  return (size() > 0) ? (&(*d->data)[d->offset]) : nullptr;
}

ByteVector ByteVector::mid(size_t index, size_t length) const {

  index = std::min(index, size());
  length = std::min(length, size() - index);

  return ByteVector(*this, index, length);

}

char ByteVector::at(size_t index) const {
  return (index < size()) ? (*d->data)[d->offset + index] : 0;
}

size_t ByteVector::find(const ByteVector &pattern, size_t offset, size_t byteAlign) const {
  return findVector<ConstIterator>(
    begin(), end(), pattern.begin(), pattern.end(), offset, byteAlign);

}

size_t ByteVector::find(char c, size_t offset, size_t byteAlign) const {
  return findChar<ConstIterator>(begin(), end(), c, offset, byteAlign);
}

size_t ByteVector::rfind(const ByteVector &pattern, size_t offset, size_t byteAlign) const {
  if (offset > 0) {
    offset = size() - offset - pattern.size();
    if (offset >= size())
      offset = 0;
  }

  const size_t pos = findVector<ConstReverseIterator>(
    rbegin(), rend(), pattern.rbegin(), pattern.rend(), offset, byteAlign);

  if (pos == npos())
    return npos();
  else
    return size() - pos - pattern.size();
}

bool ByteVector::containsAt(const ByteVector &pattern, size_t offset, size_t patternOffset, size_t patternLength) const {

  if (pattern.size() < patternLength)
    patternLength = pattern.size();

  // do some sanity checking -- all of these things are needed for the search to be valid
  const size_t compareLength = patternLength - patternOffset;
  if (offset + compareLength > size() || patternOffset >= pattern.size() || patternLength == 0)
    return false;

  return (::memcmp(data() + offset, pattern.data() + patternOffset, compareLength) == 0);

}

bool ByteVector::startsWith(const ByteVector &pattern) const {
  return containsAt(pattern, 0);
}

bool ByteVector::endsWith(const ByteVector &pattern) const {
  return containsAt(pattern, size() - pattern.size());
}

ByteVector &ByteVector::replace(char oldByte, char newByte) {

  detach();

  for (ByteVector::Iterator it = begin(); it != end(); ++it) {
    if (*it == oldByte)
      *it = newByte;
  }

  return *this;

}

ByteVector &ByteVector::replace(const ByteVector &pattern, const ByteVector &with) {

  if (pattern.size() == 1 && with.size() == 1)
    return replace(pattern[0], with[0]);

  // Check if there is at least one occurrence of the pattern.

  size_t offset = find(pattern, 0);
  if (offset == ByteVector::npos())
    return *this;

  if (pattern.size() == with.size()) {

    // We think this case might be common enough to optimize it.

    detach();
    do {
      ::memcpy(data() + offset, with.data(), with.size());
      offset = find(pattern, offset + pattern.size());
    } while (offset != ByteVector::npos());
  }
  else {

    // Loop once to calculate the result size.

    size_t dstSize = size();
    do {
      dstSize += with.size() - pattern.size();
      offset = find(pattern, offset + pattern.size());
    } while (offset != ByteVector::npos());

    // Loop again to copy modified data to the new vector.

    ByteVector dst(dstSize);
    size_t dstOffset = 0;

    offset = 0;
    while (true) {
      const size_t next = find(pattern, offset);
      if (next == ByteVector::npos()) {
        ::memcpy(dst.data() + dstOffset, data() + offset, size() - offset);
        break;
      }

      ::memcpy(dst.data() + dstOffset, data() + offset, next - offset);
      dstOffset += next - offset;

      ::memcpy(dst.data() + dstOffset, with.data(), with.size());
      dstOffset += with.size();

      offset = next + pattern.size();
    }

    swap(dst);
  }

  return *this;

}

size_t ByteVector::endsWithPartialMatch(const ByteVector &pattern) const {

  if (pattern.size() > size())
    return npos();

  const size_t startIndex = size() - pattern.size();

  // try to match the last n-1 bytes from the vector (where n is the pattern
  // size) -- continue trying to match n-2, n-3...1 bytes

  for (size_t i = 1; i < pattern.size(); i++) {
    if (containsAt(pattern, startIndex + i, 0, pattern.size() - i))
      return startIndex + i;
  }

  return npos();
}

ByteVector &ByteVector::append(const ByteVector &v) {

  if (v.isEmpty())
    return *this;

  detach();

  const size_t originalSize = size();
  const size_t appendSize = v.size();

  resize(originalSize + appendSize);
  ::memcpy(data() + originalSize, v.data(), appendSize);

  return *this;

}

ByteVector &ByteVector::append(char c) {
  resize(size() + 1, c);
  return *this;
}

ByteVector &ByteVector::clear() {
  ByteVector().swap(*this);
  return *this;
}

size_t ByteVector::size() const {
  return d->length;
}

ByteVector &ByteVector::resize(size_t size, char padding) {

  if (size != d->length) {
    detach();

    // Remove the excessive length of the internal buffer first to pad correctly.
    // This doesn't reallocate the buffer, since std::vector::resize() doesn't
    // reallocate the buffer when shrinking.

    d->data->resize(d->offset + d->length);
    d->data->resize(d->offset + size, padding);

    d->length = size;
  }

  return *this;

}

ByteVector::Iterator ByteVector::begin() {
  detach();
  return d->data->begin() + d->offset;
}

ByteVector::ConstIterator ByteVector::begin() const {
  return d->data->begin() + d->offset;
}

ByteVector::Iterator ByteVector::end() {
  detach();
  return d->data->begin() + d->offset + d->length;
}

ByteVector::ConstIterator ByteVector::end() const {
  return d->data->begin() + d->offset + d->length;
}

ByteVector::ReverseIterator ByteVector::rbegin() {
  detach();
  return d->data->rbegin() + (d->data->size() - (d->offset + d->length));
}

ByteVector::ConstReverseIterator ByteVector::rbegin() const {

  // Workaround for the Solaris Studio 12.4 compiler.
  // We need a const reference to the data vector so we can ensure the const version of rbegin() is called.
  const std::vector<char> &v = *d->data;
  return v.rbegin() + (v.size() - (d->offset + d->length));

}

ByteVector::ReverseIterator ByteVector::rend() {
  detach();
  return d->data->rbegin() + (d->data->size() - d->offset);
}

ByteVector::ConstReverseIterator ByteVector::rend() const {

  // Workaround for the Solaris Studio 12.4 compiler.
  // We need a const reference to the data vector so we can ensure the const version of rbegin() is called.
  const std::vector<char> &v = *d->data;
  return v.rbegin() + (v.size() - d->offset);

}

bool ByteVector::isEmpty() const {
  return (d->length == 0);
}

short ByteVector::toInt16LE(size_t offset) const {
  return static_cast<short>(toNumber<unsigned short, 2, LittleEndian>(*this, offset));
}

short ByteVector::toInt16BE(size_t offset) const {
  return static_cast<short>(toNumber<unsigned short, 2, BigEndian>(*this, offset));
}

unsigned short ByteVector::toUInt16LE(size_t offset) const {
  return toNumber<unsigned short, 2, LittleEndian>(*this, offset);
}

unsigned short ByteVector::toUInt16BE(size_t offset) const {
  return toNumber<unsigned short, 2, BigEndian>(*this, offset);
}

unsigned int ByteVector::toUInt24LE(size_t offset) const {
  return toNumber<unsigned int, 3, LittleEndian>(*this, offset);
}

unsigned int ByteVector::toUInt24BE(size_t offset) const {
  return toNumber<unsigned int, 3, BigEndian>(*this, offset);
}

unsigned int ByteVector::toUInt32LE(size_t offset) const {
  return toNumber<unsigned int, 4, LittleEndian>(*this, offset);
}

unsigned int ByteVector::toUInt32BE(size_t offset) const {
  return toNumber<unsigned int, 4, BigEndian>(*this, offset);
}

long long ByteVector::toInt64LE(size_t offset) const {
  return static_cast<long long>(toNumber<unsigned long long, 8, LittleEndian>(*this, offset));
}

long long ByteVector::toInt64BE(size_t offset) const {
  return static_cast<long long>(toNumber<unsigned long long, 8, BigEndian>(*this, offset));
}

float ByteVector::toFloat32LE(size_t offset) const {
  return toFloat<float, unsigned int, LittleEndian>(*this, offset);
}

float ByteVector::toFloat32BE(size_t offset) const {
  return toFloat<float, unsigned int, BigEndian>(*this, offset);
}

double ByteVector::toFloat64LE(size_t offset) const {
  return toFloat<double, unsigned long long, LittleEndian>(*this, offset);
}

double ByteVector::toFloat64BE(size_t offset) const {
  return toFloat<double, unsigned long long, BigEndian>(*this, offset);
}

long double ByteVector::toFloat80LE(size_t offset) const {
  return toFloat80<LittleEndian>(*this, offset);
}

long double ByteVector::toFloat80BE(size_t offset) const {
  return toFloat80<BigEndian>(*this, offset);
}

const char &ByteVector::operator[](size_t index) const {
  return (*d->data)[d->offset + index];
}

char &ByteVector::operator[](size_t index) {
  detach();
  return (*d->data)[d->offset + index];
}

bool ByteVector::operator==(const ByteVector &v) const {
  if (size() != v.size())
    return false;

  return (::memcmp(data(), v.data(), size()) == 0);
}

bool ByteVector::operator!=(const ByteVector &v) const {
  return !(*this == v);
}

bool ByteVector::operator==(const char *s) const {
  if (size() != ::strlen(s))
    return false;

  return (::memcmp(data(), s, size()) == 0);
}

bool ByteVector::operator!=(const char *s) const {
  return !(*this == s);
}

bool ByteVector::operator<(const ByteVector &v) const {
  const int result = ::memcmp(data(), v.data(), std::min(size(), v.size()));
  if (result != 0)
    return result < 0;
  else
    return size() < v.size();
}

bool ByteVector::operator>(const ByteVector &v) const {
  return (v < *this);
}

ByteVector ByteVector::operator+(const ByteVector &v) const {
  ByteVector sum(*this);
  sum.append(v);
  return sum;
}

ByteVector &ByteVector::operator=(const ByteVector &v) {
  ByteVector(v).swap(*this);
  return *this;
}

ByteVector &ByteVector::operator=(char c) {
  ByteVector(c).swap(*this);
  return *this;
}

ByteVector &ByteVector::operator=(const char *data) {
  ByteVector(data).swap(*this);
  return *this;
}

void ByteVector::swap(ByteVector &v) {
  using std::swap;

  swap(d, v.d);
}

ByteVector ByteVector::toHex() const {
  static const char hexTable[17] = "0123456789abcdef";

  ByteVector encoded(size() * 2);
  char *p = encoded.data();

  for (unsigned int i = 0; i < size(); i++) {
    unsigned char c = data()[i];
    *p++ = hexTable[(c >> 4) & 0x0F];
    *p++ = hexTable[(c)&0x0F];
  }

  return encoded;
}

ByteVector ByteVector::fromBase64(const ByteVector &input) {

  static const unsigned char base64[256] = {
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x3e, 0x80, 0x80, 0x80, 0x3f,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
  };

  size_t len = input.size();

  ByteVector output(len);

  const unsigned char *src = reinterpret_cast<const unsigned char*>(input.data());
  unsigned char *dst = reinterpret_cast<unsigned char*>(output.data());

  while (4 <= len) {

    // Check invalid character
    if (base64[src[0]] == 0x80)
      break;

    // Check invalid character
    if (base64[src[1]] == 0x80)
      break;

    // Decode first byte
    *dst++ = ((base64[src[0]] << 2) & 0xfc) | ((base64[src[1]] >> 4) & 0x03);

    if (src[2] != '=') {

      // Check invalid character
      if (base64[src[2]] == 0x80)
        break;

      // Decode second byte
      *dst++ = ((base64[src[1]] & 0x0f) << 4) | ((base64[src[2]] >> 2) & 0x0f);

      if (src[3] != '=') {

        // Check invalid character
        if (base64[src[3]] == 0x80)
          break;

        // Decode third byte
        *dst++ = ((base64[src[2]] & 0x03) << 6) | (base64[src[3]] & 0x3f);
      }
      else {
        // assume end of data
        len -= 4;
        break;
      }
    }
    else {
      // assume end of data
      len -= 4;
      break;
    }
    src += 4;
    len -= 4;
  }

  // Only return output if we processed all bytes
  if (len == 0) {
    output.resize(dst - reinterpret_cast<unsigned char*>(output.data()));
    return output;
  }
  return ByteVector();
}

ByteVector ByteVector::toBase64() const {

  static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  if (!isEmpty()) {
    size_t len = size();
    ByteVector output(4 * ((len - 1) / 3 + 1));  // note roundup

    const char *src = data();
    char *dst = output.data();
    while (3 <= len) {
      *dst++ = alphabet[(src[0] >> 2) & 0x3f];
      *dst++ = alphabet[((src[0] & 0x03) << 4) | ((src[1] >> 4) & 0x0f)];
      *dst++ = alphabet[((src[1] & 0x0f) << 2) | ((src[2] >> 6) & 0x03)];
      *dst++ = alphabet[src[2] & 0x3f];
      src += 3;
      len -= 3;
    }
    if (len) {
      *dst++ = alphabet[(src[0] >> 2) & 0x3f];
      if (len > 1) {
        *dst++ = alphabet[((src[0] & 0x03) << 4) | ((src[1] >> 4) & 0x0f)];
        *dst++ = alphabet[((src[1] & 0x0f) << 2)];
      }
      else {
        *dst++ = alphabet[(src[0] & 0x03) << 4];
        *dst++ = '=';
      }
      *dst++ = '=';
    }
    return output;
  }
  return ByteVector();

}


////////////////////////////////////////////////////////////////////////////////
// protected members
////////////////////////////////////////////////////////////////////////////////

void ByteVector::detach() {
  if (!d->data.unique()) {
    if (!isEmpty())
      ByteVector(&d->data->front() + d->offset, d->length).swap(*this);
    else
      ByteVector().swap(*this);
  }
}

////////////////////////////////////////////////////////////////////////////////
// related functions
////////////////////////////////////////////////////////////////////////////////

std::ostream &operator<<(std::ostream &s, const ByteVector &v) {
  for (size_t i = 0; i < v.size(); i++)
    s << v[i];
  return s;
}

}  // namespace TagLib
}  // namespace Strawberry_TagLib

