/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2020, Jonas Kvinge <jonas@jkvinge.net>
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
#include "core/song.h"
#include "playlist/playlistmanager.h"
#include "albumcoverloader.h"
#include "albumcoverloaderoptions.h"
#include "albumcoverloaderresult.h"
#include "currentalbumcoverloader.h"

CurrentAlbumCoverLoader::CurrentAlbumCoverLoader(Application *app, QObject *parent)
    : QObject(parent),
      app_(app),
      temp_file_pattern_(QDir::tempPath() + "/strawberry-cover-XXXXXX.jpg"),
      id_(0)
  {

  options_.get_image_data_ = true;
  options_.get_image_ = true;
  options_.scale_output_image_ = false;
  options_.pad_output_image_ = false;
  options_.create_thumbnail_ = true;
  options_.thumbnail_size_ = QSize(120, 120);
  options_.default_output_image_ = QImage(":/pictures/cdcase.png");
  options_.default_thumbnail_image_ = options_.default_output_image_.scaledToHeight(120, Qt::SmoothTransformation);

  QObject::connect(app_->album_cover_loader(), &AlbumCoverLoader::AlbumCoverLoaded, this, &CurrentAlbumCoverLoader::TempAlbumCoverLoaded);
  QObject::connect(app_->playlist_manager(), &PlaylistManager::CurrentSongChanged, this, &CurrentAlbumCoverLoader::LoadAlbumCover);

}

CurrentAlbumCoverLoader::~CurrentAlbumCoverLoader() {

  if (temp_cover_) temp_cover_->remove();
  if (temp_cover_thumbnail_) temp_cover_thumbnail_->remove();

}

void CurrentAlbumCoverLoader::LoadAlbumCover(const Song &song) {

  last_song_ = song;
  id_ = app_->album_cover_loader()->LoadImageAsync(options_, last_song_);

}

void CurrentAlbumCoverLoader::TempAlbumCoverLoaded(const quint64 id, AlbumCoverLoaderResult result) {

  if (id != id_) return;
  id_ = 0;

  if (!result.album_cover.image.isNull()) {
    temp_cover_.reset(new QTemporaryFile(temp_file_pattern_));
    temp_cover_->setAutoRemove(true);
    if (temp_cover_->open()) {
      if (result.album_cover.image.save(temp_cover_->fileName(), "JPEG")) {
        result.temp_cover_url = QUrl::fromLocalFile(temp_cover_->fileName());
      }
      else {
        qLog(Error) << "Unable to save cover image to" << temp_cover_->fileName();
      }
    }
    else {
      qLog(Error) << "Unable to open" << temp_cover_->fileName();
    }
  }

  QUrl thumbnail_url;
  if (!result.image_thumbnail.isNull()) {
    temp_cover_thumbnail_.reset(new QTemporaryFile(temp_file_pattern_));
    temp_cover_thumbnail_->setAutoRemove(true);
    if (temp_cover_thumbnail_->open()) {
      if (result.image_thumbnail.save(temp_cover_thumbnail_->fileName(), "JPEG")) {
        thumbnail_url = QUrl::fromLocalFile(temp_cover_thumbnail_->fileName());
      }
      else {
        qLog(Error) << "Unable to save cover thumbnail image to" << temp_cover_thumbnail_->fileName();
      }
    }
    else {
      qLog(Error) << "Unable to open" << temp_cover_thumbnail_->fileName();
    }
  }

  if (result.updated) {
    last_song_.set_art_manual(result.album_cover.cover_url);
  }

  emit AlbumCoverLoaded(last_song_, result);
  emit ThumbnailLoaded(last_song_, thumbnail_url, result.image_thumbnail);

}
