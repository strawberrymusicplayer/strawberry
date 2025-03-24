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

#include <memory>

#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QTimer>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/settings.h"
#include "core/database.h"
#include "core/song.h"
#include "core/taskmanager.h"
#include "core/networkaccessmanager.h"
#include "core/urlhandlers.h"
#include "core/oauthenticator.h"
#include "constants/tidalsettings.h"
#include "streaming/streamingsearchview.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "covermanager/albumcoverloader.h"
#include "tidalservice.h"
#include "tidalurlhandler.h"
#include "tidalbaserequest.h"
#include "tidalrequest.h"
#include "tidalfavoriterequest.h"
#include "tidalstreamurlrequest.h"

using namespace std::chrono_literals;
using namespace Qt::Literals::StringLiterals;
using std::make_shared;
using namespace TidalSettings;

const Song::Source TidalService::kSource = Song::Source::Tidal;

const char TidalService::kApiUrl[] = "https://api.tidalhifi.com/v1";
const char TidalService::kResourcesUrl[] = "https://resources.tidal.com";

namespace {

constexpr char kOAuthUrl[] = "https://login.tidal.com/authorize";
constexpr char kOAuthAccessTokenUrl[] = "https://login.tidal.com/oauth2/token";
constexpr char kOAuthRedirectUrl[] = "tidal://login/auth";
constexpr char kOAuthScope[] = "r_usr w_usr";

constexpr char kArtistsSongsTable[] = "tidal_artists_songs";
constexpr char kAlbumsSongsTable[] = "tidal_albums_songs";
constexpr char kSongsTable[] = "tidal_songs";

}  // namespace

