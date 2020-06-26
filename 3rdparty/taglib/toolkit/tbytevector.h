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

#ifndef TAGLIB_BYTEVECTOR_H
#define TAGLIB_BYTEVECTOR_H

#include "taglib.h"
#include "taglib_export.h"

#include <vector>
#include <iostream>

namespace Strawberry_TagLib {
namespace TagLib {

//! A byte vector

/*!
 * This class provides a byte vector with some methods that are useful for tagging purposes.
 * Many of the search functions are tailored to what is useful for finding tag related patterns in a data array.
 */

class TAGLIB_EXPORT ByteVector {
 public:
#ifndef DO_NOT_DOCUMENT
  typedef std::vector<char>::iterator Iterator;
  typedef std::vector<char>::const_iterator ConstIterator;
  typedef std::vector<char>::reverse_iterator ReverseIterator;
  typedef std::vector<char>::const_reverse_iterator ConstReverseIterator;
#endif

  /*!
   * Constructs an empty byte vector.
   */
  explicit ByteVector();

  /*!
   * Construct a vector of size \a size with all values set to \a value by default.
   */
  explicit ByteVector(size_t size, char value = 0);

  /*!
   * Constructs a byte vector that is a copy of \a v.
   */
  ByteVector(const ByteVector &v);

  /*!
   * Constructs a byte vector that is a copy of \a v.
   */
  explicit ByteVector(const ByteVector &v, size_t offset, size_t length);

  /*!
   * Constructs a byte vector that contains \a c.
   */
  ByteVector(char c);

  /*!
   * Constructs a byte vector that copies \a data for up to \a length bytes.
   */
  explicit ByteVector(const char *data, size_t length);

  /*!
   * Constructs a byte vector that copies \a data up to the first null byte.
   * This is particularly useful for constructing byte arrays from string constants.
   *
   * \warning The behavior is undefined if \a data is not null terminated.
   */
  ByteVector(const char *data);

  /*!
   * Destroys this ByteVector instance.
   */
  virtual ~ByteVector();

  /*!
   * Sets the data for the byte array using the first \a length bytes of \a data
   */
  ByteVector &setData(const char *data, size_t length);

  /*!
   * Sets the data for the byte array copies \a data up to the first null byte.
   * The behavior is undefined if \a data is not null terminated.
   */
  ByteVector &setData(const char *data);

  /*!
   * Returns a pointer to the internal data structure.
   *
   * \warning Care should be taken when modifying this data structure as it is easy to corrupt the ByteVector when doing so.
   * Specifically, while the data may be changed, its length may not be.
   */
  char *data();

  /*!
   * Returns a pointer to the internal data structure which may not be modified.
   */
  const char *data() const;

  /*!
   * Returns a byte vector made up of the bytes starting at \a index and for \a length bytes.
   * If \a length is not specified it will return the bytes from \a index to the end of the vector.
   */
  ByteVector mid(size_t index, size_t length = npos()) const;

  /*!
   * This essentially performs the same as operator[](), but instead of causing
   * a runtime error if the index is out of bounds, it will return a null byte.
   */
  char at(size_t index) const;

  /*!
   * Searches the ByteVector for \a pattern starting at \a offset and returns
   * the offset.  Returns -1 if the pattern was not found.
   * If \a byteAlign is specified the pattern will only be matched if it starts on a byte divisible
   * by \a byteAlign (starting from \a offset).
   */
  size_t find(const ByteVector &pattern, size_t offset = 0, size_t byteAlign = 1) const;

  /*!
   * Searches the char for \a c starting at \a offset and returns the offset.
   * Returns \a -1 if the pattern was not found.
   * If \a byteAlign is specified the pattern will only be matched if it starts on a byte divisible
   * by \a byteAlign (starting from \a offset).
   */
  size_t find(char c, size_t offset = 0, size_t byteAlign = 1) const;

  /*!
   * Searches the ByteVector for \a pattern starting from either the end of the vector or \a offset and returns the offset.
   * Returns -1 if the pattern was not found.
   * If \a byteAlign is specified the pattern will only be matched if it starts on a byte divisible by \a byteAlign (starting from \a offset).
   */
  size_t rfind(const ByteVector &pattern, size_t offset = 0, size_t byteAlign = 1) const;

