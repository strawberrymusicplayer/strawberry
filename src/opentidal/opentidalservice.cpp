/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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
#include "constants/opentidalsettings.h"
#include "streaming/streamingsearchview.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "covermanager/albumcoverloader.h"
#include "opentidalservice.h"
#include "opentidalurlhandler.h"
#include "opentidalbaserequest.h"
#include "opentidalrequest.h"
#include "opentidalfavoriterequest.h"
#include "opentidalstreamurlrequest.h"

using namespace std::chrono_literals;
using namespace Qt::Literals::StringLiterals;
using std::make_shared;
using namespace OpenTidalSettings;

const Song::Source OpenTidalService::kSource = Song::Source::OpenTidal;

const char OpenTidalService::kApiUrl[] = "https://openapi.tidal.com/v2";
const char OpenTidalService::kResourcesUrl[] = "https://resources.tidal.com";

namespace {

constexpr char kOAuthUrl[] = "https://login.tidal.com/authorize";
constexpr char kOAuthAccessTokenUrl[] = "https://auth.tidal.com/v1/oauth2/token";
constexpr char kOAuthRedirectUrl[] = "opentidal://login/auth";
constexpr char kOAuthScope[] = "user.read collection.read collection.write search.read recommendations.read playback";
constexpr char kApiClientIdB64[] = "RHBwV3FpTEM4ZFJSV1RJaQ==";
constexpr char kApiClientSecretB64[] = "cGk0QmxpclZXQWlteWpBc0RnWmZ5RmVlRzA2b3E1blVBVTljUW1IdFhDST0=";

constexpr char kArtistsSongsTable[] = "opentidal_artists_songs";
constexpr char kAlbumsSongsTable[] = "opentidal_albums_songs";
constexpr char kSongsTable[] = "opentidal_songs";

}  // namespace

OpenTidalService::OpenTidalService(const SharedPtr<TaskManager> task_manager,
                           const SharedPtr<Database> database,
                           const SharedPtr<NetworkAccessManager> network,
                           const SharedPtr<UrlHandlers> url_handlers,
                           const SharedPtr<AlbumCoverLoader> albumcover_loader,
                           QObject *parent)
    : StreamingService(Song::Source::OpenTidal, u"Open Tidal"_s, u"opentidal"_s, QLatin1String(OpenTidalSettings::kSettingsGroup), parent),
      network_(network),
      url_handler_(new OpenTidalUrlHandler(task_manager, this)),
      oauth_(new OAuthenticator(network, this)),
      artists_collection_backend_(nullptr),
      albums_collection_backend_(nullptr),
      songs_collection_backend_(nullptr),
      artists_collection_model_(nullptr),
      albums_collection_model_(nullptr),
      songs_collection_model_(nullptr),
      timer_search_delay_(new QTimer(this)),
      favorite_request_(new OpenTidalFavoriteRequest(this, network_, this)),
      enabled_(false),
      uri_scheme_(OpenTidalSettings::UriScheme::DATA),
      manifest_type_(OpenTidalSettings::ManifestType::MPEG_DASH),
      artistssearchlimit_(1),
      albumssearchlimit_(1),
      songssearchlimit_(1),
      fetchalbums_(true),
      download_album_covers_(true),
      album_explicit_(false),
      remove_remastered_(true),
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
  oauth_->set_client_id(QString::fromLatin1(QByteArray::fromBase64(kApiClientIdB64)));
  oauth_->set_client_secret(QString::fromLatin1(QByteArray::fromBase64(kApiClientSecretB64)));
  QObject::connect(oauth_, &OAuthenticator::AuthenticationFinished, this, &OpenTidalService::OAuthFinished);

  // Backends

  artists_collection_backend_ = make_shared<CollectionBackend>();
  artists_collection_backend_->moveToThread(database->thread());
  artists_collection_backend_->Init(database, task_manager, Song::Source::OpenTidal, QLatin1String(kArtistsSongsTable));

  albums_collection_backend_ = make_shared<CollectionBackend>();
  albums_collection_backend_->moveToThread(database->thread());
  albums_collection_backend_->Init(database, task_manager, Song::Source::OpenTidal, QLatin1String(kAlbumsSongsTable));

  songs_collection_backend_ = make_shared<CollectionBackend>();
  songs_collection_backend_->moveToThread(database->thread());
  songs_collection_backend_->Init(database, task_manager, Song::Source::OpenTidal, QLatin1String(kSongsTable));

  // Models
  artists_collection_model_ = new CollectionModel(artists_collection_backend_, albumcover_loader, this);
  albums_collection_model_ = new CollectionModel(albums_collection_backend_, albumcover_loader, this);
  songs_collection_model_ = new CollectionModel(songs_collection_backend_, albumcover_loader, this);

  // Search

  timer_search_delay_->setSingleShot(true);
  QObject::connect(timer_search_delay_, &QTimer::timeout, this, &OpenTidalService::StartSearch);

  QObject::connect(this, &OpenTidalService::AddArtists, favorite_request_, &OpenTidalFavoriteRequest::AddArtists);
  QObject::connect(this, &OpenTidalService::AddAlbums, favorite_request_, &OpenTidalFavoriteRequest::AddAlbums);
  QObject::connect(this, &OpenTidalService::AddSongs, favorite_request_, QOverload<const SongList&>::of(&OpenTidalFavoriteRequest::AddSongs));

  QObject::connect(this, &OpenTidalService::RemoveArtists, favorite_request_, &OpenTidalFavoriteRequest::RemoveArtists);
  QObject::connect(this, &OpenTidalService::RemoveAlbums, favorite_request_, &OpenTidalFavoriteRequest::RemoveAlbums);
  QObject::connect(this, &OpenTidalService::RemoveSongsByList, favorite_request_, QOverload<const SongList&>::of(&OpenTidalFavoriteRequest::RemoveSongs));
  QObject::connect(this, &OpenTidalService::RemoveSongsByMap, favorite_request_, QOverload<const SongMap&>::of(&OpenTidalFavoriteRequest::RemoveSongs));

  QObject::connect(favorite_request_, &OpenTidalFavoriteRequest::ArtistsAdded, &*artists_collection_backend_, &CollectionBackend::AddOrUpdateSongs);
  QObject::connect(favorite_request_, &OpenTidalFavoriteRequest::AlbumsAdded, &*albums_collection_backend_, &CollectionBackend::AddOrUpdateSongs);
  QObject::connect(favorite_request_, &OpenTidalFavoriteRequest::SongsAdded, &*songs_collection_backend_, &CollectionBackend::AddOrUpdateSongs);

  QObject::connect(favorite_request_, &OpenTidalFavoriteRequest::ArtistsRemoved, &*artists_collection_backend_, &CollectionBackend::DeleteSongs);
  QObject::connect(favorite_request_, &OpenTidalFavoriteRequest::AlbumsRemoved, &*albums_collection_backend_, &CollectionBackend::DeleteSongs);
  QObject::connect(favorite_request_, &OpenTidalFavoriteRequest::SongsRemoved, &*songs_collection_backend_, &CollectionBackend::DeleteSongs);

  OpenTidalService::ReloadSettings();
  oauth_->LoadSession();

}

