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
#include <QSslError>
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

using std::shared_ptr;

const Song::Source TidalService::kSource = Song::Source_Tidal;
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
      download_album_covers_(true),
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
  artists_collection_backend_->Init(app_->database(), Song::Source_Tidal, kArtistsSongsTable, QString(), QString(), kArtistsSongsFtsTable);

  albums_collection_backend_ = new CollectionBackend();
  albums_collection_backend_->moveToThread(app_->database()->thread());
  albums_collection_backend_->Init(app_->database(), Song::Source_Tidal, kAlbumsSongsTable, QString(), QString(), kAlbumsSongsFtsTable);

  songs_collection_backend_ = new CollectionBackend();
  songs_collection_backend_->moveToThread(app_->database()->thread());
  songs_collection_backend_->Init(app_->database(), Song::Source_Tidal, kSongsTable, QString(), QString(), kSongsFtsTable);

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
    disconnect(stream_url_req, 0, this, 0);
    stream_url_req->deleteLater();
  }

  artists_collection_backend_->deleteLater();
  albums_collection_backend_->deleteLater();
  songs_collection_backend_->deleteLater();

}

void TidalService::Exit() {

  wait_for_exit_ << artists_collection_backend_ << albums_collection_backend_ << songs_collection_backend_;

  connect(artists_collection_backend_, SIGNAL(ExitFinished()), this, SLOT(ExitReceived()));
  connect(albums_collection_backend_, SIGNAL(ExitFinished()), this, SLOT(ExitReceived()));
  connect(songs_collection_backend_, SIGNAL(ExitFinished()), this, SLOT(ExitReceived()));

  artists_collection_backend_->ExitAsync();
  albums_collection_backend_->ExitAsync();
  songs_collection_backend_->ExitAsync();

}

void TidalService::ExitReceived() {

  QObject *obj = static_cast<QObject*>(sender());
  disconnect(obj, 0, this, 0);
  qLog(Debug) << obj << "successfully exited.";
  wait_for_exit_.removeAll(obj);
  if (wait_for_exit_.isEmpty()) emit ExitFinished();

}

void TidalService::ShowConfig() {
  app_->OpenSettingsDialogAtPage(SettingsDialog::Page_Tidal);
}

void TidalService::ReloadSettings() {

  QSettings s;
  s.beginGroup(TidalSettingsPage::kSettingsGroup);

  oauth_ = s.value("oauth", false).toBool();
  client_id_ = s.value("client_id").toString();
  api_token_ = s.value("api_token").toString();
  if (api_token_.isEmpty()) api_token_ = QString::fromUtf8(QByteArray::fromBase64(kApiTokenB64));

  username_ = s.value("username").toString();
  QByteArray password = s.value("password").toByteArray();
  if (password.isEmpty()) password_.clear();
  else password_ = QString::fromUtf8(QByteArray::fromBase64(password));

  quality_ = s.value("quality", "LOSSLESS").toString();
  search_delay_ = s.value("searchdelay", 1500).toInt();
  artistssearchlimit_ = s.value("artistssearchlimit", 4).toInt();
  albumssearchlimit_ = s.value("albumssearchlimit", 10).toInt();
  songssearchlimit_ = s.value("songssearchlimit", 10).toInt();
  fetchalbums_ = s.value("fetchalbums", false).toBool();
  coversize_ = s.value("coversize", "320x320").toString();
  download_album_covers_ = s.value("downloadalbumcovers", true).toBool();
  stream_url_method_ = static_cast<TidalSettingsPage::StreamUrlMethod>(s.value("streamurl").toInt());

  user_id_ = s.value("user_id").toInt();
  country_code_ = s.value("country_code", "US").toString();
  access_token_ = s.value("access_token").toString();
  refresh_token_ = s.value("refresh_token").toString();
  session_id_ = s.value("session_id").toString();
  expiry_time_ = s.value("expiry_time").toDateTime();

  s.endGroup();

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

    login_errors_.clear();
    QNetworkReply *reply = network_->post(request, query);
    connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(HandleLoginSSLErrors(QList<QSslError>)));
    NewClosure(reply, SIGNAL(finished()), this, SLOT(AccessTokenRequestFinished(QNetworkReply*)), reply);

  }

  else {

    LoginError(tr("Reply from Tidal is missing query items."));
    return;
  }

}

void TidalService::HandleLoginSSLErrors(QList<QSslError> ssl_errors) {

  for (QSslError &ssl_error : ssl_errors) {
    login_errors_ += ssl_error.errorString();
  }

}

