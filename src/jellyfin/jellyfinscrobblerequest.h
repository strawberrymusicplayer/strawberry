/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry Music Player contributors
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

#ifndef JELLYFINSCROBBLEREQUEST_H
#define JELLYFINSCROBBLEREQUEST_H

#include "config.h"

#include <QByteArray>
#include <QDateTime>
#include <QQueue>
#include <QString>
#include <QStringList>
#include <QVariant>

#include "includes/shared_ptr.h"
#include "jellyfinbaserequest.h"

class QNetworkReply;
class NetworkAccessManager;
class JellyfinService;

class JellyfinScrobbleRequest : public JellyfinBaseRequest {
  Q_OBJECT

 public:
  explicit JellyfinScrobbleRequest(JellyfinService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

  void CreateScrobbleRequest(const QString &song_id, const bool submission, const QDateTime &start_time);
  void CreatePlaybackProgressRequest(const QString &song_id, const QDateTime &start_time);

  void FlushScrobbleRequests();

  // Re-authenticating after a 401 failed, drop the playback reports waiting for it.
  void AuthenticationFailed(const QString &error);

 Q_SIGNALS:
  // A playback report failed permanently.
  void ScrobbleError(const QString &error);

 private:
  struct Request {
    enum class Type : int {
      Start,
      Progress,
      Stopped,
    } type;
    QString song_id;
    QDateTime time;
  };

  void ScrobbleReplyReceived(QNetworkReply *reply, const Request &request);
  void FinishCheck();
  void Error(const QString &error, const QVariant &debug = QVariant()) override;

  QQueue<Request> scrobble_requests_queue_;
  int scrobble_requests_active_;
  int retries_after_401_;
  QStringList errors_;
};

#endif  // JELLYFINSCROBBLEREQUEST_H