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

#include <algorithm>

#include <QCoreApplication>
#include <QVariant>
#include <QByteArray>
#include <QString>
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

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/database.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "core/settings.h"
#include "core/taskmanager.h"
#include "core/urlhandlers.h"
#include "credentialsmanager/credentialsmanager.h"
#include "utilities/randutils.h"
#include "utilities/strutils.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "jellyfinservice.h"
#include "jellyfinrequest.h"
#include "jellyfinscrobblerequest.h"
#include "jellyfinfavoriterequest.h"
#include "jellyfinurlhandler.h"
#include "constants/jellyfinsettings.h"
#include "constants/timeconstants.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

const Song::Source JellyfinService::kSource = Song::Source::Jellyfin;

namespace {
constexpr char kClientName[] = "Strawberry";
// Property of a reply which redirect was refused, the host it redirected to.
constexpr char kRedirectRefusedProperty[] = "jellyfin_redirect_refused";
constexpr char kAuthEndpoint[] = "Users/AuthenticateByName";
// The access token is checked again before playing when it was not used for this long, the server can have revoked it.
constexpr qint64 kTokenValidationIntervalSecs = 600;
// Automatic logins wait this long after a failed login, Jellyfin locks the account after a few failed logins.
constexpr qint64 kLoginRetryDelaySecs = 300;
// Maximum number of logins after catalog 401s without a successful catalog load in between.
constexpr int kMaxCatalog401Logins = 2;

constexpr char kArtistsSongsTable[] = "jellyfin_artists_songs";
constexpr char kAlbumsSongsTable[] = "jellyfin_albums_songs";
constexpr char kSongsTable[] = "jellyfin_songs";

constexpr int kSearchDelayMs = 300;
}  // namespace

JellyfinService::JellyfinService(const SharedPtr<TaskManager> task_manager,
                                 const SharedPtr<Database> database,
                                 const SharedPtr<NetworkAccessManager> network,
                                 const SharedPtr<CredentialsManager> credentials_manager,
                                 const SharedPtr<UrlHandlers> url_handlers,
                                 const SharedPtr<AlbumCoverLoader> albumcover_loader,
                                 QObject *parent)
    : StreamingService(Song::Source::Jellyfin, u"Jellyfin"_s, u"jellyfin"_s, QLatin1String(JellyfinSettings::kSettingsGroup), parent),
      network_(network),
      credentials_manager_(credentials_manager),
      url_handler_(nullptr),
      artists_collection_backend_(nullptr),
      albums_collection_backend_(nullptr),
      songs_collection_backend_(nullptr),
      artists_collection_model_(nullptr),
      albums_collection_model_(nullptr),
      songs_collection_model_(nullptr),
      favorite_request_(nullptr),
      timer_search_delay_(new QTimer(this)),
      pending_search_id_(0),
      next_pending_search_id_(1),
      pending_search_text_(QString()),
      pending_search_type_(SearchType::Artists),
      search_id_(0),
      http2_(false),
      verify_certificate_(true),
      download_album_covers_(true),
      enabled_(false),
      login_in_flight_(false),
      login_test_pending_(false),
      login_queued_(false),
      login_queued_test_(false),
      test_login_reply_(nullptr),
      login_rejected_(false),
      token_validation_reply_(nullptr),
      catalog_401_logins_(0),
      password_saved_(false),
      login_after_password_load_(false),
      login_after_password_load_test_(false),
      reauthenticating_(false),
      pending_catalogs_(0),
      search_after_login_(false) {

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

  // Favorites

  favorite_request_ = new JellyfinFavoriteRequest(this, network_, this);
  QObject::connect(favorite_request_, &JellyfinFavoriteRequest::FavoritesChanged, this, &JellyfinService::FavoritesChanged);
  QObject::connect(favorite_request_, &JellyfinFavoriteRequest::FavoritesUnauthorized, this, &JellyfinService::FavoritesUnauthorized);
  QObject::connect(this, &JellyfinService::AuthFinished, this, &JellyfinService::SendPendingFavoriteChanges);
  QObject::connect(this, &JellyfinService::AddArtists, this, &JellyfinService::AddArtistsToFavorites);
  QObject::connect(this, &JellyfinService::AddAlbums, this, &JellyfinService::AddAlbumsToFavorites);
  QObject::connect(this, &JellyfinService::AddSongs, this, &JellyfinService::AddSongsToFavorites);
  QObject::connect(this, &JellyfinService::RemoveArtists, this, &JellyfinService::RemoveArtistsFromFavorites);
  QObject::connect(this, &JellyfinService::RemoveAlbums, this, &JellyfinService::RemoveAlbumsFromFavorites);
  QObject::connect(this, &JellyfinService::RemoveSongsByList, this, &JellyfinService::RemoveSongsFromFavorites);
  QObject::connect(this, &JellyfinService::RemoveSongsByMap, this, &JellyfinService::RemoveSongMapFromFavorites);

  // Search

  timer_search_delay_->setSingleShot(true);
  timer_search_delay_->setInterval(kSearchDelayMs);
  QObject::connect(timer_search_delay_, &QTimer::timeout, this, &JellyfinService::StartSearch);
  QObject::connect(this, &JellyfinService::AuthFinished, this, &JellyfinService::SearchLoginFinished);

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
  const bool previous_enabled = enabled_;
  enabled_ = enabled;

  const QUrl previous_server_url = server_url_;
  const QString previous_username = username_;

  server_url_ = s.value(JellyfinSettings::kUrl).toUrl();
  username_ = s.value(JellyfinSettings::kUsername).toString();

  // Allow automatic logins again when the server or username changed, a changed password is handled by SetPassword().
  if (NormalizedServerUrl(server_url_) != NormalizedServerUrl(previous_server_url) || username_ != previous_username) {
    login_rejected_ = false;
    login_failed_timer_.invalidate();
    failed_password_.clear();
    catalog_401_logins_ = 0;
    // Requests in progress and the stored favorites belong to the previous server or user.
    if (!previous_server_url.isEmpty() || !previous_username.isEmpty()) {
      AbortRequests(tr("The Jellyfin server or username changed."));
      artists_collection_backend_->DeleteAllAsync();
      albums_collection_backend_->DeleteAllAsync();
      songs_collection_backend_->DeleteAllAsync();
    }
  }

  // The password is stored in the credentials manager, only read it when Jellyfin is configured, since reading it can show a keyring password prompt.
  // Settings are reloaded for changes on every settings page, so only read it again when it could have changed.
  bool read_password = false;
  if (enabled && server_url_.isValid() && !username_.isEmpty()) {
    read_password = password_.isEmpty() || password_saved_ || NormalizedServerUrl(server_url_) != NormalizedServerUrl(previous_server_url) || username_ != previous_username;
  }
  else {
    // Jellyfin was disabled or is not configured, a password read in progress must not start a login, the login waiting for it fails below.
    if (read_password_reply_) {
      QObject::disconnect(&*read_password_reply_, nullptr, this, nullptr);
      read_password_reply_.reset();
    }
    SetPassword(QString());
    // Don't keep using the access token for a disabled service, and stop the requests using it.
    access_token_.clear();
    user_id_.clear();
    // Settings are reloaded when any settings page is saved, only abort when Jellyfin was usable before.
    if (previous_enabled && previous_server_url.isValid() && !previous_username.isEmpty()) {
      AbortRequests(enabled ? tr("The Jellyfin server URL or username is missing.") : tr("Jellyfin is disabled."));
    }
  }
  password_saved_ = false;

  http2_ = s.value(JellyfinSettings::kHTTP2, JellyfinSettings::kDefaultHTTP2).toBool();
  verify_certificate_ = s.value(JellyfinSettings::kVerifyCertificate, JellyfinSettings::kDefaultVerifyCertificate).toBool();
  download_album_covers_ = s.value(JellyfinSettings::kDownloadAlbumCovers, JellyfinSettings::kDefaultDownloadAlbumCovers).toBool();

  s.endGroup();

  // The access token belongs to the server and user it was received for, log in again if they changed.
  if (!access_token_.isEmpty() && (NormalizedServerUrl(server_url_) != authenticated_server_url_ || username_ != authenticated_username_)) {
    access_token_.clear();
    user_id_.clear();
  }

  if (read_password) {
    // Ignore a password reply from a previous reload.
    if (read_password_reply_) {
      QObject::disconnect(&*read_password_reply_, nullptr, this, nullptr);
      read_password_reply_.reset();
    }
    read_password_reply_ = credentials_manager_->ReadPasswordAsync(QLatin1String(JellyfinSettings::kCredentialsService));
    QObject::connect(&*read_password_reply_, &CredentialsReply::Finished, this, &JellyfinService::ReadPasswordFinished);
  }

  // Automatically log in with the stored credentials so the service is usable on startup, it waits for the password when it's being read.
  // Don't log in again after a failed login, settings are reloaded when any settings page is saved, and Jellyfin locks the account after a few failed logins.
  // A changed password allows logging in again, it's checked when the password was read.
  if (enabled && !authenticated() && server_url_.isValid() && HasCredentials() && (password_loading() || AutomaticLoginAllowed())) {
    Login(false);
  }

  // A login was waiting for a password read that was replaced by this reload.
  if (login_after_password_load_ && !password_loading()) {
    PasswordLoadFinished();
  }

}

