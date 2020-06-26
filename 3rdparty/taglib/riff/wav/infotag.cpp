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

#include "tdebug.h"
#include "tfile.h"
#include "tpicturemap.h"

#include "rifffile.h"
#include "infotag.h"
#include "riffutils.h"

using namespace Strawberry_TagLib::TagLib;
using namespace RIFF::Info;

namespace {
class DefaultStringHandler : public Strawberry_TagLib::TagLib::StringHandler {
 public:
  explicit DefaultStringHandler() : Strawberry_TagLib::TagLib::StringHandler() {}

  String parse(const ByteVector &data) const override {
    return String(data, String::UTF8);
  }

  ByteVector render(const String &s) const override {
    return s.data(String::UTF8);
  }
};

const DefaultStringHandler defaultStringHandler;
const Strawberry_TagLib::TagLib::StringHandler *stringHandler = &defaultStringHandler;
}  // namespace

class RIFF::Info::Tag::TagPrivate {
 public:
  FieldMap fieldMap;
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

RIFF::Info::Tag::Tag(const ByteVector &data) : d(new TagPrivate()) {
  parse(data);
}

RIFF::Info::Tag::Tag() : d(new TagPrivate()) {}

RIFF::Info::Tag::~Tag() {
  delete d;
}

String RIFF::Info::Tag::title() const {
  return fieldText("INAM");
}

String RIFF::Info::Tag::artist() const {
  return fieldText("IART");
}

String RIFF::Info::Tag::album() const {
  return fieldText("IPRD");
}

String RIFF::Info::Tag::comment() const {
  return fieldText("ICMT");
}

String RIFF::Info::Tag::genre() const {
  return fieldText("IGNR");
}

unsigned int RIFF::Info::Tag::year() const {
  return fieldText("ICRD").substr(0, 4).toInt();
}

unsigned int RIFF::Info::Tag::track() const {
  return fieldText("IPRT").toInt();
}

Strawberry_TagLib::TagLib::PictureMap RIFF::Info::Tag::pictures() const {
  return PictureMap();
}

void RIFF::Info::Tag::setTitle(const String &s) {
  setFieldText("INAM", s);
}

void RIFF::Info::Tag::setArtist(const String &s) {
  setFieldText("IART", s);
}

void RIFF::Info::Tag::setAlbum(const String &s) {
  setFieldText("IPRD", s);
}

void RIFF::Info::Tag::setComment(const String &s) {
  setFieldText("ICMT", s);
}

void RIFF::Info::Tag::setGenre(const String &s) {
  setFieldText("IGNR", s);
}

void RIFF::Info::Tag::setYear(unsigned int i) {
  if (i != 0)
    setFieldText("ICRD", String::number(i));
  else
    d->fieldMap.erase("ICRD");
}

void RIFF::Info::Tag::setTrack(unsigned int i) {
  if (i != 0)
    setFieldText("IPRT", String::number(i));
  else
    d->fieldMap.erase("IPRT");
}

void RIFF::Info::Tag::setPictures(const PictureMap&) {}

bool RIFF::Info::Tag::isEmpty() const {
  return d->fieldMap.isEmpty();
}

FieldMap RIFF::Info::Tag::fieldMap() const {
  return d->fieldMap;
}

String RIFF::Info::Tag::fieldText(const ByteVector &id) const {

  if (d->fieldMap.contains(id))
    return String(d->fieldMap[id]);
  else
    return String();

}

void RIFF::Info::Tag::setFieldText(const ByteVector &id, const String &s) {

  // id must be four-byte long pure ascii string.
  if (!isValidChunkName(id))
    return;

  if (!s.isEmpty())
    d->fieldMap[id] = s;
  else
    removeField(id);

}

void RIFF::Info::Tag::removeField(const ByteVector &id) {

  if (d->fieldMap.contains(id))
    d->fieldMap.erase(id);

}

ByteVector RIFF::Info::Tag::render() const {

  ByteVector data("INFO");

  for (FieldMap::ConstIterator it = d->fieldMap.begin(); it != d->fieldMap.end(); ++it) {
    ByteVector text = stringHandler->render(it->second);
    if (text.isEmpty())
      continue;

    data.append(it->first);
    data.append(ByteVector::fromUInt32LE(text.size() + 1));
    data.append(text);

    do {
      data.append('\0');
    } while (data.size() & 1);
  }

  if (data.size() == 4)
    return ByteVector();
  else
    return data;

}

void RIFF::Info::Tag::setStringHandler(const Strawberry_TagLib::TagLib::StringHandler *handler) {

  if (handler)
    stringHandler = handler;
  else
    stringHandler = &defaultStringHandler;

}

////////////////////////////////////////////////////////////////////////////////
// protected members
////////////////////////////////////////////////////////////////////////////////

void RIFF::Info::Tag::parse(const ByteVector &data) {

  size_t p = 4;
  while (p < data.size()) {
    const unsigned int size = data.toUInt32LE(p + 4);
    if (size > data.size() - p - 8)
      break;

    const ByteVector id = data.mid(p, 4);
    if (isValidChunkName(id)) {
      const String text = stringHandler->parse(data.mid(p + 8, size));
      d->fieldMap[id] = text;
    }

    p += ((size + 1) & ~1) + 8;
  }

}
