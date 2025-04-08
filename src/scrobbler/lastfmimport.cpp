/*
 * Strawberry Music Player
 * Copyright 2020-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>

#include <QApplication>
#include <QLocale>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QDateTime>
#include <QTimer>
#include <QSettings>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/settings.h"

#include "lastfmimport.h"

#include "scrobblingapi20.h"
#include "lastfmscrobbler.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kRequestsDelay = 2000;
}

LastFMImport::LastFMImport(const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonBaseRequest(network, parent),
      network_(network),
      timer_flush_requests_(new QTimer(this)),
      lastplayed_(false),
      playcount_(false),
      playcount_total_(0),
      lastplayed_total_(0),
      playcount_received_(0),
      lastplayed_received_(0) {

  timer_flush_requests_->setInterval(kRequestsDelay);
  timer_flush_requests_->setSingleShot(false);
  QObject::connect(timer_flush_requests_, &QTimer::timeout, this, &LastFMImport::FlushRequests);

}

LastFMImport::~LastFMImport() {

  AbortAll();

}

void LastFMImport::AbortAll() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

  playcount_total_ = 0;
  lastplayed_total_ = 0;
  playcount_received_ = 0;
  lastplayed_received_ = 0;

  recent_tracks_requests_.clear();
  top_tracks_requests_.clear();
  timer_flush_requests_->stop();

}

void LastFMImport::ReloadSettings() {

  Settings s;
  s.beginGroup(LastFMScrobbler::kSettingsGroup);
  username_ = s.value("username").toString();
  s.endGroup();

}

QNetworkReply *LastFMImport::CreateRequest(const ParamList &request_params) {

  ParamList params = ParamList()
    << Param(u"api_key"_s, QLatin1String(ScrobblingAPI20::kApiKey))
    << Param(u"user"_s, username_)
    << Param(u"lang"_s, QLocale().name().left(2).toLower())
    << Param(u"format"_s, u"json"_s)
    << request_params;

  std::sort(params.begin(), params.end());

  QNetworkReply *reply = CreateGetRequest(QUrl(QLatin1String(LastFMScrobbler::kApiUrl)), params);

  //qLog(Debug) << "Sending request" << url_query.toString(QUrl::FullyDecoded);

  return reply;

}

JsonBaseRequest::JsonObjectResult LastFMImport::ParseJsonObject(QNetworkReply *reply) {

  if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
    return ReplyDataResult(ErrorCode::NetworkError, QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
  }

  JsonObjectResult result(ErrorCode::Success);
  result.network_error = reply->error();
  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
    result.http_status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  }

  const QByteArray data = reply->readAll();
  if (!data.isEmpty()) {
    QJsonParseError json_parse_error;
    const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_parse_error);
    if (json_parse_error.error == QJsonParseError::NoError) {
      const QJsonObject json_object = json_document.object();
      if (json_object.contains("error"_L1) && json_object.contains("message"_L1)) {
        const int error = json_object["error"_L1].toInt();
        const QString message = json_object["message"_L1].toString();
        result.error_code = ErrorCode::APIError;
        result.error_message = QStringLiteral("%1 (%2)").arg(message).arg(error);
      }
      else {
        result.json_object = json_document.object();
      }
    }
    else {
      result.error_code = ErrorCode::ParseError;
      result.error_message = json_parse_error.errorString();
    }
  }

  if (result.error_code != ErrorCode::APIError) {
    if (reply->error() != QNetworkReply::NoError) {
      result.error_code = ErrorCode::NetworkError;
      result.error_message = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
    }
    else if (result.http_status_code != 200) {
      result.error_code = ErrorCode::HttpError;
      result.error_message = QStringLiteral("Received HTTP code %1").arg(result.http_status_code);
    }
  }

  return result;

}

void LastFMImport::ImportData(const bool lastplayed, const bool playcount) {

  if (!lastplayed && !playcount) return;

  ReloadSettings();

  if (username_.isEmpty()) {
    Error(tr("Missing username, please login to last.fm first!"));
    return;
  }

  AbortAll();

  lastplayed_ = lastplayed;
  playcount_ = playcount;

  if (lastplayed) AddGetRecentTracksRequest(0);
  if (playcount) AddGetTopTracksRequest(0);

}

void LastFMImport::FlushRequests() {

  if (!recent_tracks_requests_.isEmpty() && (!playcount_ || (playcount_total_ > 0 || top_tracks_requests_.isEmpty()))) {
    SendGetRecentTracksRequest(recent_tracks_requests_.dequeue());
    return;
  }

  if (!top_tracks_requests_.isEmpty()) {
    SendGetTopTracksRequest(top_tracks_requests_.dequeue());
    return;
  }

  timer_flush_requests_->stop();

}

void LastFMImport::AddGetRecentTracksRequest(const int page) {

  recent_tracks_requests_.enqueue(GetRecentTracksRequest(page));

  if (!timer_flush_requests_->isActive()) {
    timer_flush_requests_->start();
  }

}

void LastFMImport::SendGetRecentTracksRequest(GetRecentTracksRequest request) {

  ParamList params = ParamList() << Param(u"method"_s, u"user.getRecentTracks"_s);

  if (request.page == 0) {
    params << Param(u"page"_s, u"1"_s);
    params << Param(u"limit"_s, u"1"_s);
  }
  else {
    params << Param(u"page"_s, QString::number(request.page));
    params << Param(u"limit"_s, u"500"_s);
  }

  QNetworkReply *reply = CreateRequest(params);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { GetRecentTracksRequestFinished(reply, request.page); });

}

void LastFMImport::GetRecentTracksRequestFinished(QNetworkReply *reply, const int page) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  QJsonObject json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    return;
  }

  if (!json_object.contains("recenttracks"_L1)) {
    Error(u"JSON reply from server is missing recenttracks."_s, json_object);
    return;
  }

  if (!json_object["recenttracks"_L1].isObject()) {
    Error(u"Failed to parse JSON: recenttracks is not an object!"_s, json_object);
    return;
  }
  json_object = json_object["recenttracks"_L1].toObject();

  if (!json_object.contains("@attr"_L1)) {
    Error(u"JSON reply from server is missing @attr."_s, json_object);
    return;
  }

  if (!json_object.contains("track"_L1)) {
    Error(u"JSON reply from server is missing track."_s, json_object);
    return;
  }

  if (!json_object["@attr"_L1].isObject()) {
    Error(u"Failed to parse JSON: @attr is not an object."_s, json_object);
    return;
  }

  if (!json_object["track"_L1].isArray()) {
    Error(u"Failed to parse JSON: track is not an object."_s, json_object);
    return;
  }

  const QJsonObject obj_attr = json_object["@attr"_L1].toObject();

  if (!obj_attr.contains("page"_L1)) {
    Error(u"Failed to parse JSON: attr object is missing page."_s, json_object);
    return;
  }
  if (!obj_attr.contains("totalPages"_L1)) {
    Error(u"Failed to parse JSON: attr object is missing totalPages."_s, json_object);
    return;
  }
  if (!obj_attr.contains("total"_L1)) {
    Error(u"Failed to parse JSON: attr object is missing total."_s, json_object);
    return;
  }

  const int total = obj_attr["total"_L1].toString().toInt();
  const int pages = obj_attr["totalPages"_L1].toString().toInt();

  if (page == 0) {
    lastplayed_total_ = total;
    UpdateTotalCheck();
    AddGetRecentTracksRequest(1);
  }
  else {

    const QJsonArray array_track = json_object["track"_L1].toArray();

    for (const QJsonValue &value_track : array_track) {

      ++lastplayed_received_;

      if (!value_track.isObject()) {
        continue;
      }
      QJsonObject obj_track = value_track.toObject();
      if (!obj_track.contains("artist"_L1) ||
          !obj_track.contains("album"_L1) ||
          !obj_track.contains("name"_L1) ||
          !obj_track.contains("date"_L1) ||
          !obj_track["artist"_L1].isObject() ||
          !obj_track["album"_L1].isObject() ||
          !obj_track["date"_L1].isObject()
      ) {
        continue;
      }

      const QJsonObject obj_artist = obj_track["artist"_L1].toObject();
      const QJsonObject obj_album = obj_track["album"_L1].toObject();
      const QJsonObject obj_date = obj_track["date"_L1].toObject();

      if (!obj_artist.contains("#text"_L1) || !obj_album.contains("#text"_L1) || !obj_date.contains("#text"_L1)) {
        continue;
      }

      const QString artist = obj_artist["#text"_L1].toString();
      const QString album = obj_album["#text"_L1].toString();
      const QString date = obj_date["#text"_L1].toString();
      const QString title = obj_track["name"_L1].toString();
      const QDateTime datetime = QDateTime::fromString(date, u"dd MMM yyyy, hh:mm"_s);
      if (datetime.isValid()) {
        Q_EMIT UpdateLastPlayed(artist, album, title, datetime.toSecsSinceEpoch());
      }

      UpdateProgressCheck();

    }

    if (page == 1) {
      for (int i = 2; i <= pages; ++i) {
        AddGetRecentTracksRequest(i);
      }
    }

  }

  FinishCheck();

}

void LastFMImport::AddGetTopTracksRequest(const int page) {

  top_tracks_requests_.enqueue(GetTopTracksRequest(page));

  if (!timer_flush_requests_->isActive()) {
    timer_flush_requests_->start();
  }

}

void LastFMImport::SendGetTopTracksRequest(GetTopTracksRequest request) {

  ParamList params = ParamList() << Param(u"method"_s, u"user.getTopTracks"_s);

  if (request.page == 0) {
    params << Param(u"page"_s, u"1"_s);
    params << Param(u"limit"_s, u"1"_s);
  }
  else {
    params << Param(u"page"_s, QString::number(request.page));
    params << Param(u"limit"_s, u"500"_s);
  }

  QNetworkReply *reply = CreateRequest(params);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { GetTopTracksRequestFinished(reply, request.page); });

}

void LastFMImport::GetTopTracksRequestFinished(QNetworkReply *reply, const int page) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  QJsonObject json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    return;
  }

  if (!json_object.contains("toptracks"_L1)) {
    Error(u"JSON reply from server is missing toptracks."_s, json_object);
    return;
  }

  if (!json_object["toptracks"_L1].isObject()) {
    Error(u"Failed to parse JSON: toptracks is not an object!"_s, json_object);
    return;
  }
  json_object = json_object["toptracks"_L1].toObject();

  if (!json_object.contains("@attr"_L1)) {
    Error(u"JSON reply from server is missing @attr."_s, json_object);
    return;
  }

  if (!json_object.contains("track"_L1)) {
    Error(u"JSON reply from server is missing track."_s, json_object);
    return;
  }

  if (!json_object["@attr"_L1].isObject()) {
    Error(u"Failed to parse JSON: @attr is not an object."_s, json_object);
    return;
  }

  if (!json_object["track"_L1].isArray()) {
    Error(u"Failed to parse JSON: track is not an object."_s, json_object);
    return;
  }

  const QJsonObject object_attr = json_object["@attr"_L1].toObject();

  if (!object_attr.contains("page"_L1)) {
    Error(u"Failed to parse JSON: attr object is missing page."_s, json_object);
    return;
  }
  if (!object_attr.contains("totalPages"_L1)) {
    Error(u"Failed to parse JSON: attr object is missing page."_s, json_object);
    return;
  }
  if (!object_attr.contains("total"_L1)) {
    Error(u"Failed to parse JSON: attr object is missing total."_s, json_object);
    return;
  }

  const int pages = object_attr["totalPages"_L1].toString().toInt();
  const int total = object_attr["total"_L1].toString().toInt();

  if (page == 0) {
    playcount_total_ = total;
    UpdateTotalCheck();
    AddGetTopTracksRequest(1);
  }
  else {

    const QJsonArray array_track = json_object["track"_L1].toArray();
    for (QJsonArray::ConstIterator it = array_track.begin(); it != array_track.constEnd(); ++it) {

      const QJsonValue &value_track = *it;

      ++playcount_received_;

      if (!value_track.isObject()) {
        continue;
      }

      const QJsonObject obj_track = value_track.toObject();
      if (!obj_track.contains("artist"_L1) ||
          !obj_track.contains("name"_L1) ||
          !obj_track.contains("playcount"_L1) ||
          !obj_track["artist"_L1].isObject()
      ) {
        continue;
      }

      const QJsonObject obj_artist = obj_track["artist"_L1].toObject();
      if (!obj_artist.contains("name"_L1)) {
        continue;
      }

      const QString artist = obj_artist["name"_L1].toString();
      const QString title = obj_track["name"_L1].toString();
      const int playcount = obj_track["playcount"_L1].toString().toInt();

      if (playcount <= 0) continue;

      Q_EMIT UpdatePlayCount(artist, title, playcount, false);
      UpdateProgressCheck();

    }

    if (page == 1) {
      for (int i = 2; i <= pages; ++i) {
        AddGetTopTracksRequest(i);
      }
    }

  }

  FinishCheck();

}

void LastFMImport::UpdateTotalCheck() {

  Q_EMIT UpdateTotal(lastplayed_total_, playcount_total_);

}

void LastFMImport::UpdateProgressCheck() {
  Q_EMIT UpdateProgress(lastplayed_received_, playcount_received_);
}

void LastFMImport::FinishCheck() {
  if (replies_.isEmpty() && recent_tracks_requests_.isEmpty() && top_tracks_requests_.isEmpty()) Q_EMIT Finished();
}

void LastFMImport::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << error;
  if (debug.isValid()) qLog(Debug) << debug;

  Q_EMIT FinishedWithError(error);

  AbortAll();

}
