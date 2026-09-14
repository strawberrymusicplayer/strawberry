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

#include <QQueue>
#include <QString>
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

  void CreatePlaybackStartRequest(const QString &song_id);
  void CreatePlaybackProgressRequest(const QString &song_id, const qint64 position_nanosec, const bool paused);
  void CreatePlaybackStoppedRequest(const QString &song_id, const qint64 position_nanosec);

  // Logging in succeeded, send the playback reports that were waiting for it.
  void LoginSucceeded();

  // Re-authenticating after a 401 failed, drop the playback reports waiting for it.
  void AuthenticationFailed(const QString &error);

  // Drop all playback reports, for example when Jellyfin was disabled or the server changed.
  void Clear();

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
    qint64 position_nanosec;
    bool paused;
  };

  void AddRequest(const Request &request);
  void FlushScrobbleRequests();
  void ScrobbleReplyReceived(QNetworkReply *reply, const Request &request);
  void Error(const QString &error, const QVariant &debug = QVariant()) override;

  QQueue<Request> scrobble_requests_queue_;
  int scrobble_requests_active_;
  int retries_after_401_;  // Logins after 401s without a successful playback report in between.
  bool waiting_for_login_;  // A 401 was received, requests are held until logging in finished.
};

#endif  // JELLYFINSCROBBLEREQUEST_H
