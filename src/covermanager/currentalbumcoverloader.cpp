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

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QDir>
#include <QString>
#include <QStringBuilder>
#include <QImage>
#include <QTemporaryFile>

#include "core/application.h"
#include "playlist/playlistmanager.h"
#include "albumcoverloader.h"
#include "albumcoverloaderoptions.h"
#include "currentalbumcoverloader.h"

CurrentAlbumCoverLoader::CurrentAlbumCoverLoader(Application *app, QObject *parent)
    : QObject(parent),
      app_(app),
      temp_file_pattern_(QDir::tempPath() + "/strawberry-cover-XXXXXX.jpg"),
      id_(0)
  {

  options_.scale_output_image_ = false;
  options_.pad_output_image_ = false;
  options_.default_output_image_ = QImage(":/pictures/cdcase.png");

  connect(app_->album_cover_loader(), SIGNAL(ImageLoaded(quint64, QUrl, QImage)), SLOT(TempAlbumCoverLoaded(quint64, QUrl, QImage)));
  connect(app_->playlist_manager(), SIGNAL(CurrentSongChanged(Song)), SLOT(LoadAlbumCover(Song)));

}

CurrentAlbumCoverLoader::~CurrentAlbumCoverLoader() {
  if (temp_cover_) temp_cover_->remove();
  if (temp_cover_thumbnail_) temp_cover_thumbnail_->remove();
}

void CurrentAlbumCoverLoader::LoadAlbumCover(const Song &song) {
  last_song_ = song;
  id_ = app_->album_cover_loader()->LoadImageAsync(options_, last_song_);
}

void CurrentAlbumCoverLoader::TempAlbumCoverLoaded(const quint64 id, const QUrl &remote_url, const QImage &image) {

  Q_UNUSED(remote_url);

  if (id != id_) return;
  id_ = 0;

  QUrl cover_url;
  QUrl thumbnail_url;
  QImage thumbnail;

  if (!image.isNull()) {

    QString filename;

    temp_cover_.reset(new QTemporaryFile(temp_file_pattern_));
    temp_cover_->setAutoRemove(true);
    temp_cover_->open();

    image.save(temp_cover_->fileName(), "JPEG");

    // Scale the image down to make a thumbnail.  It's a bit crap doing it here since it's the GUI thread, but the alternative is hard.
    temp_cover_thumbnail_.reset(new QTemporaryFile(temp_file_pattern_));
    temp_cover_thumbnail_->open();
    temp_cover_thumbnail_->setAutoRemove(true);
    thumbnail = image.scaledToHeight(120, Qt::SmoothTransformation);
    thumbnail.save(temp_cover_thumbnail_->fileName(), "JPEG");

    cover_url = QUrl::fromLocalFile(temp_cover_->fileName());
    thumbnail_url = QUrl::fromLocalFile(temp_cover_thumbnail_->fileName());
  }

  emit AlbumCoverLoaded(last_song_, cover_url, image);
  emit ThumbnailLoaded(last_song_, thumbnail_url, thumbnail);

}