bool JellyfinService::HasCredentials() const {

  return !username_.isEmpty() && (!password_.isEmpty() || password_loading());

}

void JellyfinService::SetPassword(const QString &password) {

  // Allow automatic logins again with a new password, but not when the password of a failed login is set again, for example after disabling and enabling Jellyfin.
  if (!password.isEmpty() && password != password_ && password != failed_password_) {
    login_rejected_ = false;
    login_failed_timer_.invalidate();
    catalog_401_logins_ = 0;
  }

  password_ = password;

}

bool JellyfinService::CanLogin() const {

  return enabled_ && server_url_.isValid() && HasCredentials();

}

void JellyfinService::AbortRequests(const QString &error) {

  // Report the error for the running requests, so the views don't keep waiting for them.
  int catalogs = 0;
  if (artists_request_) catalogs |= CatalogArtists;
  if (albums_request_) catalogs |= CatalogAlbums;
  if (songs_request_) catalogs |= CatalogSongs;
  ResetRequest(artists_request_);
  ResetRequest(albums_request_);
  ResetRequest(songs_request_);
  EmitCatalogResults(catalogs, SongMap(), error);

  if (search_request_ || search_after_login_) {
    const bool search_running = static_cast<bool>(search_request_);
    const bool search_waiting = search_after_login_;
    ResetRequest(search_request_);
    search_after_login_ = false;
    if (search_running) Q_EMIT SearchResults(search_id_, SongMap(), error);
    // A newer search can be waiting for the login.
    if (search_waiting && (!search_running || pending_search_id_ != search_id_)) Q_EMIT SearchResults(pending_search_id_, SongMap(), error);
  }

  // Favorite changes belong to the previous server.
  favorite_request_->Clear();
  pending_favorite_changes_.clear();

  // Playback reports belong to the previous server, or can't be sent anymore.
  pending_scrobble_requests_.clear();
  if (scrobble_request_) scrobble_request_->Clear();
  Q_EMIT PlaybackReportsCleared();

}

void JellyfinService::AddArtistsToFavorites(const SongList &songs) {
  ChangeFavorites(CatalogArtists, songs, true);
}

void JellyfinService::AddAlbumsToFavorites(const SongList &songs) {
  ChangeFavorites(CatalogAlbums, songs, true);
}

void JellyfinService::AddSongsToFavorites(const SongList &songs) {
  ChangeFavorites(CatalogSongs, songs, true);
}

void JellyfinService::RemoveArtistsFromFavorites(const SongList &songs) {
  ChangeFavorites(CatalogArtists, songs, false);
}

void JellyfinService::RemoveAlbumsFromFavorites(const SongList &songs) {
  ChangeFavorites(CatalogAlbums, songs, false);
}

void JellyfinService::RemoveSongsFromFavorites(const SongList &songs) {
  ChangeFavorites(CatalogSongs, songs, false);
}

