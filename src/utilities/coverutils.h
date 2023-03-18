/*
 * Strawberry Music Player
 * Copyright 2019-2023, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef COVERUTILS_H
#define COVERUTILS_H

#include <QByteArray>
#include <QString>
#include <QUrl>

#include "core/song.h"
#include "coveroptions.h"

class CoverUtils {

 public:
  static QByteArray Sha1CoverHash(const QString &artist, const QString &album);
  static QString AlbumCoverFilename(QString artist, QString album, const QString &extension);
  static QString CoverFilenameFromSource(const Song::Source source, const QUrl &cover_url, const QString &artist, const QString &album, const QString &album_id, const QString &extension);
  static QString CoverFilenameFromVariable(const CoverOptions &options, const QString &artist, QString album, const QString &extension = QString());
  static QString CoverFilePath(const CoverOptions &options, const Song &song, const QString &album_dir, const QUrl &cover_url, const QString &extension = QString());
  static QString CoverFilePath(const CoverOptions &options, const Song::Source source, const QString &artist, const QString &album, const QString &album_id, const QString &album_dir, const QUrl &cover_url, const QString &extension = QString());

};

#endif  // COVERUTILS_H
