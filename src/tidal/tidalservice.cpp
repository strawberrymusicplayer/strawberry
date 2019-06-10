/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#include <stdbool.h>
#include <memory>

#include <QObject>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QCryptographicHash>
#include <QByteArray>
#include <QPair>
#include <QList>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSortFilterProxyModel>

#include "core/application.h"
#include "core/player.h"
#include "core/closure.h"
#include "core/logging.h"
#include "core/network.h"
#include "core/database.h"
#include "core/song.h"
#include "core/utilities.h"
#include "internet/internetsearch.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "tidalservice.h"
#include "tidalurlhandler.h"
#include "tidalrequest.h"
#include "tidalfavoriterequest.h"
#include "tidalstreamurlrequest.h"
#include "settings/tidalsettingspage.h"
#include "internet/localredirectserver.h"

using std::shared_ptr;

const Song::Source TidalService::kSource = Song::Source_Tidal;
const char *TidalService::kClientIdB64 = "dTVxUE5OWUliRDBTMG8zNk1yQWlGWjU2SzZxTUNyQ21ZUHpadVRuVg==";
const char *TidalService::kApiTokenB64 = "UDVYYmVvNUxGdkVTZUR5Ng==";
const char *TidalService::kOAuthUrl = "https://login.tidal.com/authorize";
const char *TidalService::kOAuthAccessTokenUrl = "https://login.tidal.com/oauth2/token";
const char *TidalService::kOAuthRedirectUrl = "tidal://login/auth";
const char *TidalService::kAuthUrl = "https://api.tidalhifi.com/v1/login/username";
const int TidalService::kLoginAttempts = 2;
const int TidalService::kTimeResetLoginAttempts = 60000;

const char *TidalService::kArtistsSongsTable = "tidal_artists_songs";
const char *TidalService::kAlbumsSongsTable = "tidal_albums_songs";
const char *TidalService::kSongsTable = "tidal_songs";

const char *TidalService::kArtistsSongsFtsTable = "tidal_artists_songs_fts";
const char *TidalService::kAlbumsSongsFtsTable = "tidal_albums_songs_fts";
const char *TidalService::kSongsFtsTable = "tidal_songs_fts";