void JellyfinService::RemoveSongMapFromFavorites(const SongMap &songs) {
  ChangeFavorites(CatalogSongs, songs.values(), false);
}

void JellyfinService::ChangeFavorites(const int catalog, const SongList &songs, const bool add) {

  // The favorite artist or album of the songs, songs of favorite artists have the artist they are shown for in the artists tab.
  JellyfinFavoriteChangeList changes;
  QStringList item_ids;
  for (const Song &song : songs) {
    const QString item_id = catalog == CatalogArtists ? song.artist_id() : catalog == CatalogAlbums ? song.album_id() : song.song_id();
    if (item_id.isEmpty() || item_ids.contains(item_id)) continue;
    item_ids << item_id;
    changes << JellyfinFavoriteChange{catalog, item_id, add, false};
  }
  if (changes.isEmpty()) return;

  // A newer change of an item replaces a change of it waiting for a login.
  pending_favorite_changes_.erase(std::remove_if(pending_favorite_changes_.begin(), pending_favorite_changes_.end(), [&item_ids](const JellyfinFavoriteChange &change) { return item_ids.contains(change.item_id); }), pending_favorite_changes_.end());

  if (authenticated()) {
    favorite_request_->SendChanges(changes);
    return;
  }

  // Wait for a login in flight or start one, like catalog requests.
  if (server_url_.isValid() && HasCredentials() && EnsureLogin()) {
    pending_favorite_changes_ << changes;
    return;
  }

  Q_EMIT ShowErrorDialog(tr("Not authenticated with Jellyfin."));

}

void JellyfinService::FavoritesUnauthorized(const QString &access_token, const JellyfinFavoriteChangeList &changes) {

  // The server rejected the access token, send the changes again with a new access token.
  TokenRejected(access_token);
  if (authenticated()) {
    favorite_request_->SendChanges(changes);
    return;
  }

  if (server_url_.isValid() && HasCredentials() && EnsureLogin()) {
    pending_favorite_changes_ << changes;
    return;
  }

  Q_EMIT ShowErrorDialog(tr("Changing Jellyfin favorites failed: not authorized."));

}

void JellyfinService::SendPendingFavoriteChanges() {

  if (pending_favorite_changes_.isEmpty()) return;

  const JellyfinFavoriteChangeList changes = pending_favorite_changes_;
  pending_favorite_changes_.clear();

  if (authenticated()) {
    favorite_request_->SendChanges(changes);
  }
  else {
    Q_EMIT ShowErrorDialog(tr("Changing Jellyfin favorites failed: not authenticated with Jellyfin."));
  }

}

void JellyfinService::FavoritesChanged(const int catalog, const QString &error) {

  if (!error.isEmpty()) {
    Q_EMIT ShowErrorDialog(error);
  }

  // Load the catalog again to show the changed favorites, also after errors, some changes can have succeeded.
  GetCatalogs(catalog);

}

void JellyfinService::ResetRequest(JellyfinRequestPtr &request) {

  // The request is deleted later, so disconnect it first, replies it is still processing must not report results for its replacement.
  // Abort the request too, so it doesn't act on replies it receives before it's deleted, for example a 401.
  if (request) {
    request->Abort();
    QObject::disconnect(&*request, nullptr, this, nullptr);
    request.reset();
  }

}

void JellyfinService::ReadPasswordFinished() {

  if (!read_password_reply_ || sender() != &*read_password_reply_) return;

  SetPassword(read_password_reply_->success() ? read_password_reply_->password() : QString());
  read_password_reply_.reset();

  // The password was removed, don't keep using the access token.
  if (password_.isEmpty()) {
    access_token_.clear();
    user_id_.clear();
  }

  PasswordLoadFinished();

}

void JellyfinService::PasswordLoadFinished() {

  if (!login_after_password_load_) return;

  const bool test = login_after_password_load_test_;
  login_after_password_load_ = false;
  login_after_password_load_test_ = false;

  if (server_url_.isValid() && HasCredentials() && (test || AutomaticLoginAllowed())) {
    Login(test);
    return;
  }

  // No password is stored, so there is nothing to log in with, or logging in failed before with the same password.
  const QString error = !enabled_ ? tr("Jellyfin is disabled.") : HasCredentials() ? tr("Logging in failed, not trying again automatically yet.") : tr("No stored Jellyfin password to log in with.");
  qLog(Error) << "Jellyfin:" << error;
  if (test) EmitTestResult(error);
  pending_scrobble_requests_.clear();
  if (scrobble_request_) scrobble_request_->AuthenticationFailed(error);
  CatalogLoginFailed(error);
  Q_EMIT AuthFinished();

}

void JellyfinService::CatalogLoginFailed(const QString &error) {

  reauthenticating_ = false;

  // Catalog requests waiting for the login can't be loaded, report the error so the views don't keep waiting.
  const int catalogs = pending_catalogs_;
  pending_catalogs_ = 0;
  EmitCatalogResults(catalogs, SongMap(), error);

}

void JellyfinService::EmitCatalogResults(const int catalogs, const SongMap &songs, const QString &error) {

  if (catalogs & CatalogArtists) Q_EMIT ArtistsResults(songs, error);
  if (catalogs & CatalogAlbums) Q_EMIT AlbumsResults(songs, error);
  if (catalogs & CatalogSongs) Q_EMIT SongsResults(songs, error);

}

void JellyfinService::HandleRedirects(QNetworkReply *reply) {

  QObject::connect(reply, &QNetworkReply::redirected, reply, [reply](const QUrl &redirect_url) {
    // Only follow redirects to the same server, since the authorization headers and the password in the login would be sent to the new location too.
    // Also allow switching from HTTP to HTTPS on the same server, which a reverse proxy commonly does.
    // Compare with the URL of the request, reply->url() is already the redirect URL here, the request URL is not changed by redirects.
    const QUrl url = reply->request().url();
    const bool same_host = redirect_url.host().compare(url.host(), Qt::CaseInsensitive) == 0;
    const int default_port = url.scheme() == "https"_L1 ? 443 : 80;
    const bool same_origin = same_host && redirect_url.scheme() == url.scheme() && redirect_url.port(default_port) == url.port(default_port);
    const bool https_upgrade = same_host && url.scheme() == "http"_L1 && redirect_url.scheme() == "https"_L1;
    if (same_origin || https_upgrade) {
      Q_EMIT reply->redirectAllowed();
    }
    else {
      qLog(Error) << "Jellyfin:" << "Not following redirect from" << Utilities::UrlForLog(url) << "to another server" << Utilities::UrlForLog(redirect_url);
      // The request fails with an operation canceled error, keep the reason to report it.
      reply->setProperty(kRedirectRefusedProperty, redirect_url.host());
      reply->abort();
    }
  });

}

