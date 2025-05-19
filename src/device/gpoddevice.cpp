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

#include <memory>

#include <glib.h>
#include <gpod/itdb.h>

#include <QtGlobal>
#include <QThread>
#include <QMutex>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QList>
#include <QString>
#include <QUrl>
#include <QImage>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/standardpaths.h"
#include "core/temporaryfile.h"
#include "core/taskmanager.h"
#include "core/database.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "covermanager/albumcoverloader.h"
#include "connecteddevice.h"
#include "gpoddevice.h"
#include "gpodloader.h"

class DeviceLister;
class DeviceManager;

using std::make_shared;
using namespace Qt::Literals::StringLiterals;

GPodDevice::GPodDevice(const QUrl &url,
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
      task_manager_(task_manager),
      loader_(nullptr),
      loader_thread_(nullptr),
      db_(nullptr),
      closing_(false) {}

bool GPodDevice::Init() {

  InitBackendDirectory(url_.path(), first_time_);
  collection_model_->Init();

  loader_ = new GPodLoader(url_.path(), task_manager_, collection_backend_, shared_from_this());
  loader_thread_ = new QThread();
  loader_->moveToThread(loader_thread_);

  QObject::connect(loader_, &GPodLoader::Error, this, &GPodDevice::LoaderError);
  QObject::connect(loader_, &GPodLoader::TaskStarted, this, &GPodDevice::TaskStarted);
  QObject::connect(loader_, &GPodLoader::LoadFinished, this, &GPodDevice::LoadFinished);
  QObject::connect(loader_thread_, &QThread::started, loader_, &GPodLoader::LoadDatabase);

  return true;

}

GPodDevice::~GPodDevice() {

  if (loader_) {
    loader_thread_->exit();
    loader_->deleteLater();
    loader_thread_->deleteLater();
    loader_ = nullptr;
    loader_thread_ = nullptr;
  }

}

void GPodDevice::ConnectAsync() {

  loader_thread_->start();

}

void GPodDevice::Close() {

  closing_ = true;

  if (IsLoading()) {
    loader_->Abort();
  }
  else {
    ConnectedDevice::Close();
  }

}

void GPodDevice::LoadFinished(Itdb_iTunesDB *db, const bool success) {

  QMutexLocker l(&db_mutex_);
  db_ = db;
  db_wait_cond_.wakeAll();

  if (loader_thread_) {
    loader_thread_->quit();
    loader_thread_->wait(1000);
    loader_thread_->deleteLater();
    loader_thread_ = nullptr;
  }

  loader_->deleteLater();
  loader_ = nullptr;

  if (closing_) {
    ConnectedDevice::Close();
  }
  else {
    Q_EMIT DeviceConnectFinished(unique_id_, success);
  }

}

void GPodDevice::LoaderError(const QString &message) {
  Q_EMIT Error(message);
}

void GPodDevice::Start() {

  {
    // Wait for the database to be loaded
    QMutexLocker l(&db_mutex_);
    if (!db_) db_wait_cond_.wait(&db_mutex_);
  }

  // Ensure only one "organize files" can be active at any one time
  db_busy_.lock();

}

bool GPodDevice::StartCopy(QList<Song::FileType> *supported_filetypes) {

  Start();

  if (supported_filetypes) GetSupportedFiletypes(supported_filetypes);

  return true;

}

Itdb_Track *GPodDevice::AddTrackToITunesDb(const Song &metadata) {

  // Create the track
  Itdb_Track *track = itdb_track_new();
  metadata.ToItdb(track);

  // Add it to the DB and the master playlist
  // The DB takes ownership of the track
  itdb_track_add(db_, track, -1);
  Itdb_Playlist *mpl = itdb_playlist_mpl(db_);
  itdb_playlist_add_track(mpl, track, -1);

  return track;

}

void GPodDevice::AddTrackToModel(Itdb_Track *track, const QString &prefix) {

  // Add it to our CollectionModel
  Song metadata_on_device;
  metadata_on_device.InitFromItdb(track, prefix);
  metadata_on_device.set_directory_id(1);
  songs_to_add_ << metadata_on_device;

}

bool GPodDevice::CopyToStorage(const CopyJob &job, QString &error_text) {

  Q_ASSERT(db_);

  Itdb_Track *track = AddTrackToITunesDb(job.metadata_);

  if (job.albumcover_) {
    bool result = false;
    if (!job.cover_image_.isNull()) {
#ifdef Q_OS_LINUX
      QString temp_path = StandardPaths::WritableLocation(StandardPaths::StandardLocation::CacheLocation) + u"/organize"_s;
#else
      QString temp_path = StandardPaths::WritableLocation(StandardPaths::StandardLocation::TempLocation);
#endif
      if (!QDir(temp_path).exists()) QDir().mkpath(temp_path);
      SharedPtr<TemporaryFile> cover_file = make_shared<TemporaryFile>(temp_path + u"/track-albumcover-XXXXXX.jpg"_s);
      if (!cover_file->filename().isEmpty()) {
        const QImage &image = job.cover_image_;
        if (image.save(cover_file->filename(), "JPG")) {
          const QByteArray filename = QFile::encodeName(cover_file->filename());
          result = itdb_track_set_thumbnails(track, filename.constData());
          if (result) {
            cover_files_ << cover_file;
            track->has_artwork = 1;
          }
        }
        else {
          qLog(Error) << "Failed to save" << cover_file->filename();
        }
      }
      else {
        qLog(Error) << "Failed to obtain temporary file";
      }
    }
    else if (!job.cover_source_.isEmpty()) {
      const QByteArray filename = QFile::encodeName(job.cover_source_);
      result = itdb_track_set_thumbnails(track, filename.constData());
      if (result) track->has_artwork = 1;
    }
    else {
      result = true;
    }
    if (!result) {
      qLog(Error) << "Failed to set album cover image";
    }
  }

  // Copy the file
  GError *error = nullptr;
  itdb_cp_track_to_ipod(track, QDir::toNativeSeparators(job.source_).toLocal8Bit().constData(), &error);
  if (error) {
    error_text = tr("Could not copy %1 to %2: %3").arg(job.metadata_.url().toLocalFile(), url_.path(), QString::fromUtf8(error->message));
    g_error_free(error);
    qLog(Error) << error_text;
    Q_EMIT Error(error_text);

    // Need to remove the track from the db again
    itdb_track_remove(track);
    return false;
  }

  // Put the track in the playlist, if one is specified
  if (!job.playlist_.isEmpty()) {
    // Does the playlist already exist?
    QByteArray playlist_name = job.playlist_.toUtf8();
    Itdb_Playlist *playlist = itdb_playlist_by_name(db_, playlist_name.data());
    if (!playlist) {
      // Create the playlist
      playlist = itdb_playlist_new(playlist_name.data(), false);
      itdb_playlist_add(db_, playlist, -1);
    }
    // Playlist should exist so add the track to the playlist
    itdb_playlist_add_track(playlist, track, -1);
  }

  AddTrackToModel(track, url_.path());

  // Remove the original if it was requested
  if (job.remove_original_) {
    QFile::remove(job.source_);
  }

  return true;

}

bool GPodDevice::WriteDatabase(QString &error_text) {

  // Write the itunes database
  GError *error = nullptr;
  const bool success = itdb_write(db_, &error);
  cover_files_.clear();
  if (!success) {
    if (error) {
      error_text = tr("Writing database failed: %1").arg(QString::fromUtf8(error->message));
      g_error_free(error);
    }
    else {
      error_text = tr("Writing database failed.");
    }
    Q_EMIT Error(error_text);
  }

  return success;

}

void GPodDevice::Finish(const bool success) {

  // Update the collection model
  if (success) {
    if (!songs_to_add_.isEmpty()) collection_backend_->AddOrUpdateSongs(songs_to_add_);
    if (!songs_to_remove_.isEmpty()) collection_backend_->DeleteSongs(songs_to_remove_);
  }

  // This is done in the organize thread so close the unique DB connection.
  collection_backend_->Close();

  songs_to_add_.clear();
  songs_to_remove_.clear();
  cover_files_.clear();

  db_busy_.unlock();

}

bool GPodDevice::FinishCopy(bool success, QString &error_text) {

  if (success) success = WriteDatabase(error_text);
  Finish(success);
  return ConnectedDevice::FinishCopy(success, error_text);

}

void GPodDevice::StartDelete() { Start(); }

bool GPodDevice::RemoveTrackFromITunesDb(const QString &path, const QString &relative_to) {

  QString ipod_filename = path;
  if (!relative_to.isEmpty() && path.startsWith(relative_to)) {
    ipod_filename.remove(0, relative_to.length() + (relative_to.endsWith(u'/') ? -1 : 0));
  }

  ipod_filename.replace(u'/', u':');

  // Find the track in the itdb, identify it by its filename
  Itdb_Track *track = nullptr;
  for (GList *tracks = db_->tracks; tracks != nullptr; tracks = tracks->next) {
    Itdb_Track *t = static_cast<Itdb_Track*>(tracks->data);

    if (QString::fromUtf8(t->ipod_path) == ipod_filename) {
      track = t;
      break;
    }
  }

  if (track == nullptr) {
    qLog(Warning) << "Couldn't find song" << path << "in iTunesDB";
    return false;
  }

  // Remove the track from all playlists
  for (GList *playlists = db_->playlists; playlists != nullptr; playlists = playlists->next) {
    Itdb_Playlist *playlist = static_cast<Itdb_Playlist*>(playlists->data);

    if (itdb_playlist_contains_track(playlist, track)) {
      itdb_playlist_remove_track(playlist, track);
    }
  }

  // Remove the track from the database, this frees the struct too
  itdb_track_remove(track);

  return true;

}

bool GPodDevice::DeleteFromStorage(const DeleteJob &job) {

  Q_ASSERT(db_);

  if (!RemoveTrackFromITunesDb(job.metadata_.url().toLocalFile(), url_.path())) {
    return false;
  }

  // Remove the file
  if (!QFile::remove(job.metadata_.url().toLocalFile())) {
    return false;
  }

  // Remove it from our collection model
  songs_to_remove_ << job.metadata_;

  return true;

}

bool GPodDevice::FinishDelete(bool success, QString &error_text) {

  if (success) success = WriteDatabase(error_text);
  Finish(success);
  return ConnectedDevice::FinishDelete(success, error_text);

}

bool GPodDevice::GetSupportedFiletypes(QList<Song::FileType> *ret) {
  *ret << Song::FileType::MP4;
  *ret << Song::FileType::MPEG;
  *ret << Song::FileType::ALAC;
  return true;
}
