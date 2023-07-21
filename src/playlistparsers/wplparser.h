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

#include "core/shared_ptr.h"
#include "core/song.h"
#include "settings/playlistsettingspage.h"
#include "xmlparser.h"

class QIODevice;
class QXmlStreamReader;
class QXmlStreamWriter;

class CollectionBackendInterface;

class WplParser : public XMLParser {
  Q_OBJECT

 public:
  explicit WplParser(SharedPtr<CollectionBackendInterface> collection_backend, QObject *parent = nullptr);

  QString name() const override { return "WPL"; }
  QStringList file_extensions() const override { return QStringList() << "wpl"; }
  QString mime_type() const override { return "application/vnd.ms-wpl"; }
  bool load_supported() const override { return true; }
  bool save_supported() const override { return true; }

  bool TryMagic(const QByteArray &data) const override;

  SongList Load(QIODevice *device, const QString &playlist_path, const QDir &dir, const bool collection_search = true) const override;
  void Save(const SongList &songs, QIODevice *device, const QDir &dir, const PlaylistSettingsPage::PathType path_type = PlaylistSettingsPage::PathType::Automatic) const override;

 private:
  void ParseSeq(const QDir &dir, QXmlStreamReader *reader, SongList *songs, const bool collection_search = true) const;
  static void WriteMeta(const QString &name, const QString &content, QXmlStreamWriter *writer);
};

#endif  // WPLPARSER_H
