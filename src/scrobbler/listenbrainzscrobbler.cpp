/*
 * Strawberry Music Player
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include "apicredentials.h"

#include <algorithm>
#include <utility>

#include <QCoreApplication>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QDateTime>
#include <QTimer>
#include <QElapsedTimer>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include "includes/shared_ptr.h"
#include "utilities/cryptutils.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "core/logging.h"
#include "core/settings.h"
#include "core/oauthenticator.h"
#include "constants/timeconstants.h"
#include "constants/scrobblersettings.h"
#include "constants/listenbrainzsettings.h"

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
constexpr char kCacheFile[] = "listenbrainzscrobbler.cache";
constexpr int kScrobblesPerRequest = 10;
// ListenBrainz allows one request per second, this is the floor kept between requests when the server has not asked us to wait longer.
constexpr int kMinRequestIntervalMsec = 1200;
// Used when the server rate limits us without saying how much of the current window is left.
constexpr int kDefaultRateLimitDelay = 5;
// The delay is bounded before it is narrowed to int for the timers.
// A bogus X-RateLimit-Reset-In value, or the system clock jumping backwards, would otherwise overflow the cast and give a negative interval, which the timers would treat as no delay at all.
constexpr qint64 kMaxRateLimitDelayMsec = 600 * kMsecPerSec;
// How many times a love is put back on the queue before giving up on it.
constexpr int kMaxLoveAttempts = 3;

// Rate limiting and server errors are temporary, so the request may succeed on a later attempt and nothing may be discarded because of them.
bool IsRetryableHttpStatusCode(const int http_status_code) {
  return http_status_code == 429 || http_status_code >= 500;
}

int RateLimitInterval(const qint64 delay_msec) {
  return static_cast<int>(std::clamp(delay_msec, static_cast<qint64>(0), kMaxRateLimitDelayMsec));
}

QString CompiledClientId() {
#ifdef LISTENBRAINZ_CLIENT_ID
  return Utilities::MaybeDecryptApiCredential(QStringLiteral(LISTENBRAINZ_CLIENT_ID));
#else
  return QString();
#endif
}

QString CompiledClientSecret() {
#ifdef LISTENBRAINZ_CLIENT_SECRET
  return Utilities::MaybeDecryptApiCredential(QStringLiteral(LISTENBRAINZ_CLIENT_SECRET));
#else
  return QString();
#endif
}
}  // namespace

bool ListenBrainzScrobbler::HasCompiledCredentials() {
#if defined(LISTENBRAINZ_CLIENT_ID) && defined(LISTENBRAINZ_CLIENT_SECRET)
  return true;
#else
  return false;
#endif
}

ListenBrainzScrobbler::ListenBrainzScrobbler(const SharedPtr<ScrobblerSettingsService> settings, const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : ScrobblerService(QLatin1String(kName), network, settings, parent),
      network_(network),
      oauth_(new OAuthenticator(network, this)),
      cache_(new ScrobblerCache(QLatin1String(kCacheFile), this)),
      timer_flush_requests_(new QTimer(this)),
      enabled_(false),
      api_credentials_initialized_(false),
      submitted_(false),
      scrobbled_(false),
      timestamp_(0),
      submit_error_(false),
      next_request_time_(0),
      now_playing_pending_(false),
      scrobbles_due_time_(0),
      prefer_albumartist_(false) {

  oauth_->set_settings_group(QLatin1String(kSettingsGroup));
  oauth_->set_type(OAuthenticator::Type::Authorization_Code);
  oauth_->set_authorize_url(QUrl(QLatin1String(kOAuthAuthorizeUrl)));
  oauth_->set_redirect_url(QUrl(QLatin1String(kOAuthRedirectUrl)));
  oauth_->set_access_token_url(QUrl(QLatin1String(kOAuthAccessTokenUrl)));
  oauth_->set_scope(QLatin1String(kOAuthScope));
  oauth_->set_use_local_redirect_server(true);
  oauth_->set_random_port(true);

  QObject::connect(oauth_, &OAuthenticator::AuthenticationFinished, this, &ListenBrainzScrobbler::OAuthFinished);

  timer_flush_requests_->setSingleShot(true);
  QObject::connect(timer_flush_requests_, &QTimer::timeout, this, &ListenBrainzScrobbler::FlushRequests);

  rate_limit_timer_.start();

  ListenBrainzScrobbler::ReloadSettings();
  oauth_->LoadSession();

}

bool ListenBrainzScrobbler::authenticated() const {

  return !oauth_->access_token().isEmpty() && !user_token_.isEmpty();

}

void ListenBrainzScrobbler::ReloadSettings() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  enabled_ = s.value(ScrobblerSettings::kEnabled, ScrobblerSettings::kDefaultEnabled).toBool();
  user_token_ = s.value(ScrobblerSettings::kUserToken).toString();
  const bool use_custom_api_credentials = !HasCompiledCredentials() || s.value(ListenBrainzSettings::kUseCustomApiCredentials, false).toBool();
  const QString client_id = use_custom_api_credentials ? s.value(ListenBrainzSettings::kClientId).toString() : CompiledClientId();
  const QString client_secret = use_custom_api_credentials ? s.value(ListenBrainzSettings::kClientSecret).toString() : CompiledClientSecret();
  s.endGroup();

  const bool api_credentials_changed = api_credentials_initialized_ && (client_id != client_id_ || client_secret != client_secret_);
  client_id_ = client_id;
  client_secret_ = client_secret;
  oauth_->set_client_id(client_id_);
  oauth_->set_client_secret(client_secret_);
  if (api_credentials_changed || client_id_.isEmpty() || client_secret_.isEmpty()) {
    oauth_->ClearSession();
  }
  api_credentials_initialized_ = true;

  s.beginGroup(ScrobblerSettings::kSettingsGroup);
  prefer_albumartist_ = s.value(ScrobblerSettings::kAlbumArtist, ScrobblerSettings::kDefaultAlbumArtist).toBool();
  s.endGroup();

  // Enabling the service here is the one way it is switched back on without going through Start(), so anything left pending has to be picked up again.
  ScheduleFlushRequests();

}

void ListenBrainzScrobbler::Authenticate() {

  if (client_id_.isEmpty() || client_secret_.isEmpty()) {
    const QString error = tr("Missing ListenBrainz client ID and/or client secret");
    qLog(Error) << error;
    Q_EMIT AuthenticationComplete(false, error);
    return;
  }

  oauth_->Authenticate();

}

void ListenBrainzScrobbler::Deauthenticate() {

  oauth_->ClearSession();

}

void ListenBrainzScrobbler::Logout() {

  Deauthenticate();

}

void ListenBrainzScrobbler::OAuthFinished(const bool success, const QString &error, const bool invalid_grant) {

  if (success) {
    qLog(Debug) << "ListenBrainz: Authentication was successful, login expires in" << oauth_->expires_in();
    Q_EMIT AuthenticationComplete(true);
    Start();
  }
  else {
    qLog(Debug) << "ListenBrainz: Authentication failed:" << error;
    if (invalid_grant) {
      qLog(Debug) << "ListenBrainz: Authorization grant is no longer valid; OAuth session was cleared and the user must authenticate again.";
    }
    Q_EMIT AuthenticationComplete(false, error);
  }

}

QNetworkReply *ListenBrainzScrobbler::CreateRequest(const QUrl &url, const QJsonDocument &json_document) {

  ReserveRequestSlot();
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
  if (!metadata.musicbrainz_release_group_id.isEmpty()) {
    object_additional_info.insert("release_group_mbid"_L1, metadata.musicbrainz_release_group_id);
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
    object_additional_info.insert("origin_url"_L1, metadata.share_url);
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
  now_playing_pending_ = false;

  if (!song.is_metadata_good() || !authenticated() || settings_->offline()) return;

  // Record that an update is due rather than sending it from here.
  // The flush loop sends whichever song is current when a slot opens, so a song change before then simply replaces this one instead of sending a stale update.
  now_playing_pending_ = true;
  ScheduleFlushRequests();

}

void ListenBrainzScrobbler::SendNowPlaying(const Song &song) {

  if (!song.is_valid() || !song.is_metadata_good()) return;

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

  UpdateRateLimit(reply);

  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  // ListenBrainz frequently close the connection, ignore any connection closed errors to avoid error popups
  if (reply->error() == QNetworkReply::NetworkError::RemoteHostClosedError) {
    JsonBaseRequest::Error(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    return;
  }

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
  now_playing_pending_ = false;

}

void ListenBrainzScrobbler::Scrobble(const Song &song) {

  if (song.id() != song_playing_.id() || song.url() != song_playing_.url() || !song.is_metadata_good()) return;

  scrobbled_ = true;

  cache_->Add(song, timestamp_);

  if (settings_->offline() || !authenticated()) return;

  Start();

}

void ListenBrainzScrobbler::Stop() {

  timer_flush_requests_->stop();

  // A now playing update describes what is playing at this moment, so it is dropped rather than held on to.
  // The queued loves and the cached scrobbles are kept, since those are still valid whenever they are sent.
  now_playing_pending_ = false;

}

void ListenBrainzScrobbler::Start(const bool initial) {

  if (!submitted_ && cache_->Count() > 0) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    // Nothing in this class passes initial as true, so scrobbles here always wait out the delay below.
    // LastFMScrobbler::Scrobble() does pass it, and so submits as soon as the song is scrobbled when the submit delay is zero, which is the default.
    if (initial && settings_->submit_delay() <= 0 && !submit_error_) {
      scrobbles_due_time_ = now;
    }
    else if (scrobbles_due_time_ <= now) {
      // Only start a new delay when one is not already running, otherwise scrobbling another song would keep pushing the submission back.
      scrobbles_due_time_ = now + static_cast<qint64>(std::max(settings_->submit_delay(), submit_error_ ? 30 : 5)) * kMsecPerSec;
    }
  }

  ScheduleFlushRequests();

}

qint64 ListenBrainzScrobbler::NextFlushRequestsDelay() const {

  qint64 delay = -1;

  // A now playing update and a love are ready as soon as the rate limiter allows.
  if (now_playing_pending_ || !queue_love_.isEmpty()) {
    delay = 0;
  }

  // Scrobbles additionally wait out the submit delay, so that several listens are sent in one request.
  if (!submitted_ && cache_->Count() > 0) {
    const qint64 scrobbles_delay = std::max(static_cast<qint64>(0), scrobbles_due_time_ - QDateTime::currentMSecsSinceEpoch());
    delay = delay < 0 ? scrobbles_delay : std::min(delay, scrobbles_delay);
  }

  if (delay < 0) return -1;

  return std::max(delay, RateLimitDelay());

}

void ListenBrainzScrobbler::ScheduleFlushRequests() {

  const qint64 delay = NextFlushRequestsDelay();
  if (delay < 0) {
    if (timer_flush_requests_->isActive()) {
      timer_flush_requests_->stop();
    }
    return;
  }

  timer_flush_requests_->setInterval(RateLimitInterval(delay));
  timer_flush_requests_->start();

}

void ListenBrainzScrobbler::FlushRequests() {

  if (!settings_->enabled() || settings_->offline() || !enabled() || !authenticated()) return;

  if (RateLimitDelay() <= 0) {
    // Send one request per pass, ordered by how quickly the work goes stale.
    // A now playing update describes what is playing right now, a love is a user action waiting to take effect, and scrobbles are cached and can wait.
    if (now_playing_pending_) {
      now_playing_pending_ = false;
      SendNowPlaying(song_playing_);
    }
    else if (!queue_love_.isEmpty()) {
      SendLove(queue_love_.dequeue());
    }
    else if (!submitted_ && cache_->Count() > 0 && scrobbles_due_time_ <= QDateTime::currentMSecsSinceEpoch()) {
      SubmitListens();
    }
  }

  ScheduleFlushRequests();

}

void ListenBrainzScrobbler::ReserveRequestSlot() {

  const qint64 now = rate_limit_timer_.elapsed();
  next_request_time_ = std::max(next_request_time_, now) + kMinRequestIntervalMsec;

}

void ListenBrainzScrobbler::UpdateRateLimit(QNetworkReply *reply) {

  // ListenBrainz reports how many requests are left in the current window and when it expires, and expects clients to use those to decide when to send the next request.
  // Reset-In is preferred over Reset because it is not affected by clock skew between the client and the server.
  bool remaining_ok = false;
  const int remaining = reply->rawHeader(QByteArray("X-RateLimit-Remaining")).toInt(&remaining_ok);

  bool reset_in_ok = false;
  const int reset_in = reply->rawHeader(QByteArray("X-RateLimit-Reset-In")).toInt(&reset_in_ok);

  const int http_status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if (http_status_code == 429 || (remaining_ok && remaining <= 0)) {
    const int seconds = reset_in_ok && reset_in > 0 ? reset_in : kDefaultRateLimitDelay;
    const qint64 delay_msec = std::clamp(static_cast<qint64>(seconds) * kMsecPerSec, static_cast<qint64>(0), kMaxRateLimitDelayMsec);
    next_request_time_ = std::max(next_request_time_, rate_limit_timer_.elapsed() + delay_msec);
    qLog(Debug) << "ListenBrainz: Rate limited, waiting" << seconds << "seconds before sending another request.";
  }

}

qint64 ListenBrainzScrobbler::RateLimitDelay() const {

  return std::max(static_cast<qint64>(0), next_request_time_ - rate_limit_timer_.elapsed());

}

void ListenBrainzScrobbler::SubmitListens() {

  qLog(Debug) << "ListenBrainz: Submitting scrobbles.";

  if (!enabled() || !authenticated() || settings_->offline()) return;

  QJsonArray array;
  ScrobblerCacheItemPtrList cache_items_sent;
  const ScrobblerCacheItemPtrList all_cache_items = cache_->List();
  for (int i = 0; i < all_cache_items.count(); i++) {
    ScrobblerCacheItemPtr cache_item = all_cache_items.at(i);
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

  if (cache_items_sent.count() <= 0) {
    // Nothing was sent, so back off rather than returning to a flush that would immediately ask for another submission and spin.
    // Getting here means the cache holds only items already flagged as sent while no submission is in progress.
    scrobbles_due_time_ = QDateTime::currentMSecsSinceEpoch() + kMinRequestIntervalMsec;
    return;
  }

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

  UpdateRateLimit(reply);

  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  submitted_ = false;

  // ListenBrainz frequently close the connection, ignore any connection closed errors to avoid error popups
  if (reply->error() == QNetworkReply::NetworkError::RemoteHostClosedError) {
    JsonBaseRequest::Error(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    cache_->ClearSent(cache_items);
    submit_error_ = true;
    Start();
    return;
  }

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
      if (IsRetryableHttpStatusCode(json_object_result.http_status_code)) {
        // Keep the listens in the cache, they are sent again once the rate limit window has passed or the server has recovered.
        Error(json_object_result.error_message);
        cache_->ClearSent(cache_items);
      }
      else if (cache_items.count() == 1) {
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

  Start();

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

  qLog(Debug) << "ListenBrainz: Queueing love for song" << song_playing_.artist() << song_playing_.album() << song_playing_.title();

  queue_love_ << LoveRequest(song_playing_.musicbrainz_recording_id());

  ScheduleFlushRequests();

}

void ListenBrainzScrobbler::SendLove(const LoveRequest &love_request) {

  QJsonObject object;
  object.insert("recording_mbid"_L1, love_request.recording_mbid);
  object.insert("score"_L1, 1);

  const QUrl url(QStringLiteral("%1/1/feedback/recording-feedback").arg(QLatin1String(kApiUrl)));
  QNetworkReply *reply = CreateRequest(url, QJsonDocument(object));
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, love_request]() { LoveRequestFinished(reply, love_request); });

}

void ListenBrainzScrobbler::LoveRequestFinished(QNetworkReply *reply, LoveRequest love_request) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);

  UpdateRateLimit(reply);

  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    // Put the love back on the queue when the failure was temporary, it is the users action and there is no other record of it.
    const bool retryable = json_object_result.error_code == ErrorCode::NetworkError || IsRetryableHttpStatusCode(json_object_result.http_status_code);
    if (retryable && love_request.attempts + 1 < kMaxLoveAttempts) {
      ++love_request.attempts;
      qLog(Debug) << "ListenBrainz: Requeueing love for" << love_request.recording_mbid << "after" << json_object_result.error_message;
      queue_love_.prepend(love_request);
      ScheduleFlushRequests();
      return;
    }
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
