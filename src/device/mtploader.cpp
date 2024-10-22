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

#include <libmtp.h>

#include <memory>

#include <QObject>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/taskmanager.h"
#include "core/song.h"
#include "collection/collectionbackend.h"
#include "mtpconnection.h"
#include "mtploader.h"

using std::make_unique;

MtpLoader::MtpLoader(const QUrl &url, const SharedPtr<TaskManager> task_manager, const SharedPtr<CollectionBackend> backend, QObject *parent)
    : QObject(parent),
      url_(url),
      task_manager_(task_manager),
      backend_(backend),
      abort_(false) {
  original_thread_ = thread();
}

MtpLoader::~MtpLoader() = default;

bool MtpLoader::Init() { return true; }

void MtpLoader::LoadDatabase() {

  int task_id = task_manager_->StartTask(tr("Loading MTP device"));
  Q_EMIT TaskStarted(task_id);

  bool success = TryLoad();

  moveToThread(original_thread_);

  task_manager_->SetTaskFinished(task_id);
  Q_EMIT LoadFinished(success, connection_.release());

}

bool MtpLoader::TryLoad() {

  connection_ = make_unique<MtpConnection>(url_);

  if (!connection_) {
    Q_EMIT Error(tr("Error connecting MTP device %1").arg(url_.toString()));
    return false;
  }

  if (!connection_->is_valid()) {
    Q_EMIT Error(tr("Error connecting MTP device %1: %2").arg(url_.toString(), connection_->error_text()));
    return false;
  }

  // Load the list of songs on the device
  SongList songs;
  LIBMTP_track_t *tracks = LIBMTP_Get_Tracklisting_With_Callback(connection_->device(), nullptr, nullptr);
  while (tracks) {

    if (abort_) break;

    LIBMTP_track_t *track = tracks;

    Song song(Song::Source::Device);
    song.InitFromMTP(track, url_.host());
    if (song.is_valid() && !song.title().isEmpty()) {
      song.set_directory_id(1);
      songs << song;
    }
    tracks = tracks->next;
    LIBMTP_destroy_track_t(track);
  }

  if (!abort_) {
    // Need to remove all the existing songs in the database first
    backend_->DeleteSongs(backend_->FindSongsInDirectory(1));

    // Add the songs we've just loaded
    backend_->AddOrUpdateSongs(songs);
  }

  // This is done in the loader thread so close the unique DB connection.
  backend_->Close();

  return !abort_;

}
