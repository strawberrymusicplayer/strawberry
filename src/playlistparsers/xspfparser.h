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

#ifndef XSPFPARSER_H
#define XSPFPARSER_H

#include "config.h"


#include <QObject>
#include <QIODevice>
#include <QByteArray>
#include <QDir>
#include <QString>
#include <QStringList>
#include <QXmlStreamReader>

#include "core/song.h"
#include "playlist/playlist.h"
#include "xmlparser.h"

class CollectionBackendInterface;

class XSPFParser : public XMLParser {
  Q_OBJECT

 public:
  XSPFParser(CollectionBackendInterface *collection, QObject *parent = nullptr);

  QString name() const { return "XSPF"; }
  QStringList file_extensions() const { return QStringList() << "xspf"; }

  bool TryMagic(const QByteArray &data) const;

  SongList Load(QIODevice *device, const QString &playlist_path = "", const QDir &dir = QDir()) const;
  void Save(const SongList &songs, QIODevice *device, const QDir &dir = QDir(), Playlist::Path path_type = Playlist::Path_Automatic) const;

 private:
  Song ParseTrack(QXmlStreamReader *reader, const QDir &dir) const;
};

#endif
