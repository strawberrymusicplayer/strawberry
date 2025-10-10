/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef CDDADEVICE_H
#define CDDADEVICE_H

#include "config.h"

#include <cstddef>

#include <cdio/types.h>
#include <cdio/cdio.h>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "core/musicstorage.h"
#include "cddasongloader.h"
#include "connecteddevice.h"

class QTimer;

class DeviceLister;
class DeviceManager;
class TaskManager;
class Database;
class TagReaderClient;
class AlbumCoverLoader;

class CDDADevice : public ConnectedDevice {
  Q_OBJECT

 public:
  Q_INVOKABLE explicit CDDADevice(const QUrl &url,
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

  ~CDDADevice();

  bool Init() override;
  bool CopyToStorage(const CopyJob&, QString&) override { return false; }
  bool DeleteFromStorage(const MusicStorage::DeleteJob&) override { return false; }

  static QStringList url_schemes() { return QStringList() << QStringLiteral("cdda"); }

  void LoadSongs();
  void WatchForDiscChanges(const bool watch);

 Q_SIGNALS:
  void SongsDiscovered(const SongList &songs);

 private Q_SLOTS:
  void CheckDiscChanged();
  void SongsLoaded(const SongList &songs = SongList());
  void SongLoadingFinished();

 private:
  CDDASongLoader cdda_song_loader_;
  CdIo_t *cdio_;
  QTimer *timer_disc_changed_;
};

#endif  // CDDADEVICE_H
