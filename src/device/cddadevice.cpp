/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QString>
#include <QUrl>

#include "core/shared_ptr.h"
#include "collection/collectionmodel.h"
#include "cddasongloader.h"
#include "connecteddevice.h"
#include "cddadevice.h"

class Application;
class DeviceLister;
class DeviceManager;

CddaDevice::CddaDevice(const QUrl &url, DeviceLister *lister, const QString &unique_id, SharedPtr<DeviceManager> manager, Application *app, int database_id, bool first_time, QObject *parent)
    : ConnectedDevice(url, lister, unique_id, manager, app, database_id, first_time, parent),
      cdda_song_loader_(url) {

  QObject::connect(&cdda_song_loader_, &CddaSongLoader::SongsLoaded, this, &CddaDevice::SongsLoaded);
  QObject::connect(&cdda_song_loader_, &CddaSongLoader::SongsDurationLoaded, this, &CddaDevice::SongsLoaded);
  QObject::connect(&cdda_song_loader_, &CddaSongLoader::SongsMetadataLoaded, this, &CddaDevice::SongsLoaded);
  QObject::connect(this, &CddaDevice::SongsDiscovered, model_, &CollectionModel::AddReAddOrUpdate);

}

bool CddaDevice::Init() {

  song_count_ = 0;  // Reset song count, in case it was already set
  cdda_song_loader_.LoadSongs();
  return true;

}

void CddaDevice::Refresh() {

  if (!cdda_song_loader_.HasChanged()) {
    return;
  }
  Init();

}

void CddaDevice::SongsLoaded(const SongList &songs) {

  model_->Reset();
  Q_EMIT SongsDiscovered(songs);
  song_count_ = songs.size();

}
