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

#ifndef MTPDEVICE_H
#define MTPDEVICE_H

#include "config.h"

#include <memory>
#include <stdbool.h>

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "core/song.h"
#include "connecteddevice.h"

class Application;
class DeviceLister;
class DeviceManager;
class DeviceConnection;
class MtpConnection;
class MtpLoader;
struct LIBMTP_mtpdevice_struct;

class MtpDevice : public ConnectedDevice {
  Q_OBJECT

 public:
  Q_INVOKABLE MtpDevice(const QUrl &url, DeviceLister *lister, const QString &unique_id, DeviceManager *manager, Application *app, int database_id, bool first_time);
  ~MtpDevice();

  static QStringList url_schemes() { return QStringList() << "mtp" << "gphoto2"; }

  bool Init();
  void NewConnection();
  void ConnectAsync();

  bool GetSupportedFiletypes(QList<Song::FileType>* ret);
  int GetFreeSpace();
  int GetCapacity();

  bool StartCopy(QList<Song::FileType>* supported_types);
  bool CopyToStorage(const CopyJob& job);
  void FinishCopy(bool success);

  void StartDelete();
  bool DeleteFromStorage(const DeleteJob& job);
  void FinishDelete(bool success);

  MtpConnection *connection() { return connection_.get(); }

 private slots:
  void LoadFinished(bool success);
  void LoaderError(const QString& message);

 private:
  bool GetSupportedFiletypes(QList<Song::FileType> *ret, LIBMTP_mtpdevice_struct *device);
  int GetFreeSpace(LIBMTP_mtpdevice_struct* device);
  int GetCapacity(LIBMTP_mtpdevice_struct* device);

 private:
  static bool sInitialisedLibMTP;

  QThread *loader_thread_;
  MtpLoader *loader_;

  QMutex db_busy_;
  SongList songs_to_add_;
  SongList songs_to_remove_;

  std::shared_ptr<MtpConnection> connection_;

};

#endif  // MTPDEVICE_H
