/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef FILESYSTEMDEVICE_H
#define FILESYSTEMDEVICE_H

#include "config.h"

#include <stdbool.h>

#include <QObject>
#include <QThread>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "core/filesystemmusicstorage.h"
#include "connecteddevice.h"

class Application;
class CollectionWatcher;
class DeviceLister;
class DeviceManager;

class FilesystemDevice : public ConnectedDevice, public virtual FilesystemMusicStorage {
  Q_OBJECT

public:
  Q_INVOKABLE FilesystemDevice(
      const QUrl &url, DeviceLister *lister,
      const QString &unique_id, DeviceManager *manager,
      Application *app,
      int database_id, bool first_time);
  ~FilesystemDevice();

  bool Init();
  void CloseAsync();

  static QStringList url_schemes() { return QStringList() << "file"; }

 private slots:
  void Close();
  void ExitFinished();

private:
  CollectionWatcher *watcher_;
  QThread *watcher_thread_;
  QList<QObject*> wait_for_exit_;
};

#endif // FILESYSTEMDEVICE_H
