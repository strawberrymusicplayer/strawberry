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

#include "config.h"

#include <QObject>
#include <QDir>
#include <QByteArray>
#include <QString>
#include <QStringList>

#include "includes/shared_ptr.h"
#include "constants/playlistsettings.h"
#include "core/song.h"
#include "xmlparser.h"

class QIODevice;
class QXmlStreamReader;
class QXmlStreamWriter;

class TagReaderClient;
class CollectionBackendInterface;

class WplParser : public XMLParser {
  Q_OBJECT

 public:
  explicit WplParser(const SharedPtr<TagReaderClient> tagreader_client, const SharedPtr<CollectionBackendInterface> collection_backend, QObject *parent = nullptr);

  QString name() const override { return QStringLiteral("WPL"); }
  QStringList file_extensions() const override { return QStringList() << QStringLiteral("wpl"); }
  QString mime_type() const override { return QStringLiteral("application/vnd.ms-wpl"); }
  bool load_supported() const override { return true; }
  bool save_supported() const override { return true; }

  bool TryMagic(const QByteArray &data) const override;

  LoadResult Load(QIODevice *device, const QString &playlist_path, const QDir &dir, const bool collection_lookup = true) const override;
  void Save(const QString &playlist_name, const SongList &songs, QIODevice *device, const QDir &dir, const PlaylistSettings::PathType path_type = PlaylistSettings::PathType::Automatic) const override;

 private:
  void ParseSeq(const QDir &dir, QXmlStreamReader *reader, SongList *songs, const bool collection_lookup) const;
  static void WriteMeta(const QString &name, const QString &content, QXmlStreamWriter *writer);
};

#endif  // WPLPARSER_H
