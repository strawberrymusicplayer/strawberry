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

#include "config.h"

#include <QObject>
#include <QDir>
#include <QByteArray>
#include <QString>
#include <QStringList>

#include "includes/shared_ptr.h"
#include "constants/playlistsettings.h"
#include "core/song.h"
#include "parserbase.h"

class QIODevice;
class TagReaderClient;
class CollectionBackendInterface;

class PLSParser : public ParserBase {
  Q_OBJECT

 public:
  explicit PLSParser(const SharedPtr<TagReaderClient> tagreader_client, const SharedPtr<CollectionBackendInterface> collection_backend, QObject *parent = nullptr);

  QString name() const override { return QStringLiteral("PLS"); }
  QStringList file_extensions() const override { return QStringList() << QStringLiteral("pls"); }
  QString mime_type() const override { return QStringLiteral("audio/x-scpls"); }
  bool load_supported() const override { return true; }
  bool save_supported() const override { return true; }

  bool TryMagic(const QByteArray &data) const override;

  LoadResult Load(QIODevice *device, const QString &playlist_path = QLatin1String(""), const QDir &dir = QDir(), const bool collection_lookup = true) const override;
  void Save(const QString &playlist_name, const SongList &songs, QIODevice *device, const QDir &dir = QDir(), const PlaylistSettings::PathType path_type = PlaylistSettings::PathType::Automatic) const override;
};

#endif  // PLSPARSER_H
