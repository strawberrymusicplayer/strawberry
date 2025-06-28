/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include <utility>

#include <QCoreApplication>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QDateTime>
#include <QTimer>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "core/logging.h"
#include "core/settings.h"
#include "core/oauthenticator.h"
#include "constants/timeconstants.h"
#include "constants/scrobblersettings.h"

#include "scrobblersettingsservice.h"
#include "scrobblerservice.h"
#include "scrobblercache.h"
#include "scrobblercacheitem.h"
#include "scrobblemetadata.h"
#include "listenbrainzscrobbler.h"

using namespace Qt::Literals::StringLiterals;

const char *ListenBrainzScrobbler::kName = "ListenBrainz";
const char *ListenBrainzScrobbler::kSettingsGroup = "ListenBrainz";

namespace {
constexpr char kOAuthAuthorizeUrl[] = "https://musicbrainz.org/oauth2/authorize";
constexpr char kOAuthAccessTokenUrl[] = "https://musicbrainz.org/oauth2/token";
constexpr char kOAuthRedirectUrl[] = "http://localhost";
constexpr char kOAuthScope[] = "profile;email;tag;rating;collection;submit_isrc;submit_barcode";
constexpr char kApiUrl[] = "https://api.listenbrainz.org";
constexpr char kClientIDB64[] = "b2VBVU53cVNRZXIwZXIwOUZpcWkwUQ==";
constexpr char kClientSecretB64[] = "Uk9GZ2hrZVEzRjNvUHlFaHFpeVdQQQ==";
constexpr char kCacheFile[] = "listenbrainzscrobbler.cache";
constexpr int kScrobblesPerRequest = 10;
}  // namespace

ListenBrainzScrobbler::ListenBrainzScrobbler(const SharedPtr<ScrobblerSettingsService> settings, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : ScrobblerService(QLatin1String(kName), network, settings, parent),
      network_(network),
      oauth_(new OAuthenticator(network, this)),
      cache_(new ScrobblerCache(QLatin1String(kCacheFile), this)),
      timer_submit_(new QTimer(this)),
      enabled_(false),
      submitted_(false),
      scrobbled_(false),
      timestamp_(0),
      submit_error_(false),
      prefer_albumartist_(false) {

  oauth_->set_settings_group(QLatin1String(kSettingsGroup));
  oauth_->set_type(OAuthenticator::Type::Authorization_Code);
  oauth_->set_authorize_url(QUrl(QLatin1String(kOAuthAuthorizeUrl)));
  oauth_->set_redirect_url(QUrl(QLatin1String(kOAuthRedirectUrl)));
  oauth_->set_access_token_url(QUrl(QLatin1String(kOAuthAccessTokenUrl)));
  oauth_->set_client_id(QString::fromLatin1(QByteArray::fromBase64(kClientIDB64)));
  oauth_->set_client_secret(QString::fromLatin1(QByteArray::fromBase64(kClientSecretB64)));
  oauth_->set_scope(QLatin1String(kOAuthScope));
  oauth_->set_use_local_redirect_server(true);
  oauth_->set_random_port(true);

  QObject::connect(oauth_, &OAuthenticator::AuthenticationFinished, this, &ListenBrainzScrobbler::OAuthFinished);

  timer_submit_->setSingleShot(true);
  QObject::connect(timer_submit_, &QTimer::timeout, this, &ListenBrainzScrobbler::Submit);

  ListenBrainzScrobbler::ReloadSettings();
  oauth_->LoadSession();

}

bool ListenBrainzScrobbler::authenticated() const {

  return !oauth_->access_token().isEmpty() && !user_token_.isEmpty();

}

void ListenBrainzScrobbler::ReloadSettings() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  enabled_ = s.value(ScrobblerSettings::kEnabled, false).toBool();
  user_token_ = s.value(ScrobblerSettings::kUserToken).toString();
  s.endGroup();

  s.beginGroup(ScrobblerSettings::kSettingsGroup);
  prefer_albumartist_ = s.value(ScrobblerSettings::kAlbumArtist, false).toBool();
  s.endGroup();

}

void ListenBrainzScrobbler::Authenticate() {

  oauth_->Authenticate();

}

void ListenBrainzScrobbler::Deauthenticate() {

  oauth_->ClearSession();

}

void ListenBrainzScrobbler::Logout() {

  Deauthenticate();

}