TidalService::TidalService(const SharedPtr<TaskManager> task_manager,
                           const SharedPtr<Database> database,
                           const SharedPtr<NetworkAccessManager> network,
                           const SharedPtr<UrlHandlers> url_handlers,
                           const SharedPtr<AlbumCoverLoader> albumcover_loader,
                           QObject *parent)
    : StreamingService(Song::Source::Tidal, u"Tidal"_s, u"tidal"_s, QLatin1String(TidalSettings::kSettingsGroup), parent),
      network_(network),
      url_handler_(new TidalUrlHandler(task_manager, this)),
      oauth_(new OAuthenticator(network, this)),
      artists_collection_backend_(nullptr),
      albums_collection_backend_(nullptr),
      songs_collection_backend_(nullptr),
      artists_collection_model_(nullptr),
      albums_collection_model_(nullptr),
      songs_collection_model_(nullptr),
      timer_search_delay_(new QTimer(this)),
      favorite_request_(new TidalFavoriteRequest(this, network_, this)),
      enabled_(false),
      artistssearchlimit_(1),
      albumssearchlimit_(1),
      songssearchlimit_(1),
      fetchalbums_(true),
      download_album_covers_(true),
      stream_url_method_(TidalSettings::StreamUrlMethod::StreamUrl),
      album_explicit_(false),
      pending_search_id_(0),
      next_pending_search_id_(1),
      pending_search_type_(SearchType::Artists),
      search_id_(0),
      next_stream_url_request_id_(0) {

  url_handlers->Register(url_handler_);

  oauth_->set_settings_group(QLatin1String(kSettingsGroup));
  oauth_->set_type(OAuthenticator::Type::Authorization_Code);
  oauth_->set_authorize_url(QUrl(QLatin1String(kOAuthUrl)));
  oauth_->set_redirect_url(QUrl(QLatin1String(kOAuthRedirectUrl)));
  oauth_->set_access_token_url(QUrl(QLatin1String(kOAuthAccessTokenUrl)));
  oauth_->set_scope(QLatin1String(kOAuthScope));
  oauth_->set_use_local_redirect_server(false);
  oauth_->set_random_port(false);
  QObject::connect(oauth_, &OAuthenticator::AuthenticationFinished, this, &TidalService::OAuthFinished);

  // Backends

  artists_collection_backend_ = make_shared<CollectionBackend>();
  artists_collection_backend_->moveToThread(database->thread());
  artists_collection_backend_->Init(database, task_manager, Song::Source::Tidal, QLatin1String(kArtistsSongsTable));

  albums_collection_backend_ = make_shared<CollectionBackend>();
  albums_collection_backend_->moveToThread(database->thread());
  albums_collection_backend_->Init(database, task_manager, Song::Source::Tidal, QLatin1String(kAlbumsSongsTable));

  songs_collection_backend_ = make_shared<CollectionBackend>();
  songs_collection_backend_->moveToThread(database->thread());
  songs_collection_backend_->Init(database, task_manager, Song::Source::Tidal, QLatin1String(kSongsTable));

  // Models
  artists_collection_model_ = new CollectionModel(artists_collection_backend_, albumcover_loader, this);
  albums_collection_model_ = new CollectionModel(albums_collection_backend_, albumcover_loader, this);
  songs_collection_model_ = new CollectionModel(songs_collection_backend_, albumcover_loader, this);

  // Search

  timer_search_delay_->setSingleShot(true);
  QObject::connect(timer_search_delay_, &QTimer::timeout, this, &TidalService::StartSearch);

  QObject::connect(this, &TidalService::AddArtists, favorite_request_, &TidalFavoriteRequest::AddArtists);
  QObject::connect(this, &TidalService::AddAlbums, favorite_request_, &TidalFavoriteRequest::AddAlbums);
  QObject::connect(this, &TidalService::AddSongs, favorite_request_, QOverload<const SongList&>::of(&TidalFavoriteRequest::AddSongs));

  QObject::connect(this, &TidalService::RemoveArtists, favorite_request_, &TidalFavoriteRequest::RemoveArtists);
  QObject::connect(this, &TidalService::RemoveAlbums, favorite_request_, &TidalFavoriteRequest::RemoveAlbums);
  QObject::connect(this, &TidalService::RemoveSongsByList, favorite_request_, QOverload<const SongList&>::of(&TidalFavoriteRequest::RemoveSongs));
  QObject::connect(this, &TidalService::RemoveSongsByMap, favorite_request_, QOverload<const SongMap&>::of(&TidalFavoriteRequest::RemoveSongs));

  QObject::connect(favorite_request_, &TidalFavoriteRequest::ArtistsAdded, &*artists_collection_backend_, &CollectionBackend::AddOrUpdateSongs);
  QObject::connect(favorite_request_, &TidalFavoriteRequest::AlbumsAdded, &*albums_collection_backend_, &CollectionBackend::AddOrUpdateSongs);
  QObject::connect(favorite_request_, &TidalFavoriteRequest::SongsAdded, &*songs_collection_backend_, &CollectionBackend::AddOrUpdateSongs);

  QObject::connect(favorite_request_, &TidalFavoriteRequest::ArtistsRemoved, &*artists_collection_backend_, &CollectionBackend::DeleteSongs);
  QObject::connect(favorite_request_, &TidalFavoriteRequest::AlbumsRemoved, &*albums_collection_backend_, &CollectionBackend::DeleteSongs);
  QObject::connect(favorite_request_, &TidalFavoriteRequest::SongsRemoved, &*songs_collection_backend_, &CollectionBackend::DeleteSongs);

  TidalService::ReloadSettings();
  oauth_->LoadSession();

}

TidalService::~TidalService() {

  while (!stream_url_requests_.isEmpty()) {
    TidalStreamURLRequestPtr stream_url_request = stream_url_requests_.take(stream_url_requests_.firstKey());
    QObject::disconnect(&*stream_url_request, nullptr, this, nullptr);
  }

}

bool TidalService::authenticated() const {

  return oauth_->authenticated();

}

QByteArray TidalService::authorization_header() const {

  return oauth_->authorization_header();

}

QString TidalService::country_code() const {

  return oauth_->country_code();

}

quint64 TidalService::user_id() const {

  return oauth_->user_id();

}

void TidalService::Exit() {

  wait_for_exit_ << &*artists_collection_backend_ << &*albums_collection_backend_ << &*songs_collection_backend_;

  QObject::connect(&*artists_collection_backend_, &CollectionBackend::ExitFinished, this, &TidalService::ExitReceived);
  QObject::connect(&*albums_collection_backend_, &CollectionBackend::ExitFinished, this, &TidalService::ExitReceived);
  QObject::connect(&*songs_collection_backend_, &CollectionBackend::ExitFinished, this, &TidalService::ExitReceived);

  artists_collection_backend_->ExitAsync();
  albums_collection_backend_->ExitAsync();
  songs_collection_backend_->ExitAsync();

}

void TidalService::ExitReceived() {

  QObject *obj = sender();
  QObject::disconnect(obj, nullptr, this, nullptr);
  qLog(Debug) << obj << "successfully exited.";
  wait_for_exit_.removeAll(obj);
  if (wait_for_exit_.isEmpty()) Q_EMIT ExitFinished();

}

