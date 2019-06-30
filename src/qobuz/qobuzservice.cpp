/*
 * Strawberry Music Player
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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
#include "qobuzservice.h"
#include "qobuzurlhandler.h"
#include "qobuzrequest.h"
#include "qobuzfavoriterequest.h"
#include "qobuzstreamurlrequest.h"
#include "settings/qobuzsettingspage.h"

using std::shared_ptr;

const Song::Source QobuzService::kSource = Song::Source_Qobuz;
const char *QobuzService::kAuthUrl = "http://www.qobuz.com/api.json/0.2/user/login";
const int QobuzService::kLoginAttempts = 2;
const int QobuzService::kTimeResetLoginAttempts = 60000;

const char *QobuzService::kArtistsSongsTable = "qobuz_artists_songs";
const char *QobuzService::kAlbumsSongsTable = "qobuz_albums_songs";
const char *QobuzService::kSongsTable = "qobuz_songs";

const char *QobuzService::kArtistsSongsFtsTable = "qobuz_artists_songs_fts";
const char *QobuzService::kAlbumsSongsFtsTable = "qobuz_albums_songs_fts";
const char *QobuzService::kSongsFtsTable = "qobuz_songs_fts";

QobuzService::QobuzService(Application *app, QObject *parent)
    : InternetService(Song::Source_Qobuz, "Qobuz", "qobuz", app, parent),
      app_(app),
      network_(new NetworkAccessManager(this)),
      url_handler_(new QobuzUrlHandler(app, this)),
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
      favorite_request_(new QobuzFavoriteRequest(this, network_, this)),
      format_(0),
      search_delay_(1500),
      artistssearchlimit_(1),
      albumssearchlimit_(1),
      songssearchlimit_(1),
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

QobuzService::~QobuzService() {

  while (!stream_url_requests_.isEmpty()) {
    QobuzStreamURLRequest *stream_url_req = stream_url_requests_.takeFirst();
    disconnect(stream_url_req, 0, nullptr, 0);
    stream_url_req->deleteLater();
  }

}

void QobuzService::ShowConfig() {
  app_->OpenSettingsDialogAtPage(SettingsDialog::Page_Qobuz);
}

void QobuzService::ReloadSettings() {

  QSettings s;
  s.beginGroup(QobuzSettingsPage::kSettingsGroup);

  app_id_ = s.value("app_id").toString();
  app_secret_ = s.value("app_secret").toString();

  username_ = s.value("username").toString();
  QByteArray password = s.value("password").toByteArray();
  if (password.isEmpty()) password_.clear();
  else password_ = QString::fromUtf8(QByteArray::fromBase64(password));

  format_ = s.value("format", 27).toInt();
  search_delay_ = s.value("searchdelay", 1500).toInt();
  artistssearchlimit_ = s.value("artistssearchlimit", 4).toInt();
  albumssearchlimit_ = s.value("albumssearchlimit", 10).toInt();
  songssearchlimit_ = s.value("songssearchlimit", 10).toInt();
  download_album_covers_ = s.value("cachealbumcovers", true).toBool();

  user_auth_token_ = s.value("user_auth_token").toString();

  s.endGroup();

}

QString QobuzService::CoverCacheDir() {
  return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/qobuzalbumcovers";
}

void QobuzService::SendLogin() {
  SendLogin(app_id_, username_, password_);
}

void QobuzService::SendLogin(const QString &app_id, const QString &username, const QString &password) {

  emit UpdateStatus(tr("Authenticating..."));

  login_sent_ = true;
  ++login_attempts_;
  if (timer_login_attempt_->isActive()) timer_login_attempt_->stop();
  timer_login_attempt_->setInterval(kTimeResetLoginAttempts);
  timer_login_attempt_->start();

  const ParamList params = ParamList() << Param("app_id", app_id)
                                       << Param("username", username)
                                       << Param("password", password);

  QUrlQuery url_query;
  for (const Param &param : params) {
    EncodedParam encoded_param(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
    url_query.addQueryItem(encoded_param.first, encoded_param.second);
  }

  QUrl url(kAuthUrl);
  QNetworkRequest req(url);

  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

  QByteArray query = url_query.toString(QUrl::FullyEncoded).toUtf8();
  QNetworkReply *reply = network_->post(req, query);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(HandleAuthReply(QNetworkReply*)), reply);

  qLog(Debug) << "Qobuz: Sending request" << url << query;

}

void QobuzService::HandleAuthReply(QNetworkReply *reply) {

  reply->deleteLater();

  login_sent_ = false;

  if (reply->error() != QNetworkReply::NoError) {
    if (reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      LoginError(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
      return;
    }
    else {
      // See if there is Json data containing "status", "code" and "message" - then use that instead.
      QByteArray data(reply->readAll());
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      QString failure_reason;
      if (json_error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("status") && json_obj.contains("code") && json_obj.contains("message")) {
          QString status = json_obj["status"].toString();
          int code = json_obj["code"].toInt();
          QString message = json_obj["message"].toString();
          failure_reason = QString("%1 (%2)").arg(message).arg(code);
        }
      }
      if (failure_reason.isEmpty()) {
        failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      }
      LoginError(failure_reason);
      return;
    }
  }

  int http_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if (http_code != 200) {
    LoginError(QString("Received HTTP code %1").arg(http_code));
    return;
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

  if (!json_obj.contains("user_auth_token")) {
    LoginError("Authentication reply from server is missing user_auth_token", json_obj);
    return;
  }

  user_auth_token_ = json_obj["user_auth_token"].toString();

  QSettings s;
  s.beginGroup(QobuzSettingsPage::kSettingsGroup);
  s.setValue("user_auth_token", user_auth_token_);
  s.endGroup();

  qLog(Debug) << "Qobuz: Login successful" << "user auth token" << user_auth_token_;

  login_attempts_ = 0;
  if (timer_login_attempt_->isActive()) timer_login_attempt_->stop();

  emit LoginComplete(true);
  emit LoginSuccess();

}

void QobuzService::Logout() {

  user_auth_token_.clear();

  QSettings s;
  s.beginGroup(QobuzSettingsPage::kSettingsGroup);
  s.remove("user_auth_token");
  s.endGroup();

}

void QobuzService::ResetLoginAttempts() {
  login_attempts_ = 0;
}

void QobuzService::TryLogin() {

  if (authenticated() || login_sent_) return;

  if (login_attempts_ >= kLoginAttempts) {
    emit LoginComplete(false, tr("Maximum number of login attempts reached."));
    return;
  }
  if (app_id_.isEmpty()) {
    emit LoginComplete(false, tr("Missing Qobuz app ID."));
    return;
  }
  if (username_.isEmpty()) {
    emit LoginComplete(false, tr("Missing Qobuz username."));
    return;
  }
  if (password_.isEmpty()) {
    emit LoginComplete(false, tr("Missing Qobuz password."));
    return;
  }

  emit Login();

}

void QobuzService::ResetArtistsRequest() {

  if (artists_request_.get()) {
    disconnect(artists_request_.get(), 0, nullptr, 0);
    disconnect(this, 0, artists_request_.get(), 0);
    artists_request_.reset();
  }

}

void QobuzService::GetArtists() {

  if (app_id().isEmpty()) {
    emit ArtistsResults(SongList(), tr("Missing Qobuz app ID."));
    return;
  }

  if (!authenticated()) {
    emit ArtistsResults(SongList(), tr("Not authenticated with Qobuz."));
    return;
  }

  ResetArtistsRequest();

  artists_request_.reset(new QobuzRequest(this, url_handler_, network_, QobuzBaseRequest::QueryType_Artists, this));

  connect(artists_request_.get(), SIGNAL(Results(const int, const SongList&, const QString&)), SLOT(ArtistsResultsReceived(const int, const SongList&, const QString&)));
  connect(artists_request_.get(), SIGNAL(UpdateStatus(const int, const QString&)), SLOT(ArtistsUpdateStatusReceived(const int, const QString&)));
  connect(artists_request_.get(), SIGNAL(ProgressSetMaximum(const int, const int)), SLOT(ArtistsProgressSetMaximumReceived(const int, const int)));
  connect(artists_request_.get(), SIGNAL(UpdateProgress(const int, const int)), SLOT(ArtistsUpdateProgressReceived(const int, const int)));

  artists_request_->Process();

}

void QobuzService::ArtistsResultsReceived(const int id, const SongList &songs, const QString &error) {
  emit ArtistsResults(songs, error);
}

void QobuzService::ArtistsUpdateStatusReceived(const int id, const QString &text) {
  emit ArtistsUpdateStatus(text);
}

void QobuzService::ArtistsProgressSetMaximumReceived(const int id, const int max) {
  emit ArtistsProgressSetMaximum(max);
}

void QobuzService::ArtistsUpdateProgressReceived(const int id, const int progress) {
  emit ArtistsUpdateProgress(progress);
}

void QobuzService::ResetAlbumsRequest() {

  if (albums_request_.get()) {
    disconnect(albums_request_.get(), 0, nullptr, 0);
    disconnect(this, 0, albums_request_.get(), 0);
    albums_request_.reset();
  }

}

void QobuzService::GetAlbums() {

  if (app_id().isEmpty()) {
    emit AlbumsResults(SongList(), tr("Missing Qobuz app ID."));
    return;
  }

  if (!authenticated()) {
    emit AlbumsResults(SongList(), tr("Not authenticated with Qobuz."));
    return;
  }

  ResetAlbumsRequest();
  albums_request_.reset(new QobuzRequest(this, url_handler_, network_, QobuzBaseRequest::QueryType_Albums, this));
  connect(albums_request_.get(), SIGNAL(Results(const int, const SongList&, const QString&)), SLOT(AlbumsResultsReceived(const int, const SongList&, const QString&)));
  connect(albums_request_.get(), SIGNAL(UpdateStatus(const int, const QString&)), SLOT(AlbumsUpdateStatusReceived(const int, const QString&)));
  connect(albums_request_.get(), SIGNAL(ProgressSetMaximum(const int, const int)), SLOT(AlbumsProgressSetMaximumReceived(const int, const int)));
  connect(albums_request_.get(), SIGNAL(UpdateProgress(const int, const int)), SLOT(AlbumsUpdateProgressReceived(const int, const int)));

  albums_request_->Process();

}

void QobuzService::AlbumsResultsReceived(const int id, const SongList &songs, const QString &error) {
  emit AlbumsResults(songs, error);
}

void QobuzService::AlbumsUpdateStatusReceived(const int id, const QString &text) {
  emit AlbumsUpdateStatus(text);
}

void QobuzService::AlbumsProgressSetMaximumReceived(const int id, const int max) {
  emit AlbumsProgressSetMaximum(max);
}

void QobuzService::AlbumsUpdateProgressReceived(const int id, const int progress) {
  emit AlbumsUpdateProgress(progress);
}

void QobuzService::ResetSongsRequest() {

  if (songs_request_.get()) {
    disconnect(songs_request_.get(), 0, nullptr, 0);
    disconnect(this, 0, songs_request_.get(), 0);
    songs_request_.reset();
  }

}

void QobuzService::GetSongs() {

  if (app_id().isEmpty()) {
    emit SongsResults(SongList(), tr("Missing Qobuz app ID."));
    return;
  }

  if (!authenticated()) {
    emit SongsResults(SongList(), tr("Not authenticated with Qobuz."));
    return;
  }

  ResetSongsRequest();
  songs_request_.reset(new QobuzRequest(this, url_handler_, network_, QobuzBaseRequest::QueryType_Songs, this));
  connect(songs_request_.get(), SIGNAL(Results(const int, const SongList&, const QString&)), SLOT(SongsResultsReceived(const int, const SongList&, const QString&)));
  connect(songs_request_.get(), SIGNAL(UpdateStatus(const int, const QString&)), SLOT(SongsUpdateStatusReceived(const int, const QString&)));
  connect(songs_request_.get(), SIGNAL(ProgressSetMaximum(const int, const int)), SLOT(SongsProgressSetMaximumReceived(const int, const int)));
  connect(songs_request_.get(), SIGNAL(UpdateProgress(const int, const int)), SLOT(SongsUpdateProgressReceived(const int, const int)));

  songs_request_->Process();

}

void QobuzService::SongsResultsReceived(const int id, const SongList &songs, const QString &error) {
  emit SongsResults(songs, error);
}

void QobuzService::SongsUpdateStatusReceived(const int id, const QString &text) {
  emit SongsUpdateStatus(text);
}

void QobuzService::SongsProgressSetMaximumReceived(const int id, const int max) {
  emit SongsProgressSetMaximum(max);
}

void QobuzService::SongsUpdateProgressReceived(const int id, const int progress) {
  emit SongsUpdateProgress(progress);
}

int QobuzService::Search(const QString &text, InternetSearch::SearchType type) {

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

  if (app_id_.isEmpty()) { // App ID is the only thing needed to search.
    emit SearchResults(search_id_, SongList(), tr("Missing Qobuz app ID."));
    return;
  }

  SendSearch();

}

void QobuzService::CancelSearch() {
}

void QobuzService::SendSearch() {

  QobuzBaseRequest::QueryType type;

  switch (pending_search_type_) {
    case InternetSearch::SearchType_Artists:
      type = QobuzBaseRequest::QueryType_SearchArtists;
      break;
    case InternetSearch::SearchType_Albums:
      type = QobuzBaseRequest::QueryType_SearchAlbums;
      break;
    case InternetSearch::SearchType_Songs:
      type = QobuzBaseRequest::QueryType_SearchSongs;
      break;
    default:
      //Error("Invalid search type.");
      return;
  }

  search_request_.reset(new QobuzRequest(this, url_handler_, network_, type, this));

  connect(search_request_.get(), SIGNAL(Results(const int, const SongList&, const QString&)), SLOT(SearchResultsReceived(const int, const SongList&, const QString&)));
  connect(search_request_.get(), SIGNAL(UpdateStatus(const int, const QString&)), SIGNAL(SearchUpdateStatus(const int, const QString&)));
  connect(search_request_.get(), SIGNAL(ProgressSetMaximum(const int, const int)), SIGNAL(SearchProgressSetMaximum(const int, const int)));
  connect(search_request_.get(), SIGNAL(UpdateProgress(const int, const int)), SIGNAL(SearchUpdateProgress(const int, const int)));

  search_request_->Search(search_id_, search_text_);
  search_request_->Process();

}

void QobuzService::SearchResultsReceived(const int id, const SongList &songs, const QString &error) {
  emit SearchResults(id, songs, error);
}

void QobuzService::GetStreamURL(const QUrl &url) {

  if (app_id().isEmpty() || app_secret().isEmpty()) { // Don't check for login here, because we allow automatic login.
    emit StreamURLFinished(url, url, Song::FileType_Stream, -1, -1, -1, tr("Missing Qobuz app ID or secret."));
    return;
  }

  QobuzStreamURLRequest *stream_url_req = new QobuzStreamURLRequest(this, network_, url, this);
  stream_url_requests_ << stream_url_req;

  connect(stream_url_req, SIGNAL(TryLogin()), this, SLOT(TryLogin()));
  connect(stream_url_req, SIGNAL(StreamURLFinished(const QUrl&, const QUrl&, const Song::FileType, const int, const int, const qint64, QString)), this, SLOT(HandleStreamURLFinished(const QUrl&, const QUrl&, const Song::FileType, const int, const int, const qint64, QString)));
  connect(this, SIGNAL(LoginComplete(const bool, QString)), stream_url_req, SLOT(LoginComplete(const bool, QString)));

  stream_url_req->Process();

}

void QobuzService::HandleStreamURLFinished(const QUrl &original_url, const QUrl &media_url, const Song::FileType filetype, const int samplerate, const int bit_depth, const qint64 duration, QString error) {

  QobuzStreamURLRequest *stream_url_req = qobject_cast<QobuzStreamURLRequest*>(sender());
  if (!stream_url_req || !stream_url_requests_.contains(stream_url_req)) return;
  stream_url_req->deleteLater();
  stream_url_requests_.removeAll(stream_url_req);

  emit StreamURLFinished(original_url, media_url, filetype, samplerate, bit_depth, duration, error);

}

QString QobuzService::LoginError(QString error, QVariant debug) {

  qLog(Error) << "Qobuz:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

  emit LoginFailure(error);
  emit LoginComplete(false, error);

  return error;

}
