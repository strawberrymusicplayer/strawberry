/*
 * Strawberry Music Player
 * Copyright 2022-2024, Jonas Kvinge <jonas@jkvinge.net>
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
#include <chrono>

#include <QObject>
#include <QDesktopServices>
#include <QCryptographicHash>
#include <QByteArray>
#include <QPair>
#include <QList>
#include <QMap>
#include <QString>
#include <QChar>
#include <QUrl>
#include <QUrlQuery>
#include <QDateTime>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslError>
#include <QTimer>
#include <QJsonValue>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QMessageBox>

#include "core/application.h"
#include "core/player.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/database.h"
#include "core/song.h"
#include "core/settings.h"
#include "core/localredirectserver.h"
#include "utilities/timeconstants.h"
#include "utilities/randutils.h"
#include "streaming/streamingsearchview.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "spotifyservice.h"
#include "spotifybaserequest.h"
#include "spotifyrequest.h"
#include "spotifyfavoriterequest.h"
#include "settings/settingsdialog.h"
#include "settings/spotifysettingspage.h"

using namespace Qt::StringLiterals;

const Song::Source SpotifyService::kSource = Song::Source::Spotify;
const char SpotifyService::kApiUrl[] = "https://api.spotify.com/v1";

namespace {

constexpr char kOAuthAuthorizeUrl[] = "https://accounts.spotify.com/authorize";
constexpr char kOAuthAccessTokenUrl[] = "https://accounts.spotify.com/api/token";
constexpr char kOAuthRedirectUrl[] = "http://localhost:63111/";
constexpr char kClientIDB64[] = "ZTZjY2Y2OTQ5NzY1NGE3NThjOTAxNWViYzdiMWQzMTc=";
constexpr char kClientSecretB64[] = "N2ZlMDMxODk1NTBlNDE3ZGI1ZWQ1MzE3ZGZlZmU2MTE=";

constexpr char kArtistsSongsTable[] = "spotify_artists_songs";
constexpr char kAlbumsSongsTable[] = "spotify_albums_songs";
constexpr char kSongsTable[] = "spotify_songs";

}  // namespace

using std::make_shared;
using namespace std::chrono_literals;

SpotifyService::SpotifyService(Application *app, QObject *parent)
    : StreamingService(Song::Source::Spotify, QStringLiteral("Spotify"), QStringLiteral("spotify"), QLatin1String(SpotifySettingsPage::kSettingsGroup), SettingsDialog::Page::Spotify, app, parent),
      app_(app),
      network_(new NetworkAccessManager(this)),
      artists_collection_backend_(nullptr),
      albums_collection_backend_(nullptr),
      songs_collection_backend_(nullptr),
      artists_collection_model_(nullptr),
      albums_collection_model_(nullptr),
      songs_collection_model_(nullptr),
      timer_search_delay_(new QTimer(this)),
      timer_refresh_login_(new QTimer(this)),
      favorite_request_(new SpotifyFavoriteRequest(this, network_, this)),
      enabled_(false),
      artistssearchlimit_(1),
      albumssearchlimit_(1),
      songssearchlimit_(1),
      fetchalbums_(true),
      download_album_covers_(true),
      expires_in_(0),
      login_time_(0),
      pending_search_id_(0),
      next_pending_search_id_(1),
      pending_search_type_(StreamingSearchView::SearchType::Artists),
      search_id_(0),
      server_(nullptr) {

  // Backends

  artists_collection_backend_ = make_shared<CollectionBackend>();
  artists_collection_backend_->moveToThread(app_->database()->thread());
  artists_collection_backend_->Init(app_->database(), app->task_manager(), Song::Source::Spotify, QLatin1String(kArtistsSongsTable));

  albums_collection_backend_ = make_shared<CollectionBackend>();
  albums_collection_backend_->moveToThread(app_->database()->thread());
  albums_collection_backend_->Init(app_->database(), app->task_manager(), Song::Source::Spotify, QLatin1String(kAlbumsSongsTable));

  songs_collection_backend_ = make_shared<CollectionBackend>();
  songs_collection_backend_->moveToThread(app_->database()->thread());
  songs_collection_backend_->Init(app_->database(), app->task_manager(), Song::Source::Spotify, QLatin1String(kSongsTable));

  // Models
  artists_collection_model_ = new CollectionModel(artists_collection_backend_, app_, this);
  albums_collection_model_ = new CollectionModel(albums_collection_backend_, app_, this);
  songs_collection_model_ = new CollectionModel(songs_collection_backend_, app_, this);

  timer_refresh_login_->setSingleShot(true);
  QObject::connect(timer_refresh_login_, &QTimer::timeout, this, &SpotifyService::RequestNewAccessToken);

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
  LoadSession();

}

SpotifyService::~SpotifyService() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

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

void SpotifyService::ShowConfig() {
  app_->OpenSettingsDialogAtPage(SettingsDialog::Page::Spotify);
}

void SpotifyService::LoadSession() {

  refresh_login_timer_.setSingleShot(true);
  QObject::connect(&refresh_login_timer_, &QTimer::timeout, this, &SpotifyService::RequestNewAccessToken);

  Settings s;
  s.beginGroup(SpotifySettingsPage::kSettingsGroup);
  access_token_ = s.value("access_token").toString();
  refresh_token_ = s.value("refresh_token").toString();
  expires_in_ = s.value("expires_in").toLongLong();
  login_time_ = s.value("login_time").toLongLong();
  s.endGroup();

  if (!refresh_token_.isEmpty()) {
    qint64 time = static_cast<qint64>(expires_in_) - (QDateTime::currentSecsSinceEpoch() - static_cast<qint64>(login_time_));
    if (time < 1) time = 1;
    refresh_login_timer_.setInterval(static_cast<int>(time * kMsecPerSec));
    refresh_login_timer_.start();
  }

}

void SpotifyService::ReloadSettings() {

  Settings s;
  s.beginGroup(SpotifySettingsPage::kSettingsGroup);

  enabled_ = s.value("enabled", false).toBool();

  quint64 search_delay = std::max(s.value("searchdelay", 1500).toInt(), 500);
  artistssearchlimit_ = s.value("artistssearchlimit", 4).toInt();
  albumssearchlimit_ = s.value("albumssearchlimit", 10).toInt();
  songssearchlimit_ = s.value("songssearchlimit", 10).toInt();
  fetchalbums_ = s.value("fetchalbums", false).toBool();
  download_album_covers_ = s.value("downloadalbumcovers", true).toBool();

  s.endGroup();

  timer_search_delay_->setInterval(static_cast<int>(search_delay));

}

void SpotifyService::Authenticate() {

  QUrl redirect_url(QString::fromLatin1(kOAuthRedirectUrl));

  if (!server_) {
    server_ = new LocalRedirectServer(this);
    int port = redirect_url.port();
    int port_max = port + 10;
    bool success = false;
    Q_FOREVER {
      server_->set_port(port);
      if (server_->Listen()) {
        success = true;
        break;
      }
      ++port;
      if (port > port_max) break;
    }
    if (!success) {
      LoginError(server_->error());
      server_->deleteLater();
      server_ = nullptr;
      return;
    }
    QObject::connect(server_, &LocalRedirectServer::Finished, this, &SpotifyService::RedirectArrived);
  }

  code_verifier_ = Utilities::CryptographicRandomString(44);
  code_challenge_ = QString::fromLatin1(QCryptographicHash::hash(code_verifier_.toUtf8(), QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding));
  if (code_challenge_.lastIndexOf(u'=') == code_challenge_.length() - 1) {
    code_challenge_.chop(1);
  }

  const ParamList params = ParamList() << Param(QStringLiteral("client_id"), QString::fromLatin1(QByteArray::fromBase64(kClientIDB64)))
                                       << Param(QStringLiteral("response_type"), QStringLiteral("code"))
                                       << Param(QStringLiteral("redirect_uri"), redirect_url.toString())
                                       << Param(QStringLiteral("state"), code_challenge_)
                                       << Param(QStringLiteral("scope"), QStringLiteral("user-follow-read user-follow-modify user-library-read user-library-modify streaming"));

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QUrl url(QString::fromLatin1(kOAuthAuthorizeUrl));
  url.setQuery(url_query);

  const bool result = QDesktopServices::openUrl(url);
  if (!result) {
    QMessageBox messagebox(QMessageBox::Information, tr("Spotify Authentication"), tr("Please open this URL in your browser") + QStringLiteral(":<br /><a href=\"%1\">%1</a>").arg(url.toString()), QMessageBox::Ok);
    messagebox.setTextFormat(Qt::RichText);
    messagebox.exec();
  }

}

void SpotifyService::Deauthenticate() {

  access_token_.clear();
  refresh_token_.clear();
  expires_in_ = 0;
  login_time_ = 0;

  Settings s;
  s.beginGroup(SpotifySettingsPage::kSettingsGroup);
  s.remove("access_token");
  s.remove("refresh_token");
  s.remove("expires_in");
  s.remove("login_time");
  s.endGroup();

  refresh_login_timer_.stop();

}

void SpotifyService::RedirectArrived() {

  if (!server_) return;

  if (server_->error().isEmpty()) {
    QUrl url = server_->request_url();
    if (url.isValid()) {
      QUrlQuery url_query(url);
      if (url_query.hasQueryItem(QStringLiteral("error"))) {
        LoginError(QUrlQuery(url).queryItemValue(QStringLiteral("error")));
      }
      else if (url_query.hasQueryItem(QStringLiteral("code")) && url_query.hasQueryItem(QStringLiteral("state"))) {
        qLog(Debug) << "Spotify: Authorization URL Received" << url;
        QString code = url_query.queryItemValue(QStringLiteral("code"));
        QUrl redirect_url(QString::fromLatin1(kOAuthRedirectUrl));
        redirect_url.setPort(server_->url().port());
        RequestAccessToken(code, redirect_url);
      }
      else {
        LoginError(tr("Redirect missing token code or state!"));
      }
    }
    else {
      LoginError(tr("Received invalid reply from web browser."));
    }
  }
  else {
    LoginError(server_->error());
  }

  server_->close();
  server_->deleteLater();
  server_ = nullptr;

}

void SpotifyService::RequestAccessToken(const QString &code, const QUrl &redirect_url) {

  refresh_login_timer_.stop();

  ParamList params = ParamList() << Param(QStringLiteral("client_id"), QString::fromLatin1(QByteArray::fromBase64(kClientIDB64)))
                                 << Param(QStringLiteral("client_secret"), QString::fromLatin1(QByteArray::fromBase64(kClientSecretB64)));

  if (!code.isEmpty() && !redirect_url.isEmpty()) {
    params << Param(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    params << Param(QStringLiteral("code"), code);
    params << Param(QStringLiteral("redirect_uri"), redirect_url.toString());
  }
  else if (!refresh_token_.isEmpty() && enabled_) {
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

  QUrl new_url(QString::fromLatin1(kOAuthAccessTokenUrl));
  QNetworkRequest req(new_url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
  QString auth_header_data = QString::fromLatin1(QByteArray::fromBase64(kClientIDB64)) + QLatin1Char(':') + QString::fromLatin1(QByteArray::fromBase64(kClientSecretB64));
  req.setRawHeader("Authorization", "Basic " + auth_header_data.toUtf8().toBase64());

  QByteArray query = url_query.toString(QUrl::FullyEncoded).toUtf8();

  QNetworkReply *reply = network_->post(req, query);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &SpotifyService::HandleLoginSSLErrors);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { AccessTokenRequestFinished(reply); });

}

void SpotifyService::HandleLoginSSLErrors(const QList<QSslError> &ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    login_errors_ += ssl_error.errorString();
  }

}

void SpotifyService::AccessTokenRequestFinished(QNetworkReply *reply) {

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
      // See if there is Json data containing "error" and "error_description" then use that instead.
      QByteArray data = reply->readAll();
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      if (json_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("error"_L1) && json_obj.contains("error_description"_L1)) {
          QString error = json_obj["error"_L1].toString();
          QString error_description = json_obj["error_description"_L1].toString();
          login_errors_ << QStringLiteral("Authentication failure: %1 (%2)").arg(error, error_description);
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

  QByteArray data = reply->readAll();

  QJsonParseError json_error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);

  if (json_error.error != QJsonParseError::NoError) {
    LoginError(QStringLiteral("Failed to parse Json data in authentication reply: %1").arg(json_error.errorString()));
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
    LoginError(QStringLiteral("Authentication reply from server is missing access token or expires in."), json_obj);
    return;
  }

  access_token_ = json_obj["access_token"_L1].toString();
  if (json_obj.contains("refresh_token"_L1)) {
    refresh_token_ = json_obj["refresh_token"_L1].toString();
  }
  expires_in_ = json_obj["expires_in"_L1].toInt();
  login_time_ = QDateTime::currentSecsSinceEpoch();

  Settings s;
  s.beginGroup(SpotifySettingsPage::kSettingsGroup);
  s.setValue("access_token", access_token_);
  s.setValue("refresh_token", refresh_token_);
  s.setValue("expires_in", expires_in_);
  s.setValue("login_time", login_time_);
  s.endGroup();

  if (expires_in_ > 0) {
    refresh_login_timer_.setInterval(static_cast<int>(expires_in_ * kMsecPerSec));
    refresh_login_timer_.start();
  }

  qLog(Debug) << "Spotify: Authentication was successful, login expires in" << expires_in_;

  Q_EMIT LoginComplete(true);
  Q_EMIT LoginSuccess();

}

void SpotifyService::ResetArtistsRequest() {

  if (artists_request_) {
    QObject::disconnect(&*artists_request_, nullptr, this, nullptr);
    QObject::disconnect(this, nullptr, &*artists_request_, nullptr);
    artists_request_.reset();
  }

}

void SpotifyService::GetArtists() {

  if (!authenticated()) {
    Q_EMIT ArtistsResults(SongMap(), tr("Not authenticated with Spotify."));
    ShowConfig();
    return;
  }

  ResetArtistsRequest();
  artists_request_.reset(new SpotifyRequest(this, app_, network_, SpotifyBaseRequest::Type::FavouriteArtists, this), [](SpotifyRequest *request) { request->deleteLater(); });
  QObject::connect(&*artists_request_, &SpotifyRequest::Results, this, &SpotifyService::ArtistsResultsReceived);
  QObject::connect(&*artists_request_, &SpotifyRequest::UpdateStatus, this, &SpotifyService::ArtistsUpdateStatusReceived);
  QObject::connect(&*artists_request_, &SpotifyRequest::ProgressSetMaximum, this, &SpotifyService::ArtistsProgressSetMaximumReceived);
  QObject::connect(&*artists_request_, &SpotifyRequest::UpdateProgress, this, &SpotifyService::ArtistsUpdateProgressReceived);

  artists_request_->Process();

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

void SpotifyService::ResetAlbumsRequest() {

  if (albums_request_) {
    QObject::disconnect(&*albums_request_, nullptr, this, nullptr);
    QObject::disconnect(this, nullptr, &*albums_request_, nullptr);
    albums_request_.reset();
  }

}

void SpotifyService::GetAlbums() {

  if (!authenticated()) {
    Q_EMIT AlbumsResults(SongMap(), tr("Not authenticated with Spotify."));
    ShowConfig();
    return;
  }

  ResetAlbumsRequest();
  albums_request_.reset(new SpotifyRequest(this, app_, network_, SpotifyBaseRequest::Type::FavouriteAlbums, this), [](SpotifyRequest *request) { request->deleteLater(); });
  QObject::connect(&*albums_request_, &SpotifyRequest::Results, this, &SpotifyService::AlbumsResultsReceived);
  QObject::connect(&*albums_request_, &SpotifyRequest::UpdateStatus, this, &SpotifyService::AlbumsUpdateStatusReceived);
  QObject::connect(&*albums_request_, &SpotifyRequest::ProgressSetMaximum, this, &SpotifyService::AlbumsProgressSetMaximumReceived);
  QObject::connect(&*albums_request_, &SpotifyRequest::UpdateProgress, this, &SpotifyService::AlbumsUpdateProgressReceived);

  albums_request_->Process();

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

void SpotifyService::ResetSongsRequest() {

  if (songs_request_) {
    QObject::disconnect(&*songs_request_, nullptr, this, nullptr);
    QObject::disconnect(this, nullptr, &*songs_request_, nullptr);
    songs_request_.reset();
  }

}

void SpotifyService::GetSongs() {

  if (!authenticated()) {
    Q_EMIT SongsResults(SongMap(), tr("Not authenticated with Spotify."));
    ShowConfig();
    return;
  }

  ResetSongsRequest();
  songs_request_.reset(new SpotifyRequest(this, app_, network_, SpotifyBaseRequest::Type::FavouriteSongs, this), [](SpotifyRequest *request) { request->deleteLater(); });
  QObject::connect(&*songs_request_, &SpotifyRequest::Results, this, &SpotifyService::SongsResultsReceived);
  QObject::connect(&*songs_request_, &SpotifyRequest::UpdateStatus, this, &SpotifyService::SongsUpdateStatusReceived);
  QObject::connect(&*songs_request_, &SpotifyRequest::ProgressSetMaximum, this, &SpotifyService::SongsProgressSetMaximumReceived);
  QObject::connect(&*songs_request_, &SpotifyRequest::UpdateProgress, this, &SpotifyService::SongsUpdateProgressReceived);

  songs_request_->Process();

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

int SpotifyService::Search(const QString &text, StreamingSearchView::SearchType type) {

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
    ShowConfig();
    return;
  }

  search_id_ = pending_search_id_;
  search_text_ = pending_search_text_;

  SendSearch();

}

void SpotifyService::CancelSearch() {
}

void SpotifyService::SendSearch() {

  SpotifyBaseRequest::Type type = SpotifyBaseRequest::Type::None;

  switch (pending_search_type_) {
    case StreamingSearchView::SearchType::Artists:
      type = SpotifyBaseRequest::Type::SearchArtists;
      break;
    case StreamingSearchView::SearchType::Albums:
      type = SpotifyBaseRequest::Type::SearchAlbums;
      break;
    case StreamingSearchView::SearchType::Songs:
      type = SpotifyBaseRequest::Type::SearchSongs;
      break;
    default:
      //Error("Invalid search type.");
      return;
  }

  search_request_.reset(new SpotifyRequest(this, app_, network_, type, this), [](SpotifyRequest *request) { request->deleteLater(); });

  QObject::connect(search_request_.get(), &SpotifyRequest::Results, this, &SpotifyService::SearchResultsReceived);
  QObject::connect(search_request_.get(), &SpotifyRequest::UpdateStatus, this, &SpotifyService::SearchUpdateStatus);
  QObject::connect(search_request_.get(), &SpotifyRequest::ProgressSetMaximum, this, &SpotifyService::SearchProgressSetMaximum);
  QObject::connect(search_request_.get(), &SpotifyRequest::UpdateProgress, this, &SpotifyService::SearchUpdateProgress);

  search_request_->Search(search_id_, search_text_);
  search_request_->Process();

}

void SpotifyService::SearchResultsReceived(const int id, const SongMap &songs, const QString &error) {

  Q_EMIT SearchResults(id, songs, error);
  search_request_.reset();

}

void SpotifyService::LoginError(const QString &error, const QVariant &debug) {

  if (!error.isEmpty()) login_errors_ << error;

  QString error_html;
  for (const QString &e : std::as_const(login_errors_)) {
    qLog(Error) << "Spotify:" << e;
    error_html += e + "<br />"_L1;
  }
  if (debug.isValid()) qLog(Debug) << debug;

  Q_EMIT LoginFailure(error_html);
  Q_EMIT LoginComplete(false);

  login_errors_.clear();

}
