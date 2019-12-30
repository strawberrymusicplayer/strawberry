/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2013, David Sansome <me@davidsansome.com>
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

#ifndef WPLPARSER_H
#define WPLPARSER_H

#include <QObject>
#include <QIODevice>
#include <QDir>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "config.h"
#include "core/song.h"
#include "playlist/playlist.h"
#include "xmlparser.h"

class CollectionBackendInterface;


class WplParser : public XMLParser {
 public:
  WplParser(CollectionBackendInterface *collection, QObject *parent = nullptr);

  QString name() const { return "WPL"; }
  QStringList file_extensions() const { return QStringList() << "wpl"; }
  QString mime_type() const { return "application/vnd.ms-wpl"; }

  bool TryMagic(const QByteArray &data) const;

  SongList Load(QIODevice *device, const QString &playlist_path, const QDir &dir) const;
  void Save(const SongList &songs, QIODevice *device, const QDir &dir, Playlist::Path path_type = Playlist::Path_Automatic) const;

private:
  void ParseSeq(const QDir &dir, QXmlStreamReader *reader, SongList *songs) const;
  void WriteMeta(const QString &name, const QString &content, QXmlStreamWriter *writer) const;
};

#endif  // WPLPARSER_H
