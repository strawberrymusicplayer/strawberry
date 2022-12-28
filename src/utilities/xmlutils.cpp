/*
 * Strawberry Music Player
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
 *
 * Strawberry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Strawberry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <QString>
#include <QXmlStreamReader>

#include "xmlutils.h"

namespace Utilities {

void ConsumeCurrentElement(QXmlStreamReader *reader) {

  int level = 1;
  while (level != 0 && !reader->atEnd()) {
    switch (reader->readNext()) {
      case QXmlStreamReader::StartElement: ++level; break;
      case QXmlStreamReader::EndElement: --level; break;
      default: break;
    }
  }

}

bool ParseUntilElement(QXmlStreamReader *reader, const QString &name) {

  while (!reader->atEnd()) {
    QXmlStreamReader::TokenType type = reader->readNext();
    if (type == QXmlStreamReader::StartElement && reader->name() == name) {
      return true;
    }
  }
  return false;

}

bool ParseUntilElementCI(QXmlStreamReader *reader, const QString &name) {

  while (!reader->atEnd()) {
    QXmlStreamReader::TokenType type = reader->readNext();
    if (type == QXmlStreamReader::StartElement) {
      QString element = reader->name().toString().toLower();
      if (element == name) {
        return true;
      }
    }
  }

  return false;

}

}  // namespace Utilities
