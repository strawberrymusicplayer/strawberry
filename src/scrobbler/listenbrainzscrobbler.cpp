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
#include <QTimer>
#include <QtDebug>

#include "core/application.h"
#include "core/network.h"
#include "core/song.h"
#include "core/timeconstants.h"
#include "core/logging.h"
#include "core/closure.h"
#include "internet/localredirectserver.h"

#include "audioscrobbler.h"
#include "scrobblerservice.h"
#include "scrobblercache.h"
#include "scrobblercacheitem.h"
#include "listenbrainzscrobbler.h"

const char *ListenBrainzScrobbler::kName = "ListenBrainz";
const char *ListenBrainzScrobbler::kSettingsGroup = "ListenBrainz";
const char *ListenBrainzScrobbler::kOAuthAuthorizeUrl = "https://musicbrainz.org/oauth2/authorize";
const char *ListenBrainzScrobbler::kOAuthAccessTokenUrl = "https://musicbrainz.org/oauth2/token";
const char *ListenBrainzScrobbler::kOAuthRedirectUrl = "http://localhost";
const char *ListenBrainzScrobbler::kApiUrl = "https://api.listenbrainz.org";
const char *ListenBrainzScrobbler::kClientIDB64 = "b2VBVU53cVNRZXIwZXIwOUZpcWkwUQ==";
const char *ListenBrainzScrobbler::kClientSecretB64 = "Uk9GZ2hrZVEzRjNvUHlFaHFpeVdQQQ==";
const char *ListenBrainzScrobbler::kCacheFile = "listenbrainzscrobbler.cache";
const int ListenBrainzScrobbler::kScrobblesPerRequest = 10;

ListenBrainzScrobbler::ListenBrainzScrobbler(Application *app, QObject *parent) : ScrobblerService(kName, app, parent),
  app_(app),
  network_(new NetworkAccessManager(this)),
  cache_(new ScrobblerCache(kCacheFile, this)),
  server_(nullptr),
  enabled_(false),
  expires_in_(-1),
  login_time_(0),
  submitted_(false),
  scrobbled_(false),
  timestamp_(0) {

  refresh_login_timer_.setSingleShot(true);
  connect(&refresh_login_timer_, SIGNAL(timeout()), SLOT(RequestAccessToken()));

  ReloadSettings();
  LoadSession();

}

ListenBrainzScrobbler::~ListenBrainzScrobbler() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

  if (server_) {
    disconnect(server_, nullptr, this, nullptr);
    if (server_->isListening()) server_->close();
    server_->deleteLater();
  }

}

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
  login_time_ = s.value("login_time").toLongLong();
  s.endGroup();

  if (!refresh_token_.isEmpty()) {
    qint64 time = expires_in_ - (QDateTime::currentDateTime().toSecsSinceEpoch() - login_time_);
    if (time < 6) time = 6;
    refresh_login_timer_.setInterval(time * kMsecPerSec);
    refresh_login_timer_.start();
  }

}

void ListenBrainzScrobbler::Logout() {

  access_token_.clear();
  token_type_.clear();
  refresh_token_.clear();
  expires_in_ = -1;
  login_time_ = 0;

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
    server_ = new LocalRedirectServer(this);
    server_->set_https(https);
    if (!server_->Listen()) {
      AuthError(server_->error());
      delete server_;
      server_ = nullptr;
      return;
    }
    connect(server_, SIGNAL(Finished()), this, SLOT(RedirectArrived()));
  }

  QUrl redirect_url(kOAuthRedirectUrl);
  redirect_url.setPort(server_->url().port());

  QUrlQuery url_query;
  url_query.addQueryItem("response_type", "code");
  url_query.addQueryItem("client_id", QByteArray::fromBase64(kClientIDB64));
  url_query.addQueryItem("redirect_uri", redirect_url.toString());
  url_query.addQueryItem("scope", "profile;email;tag;rating;collection;submit_isrc;submit_barcode");
  QUrl url(kOAuthAuthorizeUrl);
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
        RequestAccessToken(url, url_query.queryItemValue("code"));
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