OpenTidalService::~OpenTidalService() {

  while (!stream_url_requests_.isEmpty()) {
    OpenTidalStreamURLRequestPtr stream_url_request = stream_url_requests_.take(stream_url_requests_.firstKey());
    QObject::disconnect(&*stream_url_request, nullptr, this, nullptr);
  }

}

bool OpenTidalService::authenticated() const {

  return oauth_->authenticated();

}

QByteArray OpenTidalService::authorization_header() const {

  return oauth_->authorization_header();

}

QString OpenTidalService::country_code() const {

  return oauth_->country_code();

}

quint64 OpenTidalService::user_id() const {

  return oauth_->user_id();

}

void OpenTidalService::Exit() {

  wait_for_exit_ << &*artists_collection_backend_ << &*albums_collection_backend_ << &*songs_collection_backend_;

  QObject::connect(&*artists_collection_backend_, &CollectionBackend::ExitFinished, this, &OpenTidalService::ExitReceived);
  QObject::connect(&*albums_collection_backend_, &CollectionBackend::ExitFinished, this, &OpenTidalService::ExitReceived);
  QObject::connect(&*songs_collection_backend_, &CollectionBackend::ExitFinished, this, &OpenTidalService::ExitReceived);

  artists_collection_backend_->ExitAsync();
  albums_collection_backend_->ExitAsync();
  songs_collection_backend_->ExitAsync();

}

void OpenTidalService::ExitReceived() {

  QObject *obj = sender();
  QObject::disconnect(obj, nullptr, this, nullptr);
  qLog(Debug) << obj << "successfully exited.";
  wait_for_exit_.removeAll(obj);
  if (wait_for_exit_.isEmpty()) Q_EMIT ExitFinished();

}

