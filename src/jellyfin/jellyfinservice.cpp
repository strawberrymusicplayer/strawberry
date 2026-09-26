/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry Music Player contributors
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

#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QTimer>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QSslError>
#include <QJsonValue>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/database.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "core/settings.h"
#include "core/taskmanager.h"
#include "core/urlhandlers.h"
#include "utilities/randutils.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "jellyfinservice.h"
#include "jellyfinrequest.h"
#include "jellyfinscrobblerequest.h"
#include "jellyfinurlhandler.h"
#include "constants/jellyfinsettings.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

const Song::Source JellyfinService::kSource = Song::Source::Jellyfin;
const char *JellyfinService::kClientName = "Strawberry";
const char *JellyfinService::kApiVersion = "1.2.30.0";

namespace {
constexpr char kAuthEndpoint[] = "/Users/AuthenticateByName";

constexpr char kArtistsSongsTable[] = "jellyfin_artists_songs";
constexpr char kAlbumsSongsTable[] = "jellyfin_albums_songs";
constexpr char kSongsTable[] = "jellyfin_songs";

constexpr int kSearchDelayMs = 300;
}  // namespace

JellyfinService::JellyfinService(const SharedPtr<TaskManager> task_manager,
                                 const SharedPtr<Database> database,
                                 const SharedPtr<NetworkAccessManager> network,
                                 const SharedPtr<UrlHandlers> url_handlers,
                                 const SharedPtr<AlbumCoverLoader> albumcover_loader,
                                 QObject *parent)
    : StreamingService(Song::Source::Jellyfin, u"Jellyfin"_s, u"jellyfin"_s, QLatin1String(JellyfinSettings::kSettingsGroup), parent),
      network_(network),
      database_(database),
      task_manager_(task_manager),
      url_handler_(nullptr),
      artists_collection_backend_(nullptr),
      albums_collection_backend_(nullptr),
      songs_collection_backend_(nullptr),
      artists_collection_model_(nullptr),
      albums_collection_model_(nullptr),
      songs_collection_model_(nullptr),
      timer_search_delay_(new QTimer(this)),
      pending_search_id_(0),
      next_pending_search_id_(1),
      pending_search_text_(QString()),
      pending_search_type_(SearchType::Artists),
      search_id_(0),
      http2_(false),
      verify_certificate_(true),
      download_album_covers_(true),
      server_side_scrobbling_(false),
      auto_login_requested_(false),
      login_in_flight_(false),
      login_test_pending_(false),
      login_queued_(false),
      login_queued_test_(false),
      test_login_reply_(nullptr),
      reauthenticating_(false),
      pending_catalog_refresh_(false) {

  url_handler_ = new JellyfinUrlHandler(this);
  url_handlers->Register(url_handler_);

  // Backends

  artists_collection_backend_ = make_shared<CollectionBackend>();
  artists_collection_backend_->moveToThread(database->thread());
  artists_collection_backend_->Init(database, task_manager, Song::Source::Jellyfin, QLatin1String(kArtistsSongsTable));

  albums_collection_backend_ = make_shared<CollectionBackend>();
  albums_collection_backend_->moveToThread(database->thread());
  albums_collection_backend_->Init(database, task_manager, Song::Source::Jellyfin, QLatin1String(kAlbumsSongsTable));

  songs_collection_backend_ = make_shared<CollectionBackend>();
  songs_collection_backend_->moveToThread(database->thread());
  songs_collection_backend_->Init(database, task_manager, Song::Source::Jellyfin, QLatin1String(kSongsTable));

  // Models

  artists_collection_model_ = new CollectionModel(artists_collection_backend_, albumcover_loader, this);
  albums_collection_model_ = new CollectionModel(albums_collection_backend_, albumcover_loader, this);
  songs_collection_model_ = new CollectionModel(songs_collection_backend_, albumcover_loader, this);

  // Search

  timer_search_delay_->setSingleShot(true);
  timer_search_delay_->setInterval(kSearchDelayMs);
  QObject::connect(timer_search_delay_, &QTimer::timeout, this, &JellyfinService::StartSearch);

  JellyfinService::ReloadSettings();

}

JellyfinService::~JellyfinService() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

}

