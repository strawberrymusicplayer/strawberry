/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QVariant>
#include <QString>
#include <QUrl>
#include <QSharedPointer>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "tidalbaserequest.h"

class QNetworkReply;
class NetworkAccessManager;
class TidalService;

class TidalStreamURLRequest : public TidalBaseRequest {
  Q_OBJECT

 public:
  explicit TidalStreamURLRequest(TidalService *service, const SharedPtr<NetworkAccessManager> network, const QUrl &media_url, const uint id, QObject *parent = nullptr);
  ~TidalStreamURLRequest() override;

  void GetStreamURL();
  void Process();
  void Cancel();

  bool oauth() const;
  TidalSettings::StreamUrlMethod stream_url_method() const;
  QUrl media_url() const;
  int song_id() const;

 Q_SIGNALS:
  void StreamURLFailure(const uint id, const QUrl &media_url, const QString &error);
  void StreamURLSuccess(const uint id, const QUrl &media_url, const QUrl &stream_url, const Song::FileType filetype, const int samplerate = -1, const int bit_depth = -1, const qint64 duration = -1);

 private Q_SLOTS:
  void StreamURLReceived();

private:
  static QList<QUrl> ParseUrls(const QJsonObject &json_object);

 private:
  TidalService *service_;
  QNetworkReply *reply_;
  QUrl media_url_;
  uint id_;
  int song_id_;
};

using TidalStreamURLRequestPtr = QSharedPointer<TidalStreamURLRequest>;

#endif  // TIDALSTREAMURLREQUEST_H