void ListenBrainzScrobbler::RequestAccessToken(const QUrl &redirect_url, const QString &code) {

  refresh_login_timer_.stop();

  ParamList params = ParamList() << Param("client_id", QByteArray::fromBase64(kClientIDB64))
                                 << Param("client_secret", QByteArray::fromBase64(kClientSecretB64));

  if (!code.isEmpty() && !redirect_url.isEmpty()) {
    params << Param("grant_type", "authorization_code");
    params << Param("code", code);
    params << Param("redirect_uri", redirect_url.toString());
  }
  else if (!refresh_token_.isEmpty() && enabled_) {
    params << Param("grant_type", "refresh_token");
    params << Param("refresh_token", refresh_token_);
  }
  else {
    return;
  }

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
  }

  QUrl session_url(kOAuthAccessTokenUrl);

  QNetworkRequest req(session_url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#else
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  QByteArray query = url_query.toString(QUrl::FullyEncoded).toUtf8();
  QNetworkReply *reply = network_->post(req, query);
  replies_ << reply;
  connect(reply, &QNetworkReply::finished, [=] { AuthenticateReplyFinished(reply); });

}

void ListenBrainzScrobbler::AuthenticateReplyFinished(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  disconnect(reply, nullptr, this, nullptr);
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

  if (!json_obj.contains("access_token") || !json_obj.contains("expires_in") || !json_obj.contains("token_type")) {
    AuthError("Json access_token, expires_in or token_type is missing.");
    return;
  }

  access_token_ = json_obj["access_token"].toString();
  expires_in_ = json_obj["expires_in"].toInt();
  token_type_ = json_obj["token_type"].toString();
  if (json_obj.contains("refresh_token")) {
    refresh_token_ = json_obj["refresh_token"].toString();
  }
  login_time_ = QDateTime::currentDateTime().toSecsSinceEpoch();

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("access_token", access_token_);
  s.setValue("expires_in", expires_in_);
  s.setValue("token_type", token_type_);
  s.setValue("refresh_token", refresh_token_);
  s.setValue("login_time", login_time_);
  s.endGroup();

  if (expires_in_ > 0) {
    refresh_login_timer_.setInterval(expires_in_ * kMsecPerSec);
    refresh_login_timer_.start();
  }

  emit AuthenticationComplete(true);

  qLog(Debug) << "ListenBrainz: Authentication was successful, got access token" << access_token_ << "expires in" << expires_in_;

  DoSubmit();

}

QNetworkReply *ListenBrainzScrobbler::CreateRequest(const QUrl &url, const QJsonDocument &json_doc) {

  QNetworkRequest req(url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#else
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
#endif
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
  req.setRawHeader("Authorization", QString("Token %1").arg(user_token_).toUtf8());
  QNetworkReply *reply = network_->post(req, json_doc.toJson());
  replies_ << reply;

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

  CheckScrobblePrevSong();

  song_playing_ = song;
  scrobbled_ = false;
  timestamp_ = QDateTime::currentDateTime().toSecsSinceEpoch();

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
  connect(reply, &QNetworkReply::finished, [=] { UpdateNowPlayingRequestFinished(reply); });

}

void ListenBrainzScrobbler::UpdateNowPlayingRequestFinished(QNetworkReply *reply) {

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

  CheckScrobblePrevSong();
  song_playing_ = Song();
  scrobbled_ = false;
  timestamp_ = 0;

}

void ListenBrainzScrobbler::Scrobble(const Song &song) {

  if (song.id() != song_playing_.id() || song.url() != song_playing_.url() || !song.is_metadata_good()) return;

  scrobbled_ = true;

  cache_->Add(song, timestamp_);

  if (app_->scrobbler()->IsOffline() || !IsAuthenticated()) return;

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

  qLog(Debug) << "ListenBrainz: Submitting scrobbles.";

  submitted_ = false;

  if (!IsEnabled() || !IsAuthenticated() || app_->scrobbler()->IsOffline()) return;

  QJsonArray array;
  int i(0);
  QList<quint64> list;
  for (ScrobblerCacheItemPtr item : cache_->List()) {
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
  connect(reply, &QNetworkReply::finished, [=] { ScrobbleRequestFinished(reply, list); });

}

void ListenBrainzScrobbler::ScrobbleRequestFinished(QNetworkReply *reply, QList<quint64> list) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  disconnect(reply, nullptr, this, nullptr);
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

void ListenBrainzScrobbler::CheckScrobblePrevSong() {

  quint64 duration = QDateTime::currentDateTime().toSecsSinceEpoch() - timestamp_;

  if (!scrobbled_ && song_playing_.is_metadata_good() && song_playing_.source() == Song::Source_Stream && duration > 30) {
    Song song(song_playing_);
    song.set_length_nanosec(duration * kNsecPerSec);
    Scrobble(song);
  }

}