TidalService::TidalService(Application *app, QObject *parent)
    : InternetService(Song::Source_Tidal, "Tidal", "tidal", app, parent),
      app_(app),
      network_(new NetworkAccessManager(this)),
      url_handler_(new TidalUrlHandler(app, this)),
      artists_collection_backend_(nullptr),
      albums_collection_backend_(nullptr),
      songs_collection_backend_(nullptr),
      artists_collection_model_(nullptr),
      albums_collection_model_(nullptr),
      songs_collection_model_(nullptr),
      artists_collection_sort_model_(new QSortFilterProxyModel(this)),
      albums_collection_sort_model_(new QSortFilterProxyModel(this)),
      songs_collection_sort_model_(new QSortFilterProxyModel(this)),
      timer_search_delay_(new QTimer(this)),
      timer_login_attempt_(new QTimer(this)),
      favorite_request_(new TidalFavoriteRequest(this, network_, this)),
      user_id_(0),
      search_delay_(1500),
      artistssearchlimit_(1),
      albumssearchlimit_(1),
      songssearchlimit_(1),
      fetchalbums_(true),
      cache_album_covers_(true),
      pending_search_id_(0),
      next_pending_search_id_(1),
      search_id_(0),
      login_sent_(false),
      login_attempts_(0)
  {

  app->player()->RegisterUrlHandler(url_handler_);

  // Backends

  artists_collection_backend_ = new CollectionBackend();
  artists_collection_backend_->moveToThread(app_->database()->thread());
  artists_collection_backend_->Init(app_->database(), kArtistsSongsTable, QString(), QString(), kArtistsSongsFtsTable);

  albums_collection_backend_ = new CollectionBackend();
  albums_collection_backend_->moveToThread(app_->database()->thread());
  albums_collection_backend_->Init(app_->database(), kAlbumsSongsTable, QString(), QString(), kAlbumsSongsFtsTable);

  songs_collection_backend_ = new CollectionBackend();
  songs_collection_backend_->moveToThread(app_->database()->thread());
  songs_collection_backend_->Init(app_->database(), kSongsTable, QString(), QString(), kSongsFtsTable);

  artists_collection_model_ = new CollectionModel(artists_collection_backend_, app_, this);
  albums_collection_model_ = new CollectionModel(albums_collection_backend_, app_, this);
  songs_collection_model_ = new CollectionModel(songs_collection_backend_, app_, this);

  artists_collection_sort_model_->setSourceModel(artists_collection_model_);
  artists_collection_sort_model_->setSortRole(CollectionModel::Role_SortText);
  artists_collection_sort_model_->setDynamicSortFilter(true);
  artists_collection_sort_model_->setSortLocaleAware(true);
  artists_collection_sort_model_->sort(0);

  albums_collection_sort_model_->setSourceModel(albums_collection_model_);
  albums_collection_sort_model_->setSortRole(CollectionModel::Role_SortText);
  albums_collection_sort_model_->setDynamicSortFilter(true);
  albums_collection_sort_model_->setSortLocaleAware(true);
  albums_collection_sort_model_->sort(0);

  songs_collection_sort_model_->setSourceModel(songs_collection_model_);
  songs_collection_sort_model_->setSortRole(CollectionModel::Role_SortText);
  songs_collection_sort_model_->setDynamicSortFilter(true);
  songs_collection_sort_model_->setSortLocaleAware(true);
  songs_collection_sort_model_->sort(0);

  // Search

  timer_search_delay_->setSingleShot(true);
  connect(timer_search_delay_, SIGNAL(timeout()), SLOT(StartSearch()));

  timer_login_attempt_->setSingleShot(true);
  connect(timer_login_attempt_, SIGNAL(timeout()), SLOT(ResetLoginAttempts()));

  connect(this, SIGNAL(Login()), SLOT(SendLogin()));
  connect(this, SIGNAL(Login(QString, QString, QString)), SLOT(SendLogin(QString, QString, QString)));

  connect(this, SIGNAL(AddArtists(const SongList&)), favorite_request_, SLOT(AddArtists(const SongList&)));
  connect(this, SIGNAL(AddAlbums(const SongList&)), favorite_request_, SLOT(AddAlbums(const SongList&)));
  connect(this, SIGNAL(AddSongs(const SongList&)), favorite_request_, SLOT(AddSongs(const SongList&)));

  connect(this, SIGNAL(RemoveArtists(const SongList&)), favorite_request_, SLOT(RemoveArtists(const SongList&)));
  connect(this, SIGNAL(RemoveAlbums(const SongList&)), favorite_request_, SLOT(RemoveAlbums(const SongList&)));
  connect(this, SIGNAL(RemoveSongs(const SongList&)), favorite_request_, SLOT(RemoveSongs(const SongList&)));

  connect(favorite_request_, SIGNAL(ArtistsAdded(const SongList&)), artists_collection_backend_, SLOT(AddOrUpdateSongs(const SongList&)));
  connect(favorite_request_, SIGNAL(AlbumsAdded(const SongList&)), albums_collection_backend_, SLOT(AddOrUpdateSongs(const SongList&)));
  connect(favorite_request_, SIGNAL(SongsAdded(const SongList&)), songs_collection_backend_, SLOT(AddOrUpdateSongs(const SongList&)));

  connect(favorite_request_, SIGNAL(ArtistsRemoved(const SongList&)), artists_collection_backend_, SLOT(DeleteSongs(const SongList&)));
  connect(favorite_request_, SIGNAL(AlbumsRemoved(const SongList&)), albums_collection_backend_, SLOT(DeleteSongs(const SongList&)));
  connect(favorite_request_, SIGNAL(SongsRemoved(const SongList&)), songs_collection_backend_, SLOT(DeleteSongs(const SongList&)));

  ReloadSettings();

}

TidalService::~TidalService() {

  while (!stream_url_requests_.isEmpty()) {
    TidalStreamURLRequest *stream_url_req = stream_url_requests_.takeFirst();
    disconnect(stream_url_req, 0, nullptr, 0);
    stream_url_req->deleteLater();
  }

}

void TidalService::ShowConfig() {
  app_->OpenSettingsDialogAtPage(SettingsDialog::Page_Tidal);
}