  /*!
   * Checks to see if the vector contains the \a pattern starting at position \a offset.
   * Optionally, if you only want to search for part of the pattern you can specify an offset within the pattern to start from.
   * Also, you can specify to only check for the first \a patternLength bytes of \a pattern with the \a patternLength argument.
   */
  bool containsAt(const ByteVector &pattern, size_t offset, size_t patternOffset = 0, size_t patternLength = npos()) const;

  /*!
   * Returns true if the vector starts with \a pattern.
   */
  bool startsWith(const ByteVector &pattern) const;

  /*!
   * Returns true if the vector ends with \a pattern.
   */
  bool endsWith(const ByteVector &pattern) const;

  /*!
   * Replaces \a oldByte with \a newByte and returns a reference to the ByteVector after the operation.  This \e does modify the vector.
   */
  ByteVector &replace(char oldByte, char newByte);

  /*!
   * Replaces \a pattern with \a with and returns a reference to the ByteVector after the operation.  This \e does modify the vector.
   */
  ByteVector &replace(const ByteVector &pattern, const ByteVector &with);

  /*!
   * Checks for a partial match of \a pattern at the end of the vector.
   * It returns the offset of the partial match within the vector, or -1 if the pattern is not found.
   * This method is particularly useful when searching for patterns that start in one vector and end in another.
   * When combined with startsWith() it can be used to find a pattern that overlaps two buffers.
   *
   * \note This will not match the complete pattern at the end of the string; use endsWith() for that.
   */
  size_t endsWithPartialMatch(const ByteVector &pattern) const;

  /*!
   * Appends \a v to the end of the ByteVector.
   */
  ByteVector &append(const ByteVector &v);

  /*!
   * Appends \a c to the end of the ByteVector.
   */
  ByteVector &append(char c);

  /*!
   * Clears the data.
   */
  ByteVector &clear();

  /*!
   * Returns the size of the array.
   */
  size_t size() const;

  /*!
   * Resize the vector to \a size.
   * If the vector is currently less than \a size, pad the remaining spaces with \a padding.
   * Returns a reference to the resized vector.
   */
  ByteVector &resize(size_t size, char padding = 0);

  /*!
   * Returns an Iterator that points to the front of the vector.
   */
  Iterator begin();

  /*!
   * Returns a ConstIterator that points to the front of the vector.
   */
  ConstIterator begin() const;

  /*!
   * Returns an Iterator that points to the back of the vector.
   */
  Iterator end();

  /*!
   * Returns a ConstIterator that points to the back of the vector.
   */
  ConstIterator end() const;

  /*!
   * Returns a ReverseIterator that points to the front of the vector.
   */
  ReverseIterator rbegin();

  /*!
   * Returns a ConstReverseIterator that points to the front of the vector.
   */
  ConstReverseIterator rbegin() const;

  /*!
   * Returns a ReverseIterator that points to the back of the vector.
   */
  ReverseIterator rend();

  /*!
   * Returns a ConstReverseIterator that points to the back of the vector.
   */
  ConstReverseIterator rend() const;

  /*!
   * Returns true if the ByteVector is empty.
   *
   * \see size()
   */
  bool isEmpty() const;

  /*!
   * Converts the 2 bytes at \a offset of the vector to a short as a signed 16-bit little-endian integer.
   *
   * \see fromUInt16LE()
   */
  short toInt16LE(size_t offset) const;

  /*!
   * Converts the 2 bytes at \a offset of the vector to a short as a signed 16-bit big-endian integer.
   *
   * \see fromUInt16BE()
   */
  short toInt16BE(size_t offset) const;

  /*!
   * Converts the 2 bytes at \a offset of the vector to a unsigned short as an unsigned 16-bit little-endian integer.
   *
   * \see fromUInt16LE()
   */
  unsigned short toUInt16LE(size_t offset) const;

  /*!
   * Converts the 2 bytes at \a offset of the vector to a unsigned short as an unsigned 16-bit big-endian integer.
   *
   * \see fromUInt16BE()
   */
  unsigned short toUInt16BE(size_t offset) const;

