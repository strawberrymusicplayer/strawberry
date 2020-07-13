/**************************************************************************
    copyright            : (C) 2007 by Lukáš Lalinský
    email                : lalinsky@gmail.com
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
#include "mp4item.h"
#include "tutils.h"

using namespace Strawberry_TagLib::TagLib;

namespace {
struct ItemData {

  bool valid;
  MP4::AtomDataType atomDataType;
  MP4::Item::ItemType type;
  union {
    bool m_bool;
    int m_int;
    MP4::Item::IntPair m_intPair;
    unsigned char m_byte;
    unsigned int m_uint;
    long long m_longlong;
  };
  StringList m_stringList;
  ByteVectorList m_byteVectorList;
  MP4::CoverArtList m_coverArtList;
};
}  // namespace

class MP4::Item::ItemPrivate {
 public:
  explicit ItemPrivate() : data(new ItemData()) {
    data->valid = true;
    data->atomDataType = MP4::TypeUndefined;
    data->type = MP4::Item::TypeUndefined_;
  }

  std::shared_ptr<ItemData> data;
};

MP4::Item::Item() : d(new ItemPrivate()) {
  d->data->valid = false;
}

MP4::Item::Item(const Item &item) : d(new ItemPrivate(*item.d)) {}

MP4::Item &MP4::Item::operator=(const Item &item) {
  Item(item).swap(*this);
  return *this;
}

void MP4::Item::swap(Item &item) {
  using std::swap;

  swap(d, item.d);
}

MP4::Item::~Item() {
  delete d;
}

MP4::Item::Item(bool value) : d(new ItemPrivate()) {
  d->data->m_bool = value;
  d->data->type = TypeBool;
}

MP4::Item::Item(int value) : d(new ItemPrivate()) {
  d->data->m_int = value;
  d->data->type = TypeInt;
}

MP4::Item::Item(unsigned char value) : d(new ItemPrivate()) {
  d->data->m_byte = value;
  d->data->type = TypeByte;
}

MP4::Item::Item(unsigned int value) : d(new ItemPrivate()) {
  d->data->m_uint = value;
  d->data->type = TypeUInt;
}

MP4::Item::Item(long long value) : d(new ItemPrivate()) {
  d->data->m_longlong = value;
  d->data->type = TypeLongLong;
}

MP4::Item::Item(int value1, int value2) : d(new ItemPrivate()) {
  d->data->m_intPair.first = value1;
  d->data->m_intPair.second = value2;
  d->data->type = TypeIntPair;
}

MP4::Item::Item(const ByteVectorList &value) : d(new ItemPrivate()) {
  d->data->m_byteVectorList = value;
  d->data->type = TypeByteVectorList;
}

MP4::Item::Item(const StringList &value) : d(new ItemPrivate()) {
  d->data->m_stringList = value;
  d->data->type = TypeStringList;
}

MP4::Item::Item(const MP4::CoverArtList &value) : d(new ItemPrivate()) {
  d->data->m_coverArtList = value;
  d->data->type = TypeCoverArtList;
}

void MP4::Item::setAtomDataType(MP4::AtomDataType type) {
  d->data->atomDataType = type;
}

MP4::AtomDataType MP4::Item::atomDataType() const {
  return d->data->atomDataType;
}

bool MP4::Item::toBool() const {
  return d->data->m_bool;
}

int MP4::Item::toInt() const {
  return d->data->m_int;
}

unsigned char MP4::Item::toByte() const {
  return d->data->m_byte;
}

unsigned int MP4::Item::toUInt() const {
  return d->data->m_uint;
}

long long MP4::Item::toLongLong() const {
  return d->data->m_longlong;
}

MP4::Item::IntPair MP4::Item::toIntPair() const {
  return d->data->m_intPair;
}

StringList MP4::Item::toStringList() const {
  return d->data->m_stringList;
}

ByteVectorList MP4::Item::toByteVectorList() const {
  return d->data->m_byteVectorList;
}

MP4::CoverArtList MP4::Item::toCoverArtList() const {
  return d->data->m_coverArtList;
}

bool MP4::Item::isValid() const {
  return d->data->valid;
}

String MP4::Item::toString() const {

  StringList desc;
  switch (d->data->type) {
    case TypeBool:
      return d->data->m_bool ? "true" : "false";
    case TypeInt:
      return Utils::formatString("%d", d->data->m_int);
    case TypeIntPair:
      return Utils::formatString("%d/%d", d->data->m_intPair.first, d->data->m_intPair.second);
    case TypeByte:
      return Utils::formatString("%d", d->data->m_byte);
    case TypeUInt:
      return Utils::formatString("%u", d->data->m_uint);
    case TypeLongLong:
      return Utils::formatString("%lld", d->data->m_longlong);
    case TypeStringList:
      return d->data->m_stringList.toString(" / ");
    case TypeByteVectorList:
      for (size_t i = 0; i < d->data->m_byteVectorList.size(); i++) {
        desc.append(Utils::formatString(
          "[%d bytes of data]", static_cast<int>(d->data->m_byteVectorList[i].size())));
      }
      return desc.toString(", ");
    case TypeCoverArtList:
      for (size_t i = 0; i < d->data->m_coverArtList.size(); i++) {
        desc.append(Utils::formatString("[%d bytes of data]", static_cast<int>(d->data->m_coverArtList[i].data().size())));
      }
      return desc.toString(", ");
    case TypeUndefined_:
      return "[unknown]";
  }
  return String();

}
