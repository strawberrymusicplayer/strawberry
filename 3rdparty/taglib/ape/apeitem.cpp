/***************************************************************************
    copyright            : (C) 2004 by Allan Sandfeld Jensen
    email                : kde@carewolf.com
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

#include "tbytevectorlist.h"
#include "tdebug.h"

#include "apeitem.h"

using namespace Strawberry_TagLib::TagLib;
using namespace APE;

struct ItemData {
  ItemData() : type(Item::Text), readOnly(false) {}

  Item::ItemTypes type;
  String key;
  ByteVector value;
  StringList text;
  bool readOnly;
};

class APE::Item::ItemPrivate {
 public:
  ItemPrivate() : data(new ItemData()) {}

  std::shared_ptr<ItemData> data;
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

APE::Item::Item() : d(new ItemPrivate()) {}

APE::Item::Item(const String &key, const String &value) : d(new ItemPrivate()) {
  d->data->key = key;
  d->data->text.append(value);
}

APE::Item::Item(const String &key, const StringList &values) : d(new ItemPrivate()) {
  d->data->key = key;
  d->data->text = values;
}

APE::Item::Item(const String &key, const ByteVector &value, bool binary) : d(new ItemPrivate()) {
  d->data->key = key;
  if (binary) {
    d->data->type = Binary;
    d->data->value = value;
  }
  else {
    d->data->text.append(value);
  }

}

APE::Item::Item(const Item &item) : d(new ItemPrivate(*item.d)) {}

APE::Item::~Item() {
  delete d;
}

Item &APE::Item::operator=(const Item &item) {

  Item(item).swap(*this);
  return *this;

}

void APE::Item::swap(Item &item) {

  using std::swap;

  swap(d, item.d);

}

void APE::Item::setReadOnly(bool readOnly) {
  d->data->readOnly = readOnly;
}

bool APE::Item::isReadOnly() const {
  return d->data->readOnly;
}

void APE::Item::setType(APE::Item::ItemTypes val) {
  d->data->type = val;
}

APE::Item::ItemTypes APE::Item::type() const {
  return d->data->type;
}

String APE::Item::key() const {
  return d->data->key;
}

ByteVector APE::Item::binaryData() const {
  return d->data->value;
}

void APE::Item::setBinaryData(const ByteVector &value) {
  d->data->type = Binary;
  d->data->value = value;
  d->data->text.clear();
}

void APE::Item::setKey(const String &key) {
  d->data->key = key;
}

void APE::Item::setValue(const String &value) {
  d->data->type = Text;
  d->data->text = value;
  d->data->value.clear();
}

void APE::Item::setValues(const StringList &value) {
  d->data->type = Text;
  d->data->text = value;
  d->data->value.clear();
}

void APE::Item::appendValue(const String &value) {
  d->data->type = Text;
  d->data->text.append(value);
  d->data->value.clear();
}

void APE::Item::appendValues(const StringList &values) {
  d->data->type = Text;
  d->data->text.append(values);
  d->data->value.clear();
}

int APE::Item::size() const {
  size_t result = 8 + d->data->key.size() + 1;
  switch (d->data->type) {
    case Text:
      if (!d->data->text.isEmpty()) {
        StringList::ConstIterator it = d->data->text.begin();

        result += it->data(String::UTF8).size();
        it++;
        for (; it != d->data->text.end(); ++it)
          result += 1 + it->data(String::UTF8).size();
      }
      break;

    case Binary:
    case Locator:
      result += d->data->value.size();
      break;
  }
  return result;

}

StringList APE::Item::values() const {
  return d->data->text;
}

String APE::Item::toString() const {
  if (d->data->type == Text && !isEmpty())
    return d->data->text.front();
  else
    return String();

}

bool APE::Item::isEmpty() const {
  switch (d->data->type) {
    case Text:
      if (d->data->text.isEmpty())
        return true;
      if (d->data->text.size() == 1 && d->data->text.front().isEmpty())
        return true;
      return false;
    case Binary:
    case Locator:
      return d->data->value.isEmpty();
    default:
      return false;
  }

}

void APE::Item::parse(const ByteVector &data) {

  // 11 bytes is the minimum size for an APE item

  if (data.size() < 11) {
    debug("APE::Item::parse() -- no data in item");
    return;
  }

  const unsigned int valueLength = data.toUInt32LE(0);
  const unsigned int flags = data.toUInt32LE(4);

  // An item key can contain ASCII characters from 0x20 up to 0x7E, not UTF-8.
  // We assume that the validity of the given key has been checked.

  d->data->key = String(&data[8], String::Latin1);

  const ByteVector value = data.mid(8 + d->data->key.size() + 1, valueLength);

  setReadOnly(flags & 1);
  setType(ItemTypes((flags >> 1) & 3));

  if (Text == d->data->type)
    d->data->text = StringList(ByteVectorList::split(value, '\0'), String::UTF8);
  else
    d->data->value = value;
}

ByteVector APE::Item::render() const {

  ByteVector data;
  unsigned int flags = ((d->data->readOnly) ? 1 : 0) | (d->data->type << 1);
  ByteVector value;

  if (isEmpty())
    return data;

  if (d->data->type == Text) {
    StringList::ConstIterator it = d->data->text.begin();

    value.append(it->data(String::UTF8));
    it++;
    for (; it != d->data->text.end(); ++it) {
      value.append('\0');
      value.append(it->data(String::UTF8));
    }
    d->data->value = value;
  }
  else
    value.append(d->data->value);

  data.append(ByteVector::fromUInt32LE(value.size()));
  data.append(ByteVector::fromUInt32LE(flags));
  data.append(d->data->key.data(String::Latin1));
  data.append(ByteVector('\0'));
  data.append(value);

  return data;

}