void TidalService::ReloadSettings() {

  QSettings s;
  s.beginGroup(TidalSettingsPage::kSettingsGroup);

  oauth_ = s.value("oauth", false).toBool();
  client_id_ = s.value("client_id").toString();
  if (client_id_.isEmpty()) client_id_ = QString::fromUtf8(QByteArray::fromBase64(kClientIdB64));
  api_token_ = s.value("api_token").toString();
  if (api_token_.isEmpty()) api_token_ = QString::fromUtf8(QByteArray::fromBase64(kApiTokenB64));
  user_id_ = s.value("user_id", 0).toInt();
  country_code_ = s.value("country_code", "US").toString();

  username_ = s.value("username").toString();
  QByteArray password = s.value("password").toByteArray();
  if (password.isEmpty()) password_.clear();
  else password_ = QString::fromUtf8(QByteArray::fromBase64(password));

  quality_ = s.value("quality", "LOSSLESS").toString();
  search_delay_ = s.value("searchdelay", 1500).toInt();
  artistssearchlimit_ = s.value("artistssearchlimit", 5).toInt();
  albumssearchlimit_ = s.value("albumssearchlimit", 100).toInt();
  songssearchlimit_ = s.value("songssearchlimit", 100).toInt();
  fetchalbums_ = s.value("fetchalbums", false).toBool();
  coversize_ = s.value("coversize", "320x320").toString();
  cache_album_covers_ = s.value("cachealbumcovers", true).toBool();
  stream_url_method_ = static_cast<TidalSettingsPage::StreamUrlMethod>(s.value("streamurl").toInt());

  access_token_ = s.value("access_token").toString();
  refresh_token_ = s.value("refresh_token").toString();
  session_id_ = s.value("session_id").toString();
  expiry_time_ = s.value("expiry_time").toDateTime();

  s.endGroup();

}

QString TidalService::CoverCacheDir() {
  return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/tidalalbumcovers";
}

