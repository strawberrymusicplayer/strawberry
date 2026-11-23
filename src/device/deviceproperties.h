/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef DEVICEPROPERTIES_H
#define DEVICEPROPERTIES_H

#include "config.h"

#include <memory>

#include <QObject>
#include <QPointer>
#include <QDialog>
#include <QFuture>
#include <QAbstractItemModel>
#include <QList>
#include <QString>

#include "includes/shared_ptr.h"
#include "core/song.h"

class QWidget;

class ConnectedDevice;
class DeviceManager;
class Ui_DeviceProperties;

class DeviceProperties : public QDialog {
  Q_OBJECT

 public:
  explicit DeviceProperties(QWidget *parent = nullptr);
  ~DeviceProperties() override;

  void Init(const SharedPtr<DeviceManager> device_manager);
  void ShowDevice(const QModelIndex &idx);

 public Q_SLOTS:
  void accept() override;

 private:
  void UpdateHardwareInfo();
  void AddHardwareInfo(const int row, const QString &key, const QString &value);
  void LoadTranscodeMode();
  void UpdateFormats();

 private Q_SLOTS:
  void ModelChanged();
  void OpenDevice();
  void UpdateFormatsFinished();

 private:
  Ui_DeviceProperties *ui_;

  SharedPtr<DeviceManager> device_manager_;
  QPersistentModelIndex index_;

  bool updating_formats_;
  QPointer<ConnectedDevice> formats_device_;  // The device the formats are loaded for, the formats are not changed by a scan or copy.
  QList<Song::FileType> supported_formats_;
};

#endif  // DEVICEPROPERTIES_H
