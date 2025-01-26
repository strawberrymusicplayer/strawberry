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

#ifndef STRUTILS_H
#define STRUTILS_H

#include <QString>
#include <QStringList>
#include <QMetaObject>

#include "core/song.h"

namespace Utilities {

QString PrettySize(const quint64 bytes);
QString PrettySize(const QSize size);

// Get the path without the filename extension
QString PathWithoutFilenameExtension(const QString &filename);
QString FiddleFileExtension(const QString &filename, const QString &new_extension);

// Replaces some HTML entities with their normal characters.
QString DecodeHtmlEntities(const QString &text);

// Shortcut for getting a Qt-aware enum value as a string.
// Pass in the QMetaObject of the class that owns the enum, the string name of the enum and a valid value from that enum.
const char *EnumToString(const QMetaObject &meta, const char *name, int value);

QStringList Prepend(const QString &text, const QStringList &list);
QStringList Updateify(const QStringList &list);

QString ReplaceMessage(const QString &message, const Song &song, const QString &newline, const bool html_escaped = false);
QString ReplaceVariable(const QString &variable, const Song &song, const QString &newline, const bool html_escaped = false);

QString StringListToHTML(const QStringList &errors);

}  // namespace Utilities

#endif  // STRUTILS_H
