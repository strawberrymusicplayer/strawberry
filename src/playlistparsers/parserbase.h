/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef PARSERBASE_H
#define PARSERBASE_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QDir>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "constants/playlistsettings.h"

class QIODevice;
class CollectionBackendInterface;
class TagReaderClient;

class ParserBase : public QObject {
  Q_OBJECT

 public:
  explicit ParserBase(const SharedPtr<TagReaderClient> tagreader_client, const SharedPtr<CollectionBackendInterface> collection_backend, QObject *parent = nullptr);

  class LoadResult {
   public:
    LoadResult(const SongList &_songs = SongList(), const QString &_playlist_name = QString()) : songs(_songs), playlist_name(_playlist_name) {}
    SongList songs;
    QString playlist_name;
  };

  virtual QString name() const = 0;
  virtual QStringList file_extensions() const = 0;
  virtual bool load_supported() const = 0;
  virtual bool save_supported() const = 0;
  virtual QString mime_type() const { return QString(); }

  virtual bool TryMagic(const QByteArray &data) const = 0;

  // Loads all songs from playlist found at path 'playlist_path' in directory 'dir'.
  // The 'device' argument is an opened and ready to read from representation of this playlist.
  // This method might not return all the songs found in the playlist.
  // Any playlist parser may decide to leave out some entries if it finds them incomplete or invalid.
  // This means that the final resulting SongList should be considered valid (at least from the parser's point of view).
  virtual LoadResult Load(QIODevice *device, const QString &playlist_path = QLatin1String(""), const QDir &dir = QDir(), const bool collection_lookup = true) const = 0;
  virtual void Save(const QString &playlist_name, const SongList &songs, QIODevice *device, const QDir &dir = QDir(), const PlaylistSettings::PathType path_type = PlaylistSettings::PathType::Automatic) const = 0;

 Q_SIGNALS:
  void Error(const QString &error) const;

 protected:
  // Loads a song.  If filename_or_url is a URL (with a scheme other than "file") then it is set on the song and the song marked as a stream.
  // Also sets the song's metadata by searching in the Collection, or loading from the file as a fallback.
  // This function should always be used when loading a playlist.
  Song LoadSong(const QString &filename_or_url, const qint64 beginning, const int track, const QDir &dir, const bool collection_lookup) const;
  void LoadSong(const QString &filename_or_url, const qint64 beginning, const int track, const QDir &dir, Song *song, const bool collection_lookup) const;

  // If the URL is a file:// URL then returns its path, absolute or relative to the directory depending on the path_type option.
  // Otherwise, returns the URL as is. This function should always be used when saving a playlist.
  static QString URLOrFilename(const QUrl &url, const QDir &dir, const PlaylistSettings::PathType path_type);

 private:
  const SharedPtr<TagReaderClient> tagreader_client_;
  const SharedPtr<CollectionBackendInterface> collection_backend_;
};

#endif  // PARSERBASE_H
