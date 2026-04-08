/*
 * Strawberry Music Player
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QPair>
#include <QList>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QJsonValue>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSslError>
#include <QScopedPointer>
#include <QDesktopServices>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/localredirectserver.h"
#include "core/database.h"
#include "core/song.h"
#include "core/settings.h"
#include "core/urlhandlers.h"
#include "streaming/streamingsearchview.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "qobuzservice.h"
#include "qobuzurlhandler.h"
#include "qobuzbaserequest.h"
#include "qobuzrequest.h"
#include "qobuzfavoriterequest.h"
#include "qobuzstreamurlrequest.h"
#include "constants/qobuzsettings.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

const Song::Source QobuzService::kSource = Song::Source::Qobuz;
const char QobuzService::kApiUrl[] = "https://www.qobuz.com/api.json/0.2";

namespace {

constexpr char kOAuthSigninUrl[] = "https://www.qobuz.com/signin/oauth";
constexpr char kOAuthCallbackUrl[] = "https://www.qobuz.com/api.json/0.2/oauth/callback";

constexpr char kArtistsSongsTable[] = "qobuz_artists_songs";
constexpr char kAlbumsSongsTable[] = "qobuz_albums_songs";
constexpr char kSongsTable[] = "qobuz_songs";

}  // namespace

QobuzService::QobuzService(const SharedPtr<TaskManager> task_manager,
                           const SharedPtr<Database> database,
                           const SharedPtr<NetworkAccessManager> network,
                           const SharedPtr<UrlHandlers> url_handlers,
                           const SharedPtr<AlbumCoverLoader> albumcover_loader,
                           QObject *parent)
    : StreamingService(Song::Source::Qobuz, u"Qobuz"_s, u"qobuz"_s, QLatin1String(QobuzSettings::kSettingsGroup), parent),
      network_(network),
      url_handler_(new QobuzUrlHandler(task_manager, this)),
      artists_collection_backend_(nullptr),
      albums_collection_backend_(nullptr),
      songs_collection_backend_(nullptr),
      artists_collection_model_(nullptr),
      albums_collection_model_(nullptr),
      songs_collection_model_(nullptr),
      timer_search_delay_(new QTimer(this)),
      favorite_request_(new QobuzFavoriteRequest(this, network_, this)),
      local_redirect_server_(nullptr),
      format_(0),
      search_delay_(1500),
      artistssearchlimit_(1),
      albumssearchlimit_(1),
      songssearchlimit_(1),
      download_album_covers_(true),
      remove_remastered_(true),
      user_id_(-1),
      pending_search_id_(0),
      next_pending_search_id_(1),
      pending_search_type_(SearchType::Artists),
      search_id_(0),
      next_stream_url_request_id_(0) {

  url_handlers->Register(url_handler_);

  // Backends

  artists_collection_backend_ = make_shared<CollectionBackend>();
  artists_collection_backend_->moveToThread(database->thread());
  artists_collection_backend_->Init(database, task_manager, Song::Source::Qobuz, QLatin1String(kArtistsSongsTable));

  albums_collection_backend_ = make_shared<CollectionBackend>();
  albums_collection_backend_->moveToThread(database->thread());
  albums_collection_backend_->Init(database, task_manager, Song::Source::Qobuz, QLatin1String(kAlbumsSongsTable));

  songs_collection_backend_ = make_shared<CollectionBackend>();
  songs_collection_backend_->moveToThread(database->thread());
  songs_collection_backend_->Init(database, task_manager, Song::Source::Qobuz, QLatin1String(kSongsTable));

  // Models
  artists_collection_model_ = new CollectionModel(artists_collection_backend_, albumcover_loader, this);
  albums_collection_model_ = new CollectionModel(albums_collection_backend_, albumcover_loader, this);
  songs_collection_model_ = new CollectionModel(songs_collection_backend_, albumcover_loader, this);

  // Search

  timer_search_delay_->setSingleShot(true);
  QObject::connect(timer_search_delay_, &QTimer::timeout, this, &QobuzService::StartSearch);

  QObject::connect(this, &QobuzService::AddArtists, favorite_request_, &QobuzFavoriteRequest::AddArtists);
  QObject::connect(this, &QobuzService::AddAlbums, favorite_request_, &QobuzFavoriteRequest::AddAlbums);
  QObject::connect(this, &QobuzService::AddSongs, favorite_request_, QOverload<const SongList&>::of(&QobuzFavoriteRequest::AddSongs));

  QObject::connect(this, &QobuzService::RemoveArtists, favorite_request_, &QobuzFavoriteRequest::RemoveArtists);
  QObject::connect(this, &QobuzService::RemoveAlbums, favorite_request_, &QobuzFavoriteRequest::RemoveAlbums);
  QObject::connect(this, &QobuzService::RemoveSongsByList, favorite_request_, QOverload<const SongList&>::of(&QobuzFavoriteRequest::RemoveSongs));
  QObject::connect(this, &QobuzService::RemoveSongsByMap, favorite_request_, QOverload<const SongMap&>::of(&QobuzFavoriteRequest::RemoveSongs));

  QObject::connect(favorite_request_, &QobuzFavoriteRequest::ArtistsAdded, &*artists_collection_backend_, &CollectionBackend::AddOrUpdateSongs);
  QObject::connect(favorite_request_, &QobuzFavoriteRequest::AlbumsAdded, &*albums_collection_backend_, &CollectionBackend::AddOrUpdateSongs);
  QObject::connect(favorite_request_, &QobuzFavoriteRequest::SongsAdded, &*songs_collection_backend_, &CollectionBackend::AddOrUpdateSongs);

  QObject::connect(favorite_request_, &QobuzFavoriteRequest::ArtistsRemoved, &*artists_collection_backend_, &CollectionBackend::DeleteSongs);
  QObject::connect(favorite_request_, &QobuzFavoriteRequest::AlbumsRemoved, &*albums_collection_backend_, &CollectionBackend::DeleteSongs);
  QObject::connect(favorite_request_, &QobuzFavoriteRequest::SongsRemoved, &*songs_collection_backend_, &CollectionBackend::DeleteSongs);

  QobuzService::ReloadSettings();

}

QobuzService::~QobuzService() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

  while (!stream_url_requests_.isEmpty()) {
    QSharedPointer<QobuzStreamURLRequest> stream_url_req = stream_url_requests_.take(stream_url_requests_.firstKey());
    QObject::disconnect(&*stream_url_req, nullptr, this, nullptr);
  }

  artists_collection_backend_.reset();
  albums_collection_backend_.reset();
  songs_collection_backend_.reset();

}

void QobuzService::Exit() {

  wait_for_exit_ << &*artists_collection_backend_ << &*albums_collection_backend_ << &*songs_collection_backend_;

  QObject::connect(&*artists_collection_backend_, &CollectionBackend::ExitFinished, this, &QobuzService::ExitReceived);
  QObject::connect(&*albums_collection_backend_, &CollectionBackend::ExitFinished, this, &QobuzService::ExitReceived);
  QObject::connect(&*songs_collection_backend_, &CollectionBackend::ExitFinished, this, &QobuzService::ExitReceived);

  artists_collection_backend_->ExitAsync();
  albums_collection_backend_->ExitAsync();
  songs_collection_backend_->ExitAsync();

}

void QobuzService::ExitReceived() {

  QObject *obj = sender();
  QObject::disconnect(obj, nullptr, this, nullptr);
  qLog(Debug) << obj << "successfully exited.";
  wait_for_exit_.removeAll(obj);
  if (wait_for_exit_.isEmpty()) Q_EMIT ExitFinished();

}

void QobuzService::ReloadSettings() {

  Settings s;
  s.beginGroup(QobuzSettings::kSettingsGroup);

  app_id_ = s.value(QobuzSettings::kAppId).toString();
  app_secret_ = s.value(QobuzSettings::kAppSecret).toString();
  private_key_ = s.value(QobuzSettings::kPrivateKey).toString();

  format_ = s.value(QobuzSettings::kFormat, 27).toInt();
  search_delay_ = s.value(QobuzSettings::kSearchDelay, 1500).toInt();
  artistssearchlimit_ = s.value(QobuzSettings::kArtistsSearchLimit, 4).toInt();
  albumssearchlimit_ = s.value(QobuzSettings::kAlbumsSearchLimit, 10).toInt();
  songssearchlimit_ = s.value(QobuzSettings::kSongsSearchLimit, 10).toInt();
  download_album_covers_ = s.value(QobuzSettings::kDownloadAlbumCovers, true).toBool();
  remove_remastered_ = s.value(QobuzSettings::kRemoveRemastered, true).toBool();

  user_id_ = s.value(QobuzSettings::kUserId).toLongLong();
  user_auth_token_ = s.value(QobuzSettings::kUserAuthToken).toString();

  if (s.contains(QobuzSettings::kUsername)) {
    s.remove(QobuzSettings::kUsername);
  }
  if (s.contains(QobuzSettings::kPassword)) {
    s.remove(QobuzSettings::kPassword);
  }

  s.endGroup();

}

void QobuzService::Authenticate(const QString &app_id, const QString &app_secret, const QString &private_key) {

  app_id_ = app_id;
  app_secret_ = app_secret;
  private_key_ = private_key;

  Authenticate();

}

void QobuzService::Authenticate() {

  if (app_id_.isEmpty()) {
    LoginError(tr("Missing app ID. Please fetch credentials first."));
    return;
  }
  if (app_secret_.isEmpty()) {
    LoginError(tr("Missing app secret. Please fetch credentials first."));
    return;
  }
  if (private_key_.isEmpty()) {
    LoginError(tr("Missing private key. Please fetch credentials first."));
    return;
  }

  if (local_redirect_server_) {
    local_redirect_server_->close();
    local_redirect_server_->deleteLater();
    local_redirect_server_ = nullptr;
  }

  local_redirect_server_ = new LocalRedirectServer(this);
  if (!local_redirect_server_->Listen()) {
    LoginError(tr("Failed to start local server for OAuth redirect: %1").arg(local_redirect_server_->error()));
    local_redirect_server_->deleteLater();
    local_redirect_server_ = nullptr;
    return;
  }
  QObject::connect(local_redirect_server_, &LocalRedirectServer::Finished, this, &QobuzService::OAuthRedirectReceived);

  QUrl url(QString::fromLatin1(kOAuthSigninUrl));
  QUrlQuery url_query;
  url_query.addQueryItem(u"ext_app_id"_s, app_id_);
  url_query.addQueryItem(u"redirect_url"_s, QStringLiteral("http://127.0.0.1:%1").arg(local_redirect_server_->port()));
  url.setQuery(url_query);

  const bool success = QDesktopServices::openUrl(url);
  if (!success) {
    qLog(Error) << "Qobuz: Failed to open URL" << url.toString();
    LoginError(tr("Failed to open the web browser. Please open this URL manually: %1").arg(url.toString()));
    local_redirect_server_->close();
    local_redirect_server_->deleteLater();
    local_redirect_server_ = nullptr;
    return;
  }

  Q_EMIT UpdateStatus(tr("Waiting for browser authentication..."));

}

void QobuzService::OAuthRedirectReceived() {

  if (!local_redirect_server_) return;

  LocalRedirectServer *server = local_redirect_server_;
  local_redirect_server_ = nullptr;

  const QUrl request_url = server->request_url();
  server->close();
  server->deleteLater();

  if (!server->success()) {
    LoginError(tr("OAuth redirect failed: %1").arg(server->error()));
    return;
  }

  const QUrlQuery url_query(request_url);

  QString code;
  if (url_query.hasQueryItem(u"code_autorisation"_s)) {
    code = url_query.queryItemValue(u"code_autorisation"_s);
  }
  else if (url_query.hasQueryItem(u"code"_s)) {
    code = url_query.queryItemValue(u"code"_s);
  }

  if (code.isEmpty()) {
    LoginError(tr("OAuth redirect is missing authorization code."));
    return;
  }
  qLog(Debug) << "Qobuz: Received OAuth code, exchanging for token";

  Q_EMIT UpdateStatus(tr("Exchanging authorization code..."));

  QUrl callback_url(QString::fromLatin1(kOAuthCallbackUrl));
  QUrlQuery callback_query;
  callback_query.addQueryItem(u"code"_s, code);
  callback_query.addQueryItem(u"private_key"_s, private_key_);
  callback_url.setQuery(callback_query);

  QNetworkRequest network_request(callback_url);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  network_request.setRawHeader("X-App-Id", app_id_.toUtf8());

  QNetworkReply *reply = network_->get(network_request);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &QobuzService::HandleLoginSSLErrors);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { HandleOAuthCallbackReply(reply); });

}

QJsonObject QobuzService::ParseLoginReply(QNetworkReply *reply, const QString &request_name) {

  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      LoginError(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
      return QJsonObject();
    }
    // See if there is Json data containing "status", "code" and "message" - then use that instead.
    const QByteArray data = reply->readAll();
    QString error_message;
    QJsonParseError json_error;
    const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_error);
    if (json_error.error == QJsonParseError::NoError && !json_document.isEmpty() && json_document.isObject()) {
      const QJsonObject json_object = json_document.object();
      if (!json_object.isEmpty() && json_object.contains("status"_L1) && json_object.contains("code"_L1) && json_object.contains("message"_L1)) {
        const int code = json_object["code"_L1].toInt();
        const QString message = json_object["message"_L1].toString();
        error_message = QStringLiteral("%1 (%2)").arg(message).arg(code);
      }
    }
    if (error_message.isEmpty()) {
      if (reply->error() != QNetworkReply::NoError) {
        error_message = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      }
      else {
        error_message = QStringLiteral("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
      }
    }
    LoginError(error_message);
    return QJsonObject();
  }

  const QByteArray data = reply->readAll();
  QJsonParseError json_error;
  const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_error);
  if (json_error.error != QJsonParseError::NoError) {
    LoginError(QStringLiteral("%1 reply from server missing Json data.").arg(request_name));
    return QJsonObject();
  }

  if (json_document.isEmpty() || !json_document.isObject()) {
    LoginError(QStringLiteral("%1 reply from server has invalid Json.").arg(request_name));
    return QJsonObject();
  }

  const QJsonObject json_object = json_document.object();
  if (json_object.isEmpty()) {
    LoginError(QStringLiteral("%1 reply from server has empty Json object.").arg(request_name), json_document);
    return QJsonObject();
  }

  return json_object;

}

void QobuzService::HandleOAuthCallbackReply(QNetworkReply *reply) {

  const QJsonObject json_object = ParseLoginReply(reply, u"OAuth callback"_s);
  if (json_object.isEmpty()) return;

  // The OAuth callback returns {token, userId} instead of the {user_auth_token, user: {id, device, credential}} format from /user/login.
  if (json_object.contains("token"_L1)) {
    user_auth_token_ = json_object["token"_L1].toString();
  }
  else if (json_object.contains("user_auth_token"_L1)) {
    user_auth_token_ = json_object["user_auth_token"_L1].toString();
  }

  if (user_auth_token_.isEmpty()) {
    LoginError(u"OAuth callback reply is missing token"_s, json_object);
    return;
  }

  if (json_object.contains("userId"_L1)) {
    user_id_ = json_object["userId"_L1].toVariant().toLongLong();
  }

  Settings s;
  s.beginGroup(QobuzSettings::kSettingsGroup);
  s.setValue(QobuzSettings::kUserAuthToken, user_auth_token_);
  s.setValue(QobuzSettings::kUserId, user_id_);
  s.endGroup();

  qLog(Debug) << "Qobuz: OAuth login successful" << "user id" << user_id_;

  Q_EMIT LoginFinished(true);
  Q_EMIT LoginSuccess();

}

void QobuzService::HandleLoginSSLErrors(const QList<QSslError> &ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    qLog(Debug) << "Qobuz" << ssl_error.errorString();
  }

}

void QobuzService::ClearSession() {

  user_auth_token_.clear();
  user_id_ = -1;

  Settings s;
  s.beginGroup(QobuzSettings::kSettingsGroup);
  s.remove(QobuzSettings::kUserId);
  s.remove(QobuzSettings::kCredentialsId);
  s.remove(QobuzSettings::kDeviceId);
  s.remove(QobuzSettings::kUserAuthToken);
  s.endGroup();

}

void QobuzService::ResetArtistsRequest() {

  if (artists_request_) {
    QObject::disconnect(&*artists_request_, nullptr, this, nullptr);
    QObject::disconnect(this, nullptr, &*artists_request_, nullptr);
    artists_request_.reset();
  }

}

void QobuzService::GetArtists() {

  if (app_id().isEmpty()) {
    Q_EMIT ArtistsResults(SongMap(), tr("Missing Qobuz app ID."));
    return;
  }

  if (!authenticated()) {
    Q_EMIT ArtistsResults(SongMap(), tr("Not authenticated with Qobuz."));
    return;
  }

  artists_request_.reset(new QobuzRequest(this, url_handler_, network_, QobuzBaseRequest::Type::FavouriteArtists));
  QObject::connect(&*artists_request_, &QobuzRequest::Results, this, &QobuzService::ArtistsResultsReceived);
  QObject::connect(&*artists_request_, &QobuzRequest::UpdateStatus, this, &QobuzService::ArtistsUpdateStatusReceived);
  QObject::connect(&*artists_request_, &QobuzRequest::UpdateProgress, this, &QobuzService::ArtistsUpdateProgressReceived);

  artists_request_->Process();

}

void QobuzService::ArtistsResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_UNUSED(id);
  Q_EMIT ArtistsResults(songs, error);
  ResetArtistsRequest();

}

void QobuzService::ArtistsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  Q_EMIT ArtistsUpdateStatus(text);
}

void QobuzService::ArtistsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  Q_EMIT ArtistsUpdateProgress(progress);
}

void QobuzService::ResetAlbumsRequest() {

  if (albums_request_) {
    QObject::disconnect(&*albums_request_, nullptr, this, nullptr);
    QObject::disconnect(this, nullptr, &*albums_request_, nullptr);
    albums_request_.reset();
  }

}

void QobuzService::GetAlbums() {

  if (app_id().isEmpty()) {
    Q_EMIT AlbumsResults(SongMap(), tr("Missing Qobuz app ID."));
    return;
  }

  if (!authenticated()) {
    Q_EMIT AlbumsResults(SongMap(), tr("Not authenticated with Qobuz."));
    return;
  }

  albums_request_.reset(new QobuzRequest(this, url_handler_, network_, QobuzBaseRequest::Type::FavouriteAlbums));
  QObject::connect(&*albums_request_, &QobuzRequest::Results, this, &QobuzService::AlbumsResultsReceived);
  QObject::connect(&*albums_request_, &QobuzRequest::UpdateStatus, this, &QobuzService::AlbumsUpdateStatusReceived);
  QObject::connect(&*albums_request_, &QobuzRequest::UpdateProgress, this, &QobuzService::AlbumsUpdateProgressReceived);

  albums_request_->Process();

}

void QobuzService::AlbumsResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_UNUSED(id);
  Q_EMIT AlbumsResults(songs, error);
  ResetAlbumsRequest();

}

void QobuzService::AlbumsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  Q_EMIT AlbumsUpdateStatus(text);
}

void QobuzService::AlbumsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  Q_EMIT AlbumsUpdateProgress(progress);
}

void QobuzService::ResetSongsRequest() {

  if (songs_request_) {
    QObject::disconnect(&*songs_request_, nullptr, this, nullptr);
    QObject::disconnect(this, nullptr, &*songs_request_, nullptr);
    songs_request_.reset();
  }

}

void QobuzService::GetSongs() {

  if (app_id().isEmpty()) {
    Q_EMIT SongsResults(SongMap(), tr("Missing Qobuz app ID."));
    return;
  }

  if (!authenticated()) {
    Q_EMIT SongsResults(SongMap(), tr("Not authenticated with Qobuz."));
    return;
  }

  songs_request_.reset(new QobuzRequest(this, url_handler_, network_, QobuzBaseRequest::Type::FavouriteSongs));
  QObject::connect(&*songs_request_, &QobuzRequest::Results, this, &QobuzService::SongsResultsReceived);
  QObject::connect(&*songs_request_, &QobuzRequest::UpdateStatus, this, &QobuzService::SongsUpdateStatusReceived);
  QObject::connect(&*songs_request_, &QobuzRequest::UpdateProgress, this, &QobuzService::SongsUpdateProgressReceived);

  songs_request_->Process();

}

void QobuzService::SongsResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_UNUSED(id);
  Q_EMIT SongsResults(songs, error);
  ResetSongsRequest();

}

void QobuzService::SongsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  Q_EMIT SongsUpdateStatus(text);
}

void QobuzService::SongsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  Q_EMIT SongsUpdateProgress(progress);
}

int QobuzService::Search(const QString &text, const SearchType type) {

  pending_search_id_ = next_pending_search_id_;
  pending_search_text_ = text;
  pending_search_type_ = type;

  next_pending_search_id_++;

  if (text.isEmpty()) {
    timer_search_delay_->stop();
    return pending_search_id_;
  }
  timer_search_delay_->setInterval(search_delay_);
  timer_search_delay_->start();

  return pending_search_id_;

}

void QobuzService::StartSearch() {

  search_id_ = pending_search_id_;
  search_text_ = pending_search_text_;

  if (app_id_.isEmpty()) {  // App ID is the only thing needed to search.
    Q_EMIT SearchResults(search_id_, SongMap(), tr("Missing Qobuz app ID."));
    return;
  }

  SendSearch();

}

void QobuzService::CancelSearch() {}

void QobuzService::SendSearch() {

  QobuzBaseRequest::Type query_type = QobuzBaseRequest::Type::None;

  switch (pending_search_type_) {
    case SearchType::Artists:
      query_type = QobuzBaseRequest::Type::SearchArtists;
      break;
    case SearchType::Albums:
      query_type = QobuzBaseRequest::Type::SearchAlbums;
      break;
    case SearchType::Songs:
      query_type = QobuzBaseRequest::Type::SearchSongs;
      break;
  }

  search_request_.reset(new QobuzRequest(this, url_handler_, network_, query_type));
  QObject::connect(&*search_request_, &QobuzRequest::Results, this, &QobuzService::SearchResultsReceived);
  QObject::connect(&*search_request_, &QobuzRequest::UpdateStatus, this, &QobuzService::SearchUpdateStatus);
  QObject::connect(&*search_request_, &QobuzRequest::UpdateProgress, this, &QobuzService::SearchUpdateProgress);
  search_request_->Search(search_id_, search_text_);
  search_request_->Process();

}

void QobuzService::SearchResultsReceived(const int id, const SongMap &songs, const QString &error) {

  search_request_.reset();
  Q_EMIT SearchResults(id, songs, error);

}

uint QobuzService::GetStreamURL(const QUrl &url, QString &error) {

  if (app_id().isEmpty() || app_secret().isEmpty()) {
    error = tr("Missing Qobuz app ID or secret.");
    return 0;
  }

  if (!authenticated()) {
    error = tr("Not authenticated. Please login to Qobuz in the settings.");
    return 0;
  }

  uint id = 0;
  while (id == 0) id = ++next_stream_url_request_id_;
  QobuzStreamURLRequestPtr stream_url_request = QobuzStreamURLRequestPtr(new QobuzStreamURLRequest(this, network_, url, id), &QObject::deleteLater);
  stream_url_requests_.insert(id, stream_url_request);

  QObject::connect(&*stream_url_request, &QobuzStreamURLRequest::StreamURLFailure, this, &QobuzService::HandleStreamURLFailure);
  QObject::connect(&*stream_url_request, &QobuzStreamURLRequest::StreamURLSuccess, this, &QobuzService::HandleStreamURLSuccess);

  stream_url_request->Process();

  return id;

}

void QobuzService::HandleStreamURLFailure(const uint id, const QUrl &media_url, const QString &error) {

  if (!stream_url_requests_.contains(id)) return;
  stream_url_requests_.remove(id);

  Q_EMIT StreamURLFailure(id, media_url, error);

}

void QobuzService::HandleStreamURLSuccess(const uint id, const QUrl &media_url, const QUrl &stream_url, const Song::FileType filetype, const int samplerate, const int bit_depth, const qint64 duration) {

  if (!stream_url_requests_.contains(id)) return;
  stream_url_requests_.remove(id);

  Q_EMIT StreamURLSuccess(id, media_url, stream_url, filetype, samplerate, bit_depth, duration);

}

void QobuzService::LoginError(const QString &error, const QVariant &debug) {

  qLog(Error) << "Qobuz:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

  Q_EMIT LoginFailure(error);
  Q_EMIT LoginFinished(false, error);

}
