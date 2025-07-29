/*
 * Strawberry Music Player
 * Copyright 2022-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <memory>

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QTimer>

#include "constants/spotifysettings.h"
#include "core/logging.h"
#include "core/song.h"
#include "core/settings.h"
#include "core/taskmanager.h"
#include "core/database.h"
#include "core/networkaccessmanager.h"
#include "core/oauthenticator.h"
#include "streaming/streamingsearchview.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "spotifyservice.h"
#include "spotifybaserequest.h"
#include "spotifyrequest.h"
#include "spotifyfavoriterequest.h"

using namespace Qt::Literals::StringLiterals;

const Song::Source SpotifyService::kSource = Song::Source::Spotify;
const char SpotifyService::kApiUrl[] = "https://api.spotify.com/v1";

namespace {

constexpr char kOAuthAuthorizeUrl[] = "https://accounts.spotify.com/authorize";
constexpr char kOAuthAccessTokenUrl[] = "https://accounts.spotify.com/api/token";
constexpr char kOAuthRedirectUrl[] = "http://localhost:63111/";
constexpr char kOAuthScope[] = "user-follow-read user-follow-modify user-library-read user-library-modify streaming";
constexpr char kClientIDB64[] = "ZTZjY2Y2OTQ5NzY1NGE3NThjOTAxNWViYzdiMWQzMTc=";
constexpr char kClientSecretB64[] = "N2ZlMDMxODk1NTBlNDE3ZGI1ZWQ1MzE3ZGZlZmU2MTE=";
constexpr char kArtistsSongsTable[] = "spotify_artists_songs";
constexpr char kAlbumsSongsTable[] = "spotify_albums_songs";
constexpr char kSongsTable[] = "spotify_songs";

}  // namespace

using std::make_shared;
using namespace std::chrono_literals;

SpotifyService::SpotifyService(const SharedPtr<TaskManager> task_manager,
                               const SharedPtr<Database> database,
                               const SharedPtr<NetworkAccessManager> network,
                               const SharedPtr<AlbumCoverLoader> albumcover_loader,
                               QObject *parent)
    : StreamingService(Song::Source::Spotify, u"Spotify"_s, u"spotify"_s, QLatin1String(SpotifySettings::kSettingsGroup), parent),
      network_(network),
      oauth_(new OAuthenticator(network, this)),
      artists_collection_backend_(nullptr),
      albums_collection_backend_(nullptr),
      songs_collection_backend_(nullptr),
      artists_collection_model_(nullptr),
      albums_collection_model_(nullptr),
      songs_collection_model_(nullptr),
      timer_search_delay_(new QTimer(this)),
      favorite_request_(new SpotifyFavoriteRequest(this, network_, this)),
      enabled_(false),
      artistssearchlimit_(1),
      albumssearchlimit_(1),
      songssearchlimit_(1),
      fetchalbums_(true),
      download_album_covers_(true),
      pending_search_id_(0),
      next_pending_search_id_(1),
      pending_search_type_(SearchType::Artists),
      search_id_(0) {

  oauth_->set_settings_group(QLatin1String(SpotifySettings::kSettingsGroup));
  oauth_->set_type(OAuthenticator::Type::Authorization_Code);
  oauth_->set_authorize_url(QUrl(QLatin1String(kOAuthAuthorizeUrl)));
  oauth_->set_redirect_url(QUrl(QLatin1String(kOAuthRedirectUrl)));
  oauth_->set_access_token_url(QUrl(QLatin1String(kOAuthAccessTokenUrl)));
  oauth_->set_client_id(QString::fromLatin1(QByteArray::fromBase64(kClientIDB64)));
  oauth_->set_client_secret(QString::fromLatin1(QByteArray::fromBase64(kClientSecretB64)));
  oauth_->set_scope(QLatin1String(kOAuthScope));
  oauth_->set_use_local_redirect_server(true);
  oauth_->set_random_port(false);
  QObject::connect(oauth_, &OAuthenticator::AuthenticationFinished, this, &SpotifyService::OAuthFinished);

  // Backends

  artists_collection_backend_ = make_shared<CollectionBackend>();
  artists_collection_backend_->moveToThread(database->thread());
  artists_collection_backend_->Init(database, task_manager, Song::Source::Spotify, QLatin1String(kArtistsSongsTable));

  albums_collection_backend_ = make_shared<CollectionBackend>();
  albums_collection_backend_->moveToThread(database->thread());
  albums_collection_backend_->Init(database, task_manager, Song::Source::Spotify, QLatin1String(kAlbumsSongsTable));

  songs_collection_backend_ = make_shared<CollectionBackend>();
  songs_collection_backend_->moveToThread(database->thread());
  songs_collection_backend_->Init(database, task_manager, Song::Source::Spotify, QLatin1String(kSongsTable));

  // Models
  artists_collection_model_ = new CollectionModel(artists_collection_backend_, albumcover_loader, this);
  albums_collection_model_ = new CollectionModel(albums_collection_backend_, albumcover_loader, this);
  songs_collection_model_ = new CollectionModel(songs_collection_backend_, albumcover_loader, this);

  timer_search_delay_->setSingleShot(true);
  QObject::connect(timer_search_delay_, &QTimer::timeout, this, &SpotifyService::StartSearch);

  QObject::connect(this, &SpotifyService::AddArtists, favorite_request_, &SpotifyFavoriteRequest::AddArtists);
  QObject::connect(this, &SpotifyService::AddAlbums, favorite_request_, &SpotifyFavoriteRequest::AddAlbums);
  QObject::connect(this, &SpotifyService::AddSongs, favorite_request_, QOverload<const SongList&>::of(&SpotifyFavoriteRequest::AddSongs));

  QObject::connect(this, &SpotifyService::RemoveArtists, favorite_request_, &SpotifyFavoriteRequest::RemoveArtists);
  QObject::connect(this, &SpotifyService::RemoveAlbums, favorite_request_, &SpotifyFavoriteRequest::RemoveAlbums);
  QObject::connect(this, &SpotifyService::RemoveSongsByList, favorite_request_, QOverload<const SongList&>::of(&SpotifyFavoriteRequest::RemoveSongs));
  QObject::connect(this, &SpotifyService::RemoveSongsByMap, favorite_request_, QOverload<const SongMap&>::of(&SpotifyFavoriteRequest::RemoveSongs));

  QObject::connect(favorite_request_, &SpotifyFavoriteRequest::ArtistsAdded, &*artists_collection_backend_, &CollectionBackend::AddOrUpdateSongs);
  QObject::connect(favorite_request_, &SpotifyFavoriteRequest::AlbumsAdded, &*albums_collection_backend_, &CollectionBackend::AddOrUpdateSongs);
  QObject::connect(favorite_request_, &SpotifyFavoriteRequest::SongsAdded, &*songs_collection_backend_, &CollectionBackend::AddOrUpdateSongs);

  QObject::connect(favorite_request_, &SpotifyFavoriteRequest::ArtistsRemoved, &*artists_collection_backend_, &CollectionBackend::DeleteSongs);
  QObject::connect(favorite_request_, &SpotifyFavoriteRequest::AlbumsRemoved, &*albums_collection_backend_, &CollectionBackend::DeleteSongs);
  QObject::connect(favorite_request_, &SpotifyFavoriteRequest::SongsRemoved, &*songs_collection_backend_, &CollectionBackend::DeleteSongs);

  SpotifyService::ReloadSettings();
  oauth_->LoadSession();

}

SpotifyService::~SpotifyService() {

  artists_collection_backend_->deleteLater();
  albums_collection_backend_->deleteLater();
  songs_collection_backend_->deleteLater();

}

void SpotifyService::Exit() {

  wait_for_exit_ << &*artists_collection_backend_ << &*albums_collection_backend_ << &*songs_collection_backend_;

  QObject::connect(&*artists_collection_backend_, &CollectionBackend::ExitFinished, this, &SpotifyService::ExitReceived);
  QObject::connect(&*albums_collection_backend_, &CollectionBackend::ExitFinished, this, &SpotifyService::ExitReceived);
  QObject::connect(&*songs_collection_backend_, &CollectionBackend::ExitFinished, this, &SpotifyService::ExitReceived);

  artists_collection_backend_->ExitAsync();
  albums_collection_backend_->ExitAsync();
  songs_collection_backend_->ExitAsync();

}

void SpotifyService::ExitReceived() {

  QObject *obj = sender();
  QObject::disconnect(obj, nullptr, this, nullptr);
  qLog(Debug) << obj << "successfully exited.";
  wait_for_exit_.removeAll(obj);
  if (wait_for_exit_.isEmpty()) Q_EMIT ExitFinished();

}

bool SpotifyService::authenticated() const {

  return oauth_->authenticated();

}

QByteArray SpotifyService::authorization_header() const {

  return oauth_->authorization_header();

}

void SpotifyService::ReloadSettings() {

  Settings s;
  s.beginGroup(SpotifySettings::kSettingsGroup);

  enabled_ = s.value(SpotifySettings::kEnabled, false).toBool();

  quint64 search_delay = std::max(s.value(SpotifySettings::kSearchDelay, 1500).toULongLong(), 500ULL);
  artistssearchlimit_ = s.value(SpotifySettings::kArtistsSearchLimit, 4).toInt();
  albumssearchlimit_ = s.value(SpotifySettings::kAlbumsSearchLimit, 10).toInt();
  songssearchlimit_ = s.value(SpotifySettings::kSongsSearchLimit, 10).toInt();
  fetchalbums_ = s.value(SpotifySettings::kFetchAlbums, false).toBool();
  download_album_covers_ = s.value(SpotifySettings::kDownloadAlbumCovers, true).toBool();

  s.endGroup();

  timer_search_delay_->setInterval(static_cast<int>(search_delay));

}

void SpotifyService::Authenticate() {

  oauth_->Authenticate();

}

void SpotifyService::ClearSession() {

  oauth_->ClearSession();

}

void SpotifyService::OAuthFinished(const bool success, const QString &error) {

  if (success) {
    Q_EMIT LoginSuccess();
    Q_EMIT UpdateSpotifyAccessToken(oauth_->access_token());
  }
  else {
    Q_EMIT LoginFailure(error);
  }

  Q_EMIT LoginFinished(success);

}

void SpotifyService::GetArtists() {

  if (!authenticated()) {
    Q_EMIT ArtistsResults(SongMap(), tr("Not authenticated with Spotify."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  artists_request_.reset(new SpotifyRequest(this, network_, SpotifyBaseRequest::Type::FavouriteArtists, this));
  QObject::connect(&*artists_request_, &SpotifyRequest::Results, this, &SpotifyService::ArtistsResultsReceived);
  QObject::connect(&*artists_request_, &SpotifyRequest::UpdateStatus, this, &SpotifyService::ArtistsUpdateStatusReceived);
  QObject::connect(&*artists_request_, &SpotifyRequest::ProgressSetMaximum, this, &SpotifyService::ArtistsProgressSetMaximumReceived);
  QObject::connect(&*artists_request_, &SpotifyRequest::UpdateProgress, this, &SpotifyService::ArtistsUpdateProgressReceived);

  artists_request_->Process();

}

void SpotifyService::ResetArtistsRequest() {

  artists_request_.reset();

}

void SpotifyService::ArtistsResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_UNUSED(id);
  Q_EMIT ArtistsResults(songs, error);
  ResetArtistsRequest();

}

void SpotifyService::ArtistsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  Q_EMIT ArtistsUpdateStatus(text);
}

void SpotifyService::ArtistsProgressSetMaximumReceived(const int id, const int max) {
  Q_UNUSED(id);
  Q_EMIT ArtistsProgressSetMaximum(max);
}

void SpotifyService::ArtistsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  Q_EMIT ArtistsUpdateProgress(progress);
}

void SpotifyService::GetAlbums() {

  if (!authenticated()) {
    Q_EMIT AlbumsResults(SongMap(), tr("Not authenticated with Spotify."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  albums_request_.reset(new SpotifyRequest(this, network_, SpotifyBaseRequest::Type::FavouriteAlbums, this));
  QObject::connect(&*albums_request_, &SpotifyRequest::Results, this, &SpotifyService::AlbumsResultsReceived);
  QObject::connect(&*albums_request_, &SpotifyRequest::UpdateStatus, this, &SpotifyService::AlbumsUpdateStatusReceived);
  QObject::connect(&*albums_request_, &SpotifyRequest::ProgressSetMaximum, this, &SpotifyService::AlbumsProgressSetMaximumReceived);
  QObject::connect(&*albums_request_, &SpotifyRequest::UpdateProgress, this, &SpotifyService::AlbumsUpdateProgressReceived);

  albums_request_->Process();

}

void SpotifyService::ResetAlbumsRequest() {

  albums_request_.reset();

}

void SpotifyService::AlbumsResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_UNUSED(id);
  Q_EMIT AlbumsResults(songs, error);
  ResetAlbumsRequest();

}

void SpotifyService::AlbumsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  Q_EMIT AlbumsUpdateStatus(text);
}

void SpotifyService::AlbumsProgressSetMaximumReceived(const int id, const int max) {
  Q_UNUSED(id);
  Q_EMIT AlbumsProgressSetMaximum(max);
}

void SpotifyService::AlbumsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  Q_EMIT AlbumsUpdateProgress(progress);
}

void SpotifyService::GetSongs() {

  if (!authenticated()) {
    Q_EMIT SongsResults(SongMap(), tr("Not authenticated with Spotify."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  songs_request_.reset(new SpotifyRequest(this, network_, SpotifyBaseRequest::Type::FavouriteSongs, this));
  QObject::connect(&*songs_request_, &SpotifyRequest::Results, this, &SpotifyService::SongsResultsReceived);
  QObject::connect(&*songs_request_, &SpotifyRequest::UpdateStatus, this, &SpotifyService::SongsUpdateStatusReceived);
  QObject::connect(&*songs_request_, &SpotifyRequest::ProgressSetMaximum, this, &SpotifyService::SongsProgressSetMaximumReceived);
  QObject::connect(&*songs_request_, &SpotifyRequest::UpdateProgress, this, &SpotifyService::SongsUpdateProgressReceived);

  songs_request_->Process();

}

void SpotifyService::ResetSongsRequest() {

  songs_request_.reset();

}

void SpotifyService::SongsResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_UNUSED(id);
  Q_EMIT SongsResults(songs, error);
  ResetSongsRequest();

}

void SpotifyService::SongsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  Q_EMIT SongsUpdateStatus(text);
}

void SpotifyService::SongsProgressSetMaximumReceived(const int id, const int max) {
  Q_UNUSED(id);
  Q_EMIT SongsProgressSetMaximum(max);
}

void SpotifyService::SongsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  Q_EMIT SongsUpdateProgress(progress);
}

int SpotifyService::Search(const QString &text, const SearchType type) {

  pending_search_id_ = next_pending_search_id_;
  pending_search_text_ = text;
  pending_search_type_ = type;

  next_pending_search_id_++;

  if (text.isEmpty()) {
    timer_search_delay_->stop();
    return pending_search_id_;
  }
  timer_search_delay_->start();

  return pending_search_id_;

}

void SpotifyService::StartSearch() {

  if (!authenticated()) {
    Q_EMIT SearchResults(pending_search_id_, SongMap(), tr("Not authenticated with Spotify."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  search_id_ = pending_search_id_;
  search_text_ = pending_search_text_;

  SendSearch();

}

void SpotifyService::CancelSearch() {}

void SpotifyService::SendSearch() {

  SpotifyBaseRequest::Type type = SpotifyBaseRequest::Type::None;

  switch (pending_search_type_) {
    case SearchType::Artists:
      type = SpotifyBaseRequest::Type::SearchArtists;
      break;
    case SearchType::Albums:
      type = SpotifyBaseRequest::Type::SearchAlbums;
      break;
    case SearchType::Songs:
      type = SpotifyBaseRequest::Type::SearchSongs;
      break;
    default:
      //Error("Invalid search type.");
      return;
  }

  search_request_.reset(new SpotifyRequest(this, network_, type, this));
  QObject::connect(&*search_request_, &SpotifyRequest::Results, this, &SpotifyService::SearchResultsReceived);
  QObject::connect(&*search_request_, &SpotifyRequest::UpdateStatus, this, &SpotifyService::SearchUpdateStatus);
  QObject::connect(&*search_request_, &SpotifyRequest::ProgressSetMaximum, this, &SpotifyService::SearchProgressSetMaximum);
  QObject::connect(&*search_request_, &SpotifyRequest::UpdateProgress, this, &SpotifyService::SearchUpdateProgress);

  search_request_->Search(search_id_, search_text_);
  search_request_->Process();

}

void SpotifyService::SearchResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_EMIT SearchResults(id, songs, error);
  search_request_.reset();

}
