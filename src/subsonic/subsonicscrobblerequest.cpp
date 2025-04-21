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

#include "config.h"

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QDateTime>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "core/logging.h"
#include "subsonicservice.h"
#include "subsonicbaserequest.h"
#include "subsonicscrobblerequest.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kMaxConcurrentScrobbleRequests = 3;
}

SubsonicScrobbleRequest::SubsonicScrobbleRequest(SubsonicService *service, SubsonicUrlHandler *url_handler, QObject *parent)
    : SubsonicBaseRequest(service, parent),
      service_(service),
      url_handler_(url_handler),
      scrobble_requests_active_(0) {}

SubsonicScrobbleRequest::~SubsonicScrobbleRequest() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

}

void SubsonicScrobbleRequest::CreateScrobbleRequest(const QString &song_id, const bool submission, const QDateTime &start_time) {

  Request request;
  request.song_id = song_id;
  request.submission = submission;
  request.time_ms = start_time.toMSecsSinceEpoch();
  scrobble_requests_queue_.enqueue(request);
  if (scrobble_requests_active_ < kMaxConcurrentScrobbleRequests) FlushScrobbleRequests();

}

void SubsonicScrobbleRequest::FlushScrobbleRequests() {

  while (!scrobble_requests_queue_.isEmpty() && scrobble_requests_active_ < kMaxConcurrentScrobbleRequests) {

    Request request = scrobble_requests_queue_.dequeue();
    ++scrobble_requests_active_;

    ParamList params = ParamList() << Param(u"id"_s, request.song_id)
                                   << Param(u"submission"_s, QVariant(request.submission).toString())
                                   << Param(u"time"_s, QVariant(request.time_ms).toString());

    QNetworkReply *reply = CreateGetRequest(u"scrobble"_s, params);
    replies_ << reply;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { ScrobbleReplyReceived(reply); });

  }

}

void SubsonicScrobbleRequest::ScrobbleReplyReceived(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  --scrobble_requests_active_;

  // "subsonic-response" is empty on success, but some keys like status, version, or type might be present.
  // Therefore, we can only check for errors.

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    FinishCheck();
    return;
  }
  const QJsonObject &json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    FinishCheck();
    return;
  }

  FinishCheck();

}

void SubsonicScrobbleRequest::FinishCheck() {

  if (!scrobble_requests_queue_.isEmpty() && scrobble_requests_active_ < kMaxConcurrentScrobbleRequests) FlushScrobbleRequests();

}

void SubsonicScrobbleRequest::Error(const QString &error, const QVariant &debug) {

  if (!error.isEmpty()) {
    qLog(Error) << "SubsonicScrobbleRequest:" << error;
    errors_ << error;
  }
  if (debug.isValid()) qLog(Debug) << debug;

  FinishCheck();

}