void TidalService::AccessTokenRequestFinished(QNetworkReply *reply) {

  reply->deleteLater();

  login_sent_ = false;

  if (reply->error() != QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      LoginError(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
      return;
    }
    else {
      // See if there is Json data containing "status" and "userMessage" then use that instead.
      QByteArray data(reply->readAll());
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      if (json_error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("status") && json_obj.contains("userMessage")) {
          int status = json_obj["status"].toInt();
          int sub_status = json_obj["subStatus"].toInt();
          QString user_message = json_obj["userMessage"].toString();
          login_errors_ << QString("Authentication failure: %1 (%2) (%3)").arg(user_message).arg(status).arg(sub_status);
        }
      }
      if (login_errors_.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
          login_errors_ << QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
        else {
          login_errors_ << QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        }
      }
      LoginError();
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
  SendLogin(api_token_, username_, password_);
}

void TidalService::SendLogin(const QString &api_token, const QString &username, const QString &password) {

  login_sent_ = true;
  ++login_attempts_;
  if (timer_login_attempt_->isActive()) timer_login_attempt_->stop();
  timer_login_attempt_->setInterval(kTimeResetLoginAttempts);
  timer_login_attempt_->start();

  const ParamList params = ParamList() << Param("token", (api_token.isEmpty() ? api_token_ : api_token))
                                       << Param("username", username)
                                       << Param("password", password)
                                       << Param("clientVersion", "2.2.1--7");

  QUrlQuery url_query;
  for (const Param &param : params) {
    EncodedParam encoded_param(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
    url_query.addQueryItem(encoded_param.first, encoded_param.second);
  }

  QUrl url(kAuthUrl);
  QNetworkRequest req(url);

  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  req.setRawHeader("X-Tidal-Token", (api_token.isEmpty() ? api_token_.toUtf8() : api_token.toUtf8()));

  QByteArray query = url_query.toString(QUrl::FullyEncoded).toUtf8();
  QNetworkReply *reply = network_->post(req, query);
  connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(HandleLoginSSLErrors(QList<QSslError>)));
  NewClosure(reply, SIGNAL(finished()), this, SLOT(HandleAuthReply(QNetworkReply*)), reply);

  //qLog(Debug) << "Tidal: Sending request" << url << query;

}

void TidalService::HandleAuthReply(QNetworkReply *reply) {

  reply->deleteLater();

  login_sent_ = false;

  if (reply->error() != QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      LoginError(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
      login_errors_.clear();
      return;
    }
    else {
      // See if there is Json data containing "status" and  "userMessage" - then use that instead.
      QByteArray data(reply->readAll());
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      if (json_error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("status") && json_obj.contains("userMessage")) {
          int status = json_obj["status"].toInt();
          int sub_status = json_obj["subStatus"].toInt();
          QString user_message = json_obj["userMessage"].toString();
          login_errors_ << QString("Authentication failure: %1 (%2) (%3)").arg(user_message).arg(status).arg(sub_status);
        }
      }
      if (login_errors_.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
          login_errors_ << QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
        else {
          login_errors_ << QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        }
      }
      LoginError();
      login_errors_.clear();
      return;
    }
  }

  login_errors_.clear();

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
  s.remove("access_token");
  s.remove("refresh_token");
  s.remove("expiry_time");
  s.setValue("user_id", user_id_);
  s.setValue("session_id", session_id_);
  s.setValue("country_code", country_code_);
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
  s.remove("user_id");
  s.remove("country_code");
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

  if (api_token_.isEmpty()) {
    emit LoginComplete(false, tr("Missing Tidal API token."));
    return;
  }
  if (username_.isEmpty()) {
    emit LoginComplete(false, tr("Missing Tidal username."));
    return;
  }
  if (password_.isEmpty()) {
    emit LoginComplete(false, tr("Missing Tidal password."));
    return;
  }
  if (login_attempts_ >= kLoginAttempts) {
    emit LoginComplete(false, tr("Not authenticated with Tidal and reached maximum number of login attempts."));
    return;
  }

  emit Login();

}

void TidalService::ResetArtistsRequest() {

  if (artists_request_.get()) {
    disconnect(artists_request_.get(), 0, this, 0);
    disconnect(this, 0, artists_request_.get(), 0);
    artists_request_.reset();
  }

}

void TidalService::GetArtists() {

  if (!authenticated()) {
    if (oauth_) {
      emit ArtistsResults(SongList(), tr("Not authenticated with Tidal."));
      ShowConfig();
      return;
    }
    else if (api_token_.isEmpty() || username_.isEmpty() || password_.isEmpty()) {
      emit ArtistsResults(SongList(), tr("Missing Tidal API token, username or passord."));
      ShowConfig();
      return;
    }
  }

  ResetArtistsRequest();

  artists_request_.reset(new TidalRequest(this, url_handler_, app_, network_, TidalBaseRequest::QueryType_Artists, this));

  connect(artists_request_.get(), SIGNAL(Results(const int, const SongList&, const QString&)), SLOT(ArtistsResultsReceived(const int, const SongList&, const QString&)));
  connect(artists_request_.get(), SIGNAL(UpdateStatus(const int, const QString&)), SLOT(ArtistsUpdateStatusReceived(const int, const QString&)));
  connect(artists_request_.get(), SIGNAL(ProgressSetMaximum(const int, const int)), SLOT(ArtistsProgressSetMaximumReceived(const int, const int)));
  connect(artists_request_.get(), SIGNAL(UpdateProgress(const int, const int)), SLOT(ArtistsUpdateProgressReceived(const int, const int)));
  connect(this, SIGNAL(LoginComplete(const bool, QString)), artists_request_.get(), SLOT(LoginComplete(const bool, QString)));

  artists_request_->Process();

}