  /*!
   * Converts the 3 bytes at \a offset of the vector to a unsigned int as an unsigned 24-bit little-endian integer.
   */
  unsigned int toUInt24LE(size_t offset) const;

  /*!
   * Converts the 3 bytes at \a offset of the vector to a unsigned int as an unsigned 24-bit big-endian integer.
   */
  unsigned int toUInt24BE(size_t offset) const;

  /*!
   * Converts the 4 bytes at \a offset of the vector to a unsigned int as an unsigned 32-bit little-endian integer.
   *
   * \see fromUInt32LE()
   */
  unsigned int toUInt32LE(size_t offset) const;

  /*!
   * Converts the 4 bytes at \a offset of the vector to a unsigned int as an unsigned 32-bit big-endian integer.
   *
   * \see fromUInt32BE()
   */
  unsigned int toUInt32BE(size_t offset) const;

  /*!
   * Converts the 8 bytes at \a offset of the vector to a long long as a signed 64-bit little-endian integer.
   *
   * \see fromUInt64LE()
   */
  long long toInt64LE(size_t offset) const;

  /*!
   * Converts the 8 bytes at \a offset of the vector to a long long as a signed 64-bit big-endian integer.
   *
   * \see fromUInt64BE()
   */
  long long toInt64BE(size_t offset) const;

  /*
   * Converts the 4 bytes at \a offset of the vector to a float as an IEEE754 32-bit little-endian floating point number.
   */
  float toFloat32LE(size_t offset) const;

  /*
   * Converts the 4 bytes at \a offset of the vector to a float as an IEEE754 32-bit big-endian floating point number.
   */
  float toFloat32BE(size_t offset) const;

  /*
   * Converts the 8 bytes at \a offset of the vector to a double as an IEEE754 64-bit little-endian floating point number.
   */
  double toFloat64LE(size_t offset) const;

  /*
   * Converts the 8 bytes at \a offset of the vector to a double as an IEEE754 64-bit big-endian floating point number.
   */
  double toFloat64BE(size_t offset) const;

  /*
  * Converts the 10 bytes at \a offset of the vector to a long double as an IEEE754 80-bit little-endian floating point number.
  *
  * \note This may compromise the precision depends on the size of long double.
  */
  long double toFloat80LE(size_t offset) const;

  /*
   * Converts the 10 bytes at \a offset of the vector to a long double as an IEEE754 80-bit big-endian floating point number.
   *
   * \note This may compromise the precision depends on the size of long double.
   */
  long double toFloat80BE(size_t offset) const;

  /*!
   * Creates a 2 byte ByteVector based on \a value as an unsigned 16-bit little-endian integer.
   *
   * \note If \a value is larger than 16-bit, the lowest 16 bits are used.
   * \see toUInt16LE()
   */
  static ByteVector fromUInt16LE(size_t value);

  /*!
   * Creates a 2 byte ByteVector based on \a value as an unsigned 16-bit big-endian integer.
   *
   * \note If \a value is larger than 16-bit, the lowest 16 bits are used.
   * \see toUInt16BE()
   */
  static ByteVector fromUInt16BE(size_t value);

  /*!
   * Creates a 4 byte ByteVector based on \a value as an unsigned 32-bit little-endian integer.
   *
   * \note If \a value is larger than 32-bit, the lowest 32 bits are used.
   * \see toUInt32LE()
   */
  static ByteVector fromUInt32LE(size_t value);

  /*!
   * Creates a 4 byte ByteVector based on \a value as an unsigned 32-bit big-endian integer.
   *
   * \note If \a value is larger than 32-bit, the lowest 32 bits are used.
   * \see toUInt32BE()
   */
  static ByteVector fromUInt32BE(size_t value);

  /*!
   * Creates a 8 byte ByteVector based on \a value as an unsigned 64-bit little-endian integer.
   *
   * \see toUInt64LE()
   */
  static ByteVector fromUInt64LE(unsigned long long value);