void OpenTidalService::ReloadSettings() {

  Settings s;
  s.beginGroup(OpenTidalSettings::kSettingsGroup);
  enabled_ = s.value(OpenTidalSettings::kEnabled, false).toBool();
  //client_id_ = s.value(OpenTidalSettings::kClientId).toString();
  manifest_type_ = static_cast<OpenTidalSettings::ManifestType>(s.value(OpenTidalSettings::kManifestType, static_cast<int>(OpenTidalSettings::ManifestType::MPEG_DASH)).toInt());
  uri_scheme_ = static_cast<OpenTidalSettings::UriScheme>(s.value(OpenTidalSettings::kUriScheme, static_cast<int>(OpenTidalSettings::UriScheme::DATA)).toInt());
  format_ = s.value(OpenTidalSettings::kFormat, u"LOSSLESS"_s).toString();
  const quint64 search_delay = s.value(OpenTidalSettings::kSearchDelay, 1500).toULongLong();
  artistssearchlimit_ = s.value(OpenTidalSettings::kArtistsSearchLimit, 4).toInt();
  albumssearchlimit_ = s.value(OpenTidalSettings::kAlbumsSearchLimit, 10).toInt();
  songssearchlimit_ = s.value(OpenTidalSettings::kSongsSearchLimit, 10).toInt();
  fetchalbums_ = s.value(OpenTidalSettings::kFetchAlbums, false).toBool();
  coversize_ = s.value(OpenTidalSettings::kCoverSize, u"640x640"_s).toString();
  download_album_covers_ = s.value(OpenTidalSettings::kDownloadAlbumCovers, true).toBool();
  album_explicit_ = s.value(OpenTidalSettings::kAlbumExplicit, false).toBool();
  remove_remastered_ = s.value(OpenTidalSettings::kRemoveRemastered, true).toBool();
  s.endGroup();

  //if (!client_id_.isEmpty()) {
    //oauth_->set_client_id(client_id_);
  //}

  timer_search_delay_->setInterval(static_cast<int>(search_delay));

}

void OpenTidalService::StartAuthorization(const QString &client_id) {

  Q_UNUSED(client_id)

  //oauth_->set_client_id(client_id);
  oauth_->Authenticate();

}

void OpenTidalService::OAuthFinished(const bool success, const QString &error) {

  if (success) {
    qLog(Debug) << "OpenTidal: Login successful" << "user id" << user_id();
    Q_EMIT LoginFinished(true);
    Q_EMIT LoginSuccess();
  }
  else {
    Q_EMIT LoginFailure(error);
    Q_EMIT LoginFinished(false);
  }

}

void OpenTidalService::AuthorizationUrlReceived(const QUrl &url) {

  qLog(Debug) << "OpenTidal: Authorization URL Received" << url;

  oauth_->ExternalAuthorizationUrlReceived(url);

}

void OpenTidalService::ClearSession() {

  oauth_->ClearSession();

}

void OpenTidalService::GetArtists() {

  if (!authenticated()) {
    Q_EMIT ArtistsResults(SongMap(), tr("Not authenticated with Open Tidal."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  artists_request_.reset(new OpenTidalRequest(this, url_handler_, network_, OpenTidalBaseRequest::Type::FavouriteArtists, this));
  QObject::connect(&*artists_request_, &OpenTidalRequest::Results, this, &OpenTidalService::ArtistsResultsReceived);
  QObject::connect(&*artists_request_, &OpenTidalRequest::UpdateStatus, this, &OpenTidalService::ArtistsUpdateStatusReceived);
  QObject::connect(&*artists_request_, &OpenTidalRequest::UpdateProgress, this, &OpenTidalService::ArtistsUpdateProgressReceived);

  artists_request_->Process();

}


void OpenTidalService::ResetArtistsRequest() {

  artists_request_.reset();

}

void OpenTidalService::ArtistsResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_UNUSED(id);
  Q_EMIT ArtistsResults(songs, error);
  ResetArtistsRequest();

}

void OpenTidalService::ArtistsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  Q_EMIT ArtistsUpdateStatus(text);
}

void OpenTidalService::ArtistsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  Q_EMIT ArtistsUpdateProgress(progress);
}

void OpenTidalService::GetAlbums() {

  if (!authenticated()) {
    Q_EMIT AlbumsResults(SongMap(), tr("Not authenticated with Open Tidal."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  albums_request_.reset(new OpenTidalRequest(this, url_handler_, network_, OpenTidalBaseRequest::Type::FavouriteAlbums, this));
  QObject::connect(&*albums_request_, &OpenTidalRequest::Results, this, &OpenTidalService::AlbumsResultsReceived);
  QObject::connect(&*albums_request_, &OpenTidalRequest::UpdateStatus, this, &OpenTidalService::AlbumsUpdateStatusReceived);
  QObject::connect(&*albums_request_, &OpenTidalRequest::UpdateProgress, this, &OpenTidalService::AlbumsUpdateProgressReceived);

  albums_request_->Process();

}

void OpenTidalService::ResetAlbumsRequest() {

  albums_request_.reset();

}

void OpenTidalService::AlbumsResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_UNUSED(id);
  Q_EMIT AlbumsResults(songs, error);
  ResetAlbumsRequest();

}

void OpenTidalService::AlbumsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  Q_EMIT AlbumsUpdateStatus(text);
}

void OpenTidalService::AlbumsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  Q_EMIT AlbumsUpdateProgress(progress);
}