void TidalService::StartAuthorisation() {

  login_sent_ = true;
  ++login_attempts_;
  if (timer_login_attempt_->isActive()) timer_login_attempt_->stop();
  timer_login_attempt_->setInterval(kTimeResetLoginAttempts);
  timer_login_attempt_->start();

  code_verifier_ = Utilities::CryptographicRandomString(44);
  code_challenge_ = QString(QCryptographicHash::hash(code_verifier_.toUtf8(), QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding));

  if (code_challenge_.lastIndexOf(QChar('=')) == code_challenge_.length() - 1) {
    code_challenge_.chop(1);
  }

  const ParamList params = ParamList() << Param("response_type", "code")
                                       << Param("code_challenge", code_challenge_)
                                       << Param("code_challenge_method", "S256")
                                       << Param("redirect_uri", kOAuthRedirectUrl)
                                       << Param("client_id", client_id_)
                                       << Param("scope", "r_usr w_usr");

  QUrlQuery url_query;
  for (const Param &param : params) {
    EncodedParam encoded_param(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
    url_query.addQueryItem(encoded_param.first, encoded_param.second);
  }

  QUrl url = QUrl(kOAuthUrl);
  url.setQuery(url_query);
  QDesktopServices::openUrl(url);

}

void TidalService::AuthorisationUrlReceived(const QUrl &url) {

  qLog(Debug) << "Tidal: Authorisation URL Received" << url;

  QUrlQuery url_query(url);

  if (url_query.hasQueryItem("token_type") && url_query.hasQueryItem("expires_in") && url_query.hasQueryItem("access_token")) {

    access_token_ = url_query.queryItemValue("access_token").toUtf8();
    int expires_in = url_query.queryItemValue("expires_in").toInt();
    expiry_time_ = QDateTime::currentDateTime().addSecs(expires_in - 120);
    session_id_.clear();

    QSettings s;
    s.beginGroup(TidalSettingsPage::kSettingsGroup);
    s.setValue("access_token", access_token_);
    s.setValue("expiry_time", expiry_time_);
    s.remove("refresh_token");
    s.remove("session_id");
    s.endGroup();

    login_attempts_ = 0;
    if (timer_login_attempt_->isActive()) timer_login_attempt_->stop();

    emit LoginComplete(true);
    emit LoginSuccess();
  }

  else if (url_query.hasQueryItem("code") && url_query.hasQueryItem("state")) {

    QString code = url_query.queryItemValue("code");
    QString state = url_query.queryItemValue("state");

    const ParamList params = ParamList() << Param("code", code)
                                         << Param("client_id", client_id_)
                                         << Param("grant_type", "authorization_code")
                                         << Param("redirect_uri", kOAuthRedirectUrl)
                                         << Param("scope", "r_usr w_usr")
                                         << Param("code_verifier", code_verifier_);

    QUrlQuery url_query;
    for (const Param &param : params) {
      EncodedParam encoded_param(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
      url_query.addQueryItem(encoded_param.first, encoded_param.second);
    }

    QUrl url(kOAuthAccessTokenUrl);
    QNetworkRequest request = QNetworkRequest(url);
    QByteArray query = url_query.toString(QUrl::FullyEncoded).toUtf8();
    QNetworkReply *reply = network_->post(request, query);
    NewClosure(reply, SIGNAL(finished()), this, SLOT(AccessTokenRequestFinished(QNetworkReply*)), reply);

  }

  else {

    LoginError(tr("Reply from Tidal is missing query items."));
    return;
  }

}

void TidalService::AccessTokenRequestFinished(QNetworkReply *reply) {

  reply->deleteLater();

  login_sent_ = false;

  if (reply->error() != QNetworkReply::NoError) {
    if (reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      LoginError(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
      return;
    }
    else {
      // See if there is Json data containing "redirectUri" then use that instead.
      QByteArray data(reply->readAll());
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      QString failure_reason;
      if (json_error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("status") && json_obj.contains("userMessage")) {
          int status = json_obj["status"].toInt();
          int sub_status = json_obj["subStatus"].toInt();
          QString user_message = json_obj["userMessage"].toString();
          failure_reason = QString("Authentication failure: %1 (%2) (%3)").arg(user_message).arg(status).arg(sub_status);
        }
      }
      if (failure_reason.isEmpty()) {
        failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      }
      LoginError(failure_reason);
      return;
    }
  }

  QByteArray data(reply->readAll());
  QJsonParseError json_error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);

  if (json_error.error != QJsonParseError::NoError) {
    LoginError("Authentication reply from server missing Json data.");
    return;
  }

  if (json_doc.isNull() || json_doc.isEmpty()) {
    LoginError("Authentication reply from server has empty Json document.");
    return;
  }

  if (!json_doc.isObject()) {
    LoginError("Authentication reply from server has Json document that is not an object.", json_doc);
    return;
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    LoginError("Authentication reply from server has empty Json object.", json_doc);
    return;
  }

  if (!json_obj.contains("access_token") ||
      !json_obj.contains("refresh_token") ||
      !json_obj.contains("expires_in") ||
      !json_obj.contains("user")
  ) {
    LoginError("Authentication reply from server is missing access_token, refresh_token, expires_in or user", json_obj);
    return;
  }

  access_token_ = json_obj["access_token"].toString();
  refresh_token_ = json_obj["refresh_token"].toString();
  int expires_in = json_obj["expires_in"].toInt();
  expiry_time_ = QDateTime::currentDateTime().addSecs(expires_in - 120);

  QJsonValue json_user = json_obj["user"];
  if (!json_user.isObject()) {
    LoginError("Authentication reply from server has Json user that is not an object.", json_doc);
    return;
  }
  QJsonObject json_obj_user = json_user.toObject();
  if (json_obj_user.isEmpty()) {
    LoginError("Authentication reply from server has empty Json user object.", json_doc);
    return;
  }

  country_code_ = json_obj_user["countryCode"].toString();
  user_id_ = json_obj_user["userId"].toInt();
  session_id_.clear();

  QSettings s;
  s.beginGroup(TidalSettingsPage::kSettingsGroup);
  s.setValue("access_token", access_token_);
  s.setValue("refresh_token", refresh_token_);
  s.setValue("expiry_time", expiry_time_);
  s.setValue("country_code", country_code_);
  s.setValue("user_id", user_id_);
  s.remove("session_id");
  s.endGroup();

  qLog(Debug) << "Tidal: Login successful" << "user id" << user_id_ << "access token" << access_token_;

  login_attempts_ = 0;
  if (timer_login_attempt_->isActive()) timer_login_attempt_->stop();

  emit LoginComplete(true);
  emit LoginSuccess();

}

void TidalService::SendLogin() {
  SendLogin(username_, password_, api_token_);
}

void TidalService::SendLogin(const QString &username, const QString &password, const QString &token) {

  emit UpdateStatus(tr("Authenticating..."));

  login_sent_ = true;
  ++login_attempts_;
  if (timer_login_attempt_->isActive()) timer_login_attempt_->stop();
  timer_login_attempt_->setInterval(kTimeResetLoginAttempts);
  timer_login_attempt_->start();

  const ParamList params = ParamList() << Param("token", (token.isEmpty() ? api_token_ : token))
                                       << Param("username", username)
                                       << Param("password", password)
                                       << Param("clientVersion", "2.2.1--7");

  QStringList query_items;
  QUrlQuery url_query;
  for (const Param &param : params) {
    EncodedParam encoded_param(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
    query_items << QString(encoded_param.first + "=" + encoded_param.second);
    url_query.addQueryItem(encoded_param.first, encoded_param.second);
  }

  QUrl url(kAuthUrl);
  QNetworkRequest req(url);

  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  req.setRawHeader("X-Tidal-Token", (token.isEmpty() ? api_token_.toUtf8() : token.toUtf8()));

  QByteArray query = url_query.toString(QUrl::FullyEncoded).toUtf8();
  QNetworkReply *reply = network_->post(req, query);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(HandleAuthReply(QNetworkReply*)), reply);

  //qLog(Debug) << "Tidal: Sending request" << url << query;

}

void TidalService::HandleAuthReply(QNetworkReply *reply) {

  reply->deleteLater();

  login_sent_ = false;

  if (reply->error() != QNetworkReply::NoError) {
    if (reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      LoginError(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
      return;
    }
    else {
      // See if there is Json data containing "status" and  "userMessage" - then use that instead.
      QByteArray data(reply->readAll());
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      QString failure_reason;
      if (json_error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("status") && json_obj.contains("userMessage")) {
          int status = json_obj["status"].toInt();
          int sub_status = json_obj["subStatus"].toInt();
          QString user_message = json_obj["userMessage"].toString();
          failure_reason = QString("Authentication failure: %1 (%2) (%3)").arg(user_message).arg(status).arg(sub_status);
        }
      }
      if (failure_reason.isEmpty()) {
        failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      }
      LoginError(failure_reason);
      return;
    }
  }

  QByteArray data(reply->readAll());
  QJsonParseError json_error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);

  if (json_error.error != QJsonParseError::NoError) {
    LoginError("Authentication reply from server missing Json data.");
    return;
  }

  if (json_doc.isNull() || json_doc.isEmpty()) {
    LoginError("Authentication reply from server has empty Json document.");
    return;
  }

  if (!json_doc.isObject()) {
    LoginError("Authentication reply from server has Json document that is not an object.", json_doc);
    return;
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    LoginError("Authentication reply from server has empty Json object.", json_doc);
    return;
  }

  if (!json_obj.contains("userId") || !json_obj.contains("sessionId") || !json_obj.contains("countryCode") ) {
    LoginError("Authentication reply from server is missing userId, sessionId or countryCode", json_obj);
    return;
  }

  country_code_ = json_obj["countryCode"].toString();
  session_id_ = json_obj["sessionId"].toString();
  user_id_ = json_obj["userId"].toInt();
  access_token_.clear();
  refresh_token_.clear();
  expiry_time_ = QDateTime();

  QSettings s;
  s.beginGroup(TidalSettingsPage::kSettingsGroup);
  s.setValue("user_id", user_id_);
  s.setValue("session_id", session_id_);
  s.setValue("country_code", country_code_);
  s.remove("access_token");
  s.remove("refresh_token");
  s.remove("expiry_time");
  s.endGroup();

  qLog(Debug) << "Tidal: Login successful" << "user id" << user_id_ << "session id" << session_id_ << "country code" << country_code_;

  login_attempts_ = 0;
  if (timer_login_attempt_->isActive()) timer_login_attempt_->stop();

  emit LoginComplete(true);
  emit LoginSuccess();

}