QString JellyfinService::RedirectRefusedError(QNetworkReply *reply) {

  const QVariant host = reply->property(kRedirectRefusedProperty);
  if (!host.isValid()) return QString();

  return tr("The server redirected to another server %1, which is not followed, since the credentials would be sent to it.").arg(host.toString());

}

QString JellyfinService::CreateAuthorizationHeader(const bool with_token) const {

  return CreateAuthorizationHeader(with_token ? access_token_ : QString(), false);

}

QString JellyfinService::CreateAuthorizationHeader(const QString &access_token, const bool test_device) const {

  // The device ID is sent with every request, so only read it from the settings once.
  if (device_id_.isEmpty()) {
    Settings s;
    s.beginGroup(JellyfinSettings::kSettingsGroup);
    device_id_ = s.value(JellyfinSettings::kDeviceId).toString();
    if (device_id_.isEmpty()) {
      device_id_ = u"strawberry-"_s + Utilities::CryptographicRandomString(16);
      s.setValue(JellyfinSettings::kDeviceId, device_id_);
    }
    s.endGroup();
  }
  QString device_id = device_id_;

  // Logging in revokes the previous access token of the same device, so tests of other servers or credentials use another device to not revoke the configured login.
  if (test_device) device_id.append(u"-test"_s);

  QString header = u"MediaBrowser Client=\"%1\", Device=\"%2\", DeviceId=\"%3\", Version=\"%4\""_s.arg(QLatin1String(kClientName), QLatin1String(kClientName), device_id, QCoreApplication::applicationVersion());

  // On Jellyfin versions where the legacy X-Emby-Token and X-Emby-Authorization headers are not honored, the access token must be embedded in the authorization header itself.
  if (!access_token.isEmpty()) {
    header += u", Token=\"%1\""_s.arg(access_token);
  }

  return header;

}

void JellyfinService::TokenValidated() {

  token_validated_timer_.start();

}

bool JellyfinService::TokenNeedsValidation() const {

  return authenticated() && (!token_validated_timer_.isValid() || token_validated_timer_.elapsed() >= kTokenValidationIntervalSecs * kMsecPerSec);

}

void JellyfinService::ValidateToken() {

  // The check in progress finishes within the transfer timeout, also when Jellyfin was disabled meanwhile, the URLs waiting for it fail then.
  if (token_validation_reply_) return;

  if (!authenticated()) {
    Q_EMIT TokenValidationFinished();
    return;
  }

  const QString access_token = access_token_;
  QNetworkReply *reply = network_->get(CreateServerRequest(ServerUrl(server_url_, u"Users/%1"_s.arg(user_id_)), access_token, false, http2_, verify_certificate_));
  HandleRedirects(reply);
  token_validation_reply_ = reply;
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &JellyfinService::HandleSSLErrors);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, access_token]() { TokenValidationReplyReceived(reply, access_token); });

}

void JellyfinService::TokenValidationReplyReceived(QNetworkReply *reply, const QString &access_token) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  if (reply == token_validation_reply_) token_validation_reply_ = nullptr;
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  // The access token was replaced or cleared while it was checked, a login replacing it emits AuthFinished.
  if (access_token != access_token_) {
    if (!authenticated() && EnsureLogin()) return;
    Q_EMIT TokenValidationFinished();
    return;
  }

  const int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if (http_status == 401) {
    // The server revoked the access token, for example when the device was removed on the server, log in again before playing.
    qLog(Debug) << "Jellyfin:" << "The access token was rejected, logging in again.";
    TokenRejected(access_token);
    // AuthFinished is emitted when the login finished.
    if (EnsureLogin()) return;
  }
  else if (reply->error() == QNetworkReply::NoError) {
    TokenValidated();
  }
  // Other errors are reported when playing.

  Q_EMIT TokenValidationFinished();

}

void JellyfinService::TokenRejected(const QString &access_token) {

  // Don't keep using an access token the server rejected, but keep a newer token from a login that finished while the request was in flight.
  if (!access_token.isEmpty() && access_token == access_token_) {
    access_token_.clear();
  }

}

void JellyfinService::Reauthenticate(const QString &rejected_access_token) {

  TokenRejected(rejected_access_token);

  // A login finished with a new access token while the rejected request was in flight.
  if (authenticated()) {
    if (scrobble_request_) scrobble_request_->LoginSucceeded();
    return;
  }

  if (!server_url_.isValid() || !HasCredentials()) {
    // Playback reports queued for a retry after a 401 can't be sent without a login.
    if (scrobble_request_) scrobble_request_->AuthenticationFailed(tr("No stored credentials to log in with."));
    return;
  }

  if (!login_in_flight_ && !login_after_password_load_ && !AutomaticLoginAllowed()) {
    if (scrobble_request_) scrobble_request_->AuthenticationFailed(tr("Logging in failed, not trying again automatically yet."));
    return;
  }

  qLog(Debug) << "Jellyfin:" << "Re-authenticating with stored credentials.";
  Login(false);

}

bool JellyfinService::AutomaticLoginAllowed() const {

  if (login_rejected_) return false;

  return !login_failed_timer_.isValid() || login_failed_timer_.elapsed() >= kLoginRetryDelaySecs * kMsecPerSec;

}

bool JellyfinService::EnsureLogin() {

  // A login in flight, or waiting for the password read, is checked against the automatic login block when it's sent.
  if (login_in_flight_ || login_after_password_load_) return true;
  if (!server_url_.isValid() || !HasCredentials() || !AutomaticLoginAllowed()) return false;

  Login(false);

  return login_in_flight_ || login_after_password_load_;

}