void ListenBrainzScrobbler::OAuthFinished(const bool success, const QString &error) {

  if (success) {
    qLog(Debug) << "ListenBrainz: Authentication was successful, login expires in" << oauth_->expires_in();
    Q_EMIT AuthenticationComplete(true);
    StartSubmit();
  }
  else {
    qLog(Debug) << "ListenBrainz: Authentication failed:" << error;
    Q_EMIT AuthenticationComplete(false, error);
  }

}

QNetworkReply *ListenBrainzScrobbler::CreateRequest(const QUrl &url, const QJsonDocument &json_document) {

  return CreatePostRequest(url, json_document);

}

QJsonObject ListenBrainzScrobbler::JsonTrackMetadata(const ScrobbleMetadata &metadata) const {

  QJsonObject object_track_metadata;
  if (prefer_albumartist_) {
    object_track_metadata.insert("artist_name"_L1, QJsonValue::fromVariant(metadata.effective_albumartist()));
  }
  else {
    object_track_metadata.insert("artist_name"_L1, QJsonValue::fromVariant(metadata.artist));
  }

  if (!metadata.album.isEmpty()) {
    object_track_metadata.insert("release_name"_L1, QJsonValue::fromVariant(StripAlbum(metadata.album)));
  }

  object_track_metadata.insert("track_name"_L1, QJsonValue::fromVariant(StripTitle(metadata.title)));

  QJsonObject object_additional_info;

  if (metadata.length_nanosec > 0) {
    object_additional_info.insert("duration_ms"_L1, metadata.length_nanosec / kNsecPerMsec);
  }

  if (metadata.track > 0) {
    object_additional_info.insert("tracknumber"_L1, metadata.track);
  }

  object_additional_info.insert("media_player"_L1, QCoreApplication::applicationName());
  object_additional_info.insert("media_player_version"_L1, QCoreApplication::applicationVersion());
  object_additional_info.insert("submission_client"_L1, QCoreApplication::applicationName());
  object_additional_info.insert("submission_client_version"_L1, QCoreApplication::applicationVersion());

  QStringList artist_mbids_list;
  if (!metadata.musicbrainz_album_artist_id.isEmpty()) {
    artist_mbids_list << metadata.musicbrainz_album_artist_id.split(u'/');
  }
  if (!metadata.musicbrainz_artist_id.isEmpty()) {
    artist_mbids_list << metadata.musicbrainz_artist_id.split(u'/');
  }
  if (!metadata.musicbrainz_original_artist_id.isEmpty()) {
    artist_mbids_list << metadata.musicbrainz_original_artist_id.split(u'/');
  }
  if (!artist_mbids_list.isEmpty()) {
    QJsonArray artist_mbids_array;
    for (const QString &musicbrainz_artist_id : std::as_const(artist_mbids_list)) {
      if (!musicbrainz_artist_id.isEmpty() && !artist_mbids_array.contains(musicbrainz_artist_id)) {
        artist_mbids_array.append(musicbrainz_artist_id);
      }
    }
    if (!artist_mbids_array.isEmpty()) {
      object_additional_info.insert("artist_mbids"_L1, artist_mbids_array);
    }
  }

  if (!metadata.musicbrainz_album_id.isEmpty()) {
    object_additional_info.insert("release_mbid"_L1, metadata.musicbrainz_album_id);
  }
  else if (!metadata.musicbrainz_original_album_id.isEmpty()) {
    object_additional_info.insert("release_mbid"_L1, metadata.musicbrainz_original_album_id);
  }

  if (!metadata.musicbrainz_recording_id.isEmpty()) {
    object_additional_info.insert("recording_mbid"_L1, metadata.musicbrainz_recording_id);
  }
  if (!metadata.musicbrainz_track_id.isEmpty()) {
    object_additional_info.insert("track_mbid"_L1, metadata.musicbrainz_track_id);
  }
  if (!metadata.musicbrainz_work_id.isEmpty()) {
    const QStringList musicbrainz_work_id_list = metadata.musicbrainz_work_id.split(u'/');
    QJsonArray array_musicbrainz_work_id;
    for (const QString &musicbrainz_work_id : musicbrainz_work_id_list) {
      array_musicbrainz_work_id << musicbrainz_work_id;
    }
    object_additional_info.insert("work_mbids"_L1, array_musicbrainz_work_id);
  }

  if (!metadata.music_service.isEmpty()) {
    object_additional_info.insert("music_service"_L1, metadata.music_service);
  }
  if (!metadata.music_service_name.isEmpty()) {
    object_additional_info.insert("music_service_name"_L1, metadata.music_service_name);
  }

  if (!metadata.share_url.isEmpty()) {
    object_additional_info.insert("origin_url"_L1,  metadata.share_url);
  }

  if (!metadata.spotify_id.isEmpty()) {
    object_additional_info.insert("spotify_id"_L1, metadata.spotify_id);
  }

  object_track_metadata.insert("additional_info"_L1, object_additional_info);

  return object_track_metadata;

}

