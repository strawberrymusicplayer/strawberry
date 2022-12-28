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

#ifndef XMLUTILS_H
#define XMLUTILS_H

#include <QString>
#include <QXmlStreamReader>

namespace Utilities {

// Reads all children of the current element,
// and returns with the stream reader either on the EndElement for the current element, or the end of the file - whichever came first.
void ConsumeCurrentElement(QXmlStreamReader *reader);

// Advances the stream reader until it finds an element with the given name.
// Returns false if the end of the document was reached before finding a matching element.
bool ParseUntilElement(QXmlStreamReader *reader, const QString &name);
bool ParseUntilElementCI(QXmlStreamReader *reader, const QString &name);

}  // namespace Utilities

#endif  // XMLUTILS_H
