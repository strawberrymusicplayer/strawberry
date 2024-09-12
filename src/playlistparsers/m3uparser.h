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

#ifndef M3UPARSER_H
#define M3UPARSER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QDir>

#include "core/shared_ptr.h"
#include "core/song.h"
#include "settings/playlistsettingspage.h"
#include "parserbase.h"

class QIODevice;
class CollectionBackendInterface;

class M3UParser : public ParserBase {
  Q_OBJECT

 public:
  explicit M3UParser(SharedPtr<CollectionBackendInterface> collection_backend, QObject *parent = nullptr);

  QString name() const override { return QStringLiteral("M3U"); }
  QStringList file_extensions() const override { return QStringList() << QStringLiteral("m3u") << QStringLiteral("m3u8"); }
  QString mime_type() const override { return QStringLiteral("text/uri-list"); }
  bool load_supported() const override { return true; }
  bool save_supported() const override { return true; }

  bool TryMagic(const QByteArray &data) const override;

  SongList Load(QIODevice *device, const QString &playlist_path = ""_L1, const QDir &dir = QDir(), const bool collection_lookup = true) const override;
  void Save(const SongList &songs, QIODevice *device, const QDir &dir = QDir(), const PlaylistSettingsPage::PathType path_type = PlaylistSettingsPage::PathType::Automatic) const override;

 private:
  enum class M3UType {
    STANDARD = 0,
    EXTENDED,  // Includes extended info (track, artist, etc.)
    LINK,      // Points to a directory.
  };

  struct Metadata {
    Metadata() : length(-1) {}
    QString artist;
    QString title;
    qint64 length;
  };

  static bool ParseMetadata(const QString &line, Metadata *metadata);

};

#endif  // M3UPARSER_H