void JellyfinService::Exit() {

  wait_for_exit_ << &*artists_collection_backend_ << &*albums_collection_backend_ << &*songs_collection_backend_;

  QObject::connect(&*artists_collection_backend_, &CollectionBackend::ExitFinished, this, &JellyfinService::ExitReceived);
  QObject::connect(&*albums_collection_backend_, &CollectionBackend::ExitFinished, this, &JellyfinService::ExitReceived);
  QObject::connect(&*songs_collection_backend_, &CollectionBackend::ExitFinished, this, &JellyfinService::ExitReceived);

  artists_collection_backend_->ExitAsync();
  albums_collection_backend_->ExitAsync();
  songs_collection_backend_->ExitAsync();

}

void JellyfinService::ExitReceived() {

  QObject *obj = sender();
  QObject::disconnect(obj, nullptr, this, nullptr);
  qLog(Debug) << obj << "successfully exited.";
  wait_for_exit_.removeAll(obj);
  if (wait_for_exit_.isEmpty()) Q_EMIT ExitFinished();

}

void JellyfinService::ReloadSettings() {

  Settings s;
  s.beginGroup(JellyfinSettings::kSettingsGroup);

  const bool enabled = s.value(JellyfinSettings::kEnabled, JellyfinSettings::kDefaultEnabled).toBool();

  server_url_ = s.value(JellyfinSettings::kUrl).toUrl();
  username_ = s.value(JellyfinSettings::kUsername).toString();
  QByteArray password = s.value(JellyfinSettings::kPassword).toByteArray();
  if (password.isEmpty()) password_.clear();
  else password_ = QString::fromUtf8(QByteArray::fromBase64(password));

  http2_ = s.value(JellyfinSettings::kHTTP2, JellyfinSettings::kDefaultHTTP2).toBool();
  verify_certificate_ = s.value(JellyfinSettings::kVerifyCertificate, JellyfinSettings::kDefaultVerifyCertificate).toBool();
  download_album_covers_ = s.value(JellyfinSettings::kDownloadAlbumCovers, JellyfinSettings::kDefaultDownloadAlbumCovers).toBool();
  server_side_scrobbling_ = s.value(JellyfinSettings::kServerSideScrobbling, JellyfinSettings::kDefaultServerSideScrobbling).toBool();

  s.endGroup();

  // The access token belongs to the server and user it was received for, log in again if they changed.
  if (!access_token_.isEmpty() && (NormalizedServerUrl(server_url_) != authenticated_server_url_ || username_ != authenticated_username_)) {
    access_token_.clear();
    user_id_.clear();
  }

  // Automatically log in with the stored credentials so the service is usable on startup
  if (enabled && !authenticated() && server_url_.isValid() && !username_.isEmpty() && !password_.isEmpty()) {
    auto_login_requested_ = true;
    Login(false);
  }

}

QString JellyfinService::CreateAuthorizationHeader(const bool with_token) const {

  Settings s;
  s.beginGroup(JellyfinSettings::kSettingsGroup);
  QString device_id = s.value(JellyfinSettings::kDeviceId).toString();
  if (device_id.isEmpty()) {
    device_id = u"strawberry-"_s + Utilities::CryptographicRandomString(16);
    s.setValue(JellyfinSettings::kDeviceId, device_id);
  }
  s.endGroup();

  QString header = u"MediaBrowser Client=\"%1\", Device=\"%2\", DeviceId=\"%3\", Version=\"%4\""_s.arg(QLatin1String(kClientName), QLatin1String(kClientName), device_id, QLatin1String(kApiVersion));

  // On Jellyfin versions where the legacy X-Emby-Token / X-Emby-Authorization headers are not honored,
  // the access token must be embedded in the authorization header itself.
  if (with_token && !access_token_.isEmpty()) {
    header += u", Token=\"%1\""_s.arg(access_token_);
  }

  return header;

}

void JellyfinService::SendPing() {
  Login(true);
}

