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

#ifndef PLAYLISTPARSER_H
#define PLAYLISTPARSER_H

#include "config.h"

#include <QObject>
#include <QDir>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "constants/playlistsettings.h"

class QIODevice;
class TagReaderClient;
class CollectionBackendInterface;
class ParserBase;

class PlaylistParser : public QObject {
  Q_OBJECT

 public:
  explicit PlaylistParser(const SharedPtr<TagReaderClient> tagreader_client, const SharedPtr<CollectionBackendInterface> collection_backend = nullptr, QObject *parent = nullptr);

  enum class Type {
    Load,
    Save
  };

  static const int kMagicSize;

  QStringList file_extensions(const Type type) const;
  QString filters(const Type type) const;

  QStringList mime_types(const Type type) const;

  QString default_extension() const;
  QString default_filter() const;

  ParserBase *ParserForMagic(const QByteArray &data, const QString &mime_type = QString()) const;
  ParserBase *ParserForExtension(const Type type, const QString &suffix) const;
  ParserBase *ParserForMimeType(const Type type, const QString &mime) const;

  SongList LoadFromFile(const QString &filename) const;
  SongList LoadFromDevice(QIODevice *device, const QString &path_hint = QString(), const QDir &dir_hint = QDir()) const;
  void Save(const QString &playlist_name, const SongList &songs, const QString &filename, const PlaylistSettings::PathType) const;

 Q_SIGNALS:
  void Error(const QString &error) const;

 private:
  void AddParser(ParserBase *parser);
  bool ParserIsSupported(const Type type, ParserBase *parser) const;
  static QString FilterForParser(const ParserBase *parser, QStringList *all_extensions = nullptr);

 private:
  QList<ParserBase*> parsers_;
  ParserBase *default_parser_;
};

#endif  // PLAYLISTPARSER_H
