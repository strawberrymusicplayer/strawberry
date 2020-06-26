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

#ifndef AFCDEVICE_H
#define AFCDEVICE_H

#include "config.h"

#include <memory>

#include <gpod/itdb.h>

#include <QObject>
#include <QMutex>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "gpoddevice.h"

class AfcTransfer;
class iMobileDeviceConnection;

class AfcDevice : public GPodDevice {
  Q_OBJECT

public:
  Q_INVOKABLE AfcDevice(const QUrl &url, DeviceLister *lister, const QString &unique_id, DeviceManager *manager, Application *app, const int database_id, const bool first_time);
  ~AfcDevice() override;

  bool Init() override;

  static QStringList url_schemes() { return QStringList() << "afc"; }

  bool StartCopy(QList<Song::FileType> *supported_types) override;
  bool CopyToStorage(const CopyJob &job) override;
  void FinishCopy(const bool success) override;

  bool DeleteFromStorage(const DeleteJob &job) override;

 protected:
  void FinaliseDatabase() override;

 private slots:
  void CopyFinished(bool success);

 private:
  void RemoveRecursive(const QString &path);

 private:
  AfcTransfer *transfer_;
  std::shared_ptr<iMobileDeviceConnection> connection_;

  QString local_path_;
};

#endif // AFCDEVICE_H
