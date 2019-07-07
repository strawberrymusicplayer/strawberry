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

#ifndef CURRENTALBUMCOVERLOADER_H
#define CURRENTALBUMCOVERLOADER_H

#include "config.h"

#include <memory>

#include <QtGlobal>
#include <QObject>
#include <QString>
#include <QImage>
#include <QTemporaryFile>

#include "core/song.h"
#include "albumcoverloaderoptions.h"

class Application;

class CurrentAlbumCoverLoader : public QObject {
  Q_OBJECT

 public:
  explicit CurrentAlbumCoverLoader(Application *app, QObject *parent = nullptr);
  ~CurrentAlbumCoverLoader();

  const AlbumCoverLoaderOptions &options() const { return options_; }
  const Song &last_song() const { return last_song_; }

 public slots:
  void LoadAlbumCover(const Song &song);

 signals:
  void AlbumCoverLoaded(const Song &song, const QUrl &cover_url, const QImage &image);
  void ThumbnailLoaded(const Song &song, const QUrl &thumbnail_uri, const QImage &image);

 private slots:
  void TempAlbumCoverLoaded(const quint64 id, const QUrl &remote_url, const QImage &image);

 private:
  Application *app_;
  AlbumCoverLoaderOptions options_;

  QString temp_file_pattern_;

  std::unique_ptr<QTemporaryFile> temp_cover_;
  std::unique_ptr<QTemporaryFile> temp_cover_thumbnail_;
  quint64 id_;

  Song last_song_;

};

#endif  // CURRENTALBUMCOVERLOADER_H