void JellyfinService::Reauthenticate() {

  if (!server_url_.isValid() || username_.isEmpty() || password_.isEmpty()) {
    // Playback reports queued for a retry after a 401 can't be sent without a login.
    if (scrobble_request_) scrobble_request_->AuthenticationFailed(tr("No stored credentials to log in with."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  qLog(Debug) << "Jellyfin:" << "Re-authenticating with stored credentials.";
  Login(false);

}

void JellyfinService::Catalog401() {

  if (reauthenticating_) {
    pending_catalog_refresh_ = true;
    return;
  }

  reauthenticating_ = true;
  pending_catalog_refresh_ = true;

  if (!server_url_.isValid() || username_.isEmpty() || password_.isEmpty()) {
    qLog(Error) << "Jellyfin:" << "Received HTTP code 401, but no stored credentials are available to re-authenticate with.";
    reauthenticating_ = false;
    pending_catalog_refresh_ = false;
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  qLog(Debug) << "Jellyfin:" << "Received HTTP code 401, re-authenticating with stored credentials.";
  Login(false);

}

QUrl JellyfinService::GetStreamUrl(const QString &song_id) const {

  QUrl stream_url = server_url_;
  QString path = stream_url.path();
  if (path.isEmpty()) path = u"/"_s;
  else if (!path.endsWith(u'/')) path.append(u'/');
  stream_url.setPath(path + u"Audio/%1/stream"_s.arg(song_id));

  QUrlQuery query;
  query.addQueryItem(u"static"_s, u"true"_s);
  query.addQueryItem(u"api_key"_s, access_token_);
  // The lowercase api_key query parameter is only honored when the server has legacy authorization enabled,
  // so also add the capitalized variant that is always accepted.
  query.addQueryItem(u"ApiKey"_s, access_token_);
  stream_url.setQuery(query);

  return stream_url;

}

void JellyfinService::Scrobble(const QString &song_id, const bool submission, const QDateTime &time) {

  if (!server_url_.isValid()) {
    return;
  }

  if (access_token_.isEmpty()) {
    // Not authenticated yet. Buffer the report
    // and make sure a login is underway, it is sent once the login completes
    if (username_.isEmpty() || password_.isEmpty()) return;
    pending_scrobble_requests_.enqueue({submission ? PendingScrobbleRequest::Type::Stopped : PendingScrobbleRequest::Type::Start, song_id, time});
    Reauthenticate();
    return;
  }

  ScrobbleRequest()->CreateScrobbleRequest(song_id, submission, time);

}

void JellyfinService::ReportPlaybackProgress(const QString &song_id, const QDateTime &start_time) {

  if (!server_url_.isValid()) {
    return;
  }

  if (access_token_.isEmpty()) {
    // Not authenticated yet. Buffer the report
    // and make sure a login is underway, it is sent once the login completes
    if (username_.isEmpty() || password_.isEmpty()) return;
    pending_scrobble_requests_.enqueue({PendingScrobbleRequest::Type::Progress, song_id, start_time});
    Reauthenticate();
    return;
  }

  ScrobbleRequest()->CreatePlaybackProgressRequest(song_id, start_time);

}

void JellyfinService::FlushPendingScrobbles() {

  while (!pending_scrobble_requests_.isEmpty()) {
    const PendingScrobbleRequest pending = pending_scrobble_requests_.dequeue();
    switch (pending.type) {
      case PendingScrobbleRequest::Type::Start:
      case PendingScrobbleRequest::Type::Stopped:
        ScrobbleRequest()->CreateScrobbleRequest(pending.song_id, pending.type == PendingScrobbleRequest::Type::Stopped, pending.time);
        break;
      case PendingScrobbleRequest::Type::Progress:
        ScrobbleRequest()->CreatePlaybackProgressRequest(pending.song_id, pending.time);
        break;
    }
  }

}

SharedPtr<JellyfinScrobbleRequest> JellyfinService::ScrobbleRequest() {

  if (!scrobble_request_) {
    // We're doing requests every 30-240s the whole time, so keep reusing this instance
    scrobble_request_.reset(new JellyfinScrobbleRequest(this, network_), [](JellyfinScrobbleRequest *request) { request->deleteLater(); });
    QObject::connect(&*scrobble_request_, &JellyfinScrobbleRequest::ScrobbleError, this, &JellyfinService::ScrobbleError);
  }

  return scrobble_request_;

}

QUrl JellyfinService::NormalizedServerUrl(const QUrl &url) {

  QUrl normalized_url = url.adjusted(QUrl::StripTrailingSlash | QUrl::NormalizePathSegments);
  normalized_url.setHost(normalized_url.host().toLower());

  return normalized_url;

}

QUrl JellyfinService::AuthUrl(const QUrl &server_url) {

  QUrl url(server_url);
  QString path = url.path();
  if (path.isEmpty()) path = u"/"_s;
  else if (!path.endsWith(u'/')) path.append(u'/');
  url.setPath(path + QLatin1String(kAuthEndpoint + 1));

  return url;

}

bool JellyfinService::IsConfiguredLogin(const QUrl &server_url, const QString &username, const QString &password) const {

  return NormalizedServerUrl(server_url) == NormalizedServerUrl(server_url_) && username == username_ && password == password_;

}

void JellyfinService::SendPingWithCredentials(QUrl url, const QString &username, const QString &password) {

  // Testing the configured server and credentials, use a normal login, which also installs the access token.
  if (IsConfiguredLogin(url, username, password)) {
    Login(true);
    return;
  }

  // Testing another server or other credentials, only report the result, without changing the configured authentication or sending queued requests.
  if (test_login_reply_) {
    QNetworkReply *reply = test_login_reply_;
    test_login_reply_ = nullptr;
    replies_.removeAll(reply);
    QObject::disconnect(reply, nullptr, this, nullptr);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

  QNetworkReply *reply = CreateAuthenticateRequest(AuthUrl(url), username, password);
  if (!reply) return;

  test_login_reply_ = reply;
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &JellyfinService::HandleSSLErrors);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { HandleTestAuthReply(reply); });

}

void JellyfinService::Login(const bool test) {

  // A login request is already in flight (startup auto-login, a catalog re-authentication or the
  // settings page test). AuthenticateByName for the same DeviceId revokes the previous access
  // token, so overlapping logins race each other and can leave a revoked token installed in
  // access_token_. Reuse the in-flight login instead of sending a duplicate that revokes it.
  if (login_in_flight_) {
    if (IsConfiguredLogin(login_server_url_, login_username_, login_password_)) {
      qLog(Debug) << "Jellyfin:" << "A login request is already in flight, reusing it.";
      if (test) login_test_pending_ = true;
    }
    else {
      // The configured server or credentials changed since the login in flight was sent, log in again when it finishes.
      qLog(Debug) << "Jellyfin:" << "A login request for another server or other credentials is in flight, logging in again when it finishes.";
      login_queued_ = true;
      if (test) login_queued_test_ = true;
    }
    return;
  }

  QNetworkReply *reply = CreateAuthenticateRequest(AuthUrl(server_url_), username_, password_);
  if (!reply) return;

  login_in_flight_ = true;
  login_server_url_ = server_url_;
  login_username_ = username_;
  login_password_ = password_;
  login_test_pending_ = test;
  replies_ << reply;
  const QUrl server_url = server_url_;
  const QString username = username_;
  const QString password = password_;
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &JellyfinService::HandleSSLErrors);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, server_url, username, password]() { HandleAuthReply(reply, server_url, username, password); });

}