bool JellyfinService::Unauthorized(const QString &rejected_access_token, const int catalog) {

  const bool search = catalog == CatalogNone;

  // A login finished with a new access token while the rejected request was in flight, send the request again with the new access token.
  if (!access_token_.isEmpty() && rejected_access_token != access_token_) {
    if (search) {
      // The rejected search is replaced, answer it when a newer search runs instead.
      if (search_id_ != pending_search_id_) Q_EMIT SearchResults(search_id_, SongMap(), tr("Not authorized with Jellyfin."));
      // The search delay timer starts a newer search, and a cleared search is not sent.
      if (!timer_search_delay_->isActive() && !pending_search_text_.isEmpty()) StartSearch();
    }
    else {
      GetCatalogs(catalog);
    }
    return true;
  }

  TokenRejected(rejected_access_token);

  // The catalogs of the rejected request are loaded again when the login in progress finished.
  if (!search && reauthenticating_) {
    pending_catalogs_ |= catalog;
    return true;
  }

  // The server keeps rejecting the access token right after logging in, for example a reverse proxy that doesn't forward the authorization headers.
  if (catalog_401_logins_ >= kMaxCatalog401Logins) {
    qLog(Error) << "Jellyfin:" << "Received HTTP code 401 again after logging in, not logging in again. Check that a reverse proxy forwards the Authorization and X-Emby-Token headers.";
    return false;
  }

  if (!server_url_.isValid() || !HasCredentials()) {
    qLog(Error) << "Jellyfin:" << "Received HTTP code 401, but no stored credentials are available to re-authenticate with.";
    return false;
  }

  // Don't log in again after a failed login, Jellyfin locks the account after a few failed logins.
  qLog(Debug) << "Jellyfin:" << "Received HTTP code 401, re-authenticating with stored credentials.";
  const bool login_pending = login_in_flight_ || login_after_password_load_;
  if (!EnsureLogin()) {
    qLog(Error) << "Jellyfin:" << "Logging in failed before, not trying again automatically yet.";
    return false;
  }
  // Only count logins started for a 401, not a login in progress that is reused.
  if (!login_pending) ++catalog_401_logins_;

  if (search) {
    // Search again when the login finished.
    search_after_login_ = true;
  }
  else {
    reauthenticating_ = true;
    pending_catalogs_ |= catalog;
  }

  return true;

}

QUrl JellyfinService::ServerUrl(const QUrl &server_url, const QString &ressource_path) {

  QUrl url(server_url);
  QString path = url.path();
  if (path.isEmpty()) path = u"/"_s;
  else if (!path.endsWith(u'/')) path.append(u'/');
  url.setPath(path + ressource_path);

  return url;

}

QUrl JellyfinService::GetStreamUrl(const QString &song_id) const {

  QUrl stream_url = ServerUrl(server_url_, u"Audio/%1/stream"_s.arg(song_id));

  QUrlQuery query;
  query.addQueryItem(u"static"_s, u"true"_s);
  query.addQueryItem(u"api_key"_s, access_token_);
  // The lowercase api_key query parameter is only honored when the server has legacy authorization enabled, so also add the capitalized variant that is always accepted.
  query.addQueryItem(u"ApiKey"_s, access_token_);
  stream_url.setQuery(query);

  return stream_url;

}

void JellyfinService::ReportPlayback(const PendingScrobbleRequest &request) {

  if (!server_url_.isValid()) {
    return;
  }

  if (access_token_.isEmpty()) {
    // Not authenticated yet, buffer the report and make sure a login is underway.
    // It is sent once the login completes.
    if (!HasCredentials()) return;
    // Don't buffer reports or log in again after a failed login, each report would cause another failed login.
    if (!login_in_flight_ && !login_after_password_load_ && !AutomaticLoginAllowed()) return;
    pending_scrobble_requests_.enqueue(request);
    Reauthenticate();
    return;
  }

  switch (request.type) {
    case PendingScrobbleRequest::Type::Start:
      ScrobbleRequest()->CreatePlaybackStartRequest(request.song_id);
      break;
    case PendingScrobbleRequest::Type::Progress:
      ScrobbleRequest()->CreatePlaybackProgressRequest(request.song_id, request.position_nanosec, request.paused);
      break;
    case PendingScrobbleRequest::Type::Stopped:
      ScrobbleRequest()->CreatePlaybackStoppedRequest(request.song_id, request.position_nanosec);
      break;
  }

}

void JellyfinService::ReportPlaybackStart(const QString &song_id) {

  ReportPlayback({PendingScrobbleRequest::Type::Start, song_id, 0, false});

}

void JellyfinService::ReportPlaybackProgress(const QString &song_id, const qint64 position_nanosec, const bool paused) {

  ReportPlayback({PendingScrobbleRequest::Type::Progress, song_id, position_nanosec, paused});

}

void JellyfinService::ReportPlaybackStopped(const QString &song_id, const qint64 position_nanosec) {

  ReportPlayback({PendingScrobbleRequest::Type::Stopped, song_id, position_nanosec, false});

}

void JellyfinService::FlushPendingScrobbles() {

  while (!pending_scrobble_requests_.isEmpty()) {
    ReportPlayback(pending_scrobble_requests_.dequeue());
  }

}

