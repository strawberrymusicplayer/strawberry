/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef PLSPARSER_H
#define PLSPARSER_H

#include <QObject>
#include <QDir>
#include <QByteArray>
#include <QString>
#include <QStringList>

#include "config.h"
#include "core/song.h"
#include "parserbase.h"
#include "playlist/playlist.h"

class QIODevice;
class CollectionBackendInterface;

class PLSParser : public ParserBase {
  Q_OBJECT

 public:
  explicit PLSParser(CollectionBackendInterface *collection, QObject *parent = nullptr);

  QString name() const { return "PLS"; }
  QStringList file_extensions() const { return QStringList() << "pls"; }
  QString mime_type() const { return "audio/x-scpls"; }

  bool TryMagic(const QByteArray &data) const;

  SongList Load(QIODevice *device, const QString &playlist_path = "", const QDir &dir = QDir()) const;
  void Save(const SongList &songs, QIODevice *device, const QDir &dir = QDir(), Playlist::Path path_type = Playlist::Path_Automatic) const;
};

#endif  // PLSPARSER_H