QNetworkReply *JellyfinService::CreateAuthenticateRequest(const QUrl &url, const QString &username, const QString &password) {

  QNetworkRequest network_request(url);
  network_request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_s);
  // On newer Jellyfin versions (10.11+) only the standard Authorization header is read to record the client/device
  // information on the access token, so send it alongside the legacy X-Emby-Authorization header for older servers.
  const QByteArray authorization_header = CreateAuthorizationHeader().toUtf8();
  network_request.setRawHeader("Authorization", authorization_header);
  network_request.setRawHeader("X-Emby-Authorization", authorization_header);
  // Only follow redirects to the same server, since the headers and the password in the body would be sent to the new location too.
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::SameOriginRedirectPolicy);
  network_request.setAttribute(QNetworkRequest::Http2AllowedAttribute, http2_);
  network_request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
  network_request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);

  if (url.scheme() == "https"_L1 && !verify_certificate_) {
    QSslConfiguration sslconfig = QSslConfiguration::defaultConfiguration();
    sslconfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    network_request.setSslConfiguration(sslconfig);
  }

  QJsonObject json_obj;
  json_obj.insert(u"Username"_s, username);
  json_obj.insert(u"Pw"_s, password);

  return network_->post(network_request, QJsonDocument(json_obj).toJson(QJsonDocument::Compact));

}