SharedPtr<JellyfinScrobbleRequest> JellyfinService::ScrobbleRequest() {

  if (!scrobble_request_) {
    // Playback progress is reported every 30 seconds while playing, so keep reusing this instance.
    scrobble_request_.reset(new JellyfinScrobbleRequest(this, network_, this), [](JellyfinScrobbleRequest *request) { request->deleteLater(); });
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

  return ServerUrl(server_url, QLatin1String(kAuthEndpoint));

}

bool JellyfinService::IsConfiguredLogin(const QUrl &server_url, const QString &username, const QString &password) const {

  return NormalizedServerUrl(server_url) == NormalizedServerUrl(server_url_) && username == username_ && password == password_;

}

void JellyfinService::SendPingWithCredentials(const QUrl &url, const QString &username, const QString &password, const bool http2, const bool verify_certificate) {

  // Testing the configured server, credentials and connection settings while not logged in, use a normal login, which also installs the access token.
  // This is an explicit login by the user, so it's allowed even after a failed login.
  // When logged in, test like other credentials, a new login would revoke the access token which the playing stream uses.
  if (!authenticated() && IsConfiguredLogin(url, username, password) && http2 == http2_ && verify_certificate == verify_certificate_) {
    login_rejected_ = false;
    login_failed_timer_.invalidate();
    Login(true);
    return;
  }

  // Testing another server or other credentials, or the configured login while logged in, only report the result, without changing the configured authentication or sending queued requests.
  if (test_login_reply_) {
    QNetworkReply *reply = test_login_reply_;
    test_login_reply_ = nullptr;
    replies_.removeAll(reply);
    QObject::disconnect(reply, nullptr, this, nullptr);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

  // Use the connection settings from the settings page, they might not be saved yet.
  QNetworkReply *reply = CreateAuthenticateRequest(AuthUrl(url), username, password, true, http2, verify_certificate);

  test_login_reply_ = reply;
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &JellyfinService::HandleSSLErrors);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, url, http2, verify_certificate]() { HandleTestAuthReply(reply, url, http2, verify_certificate); });

}

void JellyfinService::Login(const bool test) {

  // Wait for the password to be read from the credentials manager.
  if (password_loading()) {
    login_after_password_load_ = true;
    if (test) login_after_password_load_test_ = true;
    return;
  }

  // A login request is already in flight (startup auto-login, a catalog re-authentication or the settings page test).
  // AuthenticateByName for the same DeviceId revokes the previous access token, so overlapping logins race each other and can leave a revoked token installed in access_token_.
  // Reuse the in-flight login instead of sending a duplicate that revokes it.
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

  QNetworkReply *reply = CreateAuthenticateRequest(AuthUrl(server_url_), username_, password_, false, http2_, verify_certificate_);

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

QNetworkRequest JellyfinService::CreateServerRequest(const QUrl &url, const QString &access_token, const bool test_device, const bool http2, const bool verify_certificate) const {

  QNetworkRequest network_request(url);
  // On newer Jellyfin versions (10.11+) only the standard Authorization header is read to record the client/device information on the access token.
  // Send it alongside the legacy X-Emby-Authorization header for older servers.
  const QByteArray authorization_header = CreateAuthorizationHeader(access_token, test_device).toUtf8();
  network_request.setRawHeader("Authorization", authorization_header);
  network_request.setRawHeader("X-Emby-Authorization", authorization_header);
  if (!access_token.isEmpty()) {
    network_request.setRawHeader("X-Emby-Token", access_token.toUtf8());
  }
  // Only follow redirects to the same server, since the headers and the password in the login would be sent to the new location too, see HandleRedirects().
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::UserVerifiedRedirectPolicy);
  network_request.setAttribute(QNetworkRequest::Http2AllowedAttribute, http2);
  network_request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
  network_request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);

  // Also for HTTP, the server can redirect to HTTPS.
  if (!verify_certificate) {
    QSslConfiguration sslconfig = QSslConfiguration::defaultConfiguration();
    sslconfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    network_request.setSslConfiguration(sslconfig);
  }

  return network_request;

}

QNetworkReply *JellyfinService::CreateAuthenticateRequest(const QUrl &url, const QString &username, const QString &password, const bool test_device, const bool http2, const bool verify_certificate) {

  QNetworkRequest network_request = CreateServerRequest(url, QString(), test_device, http2, verify_certificate);
  network_request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_s);

  QJsonObject json_obj;
  json_obj.insert(u"Username"_s, username);
  json_obj.insert(u"Pw"_s, password);

  QNetworkReply *reply = network_->post(network_request, QJsonDocument(json_obj).toJson(QJsonDocument::Compact));
  HandleRedirects(reply);

  return reply;

}

void JellyfinService::HandleSSLErrors(const QList<QSslError> &ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    qLog(Error) << "Jellyfin:" << "SSL error:" << ssl_error.errorString();
  }

}

JellyfinService::AuthReply JellyfinService::ParseAuthReply(QNetworkReply *reply) const {

  AuthReply auth_reply;

  const int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

  if (reply->error() != QNetworkReply::NoError || http_status != 200) {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      const QString redirect_error = RedirectRefusedError(reply);
      auth_reply.error = redirect_error.isEmpty() ? QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()) : redirect_error;
      // A refused redirect is not a login failure either.
      auth_reply.network_error = http_status == 0 || reply->error() == QNetworkReply::OperationCanceledError || reply->error() == QNetworkReply::InsecureRedirectError || reply->error() == QNetworkReply::TooManyRedirectsError;
      return auth_reply;
    }

    // Jellyfin answers 401 for a wrong username or password.
    auth_reply.credentials_rejected = http_status == 401;
    // A server error, for example from a reverse proxy while Jellyfin restarts, is not a login failure either.
    // No HTTP status means the request failed before a response from Jellyfin, for example a protocol error.
    auth_reply.network_error = http_status == 0 || http_status >= 500;

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
  // Only log in again for the new settings when logging in is still possible, for example not after Jellyfin was disabled.
  const bool login_queued = login_queued_ && CanLogin();
  const bool login_queued_test = login_queued_test_;
  // Report the result for a queued test, unless the login in flight is also a test and reports it.
  if (login_queued_ && !login_queued && login_queued_test && !test) EmitTestResult(enabled_ ? tr("Missing server url, username or password.") : tr("Jellyfin is disabled."));
  login_test_pending_ = false;
  login_queued_ = false;
  login_queued_test_ = false;

  const AuthReply auth_reply = ParseAuthReply(reply);

  if (!IsConfiguredLogin(server_url, username, password)) {
    // The configured server or credentials changed while logging in, don't use the access token for the configured server.
    qLog(Debug) << "Jellyfin:" << "Ignoring login for" << Utilities::UrlForLog(server_url) << "since the configured server or credentials changed.";
    if (test) EmitTestResult(auth_reply.error);
    // No login for the new settings follows, so the requests waiting for a login can't be sent.
    if (!login_queued && !login_after_password_load_) {
      const QString error = tr("The Jellyfin settings changed while logging in.");
      pending_scrobble_requests_.clear();
      if (scrobble_request_) scrobble_request_->AuthenticationFailed(error);
      CatalogLoginFailed(error);
    }
  }
  else if (!auth_reply.error.isEmpty()) {
    AuthError(auth_reply, test);
  }
  else {
    login_rejected_ = false;
    login_failed_timer_.invalidate();
    failed_password_.clear();
    SetAuth(auth_reply.access_token, auth_reply.user_id, server_url, username);
    if (test) EmitTestResult(QString());

    // Send any playback reports that were waiting for authentication to complete and retry the playback reports that hit a 401 while a stale access token was in use.
    FlushPendingScrobbles();
    ScrobbleRequest()->LoginSucceeded();

    // Only load the catalogs if a catalog request was made before the login completed (e.g. via the streaming tab).
    const int catalogs = pending_catalogs_;
    pending_catalogs_ = 0;
    reauthenticating_ = false;
    if (catalogs != 0) {
      qLog(Debug) << "Jellyfin:" << "Login successful, loading catalogs.";
      GetCatalogs(catalogs);
    }
  }

  // A failed login above blocks the queued login too, unless it's a test.
  if (login_queued && (login_queued_test || AutomaticLoginAllowed())) {
    Login(login_queued_test);
  }
  else if (login_queued) {
    const QString error = tr("Logging in failed, not trying again automatically yet.");
    pending_scrobble_requests_.clear();
    if (scrobble_request_) scrobble_request_->AuthenticationFailed(error);
    CatalogLoginFailed(error);
  }

  // The login waiting for the password read emits it when it finished.
  if (!login_in_flight_ && !login_after_password_load_) {
    Q_EMIT AuthFinished();
  }

}

