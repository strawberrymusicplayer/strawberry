/*
 * Strawberry Music Player
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

#ifndef QOBUZSTREAMURLREQUEST_H
#define QOBUZSTREAMURLREQUEST_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "core/shared_ptr.h"
#include "core/song.h"
#include "qobuzbaserequest.h"

class QNetworkReply;
class NetworkAccessManager;
class QobuzService;

class QobuzStreamURLRequest : public QobuzBaseRequest {
  Q_OBJECT

 public:
  explicit QobuzStreamURLRequest(QobuzService *service, SharedPtr<NetworkAccessManager> network, const QUrl &media_url, const uint id, QObject *parent = nullptr);
  ~QobuzStreamURLRequest();

  void GetStreamURL();
  void Process();
  void NeedLogin() { need_login_ = true; }
  void Cancel();

  QUrl media_url() { return media_url_; }
  int song_id() { return song_id_; }
  bool need_login() { return need_login_; }

 signals:
  void TryLogin();
  void StreamURLFailure(const uint id, const QUrl &media_url, const QString &error);
  void StreamURLSuccess(const uint id, const QUrl &media_url, const QUrl &stream_url, const Song::FileType filetype, const int samplerate, const int bit_depth, const qint64 duration);

 private slots:
  void StreamURLReceived();

 public slots:
  void LoginComplete(const bool success, const QString &error = QString());

 private:
  void Error(const QString &error, const QVariant &debug = QVariant());

  QobuzService *service_;
  QNetworkReply *reply_;
  QUrl media_url_;
  uint id_;
  int song_id_;
  int tries_;
  bool need_login_;
  QStringList errors_;
};

#endif  // QOBUZSTREAMURLREQUEST_H