JsonBaseRequest::JsonObjectResult ListenBrainzScrobbler::ParseJsonObject(QNetworkReply *reply) {

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
      if (json_object.contains("code"_L1) && json_object.contains("error"_L1)) {
        const int code = json_object["code"_L1].toInt();
        const QString error = json_object["error"_L1].toString();
        result.error_code = ErrorCode::APIError;
        result.error_message = QStringLiteral("%1 (%2)").arg(error).arg(code);
      }
      else if (json_object.contains("error"_L1) && json_object.contains("error_description"_L1)) {
        const int error = json_object["error"_L1].toInt();
        const QString error_description = json_object["error_description"_L1].toString();
        result.error_code = ErrorCode::APIError;
        result.error_message = QStringLiteral("%1 (%2)").arg(error_description).arg(error);
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

  if (reply->error() == QNetworkReply::AuthenticationRequiredError) {
    oauth_->ClearSession();
  }

  return result;

}

void ListenBrainzScrobbler::UpdateNowPlaying(const Song &song) {

  CheckScrobblePrevSong();

  song_playing_ = song;
  scrobbled_ = false;
  timestamp_ = static_cast<quint64>(QDateTime::currentSecsSinceEpoch());

  if (!song.is_metadata_good() || !authenticated() || settings_->offline()) return;

  QJsonObject object_listen;
  object_listen.insert("track_metadata"_L1, JsonTrackMetadata(ScrobbleMetadata(song)));
  QJsonArray array_payload;
  array_payload.append(object_listen);
  QJsonObject object;
  object.insert("listen_type"_L1, "playing_now"_L1);
  object.insert("payload"_L1, array_payload);
  QJsonDocument json_document(object);

  QNetworkReply *reply = CreateRequest(QUrl(QStringLiteral("%1/1/submit-listens").arg(QLatin1String(kApiUrl))), json_document);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { UpdateNowPlayingRequestFinished(reply); });

}

void ListenBrainzScrobbler::UpdateNowPlayingRequestFinished(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }
  const QJsonObject &json_object = json_object_result.json_object;

  if (!json_object.contains("status"_L1)) {
    Error(u"Now playing request is missing status from server."_s);
    return;
  }

  const QString status = json_object["status"_L1].toString();
  if (status.compare("ok"_L1, Qt::CaseInsensitive) != 0) {
    Error(QStringLiteral("Received %1 status for now playing.").arg(status));
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

  if (settings_->offline() || !authenticated()) return;

  StartSubmit();

}

void ListenBrainzScrobbler::StartSubmit(const bool initial) {

  if (!submitted_ && cache_->Count() > 0) {
    if (initial && settings_->submit_delay() <= 0 && !submit_error_) {
      if (timer_submit_->isActive()) {
        timer_submit_->stop();
      }
      Submit();
    }
    else if (!timer_submit_->isActive()) {
      int submit_delay = static_cast<int>(std::max(settings_->submit_delay(), submit_error_ ? 30 : 5) * kMsecPerSec);
      timer_submit_->setInterval(submit_delay);
      timer_submit_->start();
    }
  }

}