void OpenTidalService::GetSongs() {

  if (!authenticated()) {
    Q_EMIT SongsResults(SongMap(), tr("Not authenticated with Open Tidal."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  songs_request_.reset(new OpenTidalRequest(this, url_handler_, network_, OpenTidalBaseRequest::Type::FavouriteSongs, this));
  QObject::connect(&*songs_request_, &OpenTidalRequest::Results, this, &OpenTidalService::SongsResultsReceived);
  QObject::connect(&*songs_request_, &OpenTidalRequest::UpdateStatus, this, &OpenTidalService::SongsUpdateStatusReceived);
  QObject::connect(&*songs_request_, &OpenTidalRequest::UpdateProgress, this, &OpenTidalService::SongsUpdateProgressReceived);

  songs_request_->Process();

}

void OpenTidalService::ResetSongsRequest() {

  songs_request_.reset();

}

void OpenTidalService::SongsResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_UNUSED(id);
  Q_EMIT SongsResults(songs, error);
  ResetSongsRequest();

}

void OpenTidalService::SongsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  Q_EMIT SongsUpdateStatus(text);
}

void OpenTidalService::SongsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  Q_EMIT SongsUpdateProgress(progress);
}

int OpenTidalService::Search(const QString &text, const SearchType type) {

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

void OpenTidalService::StartSearch() {

  if (!authenticated()) {
    Q_EMIT SearchResults(pending_search_id_, SongMap(), tr("Not authenticated with Open Tidal."));
    Q_EMIT OpenSettingsDialog(kSource);
    return;
  }

  search_id_ = pending_search_id_;
  search_text_ = pending_search_text_;

  SendSearch();

}

void OpenTidalService::CancelSearch() {}

void OpenTidalService::SendSearch() {

  OpenTidalBaseRequest::Type query_type = OpenTidalBaseRequest::Type::None;

  switch (pending_search_type_) {
    case SearchType::Artists:
      query_type = OpenTidalBaseRequest::Type::SearchArtists;
      break;
    case SearchType::Albums:
      query_type = OpenTidalBaseRequest::Type::SearchAlbums;
      break;
    case SearchType::Songs:
      query_type = OpenTidalBaseRequest::Type::SearchSongs;
      break;
    default:
      // Error("Invalid search type.");
      return;
  }

  search_request_.reset(new OpenTidalRequest(this, url_handler_, network_, query_type, this));
  QObject::connect(&*search_request_, &OpenTidalRequest::Results, this, &OpenTidalService::SearchResultsReceived);
  QObject::connect(&*search_request_, &OpenTidalRequest::UpdateStatus, this, &OpenTidalService::SearchUpdateStatus);
  QObject::connect(&*search_request_, &OpenTidalRequest::UpdateProgress, this, &OpenTidalService::SearchUpdateProgress);

  search_request_->Search(search_id_, search_text_);
  search_request_->Process();

}

void OpenTidalService::SearchResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_EMIT SearchResults(id, songs, error);
  search_request_.reset();

}

uint OpenTidalService::GetStreamURL(const QUrl &url, QString &error) {

  if (!authenticated()) {
    error = tr("Not authenticated with Open Tidal.");
    return 0;
  }

  uint id = 0;
  while (id == 0) id = ++next_stream_url_request_id_;
  OpenTidalStreamURLRequestPtr stream_url_request = OpenTidalStreamURLRequestPtr(new OpenTidalStreamURLRequest(this, network_, url, id), &QObject::deleteLater);
  stream_url_requests_.insert(id, stream_url_request);
  QObject::connect(&*stream_url_request, &OpenTidalStreamURLRequest::StreamURLFailure, this, &OpenTidalService::HandleStreamURLFailure);
  QObject::connect(&*stream_url_request, &OpenTidalStreamURLRequest::StreamURLSuccess, this, &OpenTidalService::HandleStreamURLSuccess);
  stream_url_request->Process();

  return id;

}

void OpenTidalService::HandleStreamURLFailure(const uint id, const QUrl &media_url, const QString &error) {

  if (!stream_url_requests_.contains(id)) return;
  stream_url_requests_.remove(id);

  Q_EMIT StreamURLFailure(id, media_url, error);

}

void OpenTidalService::HandleStreamURLSuccess(const uint id, const QUrl &media_url, const QUrl &stream_url, const Song::FileType filetype, const int samplerate, const int bit_depth, const qint64 duration) {

  if (!stream_url_requests_.contains(id)) return;
  stream_url_requests_.remove(id);

  Q_EMIT StreamURLSuccess(id, media_url, stream_url, filetype, samplerate, bit_depth, duration);

}
