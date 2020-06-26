/**************************************************************************
    copyright            : (C) 2010 by Anton Sergunov
    email                : setosha@gmail.com
 **************************************************************************/

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

#include "taglib.h"
#include "tdebug.h"

#include "asfattribute.h"
#include "asffile.h"
#include "asfpicture.h"
#include "asfutils.h"

using namespace Strawberry_TagLib::TagLib;

namespace {
struct PictureData {
  bool valid;
  ASF::Picture::Type type;
  String mimeType;
  String description;
  ByteVector picture;
};
}  // namespace

class ASF::Picture::PicturePrivate {
 public:
  explicit PicturePrivate() : data(new PictureData()) {}

  std::shared_ptr<PictureData> data;
};

////////////////////////////////////////////////////////////////////////////////
// Picture class members
////////////////////////////////////////////////////////////////////////////////

ASF::Picture::Picture() : d(new PicturePrivate()) {
  d->data->valid = true;
}

ASF::Picture::Picture(const Picture& other) : d(new PicturePrivate(*other.d)) {}

ASF::Picture::~Picture() {
  delete d;
}

bool ASF::Picture::isValid() const {
  return d->data->valid;
}

String ASF::Picture::mimeType() const {
  return d->data->mimeType;
}

void ASF::Picture::setMimeType(const String& value) {
  d->data->mimeType = value;
}

ASF::Picture::Type ASF::Picture::type() const {
  return d->data->type;
}

void ASF::Picture::setType(const ASF::Picture::Type &t) {
  d->data->type = t;
}

String ASF::Picture::description() const {
  return d->data->description;
}

void ASF::Picture::setDescription(const String& desc) {
  d->data->description = desc;
}

ByteVector ASF::Picture::picture() const {
  return d->data->picture;
}

void ASF::Picture::setPicture(const ByteVector &p) {
  d->data->picture = p;
}

int ASF::Picture::dataSize() const {
  return 9 + (d->data->mimeType.length() + d->data->description.length()) * 2 + d->data->picture.size();
}

ASF::Picture& ASF::Picture::operator=(const ASF::Picture& other) {
  Picture(other).swap(*this);
  return *this;
}

void ASF::Picture::swap(Picture& other) {
  using std::swap;

  swap(d, other.d);
}

ByteVector ASF::Picture::render() const {

  if (!isValid())
    return ByteVector();

  return ByteVector(static_cast<char>(d->data->type)) + ByteVector::fromUInt32LE(d->data->picture.size()) + renderString(d->data->mimeType) + renderString(d->data->description) + d->data->picture;

}

void ASF::Picture::parse(const ByteVector &bytes) {

  d->data->valid = false;
  if (bytes.size() < 9)
    return;
  size_t pos = 0;
  d->data->type = static_cast<Type>(bytes[0]);
  ++pos;
  const unsigned int dataLen = bytes.toUInt32LE(pos);
  pos += 4;

  const ByteVector nullStringTerminator(2, 0);

  size_t endPos = bytes.find(nullStringTerminator, pos, 2);
  if (endPos == ByteVector::npos())
    return;
  d->data->mimeType = String(bytes.mid(pos, endPos - pos), String::UTF16LE);
  pos = endPos + 2;

  endPos = bytes.find(nullStringTerminator, pos, 2);
  if (endPos == ByteVector::npos())
    return;
  d->data->description = String(bytes.mid(pos, endPos - pos), String::UTF16LE);
  pos = endPos + 2;

  if (dataLen + pos != bytes.size())
    return;

  d->data->picture = bytes.mid(pos, dataLen);
  d->data->valid = true;

}

ASF::Picture ASF::Picture::fromInvalid() {

  Picture ret;
  ret.d->data->valid = false;
  return ret;

}
