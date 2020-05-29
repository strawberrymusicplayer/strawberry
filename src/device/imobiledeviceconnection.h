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

#ifndef IMOBILEDEVICECONNECTION_H
#define IMOBILEDEVICECONNECTION_H

#include "config.h"

#include <libimobiledevice/afc.h>
#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>

#include <gpod/itdb.h>

#include <QDir>
#include <QVariant>
#include <QString>
#include <QStringList>

#include "core/song.h"

class iMobileDeviceConnection {
public:
  explicit iMobileDeviceConnection(const QString uuid);
  ~iMobileDeviceConnection();

  afc_client_t afc() { return afc_; }

  QVariant GetProperty(const QString &property, const QString &domain = QString());
  QStringList ReadDirectory(const QString &path, QDir::Filters filters = QDir::NoFilter);
  bool MkDir(const QString &path);

  QString GetFileInfo(const QString &path, const QString &key);
  bool Exists(const QString &path);

  QString GetUnusedFilename(Itdb_iTunesDB *itdb, const Song &metadata);

  bool is_valid() { return device_ && afc_; }

private:
  Q_DISABLE_COPY(iMobileDeviceConnection)

  idevice_t device_;
  afc_client_t afc_;
};

#endif // IMOBILEDEVICECONNECTION_H