void TidalService::ReloadSettings() {

  Settings s;
  s.beginGroup(TidalSettings::kSettingsGroup);
  enabled_ = s.value(TidalSettings::kEnabled, false).toBool();
  client_id_ = s.value(TidalSettings::kClientId).toString();
  quality_ = s.value(TidalSettings::kQuality, u"LOSSLESS"_s).toString();
  quint64 search_delay = s.value(TidalSettings::kSearchDelay, 1500).toULongLong();
  artistssearchlimit_ = s.value(TidalSettings::kArtistsSearchLimit, 4).toInt();
  albumssearchlimit_ = s.value(TidalSettings::kAlbumsSearchLimit, 10).toInt();
  songssearchlimit_ = s.value(TidalSettings::kSongsSearchLimit, 10).toInt();
  fetchalbums_ = s.value(TidalSettings::kFetchAlbums, false).toBool();
  coversize_ = s.value(TidalSettings::kCoverSize, u"640x640"_s).toString();
  download_album_covers_ = s.value(TidalSettings::kDownloadAlbumCovers, true).toBool();
  stream_url_method_ = static_cast<TidalSettings::StreamUrlMethod>(s.value(TidalSettings::kStreamUrl, static_cast<int>(TidalSettings::StreamUrlMethod::StreamUrl)).toInt());
  album_explicit_ = s.value(TidalSettings::kAlbumExplicit).toBool();
  s.endGroup();

  oauth_->set_client_id(client_id_);
  timer_search_delay_->setInterval(static_cast<int>(search_delay));

}

void TidalService::StartAuthorization(const QString &client_id) {

  oauth_->set_client_id(client_id);
  oauth_->Authenticate();

}

void TidalService::OAuthFinished(const bool success, const QString &error) {

  if (success) {
    qLog(Debug) << "Tidal: Login successful" << "user id" << user_id();
    Q_EMIT LoginFinished(true);
    Q_EMIT LoginSuccess();
  }
  else {
    Q_EMIT LoginFailure(error);
    Q_EMIT LoginFinished(false);
  }

}

void TidalService::AuthorizationUrlReceived(const QUrl &url) {

  qLog(Debug) << "Tidal: Authorization URL Received" << url;

  oauth_->ExternalAuthorizationUrlReceived(url);

}

void TidalService::ClearSession() {

  oauth_->ClearSession();

}

void TidalService::GetArtists() {

  if (!authenticated()) {
    Q_EMIT ArtistsResults(SongMap(), tr("Not authenticated with Tidal."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  artists_request_.reset(new TidalRequest(this, url_handler_, network_, TidalBaseRequest::Type::FavouriteArtists, this));
  QObject::connect(&*artists_request_, &TidalRequest::Results, this, &TidalService::ArtistsResultsReceived);
  QObject::connect(&*artists_request_, &TidalRequest::UpdateStatus, this, &TidalService::ArtistsUpdateStatusReceived);
  QObject::connect(&*artists_request_, &TidalRequest::UpdateProgress, this, &TidalService::ArtistsUpdateProgressReceived);

  artists_request_->Process();

}


void TidalService::ResetArtistsRequest() {

  artists_request_.reset();

}

void TidalService::ArtistsResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_UNUSED(id);
  Q_EMIT ArtistsResults(songs, error);
  ResetArtistsRequest();

}

void TidalService::ArtistsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  Q_EMIT ArtistsUpdateStatus(text);
}

void TidalService::ArtistsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  Q_EMIT ArtistsUpdateProgress(progress);
}

void TidalService::GetAlbums() {

  if (!authenticated()) {
    Q_EMIT AlbumsResults(SongMap(), tr("Not authenticated with Tidal."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  albums_request_.reset(new TidalRequest(this, url_handler_, network_, TidalBaseRequest::Type::FavouriteAlbums, this));
  QObject::connect(&*albums_request_, &TidalRequest::Results, this, &TidalService::AlbumsResultsReceived);
  QObject::connect(&*albums_request_, &TidalRequest::UpdateStatus, this, &TidalService::AlbumsUpdateStatusReceived);
  QObject::connect(&*albums_request_, &TidalRequest::UpdateProgress, this, &TidalService::AlbumsUpdateProgressReceived);

  albums_request_->Process();

}

void TidalService::ResetAlbumsRequest() {

  albums_request_.reset();

}

void TidalService::AlbumsResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_UNUSED(id);
  Q_EMIT AlbumsResults(songs, error);
  ResetAlbumsRequest();

}

void TidalService::AlbumsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  Q_EMIT AlbumsUpdateStatus(text);
}

void TidalService::AlbumsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  Q_EMIT AlbumsUpdateProgress(progress);
}

