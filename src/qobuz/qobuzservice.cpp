/*
 * Strawberry Music Player
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <utility>
#include <memory>

#include <QObject>
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

#include "core/shared_ptr.h"
#include "core/application.h"
#include "core/player.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/database.h"
#include "core/song.h"
#include "core/settings.h"
#include "utilities/macaddrutils.h"
#include "streaming/streamingsearchview.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "collection/collectionfilter.h"
#include "qobuzservice.h"
#include "qobuzurlhandler.h"
#include "qobuzbaserequest.h"
#include "qobuzrequest.h"
#include "qobuzfavoriterequest.h"
#include "qobuzstreamurlrequest.h"
#include "settings/settingsdialog.h"
#include "settings/qobuzsettingspage.h"

using namespace Qt::StringLiterals;
using std::make_shared;

const Song::Source QobuzService::kSource = Song::Source::Qobuz;
const char QobuzService::kApiUrl[] = "https://www.qobuz.com/api.json/0.2";
const int QobuzService::kLoginAttempts = 2;

namespace {

constexpr char kAuthUrl[] = "https://www.qobuz.com/api.json/0.2/user/login";

constexpr int kTimeResetLoginAttempts = 60000;

constexpr char kArtistsSongsTable[] = "qobuz_artists_songs";
constexpr char kAlbumsSongsTable[] = "qobuz_albums_songs";
constexpr char kSongsTable[] = "qobuz_songs";

}  // namespace

QobuzService::QobuzService(Application *app, QObject *parent)
    : StreamingService(Song::Source::Qobuz, QStringLiteral("Qobuz"), QStringLiteral("qobuz"), QLatin1String(QobuzSettingsPage::kSettingsGroup), SettingsDialog::Page::Qobuz, app, parent),
      app_(app),
      network_(app->network()),
      url_handler_(new QobuzUrlHandler(app, this)),
      artists_collection_backend_(nullptr),
      albums_collection_backend_(nullptr),
      songs_collection_backend_(nullptr),
      artists_collection_model_(nullptr),
      albums_collection_model_(nullptr),
      songs_collection_model_(nullptr),
      timer_search_delay_(new QTimer(this)),
      timer_login_attempt_(new QTimer(this)),
      favorite_request_(new QobuzFavoriteRequest(this, network_, this)),
      format_(0),
      search_delay_(1500),
      artistssearchlimit_(1),
      albumssearchlimit_(1),
      songssearchlimit_(1),
      download_album_covers_(true),
      user_id_(-1),
      credential_id_(-1),
      pending_search_id_(0),
      next_pending_search_id_(1),
      pending_search_type_(StreamingSearchView::SearchType::Artists),
      search_id_(0),
      login_sent_(false),
      login_attempts_(0),
      next_stream_url_request_id_(0) {

  app->player()->RegisterUrlHandler(url_handler_);

  // Backends

  artists_collection_backend_ = make_shared<CollectionBackend>();
  artists_collection_backend_->moveToThread(app_->database()->thread());
  artists_collection_backend_->Init(app_->database(), app->task_manager(), Song::Source::Qobuz, QLatin1String(kArtistsSongsTable));

  albums_collection_backend_ = make_shared<CollectionBackend>();
  albums_collection_backend_->moveToThread(app_->database()->thread());
  albums_collection_backend_->Init(app_->database(), app->task_manager(), Song::Source::Qobuz, QLatin1String(kAlbumsSongsTable));

  songs_collection_backend_ = make_shared<CollectionBackend>();
  songs_collection_backend_->moveToThread(app_->database()->thread());
  songs_collection_backend_->Init(app_->database(), app->task_manager(), Song::Source::Qobuz, QLatin1String(kSongsTable));

  // Models
  artists_collection_model_ = new CollectionModel(artists_collection_backend_, app_, this);
  albums_collection_model_ = new CollectionModel(albums_collection_backend_, app_, this);
  songs_collection_model_ = new CollectionModel(songs_collection_backend_, app_, this);

  // Search

  timer_search_delay_->setSingleShot(true);
  QObject::connect(timer_search_delay_, &QTimer::timeout, this, &QobuzService::StartSearch);

  timer_login_attempt_->setSingleShot(true);
  QObject::connect(timer_login_attempt_, &QTimer::timeout, this, &QobuzService::ResetLoginAttempts);

  QObject::connect(this, &QobuzService::RequestLogin, this, &QobuzService::SendLogin);
  QObject::connect(this, &QobuzService::LoginWithCredentials, this, &QobuzService::SendLoginWithCredentials);

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
    SharedPtr<QobuzStreamURLRequest> stream_url_req = stream_url_requests_.take(stream_url_requests_.firstKey());
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

void QobuzService::ShowConfig() {
  app_->OpenSettingsDialogAtPage(SettingsDialog::Page::Qobuz);
}

void QobuzService::ReloadSettings() {

  Settings s;
  s.beginGroup(QobuzSettingsPage::kSettingsGroup);

  app_id_ = s.value("app_id").toString();
  app_secret_ = s.value("app_secret").toString();

  const bool base64_secret = s.value("base64secret", false).toBool();;

  username_ = s.value("username").toString();
  QByteArray password = s.value("password").toByteArray();
  if (password.isEmpty()) password_.clear();
  else password_ = QString::fromUtf8(QByteArray::fromBase64(password));

  format_ = s.value("format", 27).toInt();
  search_delay_ = s.value("searchdelay", 1500).toInt();
  artistssearchlimit_ = s.value("artistssearchlimit", 4).toInt();
  albumssearchlimit_ = s.value("albumssearchlimit", 10).toInt();
  songssearchlimit_ = s.value("songssearchlimit", 10).toInt();
  download_album_covers_ = s.value("downloadalbumcovers", true).toBool();

  user_id_ = s.value("user_id").toInt();
  device_id_ = s.value("device_id").toString();
  user_auth_token_ = s.value("user_auth_token").toString();

  s.endGroup();

  if (base64_secret) {
    app_secret_ = DecodeAppSecret(app_secret_);
  }

}

QString QobuzService::DecodeAppSecret(const QString &app_secret_base64) const {

  const QByteArray appid = app_id().toUtf8();
  const QByteArray app_secret_binary = QByteArray::fromBase64(app_secret_base64.toUtf8());
  QString app_secret_decoded;

  for (int x = 0, y = 0; x < app_secret_binary.length(); ++x , ++y) {
    if (y == appid.length()) y = 0;
    const uint rc = app_secret_binary[x] ^ appid[y];
    if (rc > 0xFFFF) {
      return app_secret_base64;
    }
    app_secret_decoded.append(QChar(rc));
  }

  return app_secret_decoded;

}

void QobuzService::SendLogin() {
  SendLoginWithCredentials(app_id_, username_, password_);
}

void QobuzService::SendLoginWithCredentials(const QString &app_id, const QString &username, const QString &password) {

  Q_EMIT UpdateStatus(tr("Authenticating..."));
  login_errors_.clear();

  login_sent_ = true;
  ++login_attempts_;
  if (timer_login_attempt_->isActive()) timer_login_attempt_->stop();
  timer_login_attempt_->setInterval(kTimeResetLoginAttempts);
  timer_login_attempt_->start();

  const ParamList params = ParamList() << Param(QStringLiteral("app_id"), app_id)
                                       << Param(QStringLiteral("username"), username)
                                       << Param(QStringLiteral("password"), password)
                                       << Param(QStringLiteral("device_manufacturer_id"), Utilities::MacAddress());

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QUrl url(QString::fromLatin1(kAuthUrl));
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

  req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

  QByteArray query = url_query.toString(QUrl::FullyEncoded).toUtf8();
  QNetworkReply *reply = network_->post(req, query);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &QobuzService::HandleLoginSSLErrors);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { HandleAuthReply(reply); });

  //qLog(Debug) << "Qobuz: Sending request" << url << query;

}

void QobuzService::HandleLoginSSLErrors(const QList<QSslError> &ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    login_errors_ += ssl_error.errorString();
  }

}

void QobuzService::HandleAuthReply(QNetworkReply *reply) {

  reply->deleteLater();

  login_sent_ = false;

  if (reply->error() != QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      LoginError(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
      return;
    }
    else {
      // See if there is Json data containing "status", "code" and "message" - then use that instead.
      QByteArray data(reply->readAll());
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      if (json_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("status"_L1) && json_obj.contains("code"_L1) && json_obj.contains("message"_L1)) {
          int code = json_obj["code"_L1].toInt();
          QString message = json_obj["message"_L1].toString();
          login_errors_ << QStringLiteral("%1 (%2)").arg(message).arg(code);
        }
      }
      if (login_errors_.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
          login_errors_ << QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
        else {
          login_errors_ << QStringLiteral("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        }
      }
      LoginError();
      return;
    }
  }

  login_errors_.clear();

  const QByteArray data = reply->readAll();
  QJsonParseError json_error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);

  if (json_error.error != QJsonParseError::NoError) {
    LoginError(QStringLiteral("Authentication reply from server missing Json data."));
    return;
  }

  if (json_doc.isEmpty()) {
    LoginError(QStringLiteral("Authentication reply from server has empty Json document."));
    return;
  }

  if (!json_doc.isObject()) {
    LoginError(QStringLiteral("Authentication reply from server has Json document that is not an object."), json_doc);
    return;
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    LoginError(QStringLiteral("Authentication reply from server has empty Json object."), json_doc);
    return;
  }

  if (!json_obj.contains("user_auth_token"_L1)) {
    LoginError(QStringLiteral("Authentication reply from server is missing user_auth_token"), json_obj);
    return;
  }
  user_auth_token_ = json_obj["user_auth_token"_L1].toString();

  if (!json_obj.contains("user"_L1)) {
    LoginError(QStringLiteral("Authentication reply from server is missing user"), json_obj);
    return;
  }
  QJsonValue value_user = json_obj["user"_L1];
  if (!value_user.isObject()) {
    LoginError(QStringLiteral("Authentication reply user is not a object"), json_obj);
    return;
  }
  QJsonObject obj_user = value_user.toObject();

  if (!obj_user.contains("id"_L1)) {
    LoginError(QStringLiteral("Authentication reply from server is missing user id"), obj_user);
    return;
  }
  user_id_ = obj_user["id"_L1].toInt();

  if (!obj_user.contains("device"_L1)) {
    LoginError(QStringLiteral("Authentication reply from server is missing user device"), obj_user);
    return;
  }
  QJsonValue value_device = obj_user["device"_L1];
  if (!value_device.isObject()) {
    LoginError(QStringLiteral("Authentication reply from server user device is not a object"), value_device);
    return;
  }
  QJsonObject obj_device = value_device.toObject();

  if (!obj_device.contains("device_manufacturer_id"_L1)) {
    LoginError(QStringLiteral("Authentication reply from server device is missing device_manufacturer_id"), obj_device);
    return;
  }
  device_id_ = obj_device["device_manufacturer_id"_L1].toString();

  if (!obj_user.contains("credential"_L1)) {
    LoginError(QStringLiteral("Authentication reply from server is missing user credential"), obj_user);
    return;
  }
  QJsonValue value_credential = obj_user["credential"_L1];
  if (!value_credential.isObject()) {
    LoginError(QStringLiteral("Authentication reply from serve userr credential is not a object"), value_device);
    return;
  }
  QJsonObject obj_credential = value_credential.toObject();

  if (!obj_credential.contains("id"_L1)) {
    LoginError(QStringLiteral("Authentication reply user credential from server is missing user credential id"), obj_credential);
    return;
  }
  credential_id_ = obj_credential["id"_L1].toInt();

  Settings s;
  s.beginGroup(QobuzSettingsPage::kSettingsGroup);
  s.setValue("user_auth_token", user_auth_token_);
  s.setValue("user_id", user_id_);
  s.setValue("credential_id", credential_id_);
  s.setValue("device_id", device_id_);
  s.endGroup();

  qLog(Debug) << "Qobuz: Login successful" << "user id" << user_id_ << "device id" << device_id_;

  login_attempts_ = 0;
  if (timer_login_attempt_->isActive()) timer_login_attempt_->stop();

  Q_EMIT LoginComplete(true);
  Q_EMIT LoginSuccess();

}

void QobuzService::Logout() {

  user_auth_token_.clear();
  device_id_.clear();
  user_id_ = -1;
  credential_id_ = -1;

  Settings s;
  s.beginGroup(QobuzSettingsPage::kSettingsGroup);
  s.remove("user_id");
  s.remove("credential_id");
  s.remove("device_id");
  s.remove("user_auth_token");
  s.endGroup();

}

void QobuzService::ResetLoginAttempts() {
  login_attempts_ = 0;
}

void QobuzService::TryLogin() {

  if (authenticated() || login_sent_) return;

  if (login_attempts_ >= kLoginAttempts) {
    Q_EMIT LoginComplete(false, tr("Maximum number of login attempts reached."));
    return;
  }
  if (app_id_.isEmpty()) {
    Q_EMIT LoginComplete(false, tr("Missing Qobuz app ID."));
    return;
  }
  if (username_.isEmpty()) {
    Q_EMIT LoginComplete(false, tr("Missing Qobuz username."));
    return;
  }
  if (password_.isEmpty()) {
    Q_EMIT LoginComplete(false, tr("Missing Qobuz password."));
    return;
  }

  Q_EMIT RequestLogin();

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

  ResetArtistsRequest();
  artists_request_.reset(new QobuzRequest(this, url_handler_, app_, network_, QobuzBaseRequest::Type::FavouriteArtists), [](QobuzRequest *request) { request->deleteLater(); });
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

  ResetAlbumsRequest();
  albums_request_.reset(new QobuzRequest(this, url_handler_, app_, network_, QobuzBaseRequest::Type::FavouriteAlbums), [](QobuzRequest *request) { request->deleteLater(); });
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

  ResetSongsRequest();
  songs_request_.reset(new QobuzRequest(this, url_handler_, app_, network_, QobuzBaseRequest::Type::FavouriteSongs), [](QobuzRequest *request) { request->deleteLater(); });
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

int QobuzService::Search(const QString &text, StreamingSearchView::SearchType type) {

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

void QobuzService::CancelSearch() {
}

void QobuzService::SendSearch() {

  QobuzBaseRequest::Type query_type = QobuzBaseRequest::Type::None;

  switch (pending_search_type_) {
    case StreamingSearchView::SearchType::Artists:
      query_type = QobuzBaseRequest::Type::SearchArtists;
      break;
    case StreamingSearchView::SearchType::Albums:
      query_type = QobuzBaseRequest::Type::SearchAlbums;
      break;
    case StreamingSearchView::SearchType::Songs:
      query_type = QobuzBaseRequest::Type::SearchSongs;
      break;
  }

  search_request_.reset(new QobuzRequest(this, url_handler_, app_, network_, query_type), [](QobuzRequest *request) { request->deleteLater(); } );

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

  if (app_id().isEmpty() || app_secret().isEmpty()) {  // Don't check for login here, because we allow automatic login.
    error = tr("Missing Qobuz app ID or secret.");
    return 0;
  }

  uint id = 0;
  while (id == 0) id = ++next_stream_url_request_id_;
  SharedPtr<QobuzStreamURLRequest> stream_url_req;
  stream_url_req.reset(new QobuzStreamURLRequest(this, network_, url, id), [](QobuzStreamURLRequest *request) { request->deleteLater(); });
  stream_url_requests_.insert(id, stream_url_req);

  QObject::connect(&*stream_url_req, &QobuzStreamURLRequest::TryLogin, this, &QobuzService::TryLogin);
  QObject::connect(&*stream_url_req, &QobuzStreamURLRequest::StreamURLFailure, this, &QobuzService::HandleStreamURLFailure);
  QObject::connect(&*stream_url_req, &QobuzStreamURLRequest::StreamURLSuccess, this, &QobuzService::HandleStreamURLSuccess);
  QObject::connect(this, &QobuzService::LoginComplete, &*stream_url_req, &QobuzStreamURLRequest::LoginComplete);

  stream_url_req->Process();

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

  if (!error.isEmpty()) login_errors_ << error;

  QString error_html;
  for (const QString &e : std::as_const(login_errors_)) {
    qLog(Error) << "Qobuz:" << e;
    error_html += e + QStringLiteral("<br />");
  }
  if (debug.isValid()) qLog(Debug) << debug;

  Q_EMIT LoginFailure(error_html);
  Q_EMIT LoginComplete(false, error_html);

  login_errors_.clear();

}
