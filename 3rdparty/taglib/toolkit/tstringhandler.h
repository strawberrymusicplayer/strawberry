/***************************************************************************
    copyright            : (C) 2012 by Tsuda Kageyu
    email                : tsuda.kageyu@gmail.com
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

#ifndef TAGLIB_STRINGHANDLER_H
#define TAGLIB_STRINGHANDLER_H

#include "tstring.h"
#include "tbytevector.h"
#include "taglib_export.h"

namespace Strawberry_TagLib {
namespace TagLib {
//! A abstraction for the string to data encoding.

/*!
   * ID3v1, ID3v2 and RIFF Info tag sometimes store strings in local encodings
   * encodings instead of ISO-8859-1 (Latin1), such as Windows-1252 for western 
   * languages, Shift_JIS for Japanese and so on. However, TagLib only supports 
   * genuine ISO-8859-1 by default.
   *
   * Here is an option to read and write tags in your preferrd encoding 
   * by subclassing this class, reimplementing parse() and render() and setting 
   * your reimplementation as the default with ID3v1::Tag::setStringHandler(),
   * ID3v2::Tag::setStringHandler() or Info::Tag::setStringHandler().
   *
   * \see ID3v1::Tag::setStringHandler()
   * \see ID3v2::Tag::setStringHandler()
   * \see Info::Tag::setStringHandler()
   */

class TAGLIB_EXPORT StringHandler {
 public:
  explicit StringHandler();
  virtual ~StringHandler();

  /*!
   * Decode a string from \a data.
   */
  virtual String parse(const ByteVector &data) const = 0;

  /*!
   * Encode a ByteVector with the data from \a s.
   */
  virtual ByteVector render(const String &s) const = 0;
};

}  // namespace TagLib
}  // namespace Strawberry_TagLib

#endif