void JellyfinService::HandleTestAuthReply(QNetworkReply *reply, const QUrl &server_url, const bool http2, const bool verify_certificate) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  if (reply == test_login_reply_) test_login_reply_ = nullptr;
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  // A test of another server or other credentials only reports the result, the configured authentication and queued requests are left alone.
  const AuthReply auth_reply = ParseAuthReply(reply);
  if (auth_reply.error.isEmpty()) {
    // The access token was only needed for the test.
    LogoutTestSession(server_url, auth_reply.access_token, http2, verify_certificate);
  }
  else {
    qLog(Error) << "Jellyfin:" << auth_reply.error;
  }
  EmitTestResult(auth_reply.error);

}

void JellyfinService::LogoutTestSession(const QUrl &server_url, const QString &access_token, const bool http2, const bool verify_certificate) {

  const QNetworkRequest network_request = CreateServerRequest(ServerUrl(server_url, u"Sessions/Logout"_s), access_token, true, http2, verify_certificate);

  QNetworkReply *reply = network_->post(network_request, QByteArray());
  HandleRedirects(reply);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &JellyfinService::HandleSSLErrors);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    if (!replies_.contains(reply)) return;
    replies_.removeAll(reply);
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
      qLog(Debug) << "Jellyfin:" << "Could not log out the test session:" << reply->errorString();
    }
  });

}

