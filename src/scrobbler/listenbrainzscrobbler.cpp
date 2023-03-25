/*
 * Strawberry Music Player
 * Copyright 2018-2023, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QCoreApplication>
#include <QtGlobal>
#include <QDesktopServices>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QDateTime>
#include <QTimer>
#include <QMessageBox>
#include <QSettings>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "core/application.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "core/logging.h"
#include "utilities/timeconstants.h"
#include "internet/localredirectserver.h"
#include "settings/scrobblersettingspage.h"

#include "audioscrobbler.h"
#include "scrobblerservice.h"
#include "scrobblercache.h"
#include "scrobblercacheitem.h"
#include "scrobblemetadata.h"
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

ListenBrainzScrobbler::ListenBrainzScrobbler(Application *app, QObject *parent)
    : ScrobblerService(kName, app, parent),
      app_(app),
      network_(new NetworkAccessManager(this)),
      cache_(new ScrobblerCache(kCacheFile, this)),
      server_(nullptr),
      enabled_(false),
      expires_in_(-1),
      login_time_(0),
      submitted_(false),
      scrobbled_(false),
      timestamp_(0),
      submit_error_(false),
      prefer_albumartist_(false) {

  refresh_login_timer_.setSingleShot(true);
  QObject::connect(&refresh_login_timer_, &QTimer::timeout, this, &ListenBrainzScrobbler::RequestNewAccessToken);

  timer_submit_.setSingleShot(true);
  QObject::connect(&timer_submit_, &QTimer::timeout, this, &ListenBrainzScrobbler::Submit);

  ListenBrainzScrobbler::ReloadSettings();
  LoadSession();

}

ListenBrainzScrobbler::~ListenBrainzScrobbler() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

  if (server_) {
    QObject::disconnect(server_, nullptr, this, nullptr);
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

  s.beginGroup(ScrobblerSettingsPage::kSettingsGroup);
  prefer_albumartist_ = s.value("albumartist", false).toBool();
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
    qint64 time = expires_in_ - (QDateTime::currentDateTime().toSecsSinceEpoch() - static_cast<qint64>(login_time_));
    if (time < 6) time = 6;
    refresh_login_timer_.setInterval(static_cast<int>(time * kMsecPerSec));
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
    QObject::connect(server_, &LocalRedirectServer::Finished, this, &ListenBrainzScrobbler::RedirectArrived);
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
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  QByteArray query = url_query.toString(QUrl::FullyEncoded).toUtf8();
  QNetworkReply *reply = network_->post(req, query);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { AuthenticateReplyFinished(reply); });

}

void ListenBrainzScrobbler::AuthenticateReplyFinished(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
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
      if (json_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (json_obj.contains("error") && json_obj.contains("error_description")) {
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
    refresh_login_timer_.setInterval(static_cast<int>(expires_in_ * kMsecPerSec));
    refresh_login_timer_.start();
  }

  emit AuthenticationComplete(true);

  qLog(Debug) << "ListenBrainz: Authentication was successful, login expires in" << expires_in_;

  StartSubmit();

}

QNetworkReply *ListenBrainzScrobbler::CreateRequest(const QUrl &url, const QJsonDocument &json_doc) {

  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
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
      if (json_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
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

QJsonObject ListenBrainzScrobbler::JsonTrackMetadata(const ScrobbleMetadata &metadata) const {

  QJsonObject object_track_metadata;
  if (prefer_albumartist_) {
    object_track_metadata.insert("artist_name", QJsonValue::fromVariant(metadata.effective_albumartist()));
  }
  else {
    object_track_metadata.insert("artist_name", QJsonValue::fromVariant(metadata.artist));
  }

  if (!metadata.album.isEmpty()) {
    object_track_metadata.insert("release_name", QJsonValue::fromVariant(StripAlbum(metadata.album)));
  }

  object_track_metadata.insert("track_name", QJsonValue::fromVariant(StripTitle(metadata.title)));

  QJsonObject object_additional_info;

  object_additional_info.insert("duration_ms", metadata.length_nanosec / kNsecPerMsec);

  if (metadata.track > 0) {
    object_additional_info.insert("tracknumber", metadata.track);
  }

  object_additional_info.insert("media_player", QCoreApplication::applicationName());
  object_additional_info.insert("media_player_version", QCoreApplication::applicationVersion());
  object_additional_info.insert("submission_client", QCoreApplication::applicationName());
  object_additional_info.insert("submission_client_version", QCoreApplication::applicationVersion());

  QJsonArray artist_mbids;
  if (!metadata.musicbrainz_album_artist_id.isEmpty()) {
    artist_mbids.append(metadata.musicbrainz_album_artist_id);
  }
  if (!metadata.musicbrainz_artist_id.isEmpty() && !artist_mbids.contains(metadata.musicbrainz_artist_id)) {
    artist_mbids.append(metadata.musicbrainz_artist_id);
  }
  if (!metadata.musicbrainz_original_artist_id.isEmpty() && !artist_mbids.contains(metadata.musicbrainz_original_artist_id)) {
    artist_mbids.append(metadata.musicbrainz_original_artist_id);
  }
  if (!artist_mbids.isEmpty()) {
    object_additional_info.insert("artist_mbids", artist_mbids);
  }

  if (!metadata.musicbrainz_album_id.isEmpty()) {
    object_additional_info.insert("release_mbid", metadata.musicbrainz_album_id);
  }
  else if (!metadata.musicbrainz_original_album_id.isEmpty()) {
    object_additional_info.insert("release_mbid", metadata.musicbrainz_original_album_id);
  }

  if (!metadata.musicbrainz_recording_id.isEmpty()) {
    object_additional_info.insert("recording_mbid", metadata.musicbrainz_recording_id);
  }
  if (!metadata.musicbrainz_track_id.isEmpty()) {
    object_additional_info.insert("track_mbid", metadata.musicbrainz_track_id);
  }
  if (!metadata.musicbrainz_work_id.isEmpty()) {
    object_additional_info.insert("work_mbids", QJsonArray() << metadata.musicbrainz_work_id);
  }

  object_track_metadata.insert("additional_info", object_additional_info);

  return object_track_metadata;

}

void ListenBrainzScrobbler::UpdateNowPlaying(const Song &song) {

  CheckScrobblePrevSong();

  song_playing_ = song;
  scrobbled_ = false;
  timestamp_ = QDateTime::currentDateTime().toSecsSinceEpoch();

  if (!song.is_metadata_good() || !IsAuthenticated() || app_->scrobbler()->IsOffline()) return;

  QJsonObject object_listen;
  object_listen.insert("track_metadata", JsonTrackMetadata(ScrobbleMetadata(song)));
  QJsonArray array_payload;
  array_payload.append(object_listen);
  QJsonObject object;
  object.insert("listen_type", "playing_now");
  object.insert("payload", array_payload);
  QJsonDocument doc(object);

  QUrl url(QString("%1/1/submit-listens").arg(kApiUrl));
  QNetworkReply *reply = CreateRequest(url, doc);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { UpdateNowPlayingRequestFinished(reply); });

}

void ListenBrainzScrobbler::UpdateNowPlayingRequestFinished(QNetworkReply *reply) {

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

  if (json_obj.contains("code") && json_obj.contains("error_description")) {
    QString error_desc = json_obj["error_description"].toString();
    Error(error_desc);
    return;
  }

  if (!json_obj.contains("status")) {
    Error("Missing status from server.", json_obj);
    return;
  }

  QString status = json_obj["status"].toString();
  if (status.compare("ok", Qt::CaseInsensitive) != 0) {
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

  StartSubmit();

}

void ListenBrainzScrobbler::StartSubmit(const bool initial) {

  if (!submitted_ && cache_->Count() > 0) {
    if (initial && app_->scrobbler()->SubmitDelay() <= 0 && !submit_error_) {
      if (timer_submit_.isActive()) {
        timer_submit_.stop();
      }
      Submit();
    }
    else if (!timer_submit_.isActive()) {
      int submit_delay = static_cast<int>(std::max(app_->scrobbler()->SubmitDelay(), submit_error_ ? 30 : 5) * kMsecPerSec);
      timer_submit_.setInterval(submit_delay);
      timer_submit_.start();
    }
  }

}

void ListenBrainzScrobbler::Submit() {

  qLog(Debug) << "ListenBrainz: Submitting scrobbles.";

  if (!IsEnabled() || !IsAuthenticated() || app_->scrobbler()->IsOffline()) return;

  QJsonArray array;
  int i = 0;
  QList<quint64> list;
  ScrobblerCacheItemPtrList cache_items = cache_->List();
  for (ScrobblerCacheItemPtr cache_item : cache_items) {
    if (cache_item->sent) continue;
    cache_item->sent = true;
    ++i;
    list << cache_item->timestamp;
    QJsonObject object_listen;
    object_listen.insert("listened_at", QJsonValue::fromVariant(cache_item->timestamp));
    object_listen.insert("track_metadata", JsonTrackMetadata(cache_item->metadata));
    array.append(QJsonValue::fromVariant(object_listen));
    if (i >= kScrobblesPerRequest) break;
  }

  if (i <= 0) return;

  submitted_ = true;

  QJsonObject object;
  object.insert("listen_type", "import");
  object.insert("payload", array);
  QJsonDocument doc(object);

  QUrl url(QString("%1/1/submit-listens").arg(kApiUrl));
  QNetworkReply *reply = CreateRequest(url, doc);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, list]() { ScrobbleRequestFinished(reply, list); });

}

void ListenBrainzScrobbler::ScrobbleRequestFinished(QNetworkReply *reply, const QList<quint64> &list) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  submitted_ = false;

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    cache_->ClearSent(list);
    submit_error_ = true;
    StartSubmit();
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    cache_->ClearSent(list);
    submit_error_ = true;
    StartSubmit();
    return;
  }

  if (json_obj.contains("code") && json_obj.contains("error_description")) {
    QString error_desc = json_obj["error_description"].toString();
    Error(error_desc);
    cache_->ClearSent(list);
    submit_error_ = true;
    StartSubmit();
    return;
  }

  if (json_obj.contains("status")) {
    QString status = json_obj["status"].toString();
    qLog(Debug) << "ListenBrainz: Received scrobble status:" << status;
  }

  cache_->Flush(list);
  submit_error_ = false;
  StartSubmit();

}

void ListenBrainzScrobbler::Love() {

  if (!song_playing_.is_valid() || !song_playing_.is_metadata_good()) return;

  if (!IsAuthenticated()) app_->scrobbler()->ShowConfig();

  if (song_playing_.musicbrainz_recording_id().isEmpty()) {
    qLog(Error) << "ListenBrainz: Missing MusicBrainz recording ID for" << song_playing_.artist() << song_playing_.album() << song_playing_.title();
    return;
  }

  qLog(Debug) << "ListenBrainz: Sending love for song" << song_playing_.artist() << song_playing_.album() << song_playing_.title();

  QJsonObject object;
  object.insert("recording_mbid", song_playing_.musicbrainz_recording_id());
  object.insert("score", 1);

  QUrl url(QString("%1/1/feedback/recording-feedback").arg(kApiUrl));
  QNetworkReply *reply = CreateRequest(url, QJsonDocument(object));
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { LoveRequestFinished(reply); });

}

void ListenBrainzScrobbler::LoveRequestFinished(QNetworkReply *reply) {

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

  if (json_obj.contains("code") && json_obj.contains("error_description")) {
    Error(json_obj["error_description"].toString());
    return;
  }

  if (json_obj.contains("status")) {
    QString status = json_obj["status"].toString();
    qLog(Debug) << "ListenBrainz: Received recording-feedback status:" << status;
  }

}

void ListenBrainzScrobbler::AuthError(const QString &error) {
  emit AuthenticationComplete(false, error);
}

void ListenBrainzScrobbler::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "ListenBrainz:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}

void ListenBrainzScrobbler::CheckScrobblePrevSong() {

  qint64 duration = QDateTime::currentDateTime().toSecsSinceEpoch() - static_cast<qint64>(timestamp_);
  if (duration < 0) duration = 0;

  if (!scrobbled_ && song_playing_.is_metadata_good() && song_playing_.is_radio() && duration > 30) {
    Song song(song_playing_);
    song.set_length_nanosec(duration * kNsecPerSec);
    Scrobble(song);
  }

}