void TidalService::ArtistsResultsReceived(const int id, const SongList &songs, const QString &error) {
  Q_UNUSED(id);
  emit ArtistsResults(songs, error);
}

void TidalService::ArtistsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  emit ArtistsUpdateStatus(text);
}

void TidalService::ArtistsProgressSetMaximumReceived(const int id, const int max) {
  Q_UNUSED(id);
  emit ArtistsProgressSetMaximum(max);
}

void TidalService::ArtistsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  emit ArtistsUpdateProgress(progress);
}

void TidalService::ResetAlbumsRequest() {

  if (albums_request_.get()) {
    disconnect(albums_request_.get(), 0, this, 0);
    disconnect(this, 0, albums_request_.get(), 0);
    albums_request_.reset();
  }

}

void TidalService::GetAlbums() {

  if (!authenticated()) {
    if (oauth_) {
      emit AlbumsResults(SongList(), tr("Not authenticated with Tidal."));
      ShowConfig();
      return;
    }
    else if (api_token_.isEmpty() || username_.isEmpty() || password_.isEmpty()) {
      emit AlbumsResults(SongList(), tr("Missing Tidal API token, username or passord."));
      ShowConfig();
      return;
    }
  }

  ResetAlbumsRequest();
  albums_request_.reset(new TidalRequest(this, url_handler_, app_, network_, TidalBaseRequest::QueryType_Albums, this));
  connect(albums_request_.get(), SIGNAL(Results(const int, const SongList&, const QString&)), SLOT(AlbumsResultsReceived(const int, const SongList&, const QString&)));
  connect(albums_request_.get(), SIGNAL(UpdateStatus(const int, const QString&)), SLOT(AlbumsUpdateStatusReceived(const int, const QString&)));
  connect(albums_request_.get(), SIGNAL(ProgressSetMaximum(const int, const int)), SLOT(AlbumsProgressSetMaximumReceived(const int, const int)));
  connect(albums_request_.get(), SIGNAL(UpdateProgress(const int, const int)), SLOT(AlbumsUpdateProgressReceived(const int, const int)));
  connect(this, SIGNAL(LoginComplete(const bool, const QString&)), albums_request_.get(), SLOT(LoginComplete(const bool, const QString&)));

  albums_request_->Process();

}

void TidalService::AlbumsResultsReceived(const int id, const SongList &songs, const QString &error) {
  Q_UNUSED(id);
  emit AlbumsResults(songs, error);
}

void TidalService::AlbumsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  emit AlbumsUpdateStatus(text);
}

void TidalService::AlbumsProgressSetMaximumReceived(const int id, const int max) {
  Q_UNUSED(id);
  emit AlbumsProgressSetMaximum(max);
}

void TidalService::AlbumsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  emit AlbumsUpdateProgress(progress);
}

void TidalService::ResetSongsRequest() {

  if (songs_request_.get()) {
    disconnect(songs_request_.get(), 0, this, 0);
    disconnect(this, 0, songs_request_.get(), 0);
    songs_request_.reset();
  }

}

void TidalService::GetSongs() {

  if (!authenticated()) {
    if (oauth_) {
      emit SongsResults(SongList(), tr("Not authenticated with Tidal."));
      ShowConfig();
      return;
    }
    else if (api_token_.isEmpty() || username_.isEmpty() || password_.isEmpty()) {
      emit SongsResults(SongList(), tr("Missing Tidal API token, username or passord."));
      ShowConfig();
      return;
    }
  }

  ResetSongsRequest();
  songs_request_.reset(new TidalRequest(this, url_handler_, app_, network_, TidalBaseRequest::QueryType_Songs, this));
  connect(songs_request_.get(), SIGNAL(Results(const int, const SongList&, const QString&)), SLOT(SongsResultsReceived(const int, const SongList&, const QString&)));
  connect(songs_request_.get(), SIGNAL(UpdateStatus(const int, const QString&)), SLOT(SongsUpdateStatusReceived(const int, const QString&)));
  connect(songs_request_.get(), SIGNAL(ProgressSetMaximum(const int, const int)), SLOT(SongsProgressSetMaximumReceived(const int, const int)));
  connect(songs_request_.get(), SIGNAL(UpdateProgress(const int, const int)), SLOT(SongsUpdateProgressReceived(const int, const int)));
  connect(this, SIGNAL(LoginComplete(const bool, const QString&)), songs_request_.get(), SLOT(LoginComplete(const bool, const QString&)));

  songs_request_->Process();

}

