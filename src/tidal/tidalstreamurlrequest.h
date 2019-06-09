/*
 * Strawberry Music Player
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

#ifndef TIDALSTREAMURLREQUEST_H
#define TIDALSTREAMURLREQUEST_H

#include "config.h"

#include <QString>
#include <QUrl>

#include "core/song.h"
#include "tidalbaserequest.h"
#include "settings/tidalsettingspage.h"

class QNetworkReply;
class NetworkAccessManager;
class TidalService;

class TidalStreamURLRequest : public TidalBaseRequest {
  Q_OBJECT

 public:
  TidalStreamURLRequest(TidalService *service, NetworkAccessManager *network, const QUrl &original_url, QObject *parent);
  ~TidalStreamURLRequest();

  void GetStreamURL();
  void Process();
  void NeedLogin() { need_login_ = true; }
  void Cancel();

  const bool oauth() { return service_->oauth(); }
  TidalSettingsPage::StreamUrlMethod stream_url_method() { return service_->stream_url_method(); }
  QUrl original_url() { return original_url_; }
  int song_id() { return song_id_; }
  bool need_login() { return need_login_; }

 signals:
  void TryLogin();
  void StreamURLFinished(const QUrl original_url, const QUrl stream_url, const Song::FileType filetype, QString error = QString());

 private slots:
  void LoginComplete(bool success, QString error = QString());
  void StreamURLReceived();

 private:
  TidalService *service_;
  QNetworkReply *reply_;
  QUrl original_url_;
  int song_id_;
  int tries_;
  bool need_login_;

};

#endif  // TIDALSTREAMURLREQUEST_H
