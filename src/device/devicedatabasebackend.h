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

#ifndef DEVICEDATABASEBACKEND_H
#define DEVICEDATABASEBACKEND_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QSet>
#include <QString>
#include <QVector>

#include "core/song.h"
#include "core/musicstorage.h"

class Database;

class DeviceDatabaseBackend : public QObject {
  Q_OBJECT

 public:
  Q_INVOKABLE DeviceDatabaseBackend(QObject *parent = nullptr);
  ~DeviceDatabaseBackend();

  struct Device {
    Device() : id_(-1) {}

    int id_;
    QString unique_id_;
    QString friendly_name_;
    quint64 size_;
    QString icon_name_;

    MusicStorage::TranscodeMode transcode_mode_;
    Song::FileType transcode_format_;
  };
  typedef QList<Device> DeviceList;

  static const int kDeviceSchemaVersion;

  void Init(Database *db);
  void Close();
  void ExitAsync();

  Database *db() const { return db_; }

  DeviceList GetAllDevices();
  int AddDevice(const Device& device);
  void RemoveDevice(int id);

  void SetDeviceOptions(int id, const QString &friendly_name, const QString &icon_name, MusicStorage::TranscodeMode mode, Song::FileType format);

 private slots:
  void Exit();

 signals:
  void ExitFinished();

 private:
  Database *db_;
  QThread *original_thread_;

};

#endif  // DEVICEDATABASEBACKEND_H
