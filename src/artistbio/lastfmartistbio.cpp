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
#include <QLocale>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QDateTime>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "core/logging.h"
#include "core/iconloader.h"

#include "lastfmartistbio.h"
#include "widgets/infotextview.h"
#include "scrobbler/scrobblingapi20.h"
#include "scrobbler/lastfmscrobbler.h"

LastFMArtistBio::LastFMArtistBio() : ArtistBioProvider(), network_(new NetworkAccessManager(this)) {}

LastFMArtistBio::~LastFMArtistBio() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

void LastFMArtistBio::Start(const int id, const Song &song) {

  ParamList params = ParamList()
    << Param("api_key", ScrobblingAPI20::kApiKey)
    << Param("lang", QLocale().name().left(2).toLower())
    << Param("format", "json")
    << Param("method", "artist.getinfo")
    << Param("artist", song.artist());

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
  connect(reply, &QNetworkReply::finished, [=] { RequestFinished(reply, id); });

  qLog(Debug) << "Sending request" << url_query.toString(QUrl::FullyDecoded);

}

QByteArray LastFMArtistBio::GetReplyData(QNetworkReply *reply) {

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

QJsonObject LastFMArtistBio::ExtractJsonObj(const QByteArray &data) {

  if (data.isEmpty()) return QJsonObject();

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

void LastFMArtistBio::RequestFinished(QNetworkReply *reply, const int id) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QJsonObject json_obj = ExtractJsonObj(GetReplyData(reply));

  QString title;
  QString text;
  if (!json_obj.isEmpty() && json_obj.contains("artist") && json_obj["artist"].isObject()) {
    json_obj = json_obj["artist"].toObject();
    if (json_obj.contains("bio") && json_obj["bio"].isObject()) {
      title = json_obj["name"].toString();
      QJsonObject obj_bio = json_obj["bio"].toObject();
      if (obj_bio.contains("content")) {
        text = obj_bio["content"].toString();
      }
    }
  }

  CollapsibleInfoPane::Data info_data;
  info_data.id_ = title;
  info_data.title_ = tr("Biography");
  info_data.type_ = CollapsibleInfoPane::Data::Type_Biography;
  info_data.icon_ = IconLoader::Load("scrobble");
  InfoTextView *editor = new InfoTextView;
  editor->SetHtml(text);
  info_data.contents_ = editor;
  emit InfoReady(id, info_data);
  emit Finished(id);

}

void LastFMArtistBio::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
