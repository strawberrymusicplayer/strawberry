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
#include <cstdint>
#include <cstdlib>

#include <QThread>
#include <QMutex>
#include <QFile>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/musicstorage.h"
#include "collection/collectionmodel.h"
#include "collection/collectionbackend.h"
#include "connecteddevice.h"
#include "mtpdevice.h"
#include "mtploader.h"
#include "mtpconnection.h"

using namespace Qt::Literals::StringLiterals;

class TaskManager;
class Database;
class AlbumCoverLoader;
class DeviceLister;
class DeviceManager;

bool MtpDevice::sInitializedLibMTP = false;

MtpDevice::MtpDevice(const QUrl &url,
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
      closing_(false) {

  if (!sInitializedLibMTP) {
    LIBMTP_Init();
    sInitializedLibMTP = true;
  }

}

MtpDevice::~MtpDevice() {

  if (loader_) {
    loader_thread_->exit();
    loader_->deleteLater();
    loader_ = nullptr;
    db_busy_.unlock();
    loader_thread_->deleteLater();
  }

}

bool MtpDevice::Init() {

  InitBackendDirectory(u"/"_s, first_time_, false);
  collection_model_->Init();

  loader_ = new MtpLoader(url_, task_manager_, collection_backend_);
  loader_thread_ = new QThread();
  loader_->moveToThread(loader_thread_);

  QObject::connect(loader_, &MtpLoader::Error, this, &MtpDevice::LoaderError);
  QObject::connect(loader_, &MtpLoader::TaskStarted, this, &MtpDevice::TaskStarted);
  QObject::connect(loader_, &MtpLoader::LoadFinished, this, &MtpDevice::LoadFinished);
  QObject::connect(loader_thread_, &QThread::started, loader_, &MtpLoader::LoadDatabase);

  return true;

}

void MtpDevice::ConnectAsync() {

  db_busy_.lock();
  loader_thread_->start();

}

void MtpDevice::Close() {

  closing_ = true;

  if (IsLoading()) {
    loader_->Abort();
  }
  else {
    ConnectedDevice::Close();
  }

}

void MtpDevice::LoadFinished(const bool success, MtpConnection *connection) {

  connection_.reset(connection);

  loader_thread_->exit();
  loader_->deleteLater();
  loader_ = nullptr;
  db_busy_.unlock();
  if (closing_) {
    ConnectedDevice::Close();
  }
  else {
    Q_EMIT DeviceConnectFinished(unique_id_, success);
  }

}

void MtpDevice::LoaderError(const QString &message) {
  Q_EMIT Error(message);
}

bool MtpDevice::StartCopy(QList<Song::FileType> *supported_types) {

  // Ensure only one "organize files" can be active at any one time
  db_busy_.lock();

  if (!connection_ || !connection_->is_valid()) return false;

  // Did the caller want a list of supported types?
  if (supported_types) {
    if (!GetSupportedFiletypes(supported_types, connection_->device())) {
      QString error_text;
      FinishCopy(false, error_text);
      return false;
    }
  }

  return true;

}

static int ProgressCallback(uint64_t const sent, uint64_t const total, void const *const data) {

  const MusicStorage::CopyJob *job = reinterpret_cast<const MusicStorage::CopyJob*>(data);
  job->progress_(static_cast<float>(sent) / static_cast<float>(total));

  return 0;

}

bool MtpDevice::CopyToStorage(const CopyJob &job, QString &error_text) {

  if (!connection_ || !connection_->is_valid()) return false;

  // Convert metadata
  LIBMTP_track_t track;
  job.metadata_.ToMTP(&track);

  // Send the file
  int ret = LIBMTP_Send_Track_From_File(connection_->device(), job.source_.toUtf8().constData(), &track, ProgressCallback, &job);
  if (ret != 0) {
    LIBMTP_error_struct *error = LIBMTP_Get_Errorstack(connection_->device());
    if (error) {
      error_text = QString::fromUtf8(error->error_text);
      qLog(Error) << error_text;
      LIBMTP_Clear_Errorstack(connection_->device());
    }
    return false;
  }

  // Add it to our CollectionModel
  Song metadata_on_device(Song::Source::Device);
  metadata_on_device.InitFromMTP(&track, url_.host());
  metadata_on_device.set_directory_id(1);
  metadata_on_device.set_artist(metadata_on_device.effective_albumartist());
  metadata_on_device.set_albumartist(""_L1);
  songs_to_add_ << metadata_on_device;

  // Remove the original if requested
  if (job.remove_original_) {
    if (!QFile::remove(job.source_)) return false;
  }

  return true;

}

bool MtpDevice::FinishCopy(const bool success, QString &error_text) {

  if (success) {
    if (!songs_to_add_.isEmpty()) collection_backend_->AddOrUpdateSongs(songs_to_add_);
    if (!songs_to_remove_.isEmpty()) collection_backend_->DeleteSongs(songs_to_remove_);
  }

  songs_to_add_.clear();
  songs_to_remove_.clear();

  // This is done in the organize thread so close the unique DB connection.
  collection_backend_->Close();

  db_busy_.unlock();

  return ConnectedDevice::FinishCopy(success, error_text);

}

void MtpDevice::StartDelete() { StartCopy(nullptr); }

bool MtpDevice::DeleteFromStorage(const DeleteJob &job) {

  if (!connection_ || !connection_->is_valid()) return false;

  // Extract the ID from the song's URL
  QString filename = job.metadata_.url().path();
  filename.remove(u'/');

  bool ok = false;
  uint32_t id = filename.toUInt(&ok);
  if (!ok) return false;

  // Remove the file
  int ret = LIBMTP_Delete_Object(connection_->device(), id);
  if (ret != 0) return false;

  // Remove it from our collection model
  songs_to_remove_ << job.metadata_;

  return true;

}

bool MtpDevice::FinishDelete(const bool success, QString &error_text) { return FinishCopy(success, error_text); }

bool MtpDevice::GetSupportedFiletypes(QList<Song::FileType> *ret) {

  QMutexLocker l(&db_busy_);
  MtpConnection connection(url_);

  if (!connection.is_valid()) {
    qLog(Warning) << "Error connecting to MTP device, couldn't get list of supported filetypes";
    return false;
  }

  return GetSupportedFiletypes(ret, connection.device());

}

bool MtpDevice::GetSupportedFiletypes(QList<Song::FileType> *ret, LIBMTP_mtpdevice_t *device) {

  uint16_t *list = nullptr;
  uint16_t length = 0;

  if (LIBMTP_Get_Supported_Filetypes(device, &list, &length) != 0 || !list || !length) {
    return false;
  }

  for (int i = 0; i < length; ++i) {
    switch (static_cast<LIBMTP_filetype_t>(list[i])) {
      case LIBMTP_FILETYPE_WAV:  *ret << Song::FileType::WAV; break;
      case LIBMTP_FILETYPE_MP2:
      case LIBMTP_FILETYPE_MP3:  *ret << Song::FileType::MPEG; break;
      case LIBMTP_FILETYPE_WMA:  *ret << Song::FileType::ASF; break;
      case LIBMTP_FILETYPE_MP4:
      case LIBMTP_FILETYPE_M4A:
      case LIBMTP_FILETYPE_AAC:  *ret << Song::FileType::MP4; break;
      case LIBMTP_FILETYPE_FLAC:
        *ret << Song::FileType::FLAC;
        *ret << Song::FileType::OggFlac;
        break;
      case LIBMTP_FILETYPE_OGG:
        *ret << Song::FileType::OggVorbis;
        *ret << Song::FileType::OggSpeex;
        *ret << Song::FileType::OggFlac;
        break;
      default:
        qLog(Error) << "Unknown MTP file format" << LIBMTP_Get_Filetype_Description(static_cast<LIBMTP_filetype_t>(list[i]));
        break;
    }
  }

  free(list);
  return true;

}