  /*!
   * Creates a 8 byte ByteVector based on \a value as an unsigned 64-bit big-endian integer.
   *
   * \see toUInt64BE()
   */
  static ByteVector fromUInt64BE(unsigned long long value);

  /*!
   * Creates a 4 byte ByteVector based on \a value as an IEEE754 32-bit little-endian floating point number.
   *
   * \see fromFloat32BE()
   */
  static ByteVector fromFloat32LE(float value);

  /*!
   * Creates a 4 byte ByteVector based on \a value as an IEEE754 32-bit big-endian floating point number.
   *
   * \see fromFloat32LE()
   */
  static ByteVector fromFloat32BE(float value);

  /*!
   * Creates a 8 byte ByteVector based on \a value as an IEEE754 64-bit little-endian floating point number.
   *
   * \see fromFloat64BE()
   */
  static ByteVector fromFloat64LE(double value);

  /*!
   * Creates a 8 byte ByteVector based on \a value as an IEEE754 64-bit big-endian floating point number.
   *
   * \see fromFloat64LE()
   */
  static ByteVector fromFloat64BE(double value);

  /*!
   * Returns a ByteVector based on the CString \a s.
   */
  static ByteVector fromCString(const char *s, size_t length = npos());

  /*!
   * Returns a const reference to the byte at \a index.
   */
  const char &operator[](size_t index) const;

  /*!
   * Returns a reference to the byte at \a index.
   */
  char &operator[](size_t index);

  /*!
   * Returns true if this ByteVector and \a v are equal.
   */
  bool operator==(const ByteVector &v) const;

  /*!
   * Returns true if this ByteVector and \a v are not equal.
   */
  bool operator!=(const ByteVector &v) const;

  /*!
   * Returns true if this ByteVector and the null terminated C string \a s contain the same data.
   */
  bool operator==(const char *s) const;

  /*!
   * Returns true if this ByteVector and the null terminated C string \a s do not contain the same data.
   */
  bool operator!=(const char *s) const;

  /*!
   * Returns true if this ByteVector is less than \a v.
   * The value of the vectors is determined by evaluating the character from left to right,
   * and in the event one vector is a superset of the other, the size is used.
   */
  bool operator<(const ByteVector &v) const;

  /*!
   * Returns true if this ByteVector is greater than \a v.
   */
  bool operator>(const ByteVector &v) const;

  /*!
   * Returns a vector that is \a v appended to this vector.
   */
  ByteVector operator+(const ByteVector &v) const;

  /*!
   * Copies ByteVector \a v.
   */
  ByteVector &operator=(const ByteVector &v);

  /*!
   * Copies a byte \a c.
   */
  ByteVector &operator=(char c);

  /*!
   * Copies \a data up to the first null byte.
   *
   * \warning The behavior is undefined if \a data is not null terminated.
   */
  ByteVector &operator=(const char *data);

  /*!
   * Exchanges the content of the ByteVector by the content of \a v.
   */
  void swap(ByteVector &v);

  /*!
   * Returns a special value used for \a length or \a patternLength parameters
   * of ByteVector's member functions, means "until the end of the data".
   * As a return value, it is usually used to indicate no matches.
   */
  static size_t npos();

  /*!
   * Returns a hex-encoded copy of the byte vector.
   */
  ByteVector toHex() const;

  /*!
   * Returns a base64 encoded copy of the byte vector
   */
  ByteVector toBase64() const;

  /*!
   * Decodes the base64 encoded byte vector.
   */
  static ByteVector fromBase64(const ByteVector &);

 protected:
  /*
   * If this ByteVector is being shared via implicit sharing, do a deep copy of the data and separate from the shared members.
   * This should be called by all non-const subclass members.
   */
  void detach();

 private:
  class ByteVectorPrivate;
  ByteVectorPrivate *d;
};

/*!
 * \relates Strawberry_TagLib::TagLib::ByteVector
 * Streams the ByteVector \a v to the output stream \a s.
 */
TAGLIB_EXPORT std::ostream &operator<<(std::ostream &s, const Strawberry_TagLib::TagLib::ByteVector &v);

}  // namespace TagLib
}  // namespace Strawberry_TagLib

#endif
