/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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

#include <libmtp.h>

#include <QObject>
#include <QUrl>

#include "core/taskmanager.h"
#include "core/song.h"
#include "core/logging.h"
#include "collection/collectionbackend.h"
#include "connecteddevice.h"
#include "mtpdevice.h"
#include "mtpconnection.h"
#include "mtploader.h"

MtpLoader::MtpLoader(const QUrl &url, TaskManager *task_manager, CollectionBackend *backend, std::shared_ptr<ConnectedDevice> device)
    : QObject(nullptr),
      url_(url),
      task_manager_(task_manager),
      backend_(backend),
      device_(device) {
  original_thread_ = thread();
}

MtpLoader::~MtpLoader() {}

bool MtpLoader::Init() { return true; }

void MtpLoader::LoadDatabase() {

  int task_id = task_manager_->StartTask(tr("Loading MTP device"));
  emit TaskStarted(task_id);

  bool success = TryLoad();

  moveToThread(original_thread_);

  task_manager_->SetTaskFinished(task_id);
  emit LoadFinished(success);

}

bool MtpLoader::TryLoad() {

  MtpDevice *device = dynamic_cast<MtpDevice*>(device_.get()); // FIXME

  if (!device->connection() || !device->connection()->is_valid())
    device->NewConnection();

  if (!device->connection() || !device->connection()->is_valid()) {
    emit Error(tr("Error connecting MTP device %1").arg(url_.toString()));
    return false;
  }

  // Load the list of songs on the device
  SongList songs;
  LIBMTP_track_t* tracks = LIBMTP_Get_Tracklisting_With_Callback(device->connection()->device(), nullptr, nullptr);
  while (tracks) {
    LIBMTP_track_t *track = tracks;

    Song song(Song::Source_Device);
    song.InitFromMTP(track, url_.host());
    if (song.is_valid() && !song.artist().isEmpty() && !song.title().isEmpty()) {
      song.set_directory_id(1);
      songs << song;
    }
    tracks = tracks->next;
    LIBMTP_destroy_track_t(track);
  }

  // Need to remove all the existing songs in the database first
  backend_->DeleteSongs(backend_->FindSongsInDirectory(1));

  // Add the songs we've just loaded
  backend_->AddOrUpdateSongs(songs);

  return true;

}

