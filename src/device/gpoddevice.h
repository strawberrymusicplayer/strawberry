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

#ifndef GPODDEVICE_H
#define GPODDEVICE_H

#include "config.h"

#include <gpod/itdb.h>

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "core/musicstorage.h"
#include "core/temporaryfile.h"
#include "connecteddevice.h"
#include "gpodloader.h"

class QThread;
class DeviceLister;
class DeviceManager;
class TaskManager;
class Database;
class TagReaderClient;
class AlbumCoverLoader;

class GPodDevice : public ConnectedDevice, public virtual MusicStorage {
  Q_OBJECT

 public:
  Q_INVOKABLE GPodDevice(const QUrl &url,
                         DeviceLister *lister,
                         const QString &unique_id,
                         DeviceManager *device_manager,
                         const SharedPtr<TaskManager> task_manager,
                         const SharedPtr<Database> database,
                         const SharedPtr<TagReaderClient> tagreader_client,
                         const SharedPtr<AlbumCoverLoader> albumcover_loader,
                         const int database_id,
                         const bool first_time,
                         QObject *parent = nullptr);

  ~GPodDevice() override;

  bool Init() override;
  void ConnectAsync() override;
  void Close() override;
  bool IsLoading() override { return loader_; }
  QObject *Loader() { return loader_; }

  static QStringList url_schemes() { return QStringList() << QStringLiteral("ipod"); }

  bool GetSupportedFiletypes(QList<Song::FileType> *ret) override;

  bool StartCopy(QList<Song::FileType> *supported_filetypes) override;
  bool CopyToStorage(const CopyJob &job, QString &error_text) override;
  bool FinishCopy(bool success, QString &error_text) override;

  void StartDelete() override;
  bool DeleteFromStorage(const DeleteJob &job) override;
  bool FinishDelete(bool success, QString &error_text) override;

 protected Q_SLOTS:
  void LoadFinished(Itdb_iTunesDB *db, const bool success);
  void LoaderError(const QString &message);

 protected:
  Itdb_Track *AddTrackToITunesDb(const Song &metadata);
  void AddTrackToModel(Itdb_Track *track, const QString &prefix);
  bool RemoveTrackFromITunesDb(const QString &path, const QString &relative_to = QString());

 private:
  void Start();
  void Finish(const bool success);
  bool WriteDatabase(QString &error_text);

 protected:
  const SharedPtr <TaskManager> task_manager_;
  GPodLoader *loader_;
  QThread *loader_thread_;

  QWaitCondition db_wait_cond_;
  QMutex db_mutex_;
  Itdb_iTunesDB *db_;
  bool closing_;

  QMutex db_busy_;
  SongList songs_to_add_;
  SongList songs_to_remove_;
  QList<SharedPtr<TemporaryFile>> cover_files_;
};

#endif  // GPODDEVICE_H