void TidalService::Logout() {

  access_token_.clear();
  session_id_.clear();
  expiry_time_ = QDateTime();

  QSettings s;
  s.beginGroup(TidalSettingsPage::kSettingsGroup);
  s.remove("access_token");
  s.remove("session_id");
  s.remove("expiry_time");
  s.endGroup();

}

void TidalService::ResetLoginAttempts() {
  login_attempts_ = 0;
}

void TidalService::TryLogin() {

  if (authenticated() || login_sent_) return;

  if (login_attempts_ >= kLoginAttempts) {
    emit LoginComplete(false, "Maximum number of login attempts reached.");
    return;
  }
  if (api_token_.isEmpty()) {
    emit LoginComplete(false, "Missing Tidal API token.");
    return;
  }
  if (username_.isEmpty()) {
    emit LoginComplete(false, "Missing Tidal username.");
    return;
  }
  if (password_.isEmpty()) {
    emit LoginComplete(false, "Missing Tidal password.");
    return;
  }

  emit Login();

}

void TidalService::ResetArtistsRequest() {

  if (artists_request_.get()) {
    disconnect(artists_request_.get(), 0, nullptr, 0);
    disconnect(this, 0, artists_request_.get(), 0);
    artists_request_.reset();
  }

}

void TidalService::GetArtists() {

  ResetArtistsRequest();

  artists_request_.reset(new TidalRequest(this, url_handler_, network_, TidalBaseRequest::QueryType_Artists, this));

  connect(artists_request_.get(), SIGNAL(ErrorSignal(QString)), SLOT(ArtistsErrorReceived(QString)));
  connect(artists_request_.get(), SIGNAL(Results(SongList)), SLOT(ArtistsResultsReceived(SongList)));
  connect(artists_request_.get(), SIGNAL(UpdateStatus(QString)), SIGNAL(ArtistsUpdateStatus(QString)));
  connect(artists_request_.get(), SIGNAL(ProgressSetMaximum(int)), SIGNAL(ArtistsProgressSetMaximum(int)));
  connect(artists_request_.get(), SIGNAL(UpdateProgress(int)), SIGNAL(ArtistsUpdateProgress(int)));
  connect(this, SIGNAL(LoginComplete(bool, QString)), artists_request_.get(), SLOT(LoginComplete(bool, QString)));

  artists_request_->Process();

}

