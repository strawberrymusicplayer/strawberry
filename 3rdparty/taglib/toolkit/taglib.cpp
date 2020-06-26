/***************************************************************************
    copyright           : (C) 2016 by Michael Helmling
    email               : helmling@mathematik.uni-kl.de
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

#include "taglib.h"
#include "tstring.h"

using namespace Strawberry_TagLib::TagLib;

String Strawberry_TagLib::TagLib::Version::string() {
  return String::number(TAGLIB_MAJOR_VERSION) + "." + String::number(TAGLIB_MINOR_VERSION) + "." + String::number(TAGLIB_PATCH_VERSION);
}

unsigned int Strawberry_TagLib::TagLib::Version::combined() {
  return (TAGLIB_MAJOR_VERSION << 16) | (TAGLIB_MINOR_VERSION << 8) | (TAGLIB_PATCH_VERSION << 4);
}

unsigned int(Strawberry_TagLib::TagLib::Version::major)() {
  return TAGLIB_MAJOR_VERSION;
}

unsigned int(Strawberry_TagLib::TagLib::Version::minor)() {
  return TAGLIB_MINOR_VERSION;
}

unsigned int Strawberry_TagLib::TagLib::Version::patch() {
  return TAGLIB_PATCH_VERSION;
}
