/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include <cdio/mmc.h>
#include <cdio/mmc_ll_cmds.h>

#include <chrono>

#include <QString>
#include <QUrl>
#include <QTimer>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "utilities/cddautils.h"
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
      media_state_(MediaState::Unknown),
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
    cdio_ = cdio_open(Utilities::CDDADeviceFromUrlPath(url_.path()).toLocal8Bit().constData(), DRIVER_DEVICE);
    if (!cdio_) return false;
  }

  media_state_ = GetMediaState();

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

CDDADevice::MediaState CDDADevice::GetMediaState() const {

  if (!cdio_) return MediaState::Unknown;

  // Query the current media status instead of relying only on media change events.
  // On Windows the operating system polls the drive itself and consumes the media events, so cdio_get_media_changed() never reports a disc change there.
  uint8_t event_status[2] = { 0, 0 };
  if (mmc_get_event_status(cdio_, event_status) != DRIVER_OP_SUCCESS) {
    return MediaState::Unknown;
  }

  const bool media_present = (event_status[1] & 0x02) != 0;
  if (!media_present) return MediaState::NoMedia;

  // The disc can be present but not ready yet while the drive is spinning up.
  if (mmc_test_unit_ready(cdio_, 0) != DRIVER_OP_SUCCESS) {
    return MediaState::NoMedia;
  }

  return MediaState::MediaReady;

}

void CDDADevice::CheckDiscChanged() {

  if (!cdio_ || cdda_song_loader_.IsActive()) return;

  bool disc_changed = cdio_get_media_changed(cdio_) == 1;

  const MediaState media_state = GetMediaState();
  if (media_state != MediaState::Unknown) {
    // Also treat the first known state as a change, in case the media state could not be determined when the device was initialized.
    if (media_state != media_state_) {
      disc_changed = true;
    }
    media_state_ = media_state;
  }

  if (!disc_changed) return;

  if (media_state == MediaState::NoMedia) {
    qLog(Debug) << "CD removed, clearing songs";
    SongsLoaded();
    return;
  }

  qLog(Debug) << "CD changed, reloading songs";
  SongsLoaded();
  LoadSongs();

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
