/*
 * Strawberry Music Player
 * Copyright 2020-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include "core/logging.h"
#include "core/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "core/settings.h"

#include "lastfmimport.h"

#include "scrobblingapi20.h"
#include "lastfmscrobbler.h"

using namespace Qt::StringLiterals;

namespace {
constexpr int kRequestsDelay = 2000;
}

LastFMImport::LastFMImport(SharedPtr<NetworkAccessManager> network, QObject *parent)
    : QObject(parent),
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
    << Param(QStringLiteral("api_key"), QLatin1String(ScrobblingAPI20::kApiKey))
    << Param(QStringLiteral("user"), username_)
    << Param(QStringLiteral("lang"), QLocale().name().left(2).toLower())
    << Param(QStringLiteral("format"), QStringLiteral("json"))
    << request_params;

  std::sort(params.begin(), params.end());

  QUrlQuery url_query;
  for (const Param &param : std::as_const(params)) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QUrl url(QString::fromLatin1(LastFMScrobbler::kApiUrl));
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

  QNetworkReply *reply = network_->get(req);
  replies_ << reply;

  //qLog(Debug) << "Sending request" << url_query.toString(QUrl::FullyDecoded);

  return reply;

}

QByteArray LastFMImport::GetReplyData(QNetworkReply *reply) {

  QByteArray data;

  if (reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
    data = reply->readAll();
  }
  else {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      Error(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    }
    else {
      QString error;
      // See if there is Json data containing "error" and "message" - then use that instead.
      data = reply->readAll();
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      if (json_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (json_obj.contains("error"_L1) && json_obj.contains("message"_L1)) {
          int error_code = json_obj["error"_L1].toInt();
          QString error_message = json_obj["message"_L1].toString();
          error = QStringLiteral("%1 (%2)").arg(error_message).arg(error_code);
        }
      }
      if (error.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
          error = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
        else {
          error = QStringLiteral("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        }
      }
      Error(error);
    }
    return QByteArray();
  }

  return data;

}

QJsonObject LastFMImport::ExtractJsonObj(const QByteArray &data) {

  QJsonParseError error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &error);

  if (error.error != QJsonParseError::NoError) {
    Error(QStringLiteral("Reply from server missing Json data."), data);
    return QJsonObject();
  }
  if (json_doc.isEmpty()) {
    Error(QStringLiteral("Received empty Json document."), json_doc);
    return QJsonObject();
  }
  if (!json_doc.isObject()) {
    Error(QStringLiteral("Json document is not an object."), json_doc);
    return QJsonObject();
  }
  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    Error(QStringLiteral("Received empty Json object."), json_doc);
    return QJsonObject();
  }

  return json_obj;

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

  if (!recent_tracks_requests_.isEmpty()) {
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

  ParamList params = ParamList() << Param(QStringLiteral("method"), QStringLiteral("user.getRecentTracks"));

  if (request.page == 0) {
    params << Param(QStringLiteral("page"), QStringLiteral("1"));
    params << Param(QStringLiteral("limit"), QStringLiteral("1"));
  }
  else {
    params << Param(QStringLiteral("page"), QString::number(request.page));
    params << Param(QStringLiteral("limit"), QStringLiteral("500"));
  }

  QNetworkReply *reply = CreateRequest(params);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { GetRecentTracksRequestFinished(reply, request.page); });

}

void LastFMImport::GetRecentTracksRequestFinished(QNetworkReply *reply, const int page) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    return;
  }

  if (json_obj.contains("error"_L1) && json_obj.contains("message"_L1)) {
    int error_code = json_obj["error"_L1].toInt();
    QString error_message = json_obj["message"_L1].toString();
    QString error_reason = QStringLiteral("%1 (%2)").arg(error_message).arg(error_code);
    Error(error_reason);
    return;
  }

  if (!json_obj.contains("recenttracks"_L1)) {
    Error(QStringLiteral("JSON reply from server is missing recenttracks."), json_obj);
    return;
  }

  if (!json_obj["recenttracks"_L1].isObject()) {
    Error(QStringLiteral("Failed to parse JSON: recenttracks is not an object!"), json_obj);
    return;
  }
  json_obj = json_obj["recenttracks"_L1].toObject();

  if (!json_obj.contains("@attr"_L1)) {
    Error(QStringLiteral("JSON reply from server is missing @attr."), json_obj);
    return;
  }

  if (!json_obj.contains("track"_L1)) {
    Error(QStringLiteral("JSON reply from server is missing track."), json_obj);
    return;
  }

  if (!json_obj["@attr"_L1].isObject()) {
    Error(QStringLiteral("Failed to parse JSON: @attr is not an object."), json_obj);
    return;
  }

  if (!json_obj["track"_L1].isArray()) {
    Error(QStringLiteral("Failed to parse JSON: track is not an object."), json_obj);
    return;
  }

  QJsonObject obj_attr = json_obj["@attr"_L1].toObject();

  if (!obj_attr.contains("page"_L1)) {
    Error(QStringLiteral("Failed to parse JSON: attr object is missing page."), json_obj);
    return;
  }
  if (!obj_attr.contains("totalPages"_L1)) {
    Error(QStringLiteral("Failed to parse JSON: attr object is missing totalPages."), json_obj);
    return;
  }
  if (!obj_attr.contains("total"_L1)) {
    Error(QStringLiteral("Failed to parse JSON: attr object is missing total."), json_obj);
    return;
  }

  int total = obj_attr["total"_L1].toString().toInt();
  int pages = obj_attr["totalPages"_L1].toString().toInt();

  if (page == 0) {
    lastplayed_total_ = total;
    UpdateTotalCheck();
    AddGetRecentTracksRequest(1);
  }
  else {

    const QJsonArray array_track = json_obj["track"_L1].toArray();

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

      QJsonObject obj_artist = obj_track["artist"_L1].toObject();
      QJsonObject obj_album = obj_track["album"_L1].toObject();
      QJsonObject obj_date = obj_track["date"_L1].toObject();

      if (!obj_artist.contains("#text"_L1) || !obj_album.contains("#text"_L1) || !obj_date.contains("#text"_L1)) {
        continue;
      }

      QString artist = obj_artist["#text"_L1].toString();
      QString album = obj_album["#text"_L1].toString();
      QString date = obj_date["#text"_L1].toString();
      QString title = obj_track["name"_L1].toString();
      QDateTime datetime = QDateTime::fromString(date, QStringLiteral("dd MMM yyyy, hh:mm"));
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

  ParamList params = ParamList() << Param(QStringLiteral("method"), QStringLiteral("user.getTopTracks"));

  if (request.page == 0) {
    params << Param(QStringLiteral("page"), QStringLiteral("1"));
    params << Param(QStringLiteral("limit"), QStringLiteral("1"));
  }
  else {
    params << Param(QStringLiteral("page"), QString::number(request.page));
    params << Param(QStringLiteral("limit"), QStringLiteral("500"));
  }

  QNetworkReply *reply = CreateRequest(params);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { GetTopTracksRequestFinished(reply, request.page); });

}

void LastFMImport::GetTopTracksRequestFinished(QNetworkReply *reply, const int page) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    return;
  }

  if (json_obj.contains("error"_L1) && json_obj.contains("message"_L1)) {
    int error_code = json_obj["error"_L1].toInt();
    QString error_message = json_obj["message"_L1].toString();
    QString error_reason = QStringLiteral("%1 (%2)").arg(error_message).arg(error_code);
    Error(error_reason);
    return;
  }

  if (!json_obj.contains("toptracks"_L1)) {
    Error(QStringLiteral("JSON reply from server is missing toptracks."), json_obj);
    return;
  }

  if (!json_obj["toptracks"_L1].isObject()) {
    Error(QStringLiteral("Failed to parse JSON: toptracks is not an object!"), json_obj);
    return;
  }
  json_obj = json_obj["toptracks"_L1].toObject();

  if (!json_obj.contains("@attr"_L1)) {
    Error(QStringLiteral("JSON reply from server is missing @attr."), json_obj);
    return;
  }

  if (!json_obj.contains("track"_L1)) {
    Error(QStringLiteral("JSON reply from server is missing track."), json_obj);
    return;
  }

  if (!json_obj["@attr"_L1].isObject()) {
    Error(QStringLiteral("Failed to parse JSON: @attr is not an object."), json_obj);
    return;
  }

  if (!json_obj["track"_L1].isArray()) {
    Error(QStringLiteral("Failed to parse JSON: track is not an object."), json_obj);
    return;
  }

  QJsonObject obj_attr = json_obj["@attr"_L1].toObject();

  if (!obj_attr.contains("page"_L1)) {
    Error(QStringLiteral("Failed to parse JSON: attr object is missing page."), json_obj);
    return;
  }
  if (!obj_attr.contains("totalPages"_L1)) {
    Error(QStringLiteral("Failed to parse JSON: attr object is missing page."), json_obj);
    return;
  }
  if (!obj_attr.contains("total"_L1)) {
    Error(QStringLiteral("Failed to parse JSON: attr object is missing total."), json_obj);
    return;
  }

  int pages = obj_attr["totalPages"_L1].toString().toInt();
  int total = obj_attr["total"_L1].toString().toInt();

  if (page == 0) {
    playcount_total_ = total;
    UpdateTotalCheck();
    AddGetTopTracksRequest(1);
  }
  else {

    QJsonArray array_track = json_obj["track"_L1].toArray();
    for (QJsonArray::iterator it = array_track.begin(); it != array_track.end(); ++it) {

      const QJsonValue &value_track = *it;

      ++playcount_received_;

      if (!value_track.isObject()) {
        continue;
      }

      QJsonObject obj_track = value_track.toObject();
      if (!obj_track.contains("artist"_L1) ||
          !obj_track.contains("name"_L1) ||
          !obj_track.contains("playcount"_L1) ||
          !obj_track["artist"_L1].isObject()
      ) {
        continue;
      }

      QJsonObject obj_artist = obj_track["artist"_L1].toObject();
      if (!obj_artist.contains("name"_L1)) {
        continue;
      }

      QString artist = obj_artist["name"_L1].toString();
      QString title = obj_track["name"_L1].toString();
      int playcount = obj_track["playcount"_L1].toString().toInt();

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

  if ((!playcount_ || playcount_total_ > 0) && (!lastplayed_ || lastplayed_total_ > 0))
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