void TidalService::GetSongs() {

  if (!authenticated()) {
    Q_EMIT SongsResults(SongMap(), tr("Not authenticated with Tidal."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  songs_request_.reset(new TidalRequest(this, url_handler_, network_, TidalBaseRequest::Type::FavouriteSongs, this));
  QObject::connect(&*songs_request_, &TidalRequest::Results, this, &TidalService::SongsResultsReceived);
  QObject::connect(&*songs_request_, &TidalRequest::UpdateStatus, this, &TidalService::SongsUpdateStatusReceived);
  QObject::connect(&*songs_request_, &TidalRequest::UpdateProgress, this, &TidalService::SongsUpdateProgressReceived);

  songs_request_->Process();

}

void TidalService::ResetSongsRequest() {

  songs_request_.reset();

}

void TidalService::SongsResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_UNUSED(id);
  Q_EMIT SongsResults(songs, error);
  ResetSongsRequest();

}

void TidalService::SongsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  Q_EMIT SongsUpdateStatus(text);
}

void TidalService::SongsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  Q_EMIT SongsUpdateProgress(progress);
}

int TidalService::Search(const QString &text, const SearchType type) {

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

void TidalService::StartSearch() {

  if (!authenticated()) {
    Q_EMIT SearchResults(pending_search_id_, SongMap(), tr("Not authenticated with Tidal."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  search_id_ = pending_search_id_;
  search_text_ = pending_search_text_;

  SendSearch();

}

void TidalService::CancelSearch() {}

void TidalService::SendSearch() {

  TidalBaseRequest::Type query_type = TidalBaseRequest::Type::None;

  switch (pending_search_type_) {
    case SearchType::Artists:
      query_type = TidalBaseRequest::Type::SearchArtists;
      break;
    case SearchType::Albums:
      query_type = TidalBaseRequest::Type::SearchAlbums;
      break;
    case SearchType::Songs:
      query_type = TidalBaseRequest::Type::SearchSongs;
      break;
    default:
      //Error("Invalid search type.");
      return;
  }

  search_request_.reset(new TidalRequest(this, url_handler_, network_, query_type, this));
  QObject::connect(&*search_request_, &TidalRequest::Results, this, &TidalService::SearchResultsReceived);
  QObject::connect(&*search_request_, &TidalRequest::UpdateStatus, this, &TidalService::SearchUpdateStatus);
  QObject::connect(&*search_request_, &TidalRequest::UpdateProgress, this, &TidalService::SearchUpdateProgress);

  search_request_->Search(search_id_, search_text_);
  search_request_->Process();

}

void TidalService::SearchResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_EMIT SearchResults(id, songs, error);
  search_request_.reset();

}

uint TidalService::GetStreamURL(const QUrl &url, QString &error) {

  if (!authenticated()) {
    error = tr("Not authenticated with Tidal.");
    return 0;
  }

  uint id = 0;
  while (id == 0) id = ++next_stream_url_request_id_;
  TidalStreamURLRequestPtr stream_url_request = TidalStreamURLRequestPtr(new TidalStreamURLRequest(this, network_, url, id), &QObject::deleteLater);
  stream_url_requests_.insert(id, stream_url_request);
  QObject::connect(&*stream_url_request, &TidalStreamURLRequest::StreamURLFailure, this, &TidalService::HandleStreamURLFailure);
  QObject::connect(&*stream_url_request, &TidalStreamURLRequest::StreamURLSuccess, this, &TidalService::HandleStreamURLSuccess);
  stream_url_request->Process();

  return id;

}

void TidalService::HandleStreamURLFailure(const uint id, const QUrl &media_url, const QString &error) {

  if (!stream_url_requests_.contains(id)) return;
  stream_url_requests_.remove(id);

  Q_EMIT StreamURLFailure(id, media_url, error);

}

void TidalService::HandleStreamURLSuccess(const uint id, const QUrl &media_url, const QUrl &stream_url, const Song::FileType filetype, const int samplerate, const int bit_depth, const qint64 duration) {

  if (!stream_url_requests_.contains(id)) return;
  stream_url_requests_.remove(id);

  Q_EMIT StreamURLSuccess(id, media_url, stream_url, filetype, samplerate, bit_depth, duration);

}
