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

#include "config.h"

#include <memory>

#include <QtGlobal>
#include <QObject>
#include <QDir>
#include <QImage>

#include "core/logging.h"
#include "core/standardpaths.h"
#include "core/song.h"
#include "core/temporaryfile.h"
#include "albumcoverloader.h"
#include "albumcoverloaderresult.h"
#include "currentalbumcoverloader.h"

using std::make_unique;
using namespace Qt::Literals::StringLiterals;

CurrentAlbumCoverLoader::CurrentAlbumCoverLoader(const SharedPtr<AlbumCoverLoader> albumcover_loader, QObject *parent)
    : QObject(parent),
      albumcover_loader_(albumcover_loader),
      temp_file_pattern_(StandardPaths::WritableLocation(StandardPaths::StandardLocation::TempLocation) + u"/strawberry-cover-XXXXXX.jpg"_s),
      id_(0) {

  setObjectName(QLatin1String(QObject::metaObject()->className()));

  options_.options = AlbumCoverLoaderOptions::Option::RawImageData | AlbumCoverLoaderOptions::Option::OriginalImage | AlbumCoverLoaderOptions::Option::ScaledImage;
  options_.desired_scaled_size = QSize(120, 120);
  options_.default_cover = u":/pictures/cdcase.png"_s;

  QObject::connect(&*albumcover_loader, &AlbumCoverLoader::AlbumCoverLoaded, this, &CurrentAlbumCoverLoader::AlbumCoverReady);

  ReloadSettingsAsync();

}

CurrentAlbumCoverLoader::~CurrentAlbumCoverLoader() = default;

void CurrentAlbumCoverLoader::ReloadSettingsAsync() {

  QMetaObject::invokeMethod(this, &CurrentAlbumCoverLoader::ReloadSettings);

}

void CurrentAlbumCoverLoader::ReloadSettings() {

  options_.types = AlbumCoverLoaderOptions::LoadTypes();

}

void CurrentAlbumCoverLoader::LoadAlbumCover(const Song &song) {

  last_song_ = song;
  id_ = albumcover_loader_->LoadImageAsync(options_, last_song_);

}

void CurrentAlbumCoverLoader::AlbumCoverReady(const quint64 id, AlbumCoverLoaderResult result) {

  if (id != id_) return;
  id_ = 0;

  if (!result.album_cover.image.isNull()) {
    temp_cover_ = make_unique<TemporaryFile>(temp_file_pattern_);
    if (!temp_cover_->filename().isEmpty()) {
      if (result.album_cover.image.save(temp_cover_->filename(), "JPEG")) {
        result.temp_cover_url = QUrl::fromLocalFile(temp_cover_->filename());
      }
      else {
        qLog(Error) << "Failed to save cover image to" << temp_cover_->filename();
      }
    }
    else {
      qLog(Error) << "Failed to open" << temp_cover_->filename();
    }
  }

  QUrl thumbnail_url;
  if (!result.image_scaled.isNull()) {
    temp_cover_thumbnail_ = make_unique<TemporaryFile>(temp_file_pattern_);
    if (!temp_cover_thumbnail_->filename().isEmpty()) {
      if (result.image_scaled.save(temp_cover_thumbnail_->filename(), "JPEG")) {
        thumbnail_url = QUrl::fromLocalFile(temp_cover_thumbnail_->filename());
      }
      else {
        qLog(Error) << "Unable to save cover thumbnail image to" << temp_cover_thumbnail_->filename();
      }
    }
    else {
      qLog(Error) << "Unable to open" << temp_cover_thumbnail_->filename();
    }
  }

  if (result.art_manual_updated.isValid()) {
    last_song_.set_art_manual(result.art_manual_updated);
  }
  if (result.art_automatic_updated.isValid()) {
    last_song_.set_art_automatic(result.art_automatic_updated);
  }

  Q_EMIT AlbumCoverLoaded(last_song_, result);
  Q_EMIT ThumbnailLoaded(last_song_, thumbnail_url, result.image_scaled);

}
