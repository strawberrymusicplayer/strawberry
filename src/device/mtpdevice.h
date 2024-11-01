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

#ifndef MTPDEVICE_H
#define MTPDEVICE_H

#include "config.h"

#include <QObject>
#include <QMutex>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "includes/scoped_ptr.h"
#include "includes/shared_ptr.h"
#include "core/song.h"
#include "connecteddevice.h"

class QThread;
class DeviceLister;
class DeviceManager;
class TaskManager;
class Database;
class TagReaderClient;
class AlbumCoverLoader;
class MtpLoader;
class MtpConnection;
struct LIBMTP_mtpdevice_struct;

class MtpDevice : public ConnectedDevice {
  Q_OBJECT

 public:
  Q_INVOKABLE MtpDevice(const QUrl &url,
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

  ~MtpDevice() override;

  static QStringList url_schemes() { return QStringList() << QStringLiteral("mtp"); }

  bool Init() override;
  void ConnectAsync() override;
  void Close() override;
  bool IsLoading() override { return loader_; }

  bool GetSupportedFiletypes(QList<Song::FileType> *ret) override;
  int GetFreeSpace();
  int GetCapacity();

  bool StartCopy(QList<Song::FileType> *supported_types) override;
  bool CopyToStorage(const CopyJob &job, QString &error_text) override;
  bool FinishCopy(const bool success, QString &error_text) override;

  void StartDelete() override;
  bool DeleteFromStorage(const DeleteJob &job) override;
  bool FinishDelete(const bool success, QString &error_text) override;

 private Q_SLOTS:
  void LoadFinished(bool success, MtpConnection *connection);
  void LoaderError(const QString &message);

 private:
  bool GetSupportedFiletypes(QList<Song::FileType> *ret, LIBMTP_mtpdevice_struct *device);
  int GetFreeSpace(LIBMTP_mtpdevice_struct *device);
  int GetCapacity(LIBMTP_mtpdevice_struct *device);

 private:
  static bool sInitializedLibMTP;

  const SharedPtr<TaskManager> task_manager_;

  MtpLoader *loader_;
  QThread *loader_thread_;
  bool closing_;

  QMutex db_busy_;
  SongList songs_to_add_;
  SongList songs_to_remove_;

  ScopedPtr<MtpConnection> connection_;
};

#endif  // MTPDEVICE_H
