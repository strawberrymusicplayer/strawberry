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

#ifndef CURRENTALBUMCOVERLOADER_H
#define CURRENTALBUMCOVERLOADER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QString>
#include <QImage>

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "core/temporaryfile.h"
#include "core/song.h"
#include "albumcoverloaderoptions.h"
#include "albumcoverloaderresult.h"

class AlbumCoverLoader;

class CurrentAlbumCoverLoader : public QObject {
  Q_OBJECT

 public:
  explicit CurrentAlbumCoverLoader(const SharedPtr<AlbumCoverLoader> albumcover_loader, QObject *parent = nullptr);
  ~CurrentAlbumCoverLoader() override;

  const AlbumCoverLoaderOptions &options() const { return options_; }
  const Song &last_song() const { return last_song_; }

  void ReloadSettingsAsync();

 public Q_SLOTS:
  void ReloadSettings();
  void LoadAlbumCover(const Song &song);

 Q_SIGNALS:
  void AlbumCoverLoaded(const Song &song, const AlbumCoverLoaderResult &result);
  void ThumbnailLoaded(const Song &song, const QUrl &thumbnail_uri, const QImage &image);

 private Q_SLOTS:
  void AlbumCoverReady(const quint64 id, AlbumCoverLoaderResult result);

 private:
  const SharedPtr<AlbumCoverLoader> albumcover_loader_;
  AlbumCoverLoaderOptions options_;

  const QString temp_file_pattern_;

  ScopedPtr<TemporaryFile> temp_cover_;
  ScopedPtr<TemporaryFile> temp_cover_thumbnail_;
  quint64 id_;

  Song last_song_;
};

#endif  // CURRENTALBUMCOVERLOADER_H