void JellyfinService::SetAuth(const QString &access_token, const QString &user_id, const QUrl &server_url, const QString &username) {

  access_token_ = access_token;
  user_id_ = user_id;
  authenticated_server_url_ = NormalizedServerUrl(server_url);
  authenticated_username_ = username;
  TokenValidated();

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

void JellyfinService::AuthError(const AuthReply &auth_reply, const bool test) {

  const QString &error = auth_reply.error;
  qLog(Error) << "Jellyfin:" << error;
  if (test) EmitTestResult(error);

  // Don't log in automatically again for a while, or at all with rejected credentials, Jellyfin locks the account after a few failed logins.
  // Failing to reach the server doesn't count towards the lockout, so logging in is tried again right away.
  if (!auth_reply.network_error) {
    login_failed_timer_.start();
    failed_password_ = password_;
  }
  if (auth_reply.credentials_rejected) login_rejected_ = true;

  // Playback reports waiting for a login can't be sent.
  pending_scrobble_requests_.clear();

  // Clear the authentication-in-progress state so a failed login attempt does not leave the service stuck, this includes a failed login on startup.
  CatalogLoginFailed(error);

  // Playback reports queued for a retry after a 401 can't be sent without a login.
  if (scrobble_request_) scrobble_request_->AuthenticationFailed(error);

}

bool JellyfinService::DeferCatalogRequest(const int catalogs) {

  // Wait for a login in flight (startup auto-login or re-authentication) or start one, the catalogs are loaded when the login succeeded.
  // A login is not started automatically again for a while after a failed login.
  if (EnsureLogin()) {
    pending_catalogs_ |= catalogs;
    reauthenticating_ = true;
    return true;
  }

  pending_catalogs_ = 0;
  reauthenticating_ = false;

  return false;

}

void JellyfinService::GetArtists() {
  GetCatalogs(CatalogArtists);
}

void JellyfinService::GetAlbums() {
  GetCatalogs(CatalogAlbums);
}

void JellyfinService::GetSongs() {
  GetCatalogs(CatalogSongs);
}

void JellyfinService::GetCatalogs(const int catalogs) {

  if (!authenticated()) {
    if (!server_url_.isValid() || !HasCredentials()) {
      EmitCatalogResults(catalogs, SongMap(), tr("Not authenticated with Jellyfin."));
      return;
    }
    if (!DeferCatalogRequest(catalogs)) {
      EmitCatalogResults(catalogs, SongMap(), tr("Not authenticated with Jellyfin, logging in failed."));
    }
    return;
  }

  if (catalogs & CatalogArtists) StartCatalogRequest(CatalogArtists);
  if (catalogs & CatalogAlbums) StartCatalogRequest(CatalogAlbums);
  if (catalogs & CatalogSongs) StartCatalogRequest(CatalogSongs);

}

JellyfinRequestPtr &JellyfinService::CatalogRequest(const int catalog) {

  switch (catalog) {
    case CatalogArtists:
      return artists_request_;
    case CatalogAlbums:
      return albums_request_;
    default:
      return songs_request_;
  }

}

void JellyfinService::StartCatalogRequest(const int catalog) {

  JellyfinBaseRequest::Type type = JellyfinBaseRequest::Type::FavouriteSongs;
  if (catalog == CatalogArtists) type = JellyfinBaseRequest::Type::FavouriteArtists;
  else if (catalog == CatalogAlbums) type = JellyfinBaseRequest::Type::FavouriteAlbums;

  JellyfinRequestPtr &request = CatalogRequest(catalog);
  ResetRequest(request);
  request.reset(new JellyfinRequest(this, network_, type, this));
  QObject::connect(&*request, &JellyfinRequest::Results, this, [this, catalog](const int id, const SongMap &songs, const QString &error) { Q_UNUSED(id) CatalogResultsReceived(catalog, songs, error); });
  QObject::connect(&*request, &JellyfinRequest::UpdateStatus, this, [this, catalog](const int id, const QString &text) { Q_UNUSED(id) CatalogUpdateStatusReceived(catalog, text); });
  QObject::connect(&*request, &JellyfinRequest::UpdateProgress, this, [this, catalog](const int id, const int progress) { Q_UNUSED(id) CatalogUpdateProgressReceived(catalog, progress); });

  request->Process();

}

void JellyfinService::ResetArtistsRequest() {
  // Don't load the aborted catalog when a login finished either.
  pending_catalogs_ &= ~CatalogArtists;
  ResetRequest(artists_request_);
}

void JellyfinService::ResetAlbumsRequest() {
  // Don't load the aborted catalog when a login finished either.
  pending_catalogs_ &= ~CatalogAlbums;
  ResetRequest(albums_request_);
}

void JellyfinService::ResetSongsRequest() {
  // Don't load the aborted catalog when a login finished either.
  pending_catalogs_ &= ~CatalogSongs;
  ResetRequest(songs_request_);
}

void JellyfinService::CatalogResultsReceived(const int catalog, const SongMap &songs, const QString &error) {

  ResetRequest(CatalogRequest(catalog));

  // The access token was rejected and the catalog is loaded again when logged in, don't show the error meanwhile.
  if (!error.isEmpty() && (pending_catalogs_ & catalog)) return;

  if (error.isEmpty()) {
    catalog_401_logins_ = 0;
    // The views update the collection backends with the results.
    EmitCatalogResults(catalog, songs, error);
  }
  else {
    // Results with an error can be incomplete, and updating the collection with them would remove the songs that were not received.
    EmitCatalogResults(catalog, SongMap(), error);
  }

}

void JellyfinService::CatalogUpdateStatusReceived(const int catalog, const QString &text) {

  switch (catalog) {
    case CatalogArtists:
      Q_EMIT ArtistsUpdateStatus(text);
      break;
    case CatalogAlbums:
      Q_EMIT AlbumsUpdateStatus(text);
      break;
    default:
      Q_EMIT SongsUpdateStatus(text);
      break;
  }

}

void JellyfinService::CatalogUpdateProgressReceived(const int catalog, const int progress) {

  switch (catalog) {
    case CatalogArtists:
      Q_EMIT ArtistsUpdateProgress(progress);
      break;
    case CatalogAlbums:
      Q_EMIT AlbumsUpdateProgress(progress);
      break;
    default:
      Q_EMIT SongsUpdateProgress(progress);
      break;
  }

}

int JellyfinService::Search(const QString &text, const SearchType type) {

  pending_search_id_ = next_pending_search_id_++;
  pending_search_text_ = text;
  pending_search_type_ = type;

  if (text.isEmpty()) {
    timer_search_delay_->stop();
    search_after_login_ = false;
    return pending_search_id_;
  }
  timer_search_delay_->start();

  return pending_search_id_;

}

void JellyfinService::CancelSearch() {
  timer_search_delay_->stop();
  search_after_login_ = false;
  ResetRequest(search_request_);
}

void JellyfinService::StartSearch() {

  search_after_login_ = false;

  if (!authenticated()) {
    if (!server_url_.isValid() || !HasCredentials()) {
      Q_EMIT SearchResults(pending_search_id_, SongMap(), tr("Not authenticated with Jellyfin."));
      return;
    }
    // Search when the login finished, a login is not started automatically again for a while after a failed login.
    if (EnsureLogin()) {
      search_after_login_ = true;
      return;
    }
    Q_EMIT SearchResults(pending_search_id_, SongMap(), tr("Not authenticated with Jellyfin, logging in failed."));
    return;
  }

  search_id_ = pending_search_id_;

  ResetRequest(search_request_);

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
  QObject::connect(&*search_request_, &JellyfinRequest::UpdateStatus, this, &JellyfinService::SearchUpdateStatusReceived);
  QObject::connect(&*search_request_, &JellyfinRequest::UpdateProgress, this, &JellyfinService::SearchUpdateProgressReceived);

  search_request_->Search(search_id_, pending_search_text_);

}

void JellyfinService::SearchLoginFinished() {

  if (!search_after_login_) return;

  // A failed login must not start another login for the same search, this would loop while the server can't be reached.
  if (!authenticated() && !login_in_flight_ && !password_loading()) {
    search_after_login_ = false;
    Q_EMIT SearchResults(pending_search_id_, SongMap(), tr("Not authenticated with Jellyfin, logging in failed."));
    return;
  }

  StartSearch();

}

void JellyfinService::SearchResultsReceived(const int id, const SongMap &songs, const QString &error) {

  ResetRequest(search_request_);

  if (error.isEmpty()) catalog_401_logins_ = 0;

  // The access token was rejected, the search is sent again when logged in, unless a newer search replaces it.
  if (search_after_login_ && id == pending_search_id_) return;

  Q_EMIT SearchResults(id, songs, error);

}

void JellyfinService::SearchUpdateStatusReceived(const int id, const QString &text) {
  Q_EMIT SearchUpdateStatus(id, text);
}

void JellyfinService::SearchUpdateProgressReceived(const int id, const int progress) {
  Q_EMIT SearchUpdateProgress(id, progress);
}
