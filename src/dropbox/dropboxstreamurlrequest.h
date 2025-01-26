/*
 * Strawberry Music Player
 * Copyright 2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef DROPBOXSTREAMURLREQUEST_H
#define DROPBOXSTREAMURLREQUEST_H

#include "config.h"

#include <QVariant>
#include <QString>
#include <QUrl>
#include <QSharedPointer>

#include "includes/shared_ptr.h"
#include "dropboxservice.h"
#include "dropboxbaserequest.h"

class QNetworkReply;
class NetworkAccessManager;

class DropboxStreamURLRequest : public DropboxBaseRequest {
  Q_OBJECT

 public:
  explicit DropboxStreamURLRequest(const SharedPtr<NetworkAccessManager> network, DropboxService *service, const uint id, const QUrl &media_url, QObject *parent = nullptr);
  ~DropboxStreamURLRequest() override;

  void Process();
  void Cancel();

 Q_SIGNALS:
  void StreamURLRequestFinished(const uint id, const QUrl &media_url, const bool success, const QUrl &stream_url, const QString &error = QString());

 private Q_SLOTS:
  void StreamURLReceived();

 private:
  void GetStreamURL();
  void Error(const QString &error_message, const QVariant &debug_output = QVariant()) override;
  void Finish();

 private:
  const SharedPtr<NetworkAccessManager> network_;
  DropboxService *service_;
  uint id_;
  QUrl media_url_;
  QUrl stream_url_;
  QNetworkReply *reply_;
  bool success_;
  QString error_;
};

using DropboxStreamURLRequestPtr = QSharedPointer<DropboxStreamURLRequest>;

#endif  // DROPBOXSTREAMURLREQUEST_H
