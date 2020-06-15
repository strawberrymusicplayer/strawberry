/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef CONNECTEDDEVICE_H
#define CONNECTEDDEVICE_H

#include "config.h"

#include <memory>

#include <QObject>
#include <QString>
#include <QUrl>

#include "core/musicstorage.h"
#include "core/song.h"

class Application;
class CollectionBackend;
class CollectionModel;
class DeviceLister;
class DeviceManager;

class ConnectedDevice : public QObject, public virtual MusicStorage, public std::enable_shared_from_this<ConnectedDevice> {
  Q_OBJECT

 public:
  explicit ConnectedDevice(const QUrl &url, DeviceLister *lister, const QString &unique_id, DeviceManager *manager, Application *app, int database_id, bool first_time);
  ~ConnectedDevice() override;

  virtual bool Init() = 0;
  virtual bool IsLoading() { return false; }
  virtual void NewConnection() {}
  virtual void ConnectAsync();
  // For some devices (e.g. CD devices) we don't have callbacks to be notified when something change:
  // we can call this method to refresh device's state
  virtual void Refresh() {}

  TranscodeMode GetTranscodeMode() const override;
  Song::FileType GetTranscodeFormat() const override;

  DeviceLister *lister() const { return lister_; }
  QString unique_id() const { return unique_id_; }
  CollectionModel *model() const { return model_; }
  QUrl url() const { return url_; }
  int song_count() const { return song_count_; }

  void FinishCopy(bool success) override;
  void FinishDelete(bool success) override;

  void Eject() override;
  virtual void Close();

 public slots:
  void CloseFinished();

 signals:
  void TaskStarted(int id);
  void SongCountUpdated(int count);
  void ConnectFinished(const QString& id, bool success);
  void CloseFinished(const QString& id);

 protected:
  void InitBackendDirectory(const QString &mount_point, const bool first_time, const bool rewrite_path = true);

 protected:
  Application *app_;

  QUrl url_;
  bool first_time_;
  DeviceLister *lister_;
  QString unique_id_;
  int database_id_;
  DeviceManager *manager_;

  CollectionBackend *backend_;
  CollectionModel *model_;

  int song_count_;

 private slots:
  void BackendTotalSongCountUpdated(int count);
};

#endif  // CONNECTEDDEVICE_H

