/*
 * Strawberry Music Player
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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
#include <chrono>
#include <memory>

#include <QObject>
#include <QDesktopServices>
#include <QCryptographicHash>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QChar>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslError>
#include <QTimer>
#include <QJsonValue>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSortFilterProxyModel>

#include "core/logging.h"
#include "core/shared_ptr.h"
#include "core/application.h"
#include "core/player.h"
#include "core/networkaccessmanager.h"
#include "core/database.h"
#include "core/song.h"
#include "core/settings.h"
#include "utilities/randutils.h"
#include "utilities/timeconstants.h"
#include "streaming/streamingsearchview.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "collection/collectionfilter.h"
#include "tidalservice.h"
#include "tidalurlhandler.h"
#include "tidalbaserequest.h"
#include "tidalrequest.h"
#include "tidalfavoriterequest.h"
#include "tidalstreamurlrequest.h"
#include "settings/settingsdialog.h"
#include "settings/tidalsettingspage.h"

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;
using std::make_shared;

const Song::Source TidalService::kSource = Song::Source::Tidal;

const char TidalService::kApiUrl[] = "https://api.tidalhifi.com/v1";
const char TidalService::kResourcesUrl[] = "https://resources.tidal.com";
const int TidalService::kLoginAttempts = 2;

namespace {

constexpr char kOAuthUrl[] = "https://login.tidal.com/authorize";
constexpr char kOAuthAccessTokenUrl[] = "https://login.tidal.com/oauth2/token";
constexpr char kOAuthRedirectUrl[] = "tidal://login/auth";
constexpr char kAuthUrl[] = "https://api.tidalhifi.com/v1/login/username";

constexpr int kTimeResetLoginAttempts = 60000;

constexpr char kArtistsSongsTable[] = "tidal_artists_songs";
constexpr char kAlbumsSongsTable[] = "tidal_albums_songs";
constexpr char kSongsTable[] = "tidal_songs";

}  // namespace

TidalService::TidalService(Application *app, QObject *parent)
    : StreamingService(Song::Source::Tidal, QStringLiteral("Tidal"), QStringLiteral("tidal"), QLatin1String(TidalSettingsPage::kSettingsGroup), SettingsDialog::Page::Tidal, app, parent),
      app_(app),
      network_(app->network()),
      url_handler_(new TidalUrlHandler(app, this)),
      artists_collection_backend_(nullptr),
      albums_collection_backend_(nullptr),
      songs_collection_backend_(nullptr),
      artists_collection_model_(nullptr),
      albums_collection_model_(nullptr),
      songs_collection_model_(nullptr),
      timer_search_delay_(new QTimer(this)),
      timer_login_attempt_(new QTimer(this)),
      timer_refresh_login_(new QTimer(this)),
      favorite_request_(new TidalFavoriteRequest(this, network_, this)),
      enabled_(false),
      oauth_(false),
      user_id_(0),
      artistssearchlimit_(1),
      albumssearchlimit_(1),
      songssearchlimit_(1),
      fetchalbums_(true),
      download_album_covers_(true),
      stream_url_method_(TidalSettingsPage::StreamUrlMethod::StreamUrl),
      album_explicit_(false),
      expires_in_(0),
      login_time_(0),
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
  artists_collection_backend_->Init(app_->database(), app->task_manager(), Song::Source::Tidal, QLatin1String(kArtistsSongsTable));

  albums_collection_backend_ = make_shared<CollectionBackend>();
  albums_collection_backend_->moveToThread(app_->database()->thread());
  albums_collection_backend_->Init(app_->database(), app->task_manager(), Song::Source::Tidal, QLatin1String(kAlbumsSongsTable));

  songs_collection_backend_ = make_shared<CollectionBackend>();
  songs_collection_backend_->moveToThread(app_->database()->thread());
  songs_collection_backend_->Init(app_->database(), app->task_manager(), Song::Source::Tidal, QLatin1String(kSongsTable));

  // Models
  artists_collection_model_ = new CollectionModel(artists_collection_backend_, app_, this);
  albums_collection_model_ = new CollectionModel(albums_collection_backend_, app_, this);
  songs_collection_model_ = new CollectionModel(songs_collection_backend_, app_, this);

  // Search

  timer_search_delay_->setSingleShot(true);
  QObject::connect(timer_search_delay_, &QTimer::timeout, this, &TidalService::StartSearch);

  timer_login_attempt_->setSingleShot(true);
  timer_login_attempt_->setInterval(kTimeResetLoginAttempts);
  QObject::connect(timer_login_attempt_, &QTimer::timeout, this, &TidalService::ResetLoginAttempts);

  timer_refresh_login_->setSingleShot(true);
  QObject::connect(timer_refresh_login_, &QTimer::timeout, this, &TidalService::RequestNewAccessToken);

  QObject::connect(this, &TidalService::RequestLogin, this, &TidalService::SendLogin);
  QObject::connect(this, &TidalService::LoginWithCredentials, this, &TidalService::SendLoginWithCredentials);

  QObject::connect(this, &TidalService::AddArtists, favorite_request_, &TidalFavoriteRequest::AddArtists);
  QObject::connect(this, &TidalService::AddAlbums, favorite_request_, &TidalFavoriteRequest::AddAlbums);
  QObject::connect(this, &TidalService::AddSongs, favorite_request_, QOverload<const SongList&>::of(&TidalFavoriteRequest::AddSongs));

  QObject::connect(this, &TidalService::RemoveArtists, favorite_request_, &TidalFavoriteRequest::RemoveArtists);
  QObject::connect(this, &TidalService::RemoveAlbums, favorite_request_, &TidalFavoriteRequest::RemoveAlbums);
  QObject::connect(this, &TidalService::RemoveSongsByList, favorite_request_, QOverload<const SongList&>::of(&TidalFavoriteRequest::RemoveSongs));
  QObject::connect(this, &TidalService::RemoveSongsByMap, favorite_request_, QOverload<const SongMap&>::of(&TidalFavoriteRequest::RemoveSongs));

  QObject::connect(favorite_request_, &TidalFavoriteRequest::RequestLogin, this, &TidalService::SendLogin);

  QObject::connect(favorite_request_, &TidalFavoriteRequest::ArtistsAdded, &*artists_collection_backend_, &CollectionBackend::AddOrUpdateSongs);
  QObject::connect(favorite_request_, &TidalFavoriteRequest::AlbumsAdded, &*albums_collection_backend_, &CollectionBackend::AddOrUpdateSongs);
  QObject::connect(favorite_request_, &TidalFavoriteRequest::SongsAdded, &*songs_collection_backend_, &CollectionBackend::AddOrUpdateSongs);

  QObject::connect(favorite_request_, &TidalFavoriteRequest::ArtistsRemoved, &*artists_collection_backend_, &CollectionBackend::DeleteSongs);
  QObject::connect(favorite_request_, &TidalFavoriteRequest::AlbumsRemoved, &*albums_collection_backend_, &CollectionBackend::DeleteSongs);
  QObject::connect(favorite_request_, &TidalFavoriteRequest::SongsRemoved, &*songs_collection_backend_, &CollectionBackend::DeleteSongs);

  TidalService::ReloadSettings();
  LoadSession();

}

TidalService::~TidalService() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

  while (!stream_url_requests_.isEmpty()) {
   SharedPtr<TidalStreamURLRequest> stream_url_req = stream_url_requests_.take(stream_url_requests_.firstKey());
    QObject::disconnect(&*stream_url_req, nullptr, this, nullptr);
  }

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

void TidalService::ShowConfig() {
  app_->OpenSettingsDialogAtPage(SettingsDialog::Page::Tidal);
}

void TidalService::LoadSession() {

  Settings s;
  s.beginGroup(TidalSettingsPage::kSettingsGroup);
  user_id_ = s.value("user_id").toInt();
  country_code_ = s.value("country_code", QStringLiteral("US")).toString();
  access_token_ = s.value("access_token").toString();
  refresh_token_ = s.value("refresh_token").toString();
  session_id_ = s.value("session_id").toString();
  expires_in_ = s.value("expires_in").toLongLong();
  login_time_ = s.value("login_time").toLongLong();
  s.endGroup();

  if (!refresh_token_.isEmpty()) {
    qint64 time = static_cast<qint64>(expires_in_) - (QDateTime::currentSecsSinceEpoch() - static_cast<qint64>(login_time_));
    if (time <= 0) {
      timer_refresh_login_->setInterval(200ms);
    }
    else {
      timer_refresh_login_->setInterval(static_cast<int>(time * kMsecPerSec));
    }
    timer_refresh_login_->start();
  }

}

void TidalService::ReloadSettings() {

  Settings s;
  s.beginGroup(TidalSettingsPage::kSettingsGroup);

  enabled_ = s.value("enabled", false).toBool();
  oauth_ = s.value("oauth", true).toBool();
  client_id_ = s.value("client_id").toString();
  api_token_ = s.value("api_token").toString();

  username_ = s.value("username").toString();
  QByteArray password = s.value("password").toByteArray();
  if (password.isEmpty()) password_.clear();
  else password_ = QString::fromUtf8(QByteArray::fromBase64(password));

  quality_ = s.value("quality", QStringLiteral("LOSSLESS")).toString();
  quint64 search_delay = s.value("searchdelay", 1500).toInt();
  artistssearchlimit_ = s.value("artistssearchlimit", 4).toInt();
  albumssearchlimit_ = s.value("albumssearchlimit", 10).toInt();
  songssearchlimit_ = s.value("songssearchlimit", 10).toInt();
  fetchalbums_ = s.value("fetchalbums", false).toBool();
  coversize_ = s.value("coversize", QStringLiteral("640x640")).toString();
  download_album_covers_ = s.value("downloadalbumcovers", true).toBool();
  stream_url_method_ = static_cast<TidalSettingsPage::StreamUrlMethod>(s.value("streamurl", static_cast<int>(TidalSettingsPage::StreamUrlMethod::StreamUrl)).toInt());
  album_explicit_ = s.value("album_explicit").toBool();

  s.endGroup();

  timer_search_delay_->setInterval(static_cast<int>(search_delay));

}

void TidalService::StartAuthorization(const QString &client_id) {

  client_id_ = client_id;
  code_verifier_ = Utilities::CryptographicRandomString(44);
  code_challenge_ = QString::fromLatin1(QCryptographicHash::hash(code_verifier_.toUtf8(), QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding));

  if (code_challenge_.lastIndexOf(u'=') == code_challenge_.length() - 1) {
    code_challenge_.chop(1);
  }

  const ParamList params = ParamList() << Param(QStringLiteral("response_type"), QStringLiteral("code"))
                                       << Param(QStringLiteral("code_challenge"), code_challenge_)
                                       << Param(QStringLiteral("code_challenge_method"), QStringLiteral("S256"))
                                       << Param(QStringLiteral("redirect_uri"), QLatin1String(kOAuthRedirectUrl))
                                       << Param(QStringLiteral("client_id"), client_id_)
                                       << Param(QStringLiteral("scope"), QStringLiteral("r_usr w_usr"));

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QUrl url = QUrl(QString::fromLatin1(kOAuthUrl));
  url.setQuery(url_query);
  QDesktopServices::openUrl(url);

}

void TidalService::AuthorizationUrlReceived(const QUrl &url) {

  qLog(Debug) << "Tidal: Authorization URL Received" << url;

  QUrlQuery url_query(url);

  if (url_query.hasQueryItem(QStringLiteral("token_type")) && url_query.hasQueryItem(QStringLiteral("expires_in")) && url_query.hasQueryItem(QStringLiteral("access_token"))) {

    access_token_ = url_query.queryItemValue(QStringLiteral("access_token"));
    if (url_query.hasQueryItem(QStringLiteral("refresh_token"))) {
      refresh_token_ = url_query.queryItemValue(QStringLiteral("refresh_token"));
    }
    expires_in_ = url_query.queryItemValue(QStringLiteral("expires_in")).toInt();
    login_time_ = QDateTime::currentSecsSinceEpoch();
    session_id_.clear();

    Settings s;
    s.beginGroup(TidalSettingsPage::kSettingsGroup);
    s.setValue("access_token", access_token_);
    s.setValue("refresh_token", refresh_token_);
    s.setValue("expires_in", expires_in_);
    s.setValue("login_time", login_time_);
    s.remove("session_id");
    s.endGroup();

    Q_EMIT LoginComplete(true);
    Q_EMIT LoginSuccess();
  }

  else if (url_query.hasQueryItem(QStringLiteral("code")) && url_query.hasQueryItem(QStringLiteral("state"))) {

    QString code = url_query.queryItemValue(QStringLiteral("code"));

    RequestAccessToken(code);

  }

  else {
    LoginError(tr("Reply from Tidal is missing query items."));
    return;
  }

}

void TidalService::RequestAccessToken(const QString &code) {

  timer_refresh_login_->stop();

  ParamList params = ParamList() << Param(QStringLiteral("client_id"), client_id_);

  if (!code.isEmpty()) {
    params << Param(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    params << Param(QStringLiteral("code"), code);
    params << Param(QStringLiteral("code_verifier"), code_verifier_);
    params << Param(QStringLiteral("redirect_uri"), QLatin1String(kOAuthRedirectUrl));
    params << Param(QStringLiteral("scope"), QStringLiteral("r_usr w_usr"));
  }
  else if (!refresh_token_.isEmpty() && enabled_ && oauth_) {
    params << Param(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    params << Param(QStringLiteral("refresh_token"), refresh_token_);
  }
  else {
    return;
  }

  QUrlQuery url_query;
  for (const Param &param : std::as_const(params)) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QUrl url(QString::fromLatin1(kOAuthAccessTokenUrl));
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  const QByteArray query = url_query.toString(QUrl::FullyEncoded).toUtf8();

  login_errors_.clear();
  QNetworkReply *reply = network_->post(req, query);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &TidalService::HandleLoginSSLErrors);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { AccessTokenRequestFinished(reply); });

}

void TidalService::HandleLoginSSLErrors(const QList<QSslError> &ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    login_errors_ += ssl_error.errorString();
  }

}

void TidalService::AccessTokenRequestFinished(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      LoginError(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
      return;
    }
    else {
      // See if there is Json data containing "status" and "userMessage" then use that instead.
      QByteArray data(reply->readAll());
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      if (json_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("status"_L1) && json_obj.contains("userMessage"_L1)) {
          int status = json_obj["status"_L1].toInt();
          int sub_status = json_obj["subStatus"_L1].toInt();
          QString user_message = json_obj["userMessage"_L1].toString();
          login_errors_ << QStringLiteral("Authentication failure: %1 (%2) (%3)").arg(user_message).arg(status).arg(sub_status);
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

  if (!json_obj.contains("access_token"_L1) || !json_obj.contains("expires_in"_L1)) {
    LoginError(QStringLiteral("Authentication reply from server is missing access_token or expires_in"), json_obj);
    return;
  }

  access_token_ = json_obj["access_token"_L1].toString();
  expires_in_ = json_obj["expires_in"_L1].toInt();
  if (json_obj.contains("refresh_token"_L1)) {
    refresh_token_ = json_obj["refresh_token"_L1].toString();
  }
  login_time_ = QDateTime::currentSecsSinceEpoch();

  if (json_obj.contains("user"_L1) && json_obj["user"_L1].isObject()) {
    QJsonObject obj_user = json_obj["user"_L1].toObject();
    if (obj_user.contains("countryCode"_L1) && obj_user.contains("userId"_L1)) {
      country_code_ = obj_user["countryCode"_L1].toString();
      user_id_ = obj_user["userId"_L1].toInt();
    }
  }

  session_id_.clear();

  Settings s;
  s.beginGroup(TidalSettingsPage::kSettingsGroup);
  s.setValue("access_token", access_token_);
  s.setValue("refresh_token", refresh_token_);
  s.setValue("expires_in", expires_in_);
  s.setValue("login_time", login_time_);
  s.setValue("country_code", country_code_);
  s.setValue("user_id", user_id_);
  s.remove("session_id");
  s.endGroup();

  if (expires_in_ > 0) {
    timer_refresh_login_->setInterval(static_cast<int>(expires_in_ * kMsecPerSec));
    timer_refresh_login_->start();
  }

  qLog(Debug) << "Tidal: Login successful" << "user id" << user_id_;

  Q_EMIT LoginComplete(true);
  Q_EMIT LoginSuccess();

}

void TidalService::SendLogin() {
  SendLoginWithCredentials(api_token_, username_, password_);
}

void TidalService::SendLoginWithCredentials(const QString &api_token, const QString &username, const QString &password) {

  login_sent_ = true;
  ++login_attempts_;
  timer_login_attempt_->start();
  timer_refresh_login_->stop();

  const ParamList params = ParamList() << Param(QStringLiteral("token"), (api_token.isEmpty() ? api_token_ : api_token))
                                       << Param(QStringLiteral("username"), username)
                                       << Param(QStringLiteral("password"), password)
                                       << Param(QStringLiteral("clientVersion"), QStringLiteral("2.2.1--7"));

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QUrl url(QString::fromLatin1(kAuthUrl));
  QNetworkRequest req(url);

  req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
  req.setRawHeader("X-Tidal-Token", (api_token.isEmpty() ? api_token_.toUtf8() : api_token.toUtf8()));

  QByteArray query = url_query.toString(QUrl::FullyEncoded).toUtf8();
  QNetworkReply *reply = network_->post(req, query);
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &TidalService::HandleLoginSSLErrors);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { HandleAuthReply(reply); });
  replies_ << reply;

  //qLog(Debug) << "Tidal: Sending request" << url << query;

}

void TidalService::HandleAuthReply(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  login_sent_ = false;

  if (reply->error() != QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      LoginError(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
      login_errors_.clear();
      return;
    }
    else {
      // See if there is Json data containing "status" and  "userMessage" - then use that instead.
      QByteArray data(reply->readAll());
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      if (json_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("status"_L1) && json_obj.contains("userMessage"_L1)) {
          int status = json_obj["status"_L1].toInt();
          int sub_status = json_obj["subStatus"_L1].toInt();
          QString user_message = json_obj["userMessage"_L1].toString();
          login_errors_ << QStringLiteral("Authentication failure: %1 (%2) (%3)").arg(user_message).arg(status).arg(sub_status);
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
      login_errors_.clear();
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

  if (!json_obj.contains("userId"_L1) || !json_obj.contains("sessionId"_L1) || !json_obj.contains("countryCode"_L1)) {
    LoginError(QStringLiteral("Authentication reply from server is missing userId, sessionId or countryCode"), json_obj);
    return;
  }

  country_code_ = json_obj["countryCode"_L1].toString();
  session_id_ = json_obj["sessionId"_L1].toString();
  user_id_ = json_obj["userId"_L1].toInt();
  access_token_.clear();
  refresh_token_.clear();

  Settings s;
  s.beginGroup(TidalSettingsPage::kSettingsGroup);
  s.remove("access_token");
  s.remove("refresh_token");
  s.remove("expires_in");
  s.remove("login_time");
  s.setValue("user_id", user_id_);
  s.setValue("session_id", session_id_);
  s.setValue("country_code", country_code_);
  s.endGroup();

  qLog(Debug) << "Tidal: Login successful" << "user id" << user_id_ << "session id" << session_id_ << "country code" << country_code_;

  login_attempts_ = 0;
  timer_login_attempt_->stop();

  Q_EMIT LoginComplete(true);
  Q_EMIT LoginSuccess();

}

void TidalService::Logout() {

  user_id_ = 0;
  country_code_.clear();
  access_token_.clear();
  session_id_.clear();
  expires_in_ = 0;
  login_time_ = 0;

  Settings s;
  s.beginGroup(TidalSettingsPage::kSettingsGroup);
  s.remove("user_id");
  s.remove("country_code");
  s.remove("access_token");
  s.remove("session_id");
  s.remove("expires_in");
  s.remove("login_time");
  s.endGroup();

  timer_refresh_login_->stop();

}

void TidalService::ResetLoginAttempts() {
  login_attempts_ = 0;
}

void TidalService::TryLogin() {

  if (authenticated() || login_sent_) return;

  if (api_token_.isEmpty()) {
    Q_EMIT LoginComplete(false, tr("Missing Tidal API token."));
    return;
  }
  if (username_.isEmpty()) {
    Q_EMIT LoginComplete(false, tr("Missing Tidal username."));
    return;
  }
  if (password_.isEmpty()) {
    Q_EMIT LoginComplete(false, tr("Missing Tidal password."));
    return;
  }
  if (login_attempts_ >= kLoginAttempts) {
    Q_EMIT LoginComplete(false, tr("Not authenticated with Tidal and reached maximum number of login attempts."));
    return;
  }

  Q_EMIT RequestLogin();

}

void TidalService::ResetArtistsRequest() {

  if (artists_request_) {
    QObject::disconnect(&*artists_request_, nullptr, this, nullptr);
    QObject::disconnect(this, nullptr, &*artists_request_, nullptr);
    artists_request_.reset();
  }

}

void TidalService::GetArtists() {

  if (!authenticated()) {
    if (oauth_) {
      Q_EMIT ArtistsResults(SongMap(), tr("Not authenticated with Tidal."));
      ShowConfig();
      return;
    }
    else if (api_token_.isEmpty() || username_.isEmpty() || password_.isEmpty()) {
      Q_EMIT ArtistsResults(SongMap(), tr("Missing Tidal API token, username or password."));
      ShowConfig();
      return;
    }
  }

  ResetArtistsRequest();
  artists_request_.reset(new TidalRequest(this, url_handler_, app_, network_, TidalBaseRequest::Type::FavouriteArtists, this), [](TidalRequest *request) { request->deleteLater(); });
  QObject::connect(&*artists_request_, &TidalRequest::RequestLogin, this, &TidalService::SendLogin);
  QObject::connect(&*artists_request_, &TidalRequest::Results, this, &TidalService::ArtistsResultsReceived);
  QObject::connect(&*artists_request_, &TidalRequest::UpdateStatus, this, &TidalService::ArtistsUpdateStatusReceived);
  QObject::connect(&*artists_request_, &TidalRequest::UpdateProgress, this, &TidalService::ArtistsUpdateProgressReceived);
  QObject::connect(this, &TidalService::LoginComplete, &*artists_request_, &TidalRequest::LoginComplete);

  artists_request_->Process();

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

void TidalService::ResetAlbumsRequest() {

  if (albums_request_) {
    QObject::disconnect(&*albums_request_, nullptr, this, nullptr);
    QObject::disconnect(this, nullptr, &*albums_request_, nullptr);
    albums_request_.reset();
  }

}

void TidalService::GetAlbums() {

  if (!authenticated()) {
    if (oauth_) {
      Q_EMIT AlbumsResults(SongMap(), tr("Not authenticated with Tidal."));
      ShowConfig();
      return;
    }
    else if (api_token_.isEmpty() || username_.isEmpty() || password_.isEmpty()) {
      Q_EMIT AlbumsResults(SongMap(), tr("Missing Tidal API token, username or password."));
      ShowConfig();
      return;
    }
  }

  ResetAlbumsRequest();
  albums_request_.reset(new TidalRequest(this, url_handler_, app_, network_, TidalBaseRequest::Type::FavouriteAlbums, this), [](TidalRequest *request) { request->deleteLater(); });
  QObject::connect(&*albums_request_, &TidalRequest::RequestLogin, this, &TidalService::SendLogin);
  QObject::connect(&*albums_request_, &TidalRequest::Results, this, &TidalService::AlbumsResultsReceived);
  QObject::connect(&*albums_request_, &TidalRequest::UpdateStatus, this, &TidalService::AlbumsUpdateStatusReceived);
  QObject::connect(&*albums_request_, &TidalRequest::UpdateProgress, this, &TidalService::AlbumsUpdateProgressReceived);
  QObject::connect(this, &TidalService::LoginComplete, &*albums_request_, &TidalRequest::LoginComplete);

  albums_request_->Process();

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

void TidalService::ResetSongsRequest() {

  if (songs_request_) {
    QObject::disconnect(&*songs_request_, nullptr, this, nullptr);
    QObject::disconnect(this, nullptr, &*songs_request_, nullptr);
    songs_request_.reset();
  }

}

void TidalService::GetSongs() {

  if (!authenticated()) {
    if (oauth_) {
      Q_EMIT SongsResults(SongMap(), tr("Not authenticated with Tidal."));
      ShowConfig();
      return;
    }
    else if (api_token_.isEmpty() || username_.isEmpty() || password_.isEmpty()) {
      Q_EMIT SongsResults(SongMap(), tr("Missing Tidal API token, username or password."));
      ShowConfig();
      return;
    }
  }

  ResetSongsRequest();
  songs_request_.reset(new TidalRequest(this, url_handler_, app_, network_, TidalBaseRequest::Type::FavouriteSongs, this), [](TidalRequest *request) { request->deleteLater(); });
  QObject::connect(&*songs_request_, &TidalRequest::RequestLogin, this, &TidalService::SendLogin);
  QObject::connect(&*songs_request_, &TidalRequest::Results, this, &TidalService::SongsResultsReceived);
  QObject::connect(&*songs_request_, &TidalRequest::UpdateStatus, this, &TidalService::SongsUpdateStatusReceived);
  QObject::connect(&*songs_request_, &TidalRequest::UpdateProgress, this, &TidalService::SongsUpdateProgressReceived);
  QObject::connect(this, &TidalService::LoginComplete, &*songs_request_, &TidalRequest::LoginComplete);

  songs_request_->Process();

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

int TidalService::Search(const QString &text, StreamingSearchView::SearchType type) {

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
    if (oauth_) {
      Q_EMIT SearchResults(pending_search_id_, SongMap(), tr("Not authenticated with Tidal."));
      ShowConfig();
      return;
    }
    else if (api_token_.isEmpty() || username_.isEmpty() || password_.isEmpty()) {
      Q_EMIT SearchResults(pending_search_id_, SongMap(), tr("Missing Tidal API token, username or password."));
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

  TidalBaseRequest::Type query_type = TidalBaseRequest::Type::None;

  switch (pending_search_type_) {
    case StreamingSearchView::SearchType::Artists:
      query_type = TidalBaseRequest::Type::SearchArtists;
      break;
    case StreamingSearchView::SearchType::Albums:
      query_type = TidalBaseRequest::Type::SearchAlbums;
      break;
    case StreamingSearchView::SearchType::Songs:
      query_type = TidalBaseRequest::Type::SearchSongs;
      break;
    default:
      //Error("Invalid search type.");
      return;
  }

  search_request_.reset(new TidalRequest(this, url_handler_, app_, network_, query_type, this), [](TidalRequest *request) { request->deleteLater(); });

  QObject::connect(&*search_request_, &TidalRequest::RequestLogin, this, &TidalService::SendLogin);
  QObject::connect(&*search_request_, &TidalRequest::Results, this, &TidalService::SearchResultsReceived);
  QObject::connect(&*search_request_, &TidalRequest::UpdateStatus, this, &TidalService::SearchUpdateStatus);
  QObject::connect(&*search_request_, &TidalRequest::UpdateProgress, this, &TidalService::SearchUpdateProgress);
  QObject::connect(this, &TidalService::LoginComplete, &*search_request_, &TidalRequest::LoginComplete);

  search_request_->Search(search_id_, search_text_);
  search_request_->Process();

}

void TidalService::SearchResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_EMIT SearchResults(id, songs, error);
  search_request_.reset();

}

uint TidalService::GetStreamURL(const QUrl &url, QString &error) {

  if (!authenticated()) {
    if (oauth_) {
      error = tr("Not authenticated with Tidal.");
      return 0;
    }
    else if (api_token_.isEmpty() || username_.isEmpty() || password_.isEmpty()) {
      error = tr("Missing Tidal API token, username or password.");
      return 0;
    }
  }

  uint id = 0;
  while (id == 0) id = ++next_stream_url_request_id_;
 SharedPtr<TidalStreamURLRequest> stream_url_req;
  stream_url_req.reset(new TidalStreamURLRequest(this, network_, url, id), [](TidalStreamURLRequest *request) { request->deleteLater(); });
  stream_url_requests_.insert(id, stream_url_req);

  QObject::connect(&*stream_url_req, &TidalStreamURLRequest::TryLogin, this, &TidalService::TryLogin);
  QObject::connect(&*stream_url_req, &TidalStreamURLRequest::StreamURLFailure, this, &TidalService::HandleStreamURLFailure);
  QObject::connect(&*stream_url_req, &TidalStreamURLRequest::StreamURLSuccess, this, &TidalService::HandleStreamURLSuccess);
  QObject::connect(this, &TidalService::LoginComplete, &*stream_url_req, &TidalStreamURLRequest::LoginComplete);

  stream_url_req->Process();

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

void TidalService::LoginError(const QString &error, const QVariant &debug) {

  if (!error.isEmpty()) login_errors_ << error;

  QString error_html;
  for (const QString &e : std::as_const(login_errors_)) {
    qLog(Error) << "Tidal:" << e;
    error_html += e + "<br />"_L1;
  }
  if (debug.isValid()) qLog(Debug) << debug;

  Q_EMIT LoginFailure(error_html);
  Q_EMIT LoginComplete(false, error_html);

  login_errors_.clear();

}
