/***************************************************************************
    copyright            : (C) 2011 by Lukas Lalinsky
    email                : lalinsky@gmail.com
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

#ifdef _WIN32
#  include <memory>
#  include <windows.h>
#  include "tstring.h"
#endif

#include "tiostream.h"

using namespace Strawberry_TagLib::TagLib;

#ifdef _WIN32

namespace {
std::wstring ansiToUnicode(const char *str) {

  const int len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
  if (len == 0)
    return std::wstring();

  std::wstring wstr(len - 1, L'\0');
  MultiByteToWideChar(CP_ACP, 0, str, -1, &wstr[0], len);

  return wstr;

}
}  // namespace

class FileName::FileNamePrivate {
 public:
  FileNamePrivate() : data(new std::wstring()) {}

  FileNamePrivate(const wchar_t *name) : data(new std::wstring(name)) {}

  FileNamePrivate(const char *name) : data(new std::wstring(ansiToUnicode(name))) {}

  std::shared_ptr<std::wstring> data;
};

FileName::FileName(const wchar_t *name) : d(new FileNamePrivate(name)) {}

FileName::FileName(const char *name) : d(new FileNamePrivate(name)) {}

FileName::FileName(const FileName &name) : d(new FileNamePrivate()) {
  *d = *name.d;
}

FileName::~FileName() {
  delete d;
}

FileName &FileName::operator=(const FileName &name) {
  *d = *name.d;
  return *this;
}

const wchar_t *FileName::wstr() const {
  return d->data->c_str();
}

#endif  // _WIN32

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

IOStream::IOStream() {}

IOStream::~IOStream() {}

void IOStream::clear() {}