void JellyfinService::HandleSSLErrors(const QList<QSslError> &ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    errors_ += ssl_error.errorString();
  }

}

JellyfinService::AuthReply JellyfinService::ParseAuthReply(QNetworkReply *reply) const {

  AuthReply auth_reply;

  const int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

  if (reply->error() != QNetworkReply::NoError || http_status != 200) {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      auth_reply.error = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      return auth_reply;
    }

    const QByteArray data = reply->readAll();
    QJsonParseError parse_error;
    QJsonDocument json_doc = QJsonDocument::fromJson(data, &parse_error);
    if (parse_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
      const QJsonObject json_obj = json_doc.object();
      if (json_obj.contains(u"Message"_s)) {
        auth_reply.error = QStringLiteral("%1 (%2)").arg(json_obj.value(u"Message"_s).toString()).arg(http_status);
        return auth_reply;
      }
    }

    auth_reply.error = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(http_status);
    return auth_reply;
  }

  const QByteArray data = reply->readAll();
  QJsonParseError parse_error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &parse_error);
  if (parse_error.error != QJsonParseError::NoError || json_doc.isEmpty() || !json_doc.isObject()) {
    auth_reply.error = tr("Missing Json response.");
    return auth_reply;
  }

  const QJsonObject json_obj = json_doc.object();
  if (!json_obj.contains(u"AccessToken"_s) || !json_obj.contains(u"User"_s)) {
    auth_reply.error = tr("Response is missing AccessToken or User.");
    return auth_reply;
  }

  auth_reply.access_token = json_obj.value(u"AccessToken"_s).toString();
  auth_reply.user_id = json_obj.value(u"User"_s).toObject().value(u"Id"_s).toString();
  if (auth_reply.access_token.isEmpty() || auth_reply.user_id.isEmpty()) {
    auth_reply.error = tr("Response contains an empty AccessToken or User Id.");
  }

  return auth_reply;

}

void JellyfinService::HandleAuthReply(QNetworkReply *reply, const QUrl &server_url, const QString &username, const QString &password) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  login_in_flight_ = false;
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const bool test = login_test_pending_;
  const bool login_queued = login_queued_;
  const bool login_queued_test = login_queued_test_;
  login_test_pending_ = false;
  login_queued_ = false;
  login_queued_test_ = false;

  const AuthReply auth_reply = ParseAuthReply(reply);

  if (!IsConfiguredLogin(server_url, username, password)) {
    // The configured server or credentials changed while logging in, don't use the access token for the configured server.
    qLog(Debug) << "Jellyfin:" << "Ignoring login for" << server_url << "since the configured server or credentials changed.";
    if (test) EmitTestResult(auth_reply.error);
  }
  else if (!auth_reply.error.isEmpty()) {
    AuthError(auth_reply.error, test);
  }
  else {
    SetAuth(auth_reply.access_token, auth_reply.user_id, server_url, username);
    if (test) EmitTestResult(QString());

    // Send any playback reports that were waiting for authentication to complete and retry the
    // playback reports that hit a 401 while a stale access token was in use.
    FlushPendingScrobbles();
    ScrobbleRequest()->FlushScrobbleRequests();

    // Only load the catalogs if a catalog request was made before the login completed (e.g. via the streaming tab).
    const bool load_catalogs = pending_catalog_refresh_;
    auto_login_requested_ = false;
    pending_catalog_refresh_ = false;
    reauthenticating_ = false;
    if (load_catalogs) {
      qLog(Debug) << "Jellyfin:" << "Login successful, loading catalogs.";
      GetArtists();
      GetAlbums();
      GetSongs();
    }
  }

  if (login_queued) {
    Login(login_queued_test);
  }

}

void JellyfinService::HandleTestAuthReply(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  if (reply == test_login_reply_) test_login_reply_ = nullptr;
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  // A test of another server or other credentials only reports the result, the configured authentication and queued requests are left alone.
  const AuthReply auth_reply = ParseAuthReply(reply);
  if (!auth_reply.error.isEmpty()) {
    qLog(Error) << "Jellyfin:" << auth_reply.error;
  }
  EmitTestResult(auth_reply.error);

}

