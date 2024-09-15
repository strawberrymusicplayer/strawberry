/*
 * Strawberry Music Player
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SAVETAGCOVERDATA_H
#define SAVETAGCOVERDATA_H

#include <QByteArray>
#include <QString>

class SaveTagCoverData {
 public:
  SaveTagCoverData(const QString &_cover_filename = QString(), const QByteArray &_cover_data = QByteArray(), const QString &_cover_mimetype = QString());
  SaveTagCoverData(const QByteArray &_cover_data, const QString &_cover_mimetype = QString());
  QString cover_filename;
  QByteArray cover_data;
  QString cover_mimetype;
};

#endif // SAVETAGCOVERDATA_H