void TidalService::ArtistsResultsReceived(SongList songs) {

  emit ArtistsResults(songs);

}

void TidalService::ArtistsErrorReceived(QString error) {

  emit ArtistsError(error);

}

void TidalService::ResetAlbumsRequest() {

  if (albums_request_.get()) {
    disconnect(albums_request_.get(), 0, nullptr, 0);
    disconnect(this, 0, albums_request_.get(), 0);
    albums_request_.reset();
  }

}

void TidalService::GetAlbums() {

  ResetAlbumsRequest();
  albums_request_.reset(new TidalRequest(this, url_handler_, network_, TidalBaseRequest::QueryType_Albums, this));
  connect(albums_request_.get(), SIGNAL(ErrorSignal(QString)), SLOT(AlbumsErrorReceived(QString)));
  connect(albums_request_.get(), SIGNAL(Results(SongList)), SLOT(AlbumsResultsReceived(SongList)));
  connect(albums_request_.get(), SIGNAL(UpdateStatus(QString)), SIGNAL(AlbumsUpdateStatus(QString)));
  connect(albums_request_.get(), SIGNAL(ProgressSetMaximum(int)), SIGNAL(AlbumsProgressSetMaximum(int)));
  connect(albums_request_.get(), SIGNAL(UpdateProgress(int)), SIGNAL(AlbumsUpdateProgress(int)));
  connect(this, SIGNAL(LoginComplete(bool, QString)), albums_request_.get(), SLOT(LoginComplete(bool, QString)));

  albums_request_->Process();

}

void TidalService::AlbumsResultsReceived(SongList songs) {

  emit AlbumsResults(songs);

}

void TidalService::AlbumsErrorReceived(QString error) {

  emit AlbumsError(error);

}

void TidalService::ResetSongsRequest() {

  if (songs_request_.get()) {
    disconnect(songs_request_.get(), 0, nullptr, 0);
    disconnect(this, 0, songs_request_.get(), 0);
    songs_request_.reset();
  }

}