void ListenBrainzScrobbler::Submit() {

  qLog(Debug) << "ListenBrainz: Submitting scrobbles.";

  if (!enabled() || !authenticated() || settings_->offline()) return;

  QJsonArray array;
  ScrobblerCacheItemPtrList cache_items_sent;
  const ScrobblerCacheItemPtrList all_cache_items = cache_->List();
  for (ScrobblerCacheItemPtr cache_item : all_cache_items) {
    if (cache_item->sent) continue;
    if (cache_item->error && cache_items_sent.count() > 0) break;
    cache_item->sent = true;
    cache_items_sent << cache_item;
    QJsonObject object_listen;
    object_listen.insert("listened_at"_L1, QJsonValue::fromVariant(cache_item->timestamp));
    object_listen.insert("track_metadata"_L1, JsonTrackMetadata(cache_item->metadata));
    array.append(QJsonValue::fromVariant(object_listen));
    if (cache_items_sent.count() >= kScrobblesPerRequest || cache_item->error) break;
  }

  if (cache_items_sent.count() <= 0) return;

  submitted_ = true;

  QJsonObject object;
  object.insert("listen_type"_L1, "import"_L1);
  object.insert("payload"_L1, array);
  QJsonDocument doc(object);

  const QUrl url(QStringLiteral("%1/1/submit-listens").arg(QLatin1String(kApiUrl)));
  QNetworkReply *reply = CreateRequest(url, doc);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, cache_items_sent]() { ScrobbleRequestFinished(reply, cache_items_sent); });

}

void ListenBrainzScrobbler::ScrobbleRequestFinished(QNetworkReply *reply, ScrobblerCacheItemPtrList cache_items) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  submitted_ = false;

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (json_object_result.success()) {
    const QJsonObject &json_object = json_object_result.json_object;
    if (json_object.contains("status"_L1)) {
      const QString status = json_object["status"_L1].toString();
      qLog(Debug) << "ListenBrainz: Received scrobble status:" << status;
    }
    else {
      qLog(Debug) << "ListenBrainz: Received scrobble reply without status.";
    }
    cache_->Flush(cache_items);
    submit_error_ = false;
  }
  else {
    submit_error_ = true;
    if (json_object_result.error_code == ErrorCode::APIError) {
      if (cache_items.count() == 1) {
        const ScrobbleMetadata &metadata = cache_items.first()->metadata;
        Error(tr("Unable to scrobble %1 - %2 because of error: %3").arg(metadata.effective_albumartist(), metadata.title, json_object_result.error_message));
        cache_->Flush(cache_items);
      }
      else {
        Error(json_object_result.error_message);
        cache_->SetError(cache_items);
        cache_->ClearSent(cache_items);
      }
    }
    else {
      Error(json_object_result.error_message);
      cache_->ClearSent(cache_items);
    }
  }

  StartSubmit();

}

void ListenBrainzScrobbler::Love() {

  if (!song_playing_.is_valid() || !song_playing_.is_metadata_good()) return;

  if (!authenticated()) {
    Q_EMIT OpenSettingsDialog();
    return;
  }

  if (song_playing_.musicbrainz_recording_id().isEmpty()) {
    Error(tr("Missing MusicBrainz recording ID for %1 %2 %3").arg(song_playing_.artist(), song_playing_.album(), song_playing_.title()));
    return;
  }

  qLog(Debug) << "ListenBrainz: Sending love for song" << song_playing_.artist() << song_playing_.album() << song_playing_.title();

  QJsonObject object;
  object.insert("recording_mbid"_L1, song_playing_.musicbrainz_recording_id());
  object.insert("score"_L1, 1);

  QUrl url(QStringLiteral("%1/1/feedback/recording-feedback").arg(QLatin1String(kApiUrl)));
  QNetworkReply *reply = CreateRequest(url, QJsonDocument(object));
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { LoveRequestFinished(reply); });

}

void ListenBrainzScrobbler::LoveRequestFinished(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }
  const QJsonObject &json_object = json_object_result.json_object;

  if (json_object.contains("status"_L1)) {
    qLog(Debug) << "ListenBrainz: Received recording-feedback status:" << json_object["status"_L1].toString();
  }

}

void ListenBrainzScrobbler::Error(const QString &error_message, const QVariant &debug_output) {

  JsonBaseRequest::Error(error_message, debug_output);

  if (settings_->show_error_dialog()) {
    Q_EMIT ErrorMessage(tr("ListenBrainz error: %1").arg(error_message));
  }

}

void ListenBrainzScrobbler::CheckScrobblePrevSong() {

  const qint64 duration = std::max(0LL, QDateTime::currentSecsSinceEpoch() - static_cast<qint64>(timestamp_));

  if (!scrobbled_ && song_playing_.is_metadata_good() && song_playing_.is_radio() && duration > 30) {
    Song song(song_playing_);
    song.set_length_nanosec(duration * kNsecPerSec);
    Scrobble(song);
  }

}
