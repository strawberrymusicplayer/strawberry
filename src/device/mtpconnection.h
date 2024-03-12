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

#ifndef MTPCONNECTION_H
#define MTPCONNECTION_H

#include "config.h"

#include <memory>

#include <libmtp.h>

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QUrl>

#include "core/song.h"

using std::enable_shared_from_this;

class MtpConnection : public QObject, public enable_shared_from_this<MtpConnection> {
  Q_OBJECT

 public:
  explicit MtpConnection(const QUrl &url, QObject *parent = nullptr);
  ~MtpConnection() override;

  bool is_valid() const { return device_; }
  QString error_text() const { return error_text_; }
  LIBMTP_mtpdevice_t *device() const { return device_; }
  bool GetSupportedFiletypes(QList<Song::FileType> *ret);

  static QString ErrorString(const LIBMTP_error_number_t error_number);

 private:
  Q_DISABLE_COPY(MtpConnection)

  LIBMTP_mtpdevice_t *device_;
  QString error_text_;
};

#endif  // MTPCONNECTION_H