void JellyfinService::SetAuth(const QString &access_token, const QString &user_id, const QUrl &server_url, const QString &username) {

  access_token_ = access_token;
  user_id_ = user_id;
  authenticated_server_url_ = NormalizedServerUrl(server_url);
  authenticated_username_ = username;

}

void JellyfinService::EmitTestResult(const QString &error) {

  if (error.isEmpty()) {
    Q_EMIT TestSuccess();
    Q_EMIT TestComplete(true);
  }
  else {
    Q_EMIT TestFailure(error);
    Q_EMIT TestComplete(false, error);
  }

}

void JellyfinService::AuthError(const QString &error, const bool test) {

  qLog(Error) << "Jellyfin:" << error;
  if (test) EmitTestResult(error);

  // Clear the authentication-in-progress state so a failed login attempt does not leave the service stuck.
  auto_login_requested_ = false;
  if (reauthenticating_) {
    reauthenticating_ = false;
    pending_catalog_refresh_ = false;
  }

  // Playback reports queued for a retry after a 401 can't be sent without a login.
  if (scrobble_request_) scrobble_request_->AuthenticationFailed(error);

}

void JellyfinService::GetArtists() {

  if (!authenticated()) {

    // If a login is already in flight (startup auto-login or re-authentication) or stored
    // credentials are available, defer this request until the login completes instead of
    // failing with an authentication error.
    if (server_url_.isValid() && !username_.isEmpty() && !password_.isEmpty()) {
      pending_catalog_refresh_ = true;
      if (!reauthenticating_ && !auto_login_requested_) {
        reauthenticating_ = true;
        Login(false);
      }
      return;
    }

    Q_EMIT ArtistsResults(SongMap(), tr("Not authenticated with Jellyfin."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  artists_request_.reset(new JellyfinRequest(this, network_, JellyfinBaseRequest::Type::FavouriteArtists, this));
  QObject::connect(&*artists_request_, &JellyfinRequest::Results, this, &JellyfinService::ArtistsResultsReceived);
  QObject::connect(&*artists_request_, &JellyfinRequest::UpdateStatus, this, &JellyfinService::ArtistsUpdateStatusReceived);
  QObject::connect(&*artists_request_, &JellyfinRequest::UpdateProgress, this, &JellyfinService::ArtistsUpdateProgressReceived);

  artists_request_->Process();

}

void JellyfinService::ResetArtistsRequest() {
  artists_request_.reset();
}

void JellyfinService::ArtistsResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_UNUSED(id);
  Q_EMIT ArtistsResults(songs, error);
  artists_collection_backend_->UpdateSongsBySongIDAsync(songs);
  ResetArtistsRequest();

}

void JellyfinService::ArtistsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  Q_EMIT ArtistsUpdateStatus(text);
}

void JellyfinService::ArtistsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  Q_EMIT ArtistsUpdateProgress(progress);
}

void JellyfinService::GetAlbums() {

  if (!authenticated()) {

    // If a login is already in flight (startup auto-login or re-authentication) or stored
    // credentials are available, defer this request until the login completes instead of
    // failing with an authentication error.
    if (server_url_.isValid() && !username_.isEmpty() && !password_.isEmpty()) {
      pending_catalog_refresh_ = true;
      if (!reauthenticating_ && !auto_login_requested_) {
        reauthenticating_ = true;
        Login(false);
      }
      return;
    }

    Q_EMIT AlbumsResults(SongMap(), tr("Not authenticated with Jellyfin."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  albums_request_.reset(new JellyfinRequest(this, network_, JellyfinBaseRequest::Type::FavouriteAlbums, this));
  QObject::connect(&*albums_request_, &JellyfinRequest::Results, this, &JellyfinService::AlbumsResultsReceived);
  QObject::connect(&*albums_request_, &JellyfinRequest::UpdateStatus, this, &JellyfinService::AlbumsUpdateStatusReceived);
  QObject::connect(&*albums_request_, &JellyfinRequest::UpdateProgress, this, &JellyfinService::AlbumsUpdateProgressReceived);

  albums_request_->Process();

}

void JellyfinService::ResetAlbumsRequest() {
  albums_request_.reset();
}

void JellyfinService::AlbumsResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_UNUSED(id);
  Q_EMIT AlbumsResults(songs, error);
  albums_collection_backend_->UpdateSongsBySongIDAsync(songs);
  ResetAlbumsRequest();

}

void JellyfinService::AlbumsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  Q_EMIT AlbumsUpdateStatus(text);
}

void JellyfinService::AlbumsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  Q_EMIT AlbumsUpdateProgress(progress);
}

