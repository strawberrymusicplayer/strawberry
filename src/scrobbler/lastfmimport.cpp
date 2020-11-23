/*
 * Strawberry Music Player
 * Copyright 2020, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
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

#include "core/network.h"
#include "core/timeconstants.h"
#include "core/logging.h"

#include "lastfmimport.h"

#include "scrobblingapi20.h"
#include "lastfmscrobbler.h"

const int LastFMImport::kRequestsDelay = 2000;

LastFMImport::LastFMImport(QObject *parent) :
  QObject(parent),
  network_(new NetworkAccessManager(this)),
  timer_flush_requests_(new QTimer(this)),
  lastplayed_(false),
  playcount_(false),
  playcount_total_(0),
  lastplayed_total_(0),
  playcount_received_(0),
  lastplayed_received_(0) {

  timer_flush_requests_->setInterval(kRequestsDelay);
  timer_flush_requests_->setSingleShot(false);
  connect(timer_flush_requests_, SIGNAL(timeout()), this, SLOT(FlushRequests()));

}

LastFMImport::~LastFMImport() {

  AbortAll();

}

void LastFMImport::AbortAll() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    disconnect(reply, nullptr, this, nullptr);
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

  QSettings s;
  s.beginGroup(LastFMScrobbler::kSettingsGroup);
  username_ = s.value("username").toString();
  s.endGroup();

}

QNetworkReply *LastFMImport::CreateRequest(const ParamList &request_params) {

  ParamList params = ParamList()
    << Param("api_key", ScrobblingAPI20::kApiKey)
    << Param("user", username_)
    << Param("lang", QLocale().name().left(2).toLower())
    << Param("format", "json")
    << request_params;

  std::sort(params.begin(), params.end());

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
  }

  QUrl url(LastFMScrobbler::kApiUrl);
  url.setQuery(url_query);
  QNetworkRequest req(url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#else
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

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
      Error(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    }
    else {
      QString error;
      // See if there is Json data containing "error" and "message" - then use that instead.
      data = reply->readAll();
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      int error_code = -1;
      if (json_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (json_obj.contains("error") && json_obj.contains("message")) {
          error_code = json_obj["error"].toInt();
          QString error_message = json_obj["message"].toString();
          error = QString("%1 (%2)").arg(error_message).arg(error_code);
        }
      }
      if (error.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
          error = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
        else {
          error = QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
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
    Error("Reply from server missing Json data.", data);
    return QJsonObject();
  }
  if (json_doc.isEmpty()) {
    Error("Received empty Json document.", json_doc);
    return QJsonObject();
  }
  if (!json_doc.isObject()) {
    Error("Json document is not an object.", json_doc);
    return QJsonObject();
  }
  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    Error("Received empty Json object.", json_doc);
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

  ParamList params = ParamList() << Param("method", "user.getRecentTracks");

  if (request.page == 0) {
    params << Param("page", "1");
    params << Param("limit", "1");
  }
  else {
    params << Param("page", QString::number(request.page));
    params << Param("limit", "500");
  }

  QNetworkReply *reply = CreateRequest(params);
  connect(reply, &QNetworkReply::finished, [=] { GetRecentTracksRequestFinished(reply, request.page); });

}

void LastFMImport::GetRecentTracksRequestFinished(QNetworkReply *reply, const int page) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    return;
  }

  if (json_obj.contains("error") && json_obj.contains("message")) {
    int error_code = json_obj["error"].toInt();
    QString error_message = json_obj["message"].toString();
    QString error_reason = QString("%1 (%2)").arg(error_message).arg(error_code);
    Error(error_reason);
    return;
  }

  if (!json_obj.contains("recenttracks")) {
    Error("JSON reply from server is missing recenttracks.", json_obj);
    return;
  }

  if (!json_obj["recenttracks"].isObject()) {
    Error("Failed to pase JSON: recenttracks is not an object!", json_obj);
    return;
  }
  json_obj = json_obj["recenttracks"].toObject();

  if (!json_obj.contains("@attr")) {
    Error("JSON reply from server is missing @attr.", json_obj);
    return;
  }

  if (!json_obj.contains("track")) {
    Error("JSON reply from server is missing track.", json_obj);
    return;
  }

  if (!json_obj["@attr"].isObject()) {
    Error("Failed to pase JSON: @attr is not an object.", json_obj);
    return;
  }

  if (!json_obj["track"].isArray()) {
    Error("Failed to pase JSON: track is not an object.", json_obj);
    return;
  }

  QJsonObject obj_attr = json_obj["@attr"].toObject();

  if (!obj_attr.contains("page")) {
    Error("Failed to pase JSON: attr object is missing page.", json_obj);
    return;
  }
  if (!obj_attr.contains("totalPages")) {
    Error("Failed to pase JSON: attr object is missing totalPages.", json_obj);
    return;
  }
  if (!obj_attr.contains("total")) {
    Error("Failed to pase JSON: attr object is missing total.", json_obj);
    return;
  }

  int total = obj_attr["total"].toString().toInt();
  int pages = obj_attr["totalPages"].toString().toInt();

  if (page == 0) {
    lastplayed_total_ = total;
    UpdateTotal();
    AddGetRecentTracksRequest(1);
  }
  else {

    QJsonArray array_track = json_obj["track"].toArray();

    for (const QJsonValue value_track : array_track) {

      ++lastplayed_received_;

      if (!value_track.isObject()) {
        continue;
      }
      QJsonObject obj_track = value_track.toObject();
      if (!obj_track.contains("artist") ||
          !obj_track.contains("album") ||
          !obj_track.contains("name") ||
          !obj_track.contains("date") ||
          !obj_track["artist"].isObject() ||
          !obj_track["album"].isObject() ||
          !obj_track["date"].isObject()
      ) {
        continue;
      }

      QJsonObject obj_artist = obj_track["artist"].toObject();
      QJsonObject obj_album = obj_track["album"].toObject();
      QJsonObject obj_date = obj_track["date"].toObject();

      if (!obj_artist.contains("#text") || !obj_album.contains("#text") || !obj_date.contains("#text")) {
        continue;
      }

      QString artist = obj_artist["#text"].toString();
      QString album = obj_album["#text"].toString();
      QString date = obj_date["#text"].toString();
      QString title = obj_track["name"].toString();
      QDateTime datetime = QDateTime::fromString(date, "dd MMM yyyy, hh:mm");
      if (datetime.isValid()) {
        emit UpdateLastPlayed(artist, album, title, datetime.toSecsSinceEpoch());
      }

      UpdateProgress();

    }

    if (page == 1) {
      for (int i = 2 ; i <= pages ; ++i) {
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

  ParamList params = ParamList() << Param("method", "user.getTopTracks");

  if (request.page == 0) {
    params << Param("page", "1");
    params << Param("limit", "1");
  }
  else {
    params << Param("page", QString::number(request.page));
    params << Param("limit", "500");
  }

  QNetworkReply *reply = CreateRequest(params);
  connect(reply, &QNetworkReply::finished, [=] { GetTopTracksRequestFinished(reply, request.page); });

}

void LastFMImport::GetTopTracksRequestFinished(QNetworkReply *reply, const int page) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    return;
  }

  if (json_obj.contains("error") && json_obj.contains("message")) {
    int error_code = json_obj["error"].toInt();
    QString error_message = json_obj["message"].toString();
    QString error_reason = QString("%1 (%2)").arg(error_message).arg(error_code);
    Error(error_reason);
    return;
  }

  if (!json_obj.contains("toptracks")) {
    Error("JSON reply from server is missing toptracks.", json_obj);
    return;
  }

  if (!json_obj["toptracks"].isObject()) {
    Error("Failed to pase JSON: toptracks is not an object!", json_obj);
    return;
  }
  json_obj = json_obj["toptracks"].toObject();

  if (!json_obj.contains("@attr")) {
    Error("JSON reply from server is missing @attr.", json_obj);
    return;
  }

  if (!json_obj.contains("track")) {
    Error("JSON reply from server is missing track.", json_obj);
    return;
  }

  if (!json_obj["@attr"].isObject()) {
    Error("Failed to pase JSON: @attr is not an object.", json_obj);
    return;
  }

  if (!json_obj["track"].isArray()) {
    Error("Failed to pase JSON: track is not an object.", json_obj);
    return;
  }

  QJsonObject obj_attr = json_obj["@attr"].toObject();

  if (!obj_attr.contains("page")) {
    Error("Failed to pase JSON: attr object is missing page.", json_obj);
    return;
  }
  if (!obj_attr.contains("totalPages")) {
    Error("Failed to pase JSON: attr object is missing page.", json_obj);
    return;
  }
  if (!obj_attr.contains("total")) {
    Error("Failed to pase JSON: attr object is missing total.", json_obj);
    return;
  }

  int pages = obj_attr["totalPages"].toString().toInt();
  int total = obj_attr["total"].toString().toInt();

  if (page == 0) {
    playcount_total_ = total;
    UpdateTotal();
    AddGetTopTracksRequest(1);
  }
  else {

    QJsonArray array_track = json_obj["track"].toArray();
    for (const QJsonValue value_track : array_track) {

      ++playcount_received_;

      if (!value_track.isObject()) {
        continue;
      }

      QJsonObject obj_track = value_track.toObject();
      if (!obj_track.contains("artist") ||
          !obj_track.contains("name") ||
          !obj_track.contains("playcount") ||
          !obj_track["artist"].isObject()
      ) {
        continue;
      }

      QJsonObject obj_artist = obj_track["artist"].toObject();
      if (!obj_artist.contains("name")) {
        continue;
      }

      QString artist = obj_artist["name"].toString();
      QString title = obj_track["name"].toString();
      int playcount = obj_track["playcount"].toString().toInt();

      if (playcount <= 0) continue;

      emit UpdatePlayCount(artist, title, playcount);
      UpdateProgress();

    }

    if (page == 1) {
      for (int i = 2 ; i <= pages ; ++i) {
        AddGetTopTracksRequest(i);
      }
    }

  }

  FinishCheck();

}

void LastFMImport::UpdateTotal() {

  if ((!playcount_ || playcount_total_ > 0) && (!lastplayed_ || lastplayed_total_ > 0))
    emit UpdateTotal(lastplayed_total_, playcount_total_);

}

void LastFMImport::UpdateProgress() {
  emit UpdateProgress(lastplayed_received_, playcount_received_);
}

void LastFMImport::FinishCheck() {
  if (replies_.isEmpty() && recent_tracks_requests_.isEmpty() && top_tracks_requests_.isEmpty()) emit Finished();
}

void LastFMImport::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << error;
  if (debug.isValid()) qLog(Debug) << debug;

  emit FinishedWithError(error);

  AbortAll();

}
