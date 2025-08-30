/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <cstddef>

#include <cdio/types.h>
#include <cdio/cdio.h>

#include <chrono>

#include <QString>
#include <QUrl>
#include <QTimer>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "collection/collectionmodel.h"
#include "cddasongloader.h"
#include "connecteddevice.h"
#include "cddadevice.h"

class DeviceLister;
class DeviceManager;

using namespace std::chrono_literals;

CDDADevice::CDDADevice(const QUrl &url,
                       DeviceLister *lister,
                       const QString &unique_id,
                       DeviceManager *device_manager,
                       const SharedPtr<TaskManager> task_manager,
                       const SharedPtr<Database> database,
                       const SharedPtr<TagReaderClient> tagreader_client,
                       const SharedPtr<AlbumCoverLoader> albumcover_loader,
                       const int database_id,
                       const bool first_time,
                       QObject *parent)
    : ConnectedDevice(url, lister, unique_id, device_manager, task_manager, database, tagreader_client, albumcover_loader, database_id, first_time, parent),
      cdda_song_loader_(url),
      cdio_(nullptr),
      timer_disc_changed_(new QTimer(this)) {

  timer_disc_changed_->setInterval(1s);

  QObject::connect(&cdda_song_loader_, &CDDASongLoader::SongsLoaded, this, &CDDADevice::SongsLoaded);
  QObject::connect(&cdda_song_loader_, &CDDASongLoader::SongsUpdated, this, &CDDADevice::SongsLoaded);
  QObject::connect(&cdda_song_loader_, &CDDASongLoader::LoadingFinished, this, &CDDADevice::SongLoadingFinished);
  QObject::connect(this, &CDDADevice::SongsDiscovered, collection_model_, &CollectionModel::AddReAddOrUpdate);
  QObject::connect(timer_disc_changed_, &QTimer::timeout, this, &CDDADevice::CheckDiscChanged);

}

CDDADevice::~CDDADevice() {

  if (cdio_) {
    cdio_destroy(cdio_);
    cdio_ = nullptr;
  }

}

bool CDDADevice::Init() {

  if (!cdio_) {
    cdio_ = cdio_open(url_.path().toLocal8Bit().constData(), DRIVER_DEVICE);
    if (!cdio_) return false;
  }

  LoadSongs();

  WatchForDiscChanges(true);

  return true;

}

void CDDADevice::WatchForDiscChanges(const bool watch) {

  if (watch && !timer_disc_changed_->isActive()) {
    timer_disc_changed_->start();
  }
  else if (!watch && timer_disc_changed_->isActive()) {
    timer_disc_changed_->stop();
  }

}

void CDDADevice::CheckDiscChanged() {

  if (!cdio_ || cdda_song_loader_.IsActive()) return;

  if (cdio_get_media_changed(cdio_) == 1) {
    qLog(Debug) << "CD changed, reloading songs";
    SongsLoaded();
    LoadSongs();
  }

}

void CDDADevice::LoadSongs() {

  cdda_song_loader_.LoadSongs();
  WatchForDiscChanges(false);

}

void CDDADevice::SongsLoaded(const SongList &songs) {

  collection_model_->Reset();
  Q_EMIT SongsDiscovered(songs);
  song_count_ = songs.size();
  (void)cdio_get_media_changed(cdio_);

}

void CDDADevice::SongLoadingFinished() {

  WatchForDiscChanges(true);

}