void TidalService::SongsResultsReceived(const int id, const SongList &songs, const QString &error) {
  Q_UNUSED(id);
  emit SongsResults(songs, error);
}

void TidalService::SongsUpdateStatusReceived(const int id, const QString &text) {
  Q_UNUSED(id);
  emit SongsUpdateStatus(text);
}

void TidalService::SongsProgressSetMaximumReceived(const int id, const int max) {
  Q_UNUSED(id);
  emit SongsProgressSetMaximum(max);
}

void TidalService::SongsUpdateProgressReceived(const int id, const int progress) {
  Q_UNUSED(id);
  emit SongsUpdateProgress(progress);
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

  if (!authenticated()) {
    if (oauth_) {
      emit SearchResults(pending_search_id_, SongList(), tr("Not authenticated with Tidal."));
      ShowConfig();
      return;
    }
    else if (api_token_.isEmpty() || username_.isEmpty() || password_.isEmpty()) {
      emit SearchResults(pending_search_id_, SongList(), tr("Missing Tidal API token, username or passord."));
      ShowConfig();
      return;
    }
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

  search_request_.reset(new TidalRequest(this, url_handler_, app_, network_, type, this));

  connect(search_request_.get(), SIGNAL(Results(const int, const SongList&, const QString&)), SLOT(SearchResultsReceived(const int, const SongList&, const QString&)));
  connect(search_request_.get(), SIGNAL(UpdateStatus(const int, const QString&)), SIGNAL(SearchUpdateStatus(const int, const QString&)));
  connect(search_request_.get(), SIGNAL(ProgressSetMaximum(const int, const int)), SIGNAL(SearchProgressSetMaximum(const int, const int)));
  connect(search_request_.get(), SIGNAL(UpdateProgress(const int, const int)), SIGNAL(SearchUpdateProgress(const int, const int)));
  connect(this, SIGNAL(LoginComplete(const bool, const QString&)), search_request_.get(), SLOT(LoginComplete(const bool, const QString&)));

  search_request_->Search(search_id_, search_text_);
  search_request_->Process();

}

void TidalService::SearchResultsReceived(const int id, const SongList &songs, const QString &error) {
  emit SearchResults(id, songs, error);
}

void TidalService::GetStreamURL(const QUrl &url) {

  if (!authenticated()) {
    if (oauth_) {
      emit StreamURLFinished(url, url, Song::FileType_Stream, -1, -1, -1, tr("Not authenticated with Tidal."));
      return;
    }
    else if (api_token_.isEmpty() || username_.isEmpty() || password_.isEmpty()) {
      emit StreamURLFinished(url, url, Song::FileType_Stream, -1, -1, -1, tr("Missing Tidal API token, username or passord."));
      return;
    }
  }

  TidalStreamURLRequest *stream_url_req = new TidalStreamURLRequest(this, network_, url, this);
  stream_url_requests_ << stream_url_req;

  connect(stream_url_req, SIGNAL(TryLogin()), this, SLOT(TryLogin()));
  connect(stream_url_req, SIGNAL(StreamURLFinished(const QUrl&, const QUrl&, const Song::FileType, const int, const int, const qint64, QString)), this, SLOT(HandleStreamURLFinished(const QUrl&, const QUrl&, const Song::FileType, const int, const int, const qint64, QString)));
  connect(this, SIGNAL(LoginComplete(const bool, const QString&)), stream_url_req, SLOT(LoginComplete(const bool, QString)));

  stream_url_req->Process();

}

void TidalService::HandleStreamURLFinished(const QUrl &original_url, const QUrl &stream_url, const Song::FileType filetype, const int samplerate, const int bit_depth, const qint64 duration, QString error) {

  TidalStreamURLRequest *stream_url_req = qobject_cast<TidalStreamURLRequest*>(sender());
  if (!stream_url_req || !stream_url_requests_.contains(stream_url_req)) return;
  stream_url_req->deleteLater();
  stream_url_requests_.removeAll(stream_url_req);

  emit StreamURLFinished(original_url, stream_url, filetype, samplerate, bit_depth, duration, error);

}

void TidalService::LoginError(const QString &error, const QVariant &debug) {

  if (!error.isEmpty()) login_errors_ << error;

  QString error_html;
  for (const QString &error : login_errors_) {
    qLog(Error) << "Tidal:" << error;
    error_html += error + "<br />";
  }
  if (debug.isValid()) qLog(Debug) << debug;

  emit LoginFailure(error_html);
  emit LoginComplete(false, error_html);

  login_errors_.clear();

}