void TidalService::GetSongs() {

  ResetSongsRequest();
  songs_request_.reset(new TidalRequest(this, url_handler_, network_, TidalBaseRequest::QueryType_Songs, this));
  connect(songs_request_.get(), SIGNAL(ErrorSignal(QString)), SLOT(SongsErrorReceived(QString)));
  connect(songs_request_.get(), SIGNAL(Results(SongList)), SLOT(SongsResultsReceived(SongList)));
  connect(songs_request_.get(), SIGNAL(UpdateStatus(QString)), SIGNAL(SongsUpdateStatus(QString)));
  connect(songs_request_.get(), SIGNAL(ProgressSetMaximum(int)), SIGNAL(SongsProgressSetMaximum(int)));
  connect(songs_request_.get(), SIGNAL(UpdateProgress(int)), SIGNAL(SongsUpdateProgress(int)));
  connect(this, SIGNAL(LoginComplete(bool, QString)), songs_request_.get(), SLOT(LoginComplete(bool, QString)));

  songs_request_->Process();

}

void TidalService::SongsResultsReceived(SongList songs) {

  emit SongsResults(songs);

}

void TidalService::SongsErrorReceived(QString error) {

  emit SongsError(error);

}

int TidalService::Search(const QString &text, InternetSearch::SearchType type) {

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

void TidalService::StartSearch() {

  if ((oauth_ && !authenticated()) || api_token_.isEmpty() || username_.isEmpty() || password_.isEmpty()) {
    emit SearchError(pending_search_id_, tr("Not authenticated."));
    next_pending_search_id_ = 1;
    ShowConfig();
    return;
  }

  search_id_ = pending_search_id_;
  search_text_ = pending_search_text_;

  SendSearch();

}

void TidalService::CancelSearch() {
}

void TidalService::SendSearch() {

  TidalBaseRequest::QueryType type;

  switch (pending_search_type_) {
    case InternetSearch::SearchType_Artists:
      type = TidalBaseRequest::QueryType_SearchArtists;
      break;
    case InternetSearch::SearchType_Albums:
      type = TidalBaseRequest::QueryType_SearchAlbums;
      break;
    case InternetSearch::SearchType_Songs:
      type = TidalBaseRequest::QueryType_SearchSongs;
      break;
    default:
      //Error("Invalid search type.");
      return;
  }

  search_request_.reset(new TidalRequest(this, url_handler_, network_, type, this));

  connect(search_request_.get(), SIGNAL(SearchResults(int, SongList)), SIGNAL(SearchResults(int, SongList)));
  connect(search_request_.get(), SIGNAL(ErrorSignal(int, QString)), SIGNAL(SearchError(int, QString)));
  connect(search_request_.get(), SIGNAL(UpdateStatus(QString)), SIGNAL(SearchUpdateStatus(QString)));
  connect(search_request_.get(), SIGNAL(ProgressSetMaximum(int)), SIGNAL(SearchProgressSetMaximum(int)));
  connect(search_request_.get(), SIGNAL(UpdateProgress(int)), SIGNAL(SearchUpdateProgress(int)));
  connect(this, SIGNAL(LoginComplete(bool, QString)), search_request_.get(), SLOT(LoginComplete(bool, QString)));

  search_request_->Search(search_id_, search_text_);
  search_request_->Process();

}

void TidalService::GetStreamURL(const QUrl &url) {

  TidalStreamURLRequest *stream_url_req = new TidalStreamURLRequest(this, network_, url, this);
  stream_url_requests_ << stream_url_req;

  connect(stream_url_req, SIGNAL(TryLogin()), this, SLOT(TryLogin()));
  connect(stream_url_req, SIGNAL(StreamURLFinished(QUrl, QUrl, Song::FileType, QString)), this, SLOT(HandleStreamURLFinished(QUrl, QUrl, Song::FileType, QString)));
  connect(this, SIGNAL(LoginComplete(bool, QString)), stream_url_req, SLOT(LoginComplete(bool, QString)));

  stream_url_req->Process();

}

void TidalService::HandleStreamURLFinished(const QUrl original_url, const QUrl stream_url, const Song::FileType filetype, QString error) {

  TidalStreamURLRequest *stream_url_req = qobject_cast<TidalStreamURLRequest*>(sender());
  if (!stream_url_req || !stream_url_requests_.contains(stream_url_req)) return;
  stream_url_req->deleteLater();
  stream_url_requests_.removeAll(stream_url_req);

  emit StreamURLFinished(original_url, stream_url, filetype, error);

}

QString TidalService::LoginError(QString error, QVariant debug) {

  qLog(Error) << "Tidal:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

  emit LoginFailure(error);
  emit LoginComplete(false, error);

  return error;

}
