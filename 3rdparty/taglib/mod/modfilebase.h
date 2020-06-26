/***************************************************************************
    copyright           : (C) 2011 by Mathias Panzenböck
    email               : grosser.meister.morti@gmx.net
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

#ifndef TAGLIB_MODFILEBASE_H
#define TAGLIB_MODFILEBASE_H

#include "taglib.h"
#include "tfile.h"
#include "tstring.h"
#include "tlist.h"
#include "taglib_export.h"

#include <algorithm>

namespace Strawberry_TagLib {
namespace TagLib {
namespace Mod {

class TAGLIB_EXPORT FileBase : public Strawberry_TagLib::TagLib::File {
 protected:
  explicit FileBase(FileName file);
  explicit FileBase(IOStream *stream);

  void writeString(const String &s, unsigned int size, char padding = 0);
  void writeByte(unsigned char byte);
  void writeU16L(unsigned short number);
  void writeU32L(unsigned int number);
  void writeU16B(unsigned short number);
  void writeU32B(unsigned int number);

  bool readString(String &s, unsigned int size);
  bool readByte(unsigned char &_byte);
  bool readU16L(unsigned short &number);
  bool readU32L(unsigned int &number);
  bool readU16B(unsigned short &number);
  bool readU32B(unsigned int &number);
};

}  // namespace Mod
}  // namespace TagLib
}  // namespace Strawberry_TagLib

#endif
