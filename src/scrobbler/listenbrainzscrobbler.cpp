/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QDesktopServices>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QDateTime>
#include <QMessageBox>
#include <QSettings>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QtDebug>

#include "core/application.h"
#include "core/closure.h"
#include "core/network.h"
#include "core/song.h"
#include "core/timeconstants.h"
#include "core/logging.h"
#include "internet/localredirectserver.h"

#include "audioscrobbler.h"
#include "scrobblerservice.h"
#include "scrobblercache.h"
#include "scrobblercacheitem.h"
#include "listenbrainzscrobbler.h"

const char *ListenBrainzScrobbler::kName = "ListenBrainz";
const char *ListenBrainzScrobbler::kSettingsGroup = "ListenBrainz";
const char *ListenBrainzScrobbler::kAuthUrl = "https://musicbrainz.org/oauth2/authorize";
const char *ListenBrainzScrobbler::kAuthTokenUrl = "https://musicbrainz.org/oauth2/token";
const char *ListenBrainzScrobbler::kRedirectUrl = "http://localhost";
const char *ListenBrainzScrobbler::kApiUrl = "https://api.listenbrainz.org";
const char *ListenBrainzScrobbler::kClientID = "oeAUNwqSQer0er09Fiqi0Q";
const char *ListenBrainzScrobbler::kClientSecret = "ROFghkeQ3F3oPyEhqiyWPA";
const char *ListenBrainzScrobbler::kCacheFile = "listenbrainzscrobbler.cache";
const int ListenBrainzScrobbler::kScrobblesPerRequest = 10;

ListenBrainzScrobbler::ListenBrainzScrobbler(Application *app, QObject *parent) : ScrobblerService(kName, app, parent),
  app_(app),
  network_(new NetworkAccessManager(this)),
  cache_(new ScrobblerCache(kCacheFile, this)),
  server_(nullptr),
  enabled_(false),
  expires_in_(-1),
  submitted_(false),
  timestamp_(0) {

  ReloadSettings();
  LoadSession();

}

ListenBrainzScrobbler::~ListenBrainzScrobbler() {}

void ListenBrainzScrobbler::ReloadSettings() {

  QSettings s;
  s.beginGroup(kSettingsGroup);
  enabled_ = s.value("enabled", false).toBool();
  user_token_ = s.value("user_token").toString();
  s.endGroup();

}

void ListenBrainzScrobbler::LoadSession() {

  QSettings s;
  s.beginGroup(kSettingsGroup);
  access_token_ = s.value("access_token").toString();
  expires_in_ = s.value("expires_in", -1).toInt();
  token_type_ = s.value("token_type").toString();
  refresh_token_ = s.value("refresh_token").toString();
  s.endGroup();

}

void ListenBrainzScrobbler::Logout() {

  access_token_.clear();
  expires_in_ = -1;
  token_type_.clear();
  refresh_token_.clear();

  QSettings settings;
  settings.beginGroup(kSettingsGroup);
  settings.remove("access_token");
  settings.remove("expires_in");
  settings.remove("token_type");
  settings.remove("refresh_token");
  settings.endGroup();

}

void ListenBrainzScrobbler::Authenticate(const bool https) {

  if (!server_) {
    server_ = new LocalRedirectServer(https, this);
    if (!server_->Listen()) {
      AuthError(server_->error());
      delete server_;
      server_ = nullptr;
      return;
    }
    connect(server_, SIGNAL(Finished()), this, SLOT(RedirectArrived()));
  }

  QUrl redirect_url(kRedirectUrl);
  redirect_url.setPort(server_->url().port());

  QUrlQuery url_query;
  url_query.addQueryItem("response_type", "code");
  url_query.addQueryItem("client_id", kClientID);
  url_query.addQueryItem("redirect_uri", redirect_url.toString());
  url_query.addQueryItem("scope", "profile;email;tag;rating;collection;submit_isrc;submit_barcode");
  QUrl url(kAuthUrl);
  url.setQuery(url_query);

  bool result = QDesktopServices::openUrl(url);
  if (!result) {
    QMessageBox messagebox(QMessageBox::Information, tr("ListenBrainz Authentication"), tr("Please open this URL in your browser") + QString(":<br /><a href=\"%1\">%1</a>").arg(url.toString()), QMessageBox::Ok);
    messagebox.setTextFormat(Qt::RichText);
    messagebox.exec();
  }

}

void ListenBrainzScrobbler::RedirectArrived() {

  if (!server_) return;

  if (server_->error().isEmpty()) {
    QUrl url = server_->request_url();
    if (url.isValid()) {
      QUrlQuery url_query(url);
      if (url_query.hasQueryItem("error")) {
        AuthError(QUrlQuery(url).queryItemValue("error"));
      }
      else if (url_query.hasQueryItem("code")) {
        RequestSession(url, url_query.queryItemValue("code"));
      }
      else {
        AuthError(tr("Redirect missing token code!"));
      }
    }
    else {
      AuthError(tr("Received invalid reply from web browser."));
    }
  }
  else {
    AuthError(server_->error());
  }

  server_->close();
  server_->deleteLater();
  server_ = nullptr;

}

