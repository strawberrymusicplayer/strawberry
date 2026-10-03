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

#include "config.h"

#include <QObject>
#include <QString>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonObject>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "utilities/strutils.h"
#include "jellyfinservice.h"
#include "jellyfinbaserequest.h"
#include "jellyfinscrobblerequest.h"

using namespace Qt::Literals::StringLiterals;

namespace {
// Send one playback report at a time, the server must receive them in order, for example start before stop.
constexpr int kMaxConcurrentScrobbleRequests = 1;
constexpr int kMaxRetriesAfter401 = 3;
constexpr qint64 kNsecPerTick = 100;
}  // namespace

JellyfinScrobbleRequest::JellyfinScrobbleRequest(JellyfinService *service, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JellyfinBaseRequest(service, network, parent),
      scrobble_requests_active_(0),
      retries_after_401_(0),
      waiting_for_login_(false) {}

void JellyfinScrobbleRequest::CreatePlaybackStartRequest(const QString &song_id) {

  AddRequest({Request::Type::Start, song_id, 0, false});

}

void JellyfinScrobbleRequest::CreatePlaybackProgressRequest(const QString &song_id, const qint64 position_nanosec, const bool paused) {

  AddRequest({Request::Type::Progress, song_id, position_nanosec, paused});

}

void JellyfinScrobbleRequest::CreatePlaybackStoppedRequest(const QString &song_id, const qint64 position_nanosec) {

  AddRequest({Request::Type::Stopped, song_id, position_nanosec, false});

}

void JellyfinScrobbleRequest::AddRequest(const Request &request) {

  scrobble_requests_queue_.enqueue(request);
  FlushScrobbleRequests();

}

void JellyfinScrobbleRequest::LoginSucceeded() {

  waiting_for_login_ = false;
  FlushScrobbleRequests();

}

void JellyfinScrobbleRequest::FlushScrobbleRequests() {

  // Requests sent before logging in finished would be rejected with the same access token.
  if (waiting_for_login_) return;

  while (!scrobble_requests_queue_.isEmpty() && scrobble_requests_active_ < kMaxConcurrentScrobbleRequests) {

    Request request = scrobble_requests_queue_.dequeue();
    ++scrobble_requests_active_;

    QJsonObject json_object;
    json_object.insert(u"ItemId"_s, request.song_id);

    QString ressource_path;
    switch (request.type) {
      case Request::Type::Progress:
        // PlaybackProgressInfo: the current position, in ticks (100 ns units), to keep the session alive on the server.
        json_object.insert(u"PositionTicks"_s, request.position_nanosec / kNsecPerTick);
        json_object.insert(u"IsPaused"_s, request.paused);
        ressource_path = u"Sessions/Playing/Progress"_s;
        break;
      case Request::Type::Stopped:
        // PlaybackStopInfo: the position, in ticks (100 ns units), where playback stopped.
        json_object.insert(u"PositionTicks"_s, request.position_nanosec / kNsecPerTick);
        json_object.insert(u"Failed"_s, false);
        ressource_path = u"Sessions/Playing/Stopped"_s;
        break;
      case Request::Type::Start:
        // PlaybackStartInfo: the scrobbler reports the position with a playback progress report right after it, playback can start at an offset.
        ressource_path = u"Sessions/Playing"_s;
        break;
    }

    QNetworkReply *reply = CreatePostRequest(ressource_path, json_object);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { ScrobbleReplyReceived(reply, request); });

  }

}

void JellyfinScrobbleRequest::ScrobbleReplyReceived(QNetworkReply *reply, const Request &request) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  --scrobble_requests_active_;

  // The playback-reporting endpoints answer with 204 No Content on success.
  const int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

  if (reply->error() != QNetworkReply::NoError || http_status != 204) {

    qLog(Debug) << "Jellyfin:" << "Playback report to" << Utilities::UrlForLog(reply->url()) << "failed with HTTP code" << http_status << reply->errorString();

    if (http_status == 401) {
      const QString rejected_access_token = QString::fromUtf8(reply->request().rawHeader("X-Emby-Token"));
      // A login finished with a new access token while the request was in flight, send it again with the new access token.
      if (!service()->access_token().isEmpty() && rejected_access_token != service()->access_token()) {
        scrobble_requests_queue_.prepend(request);
        FlushScrobbleRequests();
        return;
      }
      // Count logins rather than replies, a server that keeps rejecting new access tokens stops the retries.
      if (retries_after_401_ < kMaxRetriesAfter401) {
        ++retries_after_401_;
        waiting_for_login_ = true;
        scrobble_requests_queue_.prepend(request);
        qLog(Debug) << "Jellyfin:" << "Playback report was rejected with HTTP code 401, logging in again.";
        service()->Reauthenticate(rejected_access_token);
        return;
      }
    }

    const QString redirect_error = JellyfinService::RedirectRefusedError(reply);
    const QString error = redirect_error.isEmpty() ? QStringLiteral("%1 (%2)").arg(reply->errorString(), QString::number(http_status)) : redirect_error;
    Error(error);
    // Progress reports are sent every 30 seconds, so don't show an error dialog for each of them.
    if (request.type != Request::Type::Progress) {
      Q_EMIT ScrobbleError(error);
    }
    return;
  }

  // The server accepted the access token.
  retries_after_401_ = 0;
  service()->TokenValidated();
  FlushScrobbleRequests();

}

void JellyfinScrobbleRequest::Clear() {

  AbortNetworkReplies();
  scrobble_requests_queue_.clear();
  scrobble_requests_active_ = 0;
  waiting_for_login_ = false;
  retries_after_401_ = 0;

}

void JellyfinScrobbleRequest::AuthenticationFailed(const QString &error) {

  // Only relevant while waiting for a login after a 401, all queued reports would fail with the same access token.
  if (!waiting_for_login_) return;

  const qsizetype dropped = scrobble_requests_queue_.count();
  scrobble_requests_queue_.clear();
  waiting_for_login_ = false;

  const QString message = tr("Could not log in again, dropped %n playback report(s): %1", "", static_cast<int>(dropped)).arg(error);
  Error(message);
  Q_EMIT ScrobbleError(message);

}

void JellyfinScrobbleRequest::Error(const QString &error, const QVariant &debug) {

  if (!error.isEmpty()) {
    qLog(Error) << "Jellyfin:" << error;
  }
  if (debug.isValid()) qLog(Debug) << debug;

  FlushScrobbleRequests();

}
