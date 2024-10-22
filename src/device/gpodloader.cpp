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

#include <glib.h>
#include <gpod/itdb.h>

#include <QObject>
#include <QDir>
#include <QByteArray>
#include <QString>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/song.h"
#include "core/taskmanager.h"
#include "collection/collectionbackend.h"
#include "gpodloader.h"

GPodLoader::GPodLoader(const QString &mount_point,
                       const SharedPtr<TaskManager> task_manager,
                       const SharedPtr<CollectionBackend> backend,
                       const SharedPtr<ConnectedDevice> device,
                       QObject *parent)
    : QObject(parent),
      device_(device),
      mount_point_(mount_point),
      type_(Song::FileType::Unknown),
      task_manager_(task_manager),
      backend_(backend),
      abort_(false) {
  original_thread_ = thread();
}

GPodLoader::~GPodLoader() = default;

void GPodLoader::LoadDatabase() {

  int task_id = task_manager_->StartTask(tr("Loading iPod database"));
  Q_EMIT TaskStarted(task_id);

  Itdb_iTunesDB *db = TryLoad();

  moveToThread(original_thread_);

  task_manager_->SetTaskFinished(task_id);
  Q_EMIT LoadFinished(db, !abort_);

}

Itdb_iTunesDB *GPodLoader::TryLoad() {

  // Load the iTunes database
  const QByteArray mountpoint = QDir::toNativeSeparators(mount_point_).toLocal8Bit();
  GError *error = nullptr;
  Itdb_iTunesDB *db = itdb_parse(mountpoint.constData(), &error);

  // Check for errors
  if (!db) {
    if (error) {
      qLog(Error) << "loading database failed:" << error->message;
      Q_EMIT Error(QString::fromUtf8(error->message));
      g_error_free(error);
    }
    else {
      Q_EMIT Error(tr("An error occurred loading the iTunes database"));
    }

    return db;
  }

  // Convert all the tracks from libgpod structs into Song classes
  const QString prefix = path_prefix_.isEmpty() ? QDir::fromNativeSeparators(mount_point_) : path_prefix_;

  SongList songs;
  for (GList *tracks = db->tracks; tracks != nullptr; tracks = tracks->next) {

    if (abort_) break;

    Itdb_Track *track = static_cast<Itdb_Track*>(tracks->data);

    Song song(Song::Source::Device);
    song.InitFromItdb(track, prefix);
    song.set_directory_id(1);

    if (type_ != Song::FileType::Unknown) song.set_filetype(type_);
    songs << song;
  }

  // Need to remove all the existing songs in the database first
  backend_->DeleteSongs(backend_->FindSongsInDirectory(1));

  if (!abort_) {
    // Add the songs we've just loaded
    backend_->AddOrUpdateSongs(songs);
  }

  // This is done in the loader thread so close the unique DB connection.
  backend_->Close();

  return db;

}