void ListenBrainzScrobbler::RequestSession(const QUrl &url, const QString &token) {

  QUrl session_url(kAuthTokenUrl);
  QUrlQuery url_query;
  url_query.addQueryItem("grant_type", "authorization_code");
  url_query.addQueryItem("code", token);
  url_query.addQueryItem("client_id", kClientID);
  url_query.addQueryItem("client_secret", kClientSecret);
  url_query.addQueryItem("redirect_uri", url.toString());

  QNetworkRequest req(session_url);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  QByteArray query = url_query.toString(QUrl::FullyEncoded).toUtf8();
  QNetworkReply *reply = network_->post(req, query);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(AuthenticateReplyFinished(QNetworkReply*)), reply);

}

void ListenBrainzScrobbler::AuthenticateReplyFinished(QNetworkReply *reply) {

  reply->deleteLater();

  QByteArray data;

  if (reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
    data = reply->readAll();
  }
  else {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      AuthError(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    }
    else {
      // See if there is Json data containing "error" and "error_description" - then use that instead.
      data = reply->readAll();
      QString error;
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      if (json_error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (json_obj.contains("error") && json_obj.contains("error_description")) {
          QString error_code = json_obj["error"].toString();
          error = json_obj["error_description"].toString();
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
      AuthError(error);
    }
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    AuthError("Json document from server was empty.");
    return;
  }
  
  if (json_obj.contains("error") && json_obj.contains("error_description")) {
    QString error = json_obj["error"].toString();
    QString failure_reason = json_obj["error_description"].toString();
    AuthError(failure_reason);
    return;
  }

  if (!json_obj.contains("access_token") || !json_obj.contains("expires_in") || !json_obj.contains("token_type") || !json_obj.contains("refresh_token")) {
    AuthError("Json access_token, expires_in or token_type is missing.");
    return;
  }

  access_token_ = json_obj["access_token"].toString();
  expires_in_ = json_obj["expires_in"].toInt();
  token_type_ = json_obj["token_type"].toString();
  refresh_token_ = json_obj["refresh_token"].toString();

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("access_token", access_token_);
  s.setValue("expires_in", expires_in_);
  s.setValue("token_type", token_type_);
  s.setValue("refresh_token", refresh_token_);
  s.endGroup();

  emit AuthenticationComplete(true);

  DoSubmit();

}

QNetworkReply *ListenBrainzScrobbler::CreateRequest(const QUrl &url, const QJsonDocument &json_doc) {

  QNetworkRequest req(url);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  req.setRawHeader("Authorization", QString("Token %1").arg(user_token_).toUtf8());
  QNetworkReply *reply = network_->post(req, json_doc.toJson());

  //qLog(Debug) << "ListenBrainz: Sending request" << json_doc.toJson();

  return reply;

}

QByteArray ListenBrainzScrobbler::GetReplyData(QNetworkReply *reply) {

  QByteArray data;

  if (reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
    data = reply->readAll();
  }
  else {
    if (reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      Error(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    }
    else {
      // See if there is Json data containing "code" and "error" - then use that instead.
      data = reply->readAll();
      QString error;
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      if (json_error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (json_obj.contains("code") && json_obj.contains("error")) {
          int error_code = json_obj["code"].toInt();
          QString error_message = json_obj["error"].toString();
          error = QString("%1 (%2)").arg(error_message).arg(error_code);
        }
        else {
          error = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
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
      if (reply->error() == QNetworkReply::ContentAccessDenied || reply->error() == QNetworkReply::ContentOperationNotPermittedError || reply->error() == QNetworkReply::AuthenticationRequiredError) {
        // Session is probably expired
        Logout();
      }
      Error(error);
    }
    return QByteArray();
  }

  return data;
  
}

void ListenBrainzScrobbler::UpdateNowPlaying(const Song &song) {

  song_playing_ = song;
  timestamp_ = QDateTime::currentDateTime().toTime_t();

  if (!song.is_metadata_good() || !IsAuthenticated() || app_->scrobbler()->IsOffline()) return;

  QString album = song.album();
  album = album.remove(Song::kAlbumRemoveDisc);
  album = album.remove(Song::kAlbumRemoveMisc);

  QJsonObject object_track_metadata;
  if (song.albumartist().isEmpty() || song.albumartist().toLower() == Song::kVariousArtists) {
    object_track_metadata.insert("artist_name", QJsonValue::fromVariant(song.artist()));
  }
  else {
    object_track_metadata.insert("artist_name", QJsonValue::fromVariant(song.albumartist()));
  }

  if (!album.isEmpty())
    object_track_metadata.insert("release_name", QJsonValue::fromVariant(album));

  object_track_metadata.insert("track_name", QJsonValue::fromVariant(song.title()));

  QJsonObject object_listen;
  object_listen.insert("track_metadata", object_track_metadata);

  QJsonArray array_payload;
  array_payload.append(object_listen);

  QJsonObject object;
  object.insert("listen_type", "playing_now");
  object.insert("payload", array_payload);
  QJsonDocument doc(object);

  QUrl url(QString("%1/1/submit-listens").arg(kApiUrl));
  QNetworkReply *reply = CreateRequest(url, doc);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(UpdateNowPlayingRequestFinished(QNetworkReply*)), reply);

}

void ListenBrainzScrobbler::UpdateNowPlayingRequestFinished(QNetworkReply *reply) {

  reply->deleteLater();

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    return;
  }

  if (json_obj.contains("code") && json_obj.contains("error_description")) {
    QString error_code = json_obj["code"].toString();
    QString error_desc = json_obj["error_description"].toString();
    Error(error_desc);
    return;
  }

  if (!json_obj.contains("status")) {
    Error("Missing status from server.", json_obj);
    return;
  }

  QString status = json_obj["status"].toString();
  if (status.toLower() != "ok") {
    Error(status);
  }

}

void ListenBrainzScrobbler::ClearPlaying() {
  song_playing_ = Song();
}

void ListenBrainzScrobbler::Scrobble(const Song &song) {

  if (song.id() != song_playing_.id() || song.url() != song_playing_.url() || !song.is_metadata_good()) return;

  cache_->Add(song, timestamp_);

  if (app_->scrobbler()->IsOffline()) return;

  if (!IsAuthenticated()) {
    emit ErrorMessage("ListenBrainz is not authenticated!");
    return;
  }

  if (!submitted_) {
    submitted_ = true;
    if (app_->scrobbler()->SubmitDelay() <= 0) {
      Submit();
    }
    else {
      qint64 msec = (app_->scrobbler()->SubmitDelay() * 60 * kMsecPerSec);
      DoAfter(this, SLOT(Submit()), msec);
    }
  }

}

void ListenBrainzScrobbler::DoSubmit() {

  if (!submitted_ && cache_->Count() > 0) {
    submitted_ = true;
    qint64 msec = 30000ll;
    if (app_->scrobbler()->SubmitDelay() != 0) msec = (app_->scrobbler()->SubmitDelay() * 60 * kMsecPerSec);
    DoAfter(this, SLOT(Submit()), msec);
  }

}

void ListenBrainzScrobbler::Submit() {

  qLog(Debug) << __PRETTY_FUNCTION__;

  submitted_ = false;

  if (!IsEnabled() || !IsAuthenticated() || app_->scrobbler()->IsOffline()) return;

  QJsonArray array;
  int i(0);
  QList<quint64> list;
  for (ScrobblerCacheItem *item : cache_->List()) {
    if (item->sent_) continue;
    item->sent_ = true;
    ++i;
    list << item->timestamp_;
    QJsonObject object_listen;
    object_listen.insert("listened_at", QJsonValue::fromVariant(item->timestamp_));
    QJsonObject object_track_metadata;
    if (item->albumartist_.isEmpty() || item->albumartist_.toLower() == Song::kVariousArtists)
      object_track_metadata.insert("artist_name", QJsonValue::fromVariant(item->artist_));
    else
      object_track_metadata.insert("artist_name", QJsonValue::fromVariant(item->albumartist_));

    if (!item->album_.isEmpty())
      object_track_metadata.insert("release_name", QJsonValue::fromVariant(item->album_));

    object_track_metadata.insert("track_name", QJsonValue::fromVariant(item->song_));
    object_listen.insert("track_metadata", object_track_metadata);
    array.append(QJsonValue::fromVariant(object_listen));
    if (i >= kScrobblesPerRequest) break;
  }

  if (i <= 0) return;

  QJsonObject object;
  object.insert("listen_type", "import");
  object.insert("payload", array);
  QJsonDocument doc(object);

  QUrl url(QString("%1/1/submit-listens").arg(kApiUrl));
  QNetworkReply *reply = CreateRequest(url, doc);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(ScrobbleRequestFinished(QNetworkReply*, QList<quint64>)), reply, list);

}

void ListenBrainzScrobbler::ScrobbleRequestFinished(QNetworkReply *reply, QList<quint64> list) {

  reply->deleteLater();

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    cache_->ClearSent(list);
    DoSubmit();
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    cache_->ClearSent(list);
    DoSubmit();
    return;
  }

  if (json_obj.contains("code") && json_obj.contains("error_description")) {
    QString error_code = json_obj["code"].toString();
    QString error_desc = json_obj["error_description"].toString();
    Error(error_desc);
    cache_->ClearSent(list);
    DoSubmit();
    return;
  }

  if (json_obj.contains("status")) {
    QString status = json_obj["status"].toString();
    qLog(Debug) << "ListenBrainz: Received scrobble status:" << status;
  }

  cache_->Flush(list);
  DoSubmit();

}

void ListenBrainzScrobbler::AuthError(const QString &error) {
  emit AuthenticationComplete(false, error);
}

void ListenBrainzScrobbler::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "ListenBrainz:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}