void JellyfinService::GetSongs() {

  if (!authenticated()) {

    // If a login is already in flight (startup auto-login or re-authentication) or stored
    // credentials are available, defer this request until the login completes instead of
    // failing with an authentication error.
    if (server_url_.isValid() && !username_.isEmpty() && !password_.isEmpty()) {
      pending_catalog_refresh_ = true;
      if (!reauthenticating_ && !auto_login_requested_) {
        reauthenticating_ = true;
        Login(false);
      }
      return;
    }

    Q_EMIT SongsResults(SongMap(), tr("Not authenticated with Jellyfin."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  songs_request_.reset(new JellyfinRequest(this, network_, JellyfinBaseRequest::Type::FavouriteSongs, this));
  QObject::connect(&*songs_request_, &JellyfinRequest::Results, this, &JellyfinService::SongsResultsReceived);
  QObject::connect(&*songs_request_, &JellyfinRequest::UpdateStatus, this, &JellyfinService::SongsUpdateStatusReceived);
  QObject::connect(&*songs_request_, &JellyfinRequest::UpdateProgress, this, &JellyfinService::SongsUpdateProgressReceived);

  songs_request_->Process();

}

void JellyfinService::ResetSongsRequest() {
  songs_request_.reset();
}

void JellyfinService::SongsResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_UNUSED(id);
  Q_EMIT SongsResults(songs, error);
  songs_collection_backend_->UpdateSongsBySongIDAsync(songs);
  ResetSongsRequest();

}

void JellyfinService::SongsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  Q_EMIT SongsUpdateStatus(text);
}

void JellyfinService::SongsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  Q_EMIT SongsUpdateProgress(progress);
}

int JellyfinService::Search(const QString &text, const SearchType type) {

  pending_search_id_ = next_pending_search_id_++;
  pending_search_text_ = text;
  pending_search_type_ = type;

  if (text.isEmpty()) {
    timer_search_delay_->stop();
    return pending_search_id_;
  }
  timer_search_delay_->start();

  return pending_search_id_;

}

void JellyfinService::CancelSearch() {
  search_request_.reset();
}

void JellyfinService::StartSearch() {

  if (!authenticated()) {
    Q_EMIT SearchResults(pending_search_id_, SongMap(), tr("Not authenticated with Jellyfin."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  search_id_ = pending_search_id_;
  search_text_ = pending_search_text_;

  if (search_request_) {
    search_request_.reset();
  }

  JellyfinBaseRequest::Type query_type = JellyfinBaseRequest::Type::None;

  switch (pending_search_type_) {
    case SearchType::Artists:
      query_type = JellyfinBaseRequest::Type::SearchArtists;
      break;
    case SearchType::Albums:
      query_type = JellyfinBaseRequest::Type::SearchAlbums;
      break;
    case SearchType::Songs:
      query_type = JellyfinBaseRequest::Type::SearchSongs;
      break;
    default:
      return;
  }

  search_request_.reset(new JellyfinRequest(this, network_, query_type, this));
  QObject::connect(&*search_request_, &JellyfinRequest::Results, this, &JellyfinService::SearchResultsReceived);
  QObject::connect(&*search_request_, &JellyfinRequest::UpdateStatus, this, &JellyfinService::SearchUpdateStatus);
  QObject::connect(&*search_request_, &JellyfinRequest::UpdateProgress, this, &JellyfinService::SearchUpdateProgress);

  search_request_->Search(search_id_, search_text_);
  search_request_->Process();

}

void JellyfinService::SearchResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_EMIT SearchResults(id, songs, error);
  search_request_.reset();

}

void JellyfinService::SearchUpdateStatus(const int id, const QString &text) {
  Q_EMIT StreamingService::SearchUpdateStatus(id, text);
}

void JellyfinService::SearchUpdateProgress(const int id, const int progress) {
  Q_EMIT StreamingService::SearchUpdateProgress(id, progress);
}