/*
 * Strawberry Music Player
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
 * Copyright 2020-2021, Pascal Below <spezifisch@below.fr>
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

#ifndef SUBSONICSCROBBLEREQUEST_H
#define SUBSONICSCROBBLEREQUEST_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QDateTime>
#include <QQueue>
#include <QVariant>
#include <QString>
#include <QStringList>

#include "subsonicbaserequest.h"

class QNetworkReply;
class SubsonicService;
class SubsonicUrlHandler;

class SubsonicScrobbleRequest : public SubsonicBaseRequest {
  Q_OBJECT

 public:
  explicit SubsonicScrobbleRequest(SubsonicService *service, SubsonicUrlHandler *url_handler, QObject *parent = nullptr);
  ~SubsonicScrobbleRequest() override;

  void CreateScrobbleRequest(const QString &song_id, const bool submission, const QDateTime &start_time);

 private Q_SLOTS:
  void ScrobbleReplyReceived(QNetworkReply *reply);

 private:

  struct Request {
    explicit Request() : submission(false) {}
    // subsonic song id
    QString song_id;
    // submission: true=Submission, false=NowPlaying
    bool submission;
    // song start time
    qint64 time_ms;
  };

  void FlushScrobbleRequests();
  void FinishCheck();

  void Error(const QString &error, const QVariant &debug = QVariant()) override;

  SubsonicService *service_;
  SubsonicUrlHandler *url_handler_;

  QQueue<Request> scrobble_requests_queue_;

  int scrobble_requests_active_;

  QStringList errors_;
  QList<QNetworkReply*> replies_;
};

#endif  // SUBSONICSCROBBLEREQUEST_H
