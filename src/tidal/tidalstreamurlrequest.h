/*
 * Strawberry Music Player
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef TIDALSTREAMURLREQUEST_H
#define TIDALSTREAMURLREQUEST_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "core/song.h"
#include "tidalservice.h"
#include "tidalbaserequest.h"
#include "settings/tidalsettingspage.h"

class QNetworkReply;
class NetworkAccessManager;

class TidalStreamURLRequest : public TidalBaseRequest {
  Q_OBJECT

 public:
  explicit TidalStreamURLRequest(TidalService *service, NetworkAccessManager *network, const QUrl &original_url, QObject *parent);
  ~TidalStreamURLRequest() override;

  void GetStreamURL();
  void Process();
  void Cancel();

  bool oauth() { return service_->oauth(); }
  TidalSettingsPage::StreamUrlMethod stream_url_method() { return service_->stream_url_method(); }
  QUrl original_url() { return original_url_; }
  int song_id() { return song_id_; }

  void set_need_login() override { need_login_ = true; }
  bool need_login() { return need_login_; }

 signals:
  void TryLogin();
  void StreamURLFinished(QUrl original_url, QUrl stream_url, Song::FileType filetype, int samplerate, int bit_depth, qint64 duration, QString error = QString());

 private slots:
  void StreamURLReceived();

 public slots:
  void LoginComplete(const bool success, const QString &error = QString());

 private:
  void Error(const QString &error, const QVariant &debug = QVariant()) override;

  TidalService *service_;
  QNetworkReply *reply_;
  QUrl original_url_;
  int song_id_;
  int tries_;
  bool need_login_;
  QStringList errors_;

};

#endif  // TIDALSTREAMURLREQUEST_H
