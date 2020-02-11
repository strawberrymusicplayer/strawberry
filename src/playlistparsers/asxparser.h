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

#ifndef ASXPARSER_H
#define ASXPARSER_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QDir>

#include "config.h"
#include "core/song.h"
#include "playlist/playlist.h"
#include "xmlparser.h"

class QIODevice;
class QXmlStreamReader;

class CollectionBackendInterface;

class ASXParser : public XMLParser {
  Q_OBJECT

 public:
  ASXParser(CollectionBackendInterface *collection, QObject *parent = nullptr);

  QString name() const { return "ASX"; }
  QStringList file_extensions() const { return QStringList() << "asx"; }

  bool TryMagic(const QByteArray &data) const;

  SongList Load(QIODevice *device, const QString &playlist_path = "", const QDir &dir = QDir()) const;
  void Save(const SongList &songs, QIODevice *device, const QDir &dir = QDir(), Playlist::Path path_type = Playlist::Path_Automatic) const;

 private:
  Song ParseTrack(QXmlStreamReader *reader, const QDir &dir) const;
};

#endif
