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

#ifdef HAVE_DZMEDIA
#  include <dzmedia.h>
#endif

#include <QObject>
#include <QByteArray>
#include <QList>
#include <QVector>
#include <QPair>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QMenu>
#include <QDesktopServices>
#include <QSettings>

#include "core/application.h"
#include "core/player.h"
#include "core/closure.h"
#include "core/logging.h"
#include "core/mergedproxymodel.h"
#include "core/network.h"
#include "core/song.h"
#include "core/iconloader.h"
#include "core/taskmanager.h"
#include "core/timeconstants.h"
#include "core/utilities.h"
#include "internet/internetservices.h"
#include "internet/internetsearch.h"
#include "internet/localredirectserver.h"
#include "deezerservice.h"
#include "deezerurlhandler.h"
#include "settings/deezersettingspage.h"

const Song::Source DeezerService::kSource = Song::Source_Deezer;
const char *DeezerService::kApiUrl = "https://api.deezer.com";
const char *DeezerService::kOAuthUrl = "https://connect.deezer.com/oauth/auth.php";
const char *DeezerService::kOAuthAccessTokenUrl = "https://connect.deezer.com/oauth/access_token.php";
const char *DeezerService::kOAuthRedirectUrl = "https://oauth.strawbs.net";
const int DeezerService::kAppID = 303684;
const char *DeezerService::kSecretKey = "06911976010b9ddd7256769adf2b2e56";

typedef QPair<QString, QString> Param;

DeezerService::DeezerService(Application *app, QObject *parent)
    : InternetService(Song::Source_Deezer, "Deezer", "dzmedia", app, parent),
      network_(new NetworkAccessManager(this)),
      url_handler_(new DeezerUrlHandler(app, this)),
#ifdef HAVE_DZMEDIA
      dzmedia_(new DZMedia(this)),
#endif
      timer_searchdelay_(new QTimer(this)),
      searchdelay_(1500),
      albumssearchlimit_(1),
      songssearchlimit_(1),
      fetchalbums_(false),
      preview_(false),
      pending_search_id_(0),
      next_pending_search_id_(1),
      search_id_(0),
      albums_requested_(0),
      albums_received_(0)
  {

  timer_searchdelay_->setSingleShot(true);
  connect(timer_searchdelay_, SIGNAL(timeout()), SLOT(StartSearch()));

  connect(this, SIGNAL(Authenticated()), app->player(), SLOT(HandleAuthentication()));

  app->player()->RegisterUrlHandler(url_handler_);

  ReloadSettings();
  LoadAccessToken();

#ifdef HAVE_DZMEDIA
  connect(dzmedia_, SIGNAL(StreamURLReceived(QUrl, QUrl, DZMedia::FileType)), this, SLOT(GetStreamURLFinished(QUrl, QUrl, DZMedia::FileType)));
#endif

}

DeezerService::~DeezerService() {}

void DeezerService::ShowConfig() {
  app_->OpenSettingsDialogAtPage(SettingsDialog::Page_Deezer);
}

void DeezerService::ReloadSettings() {

  QSettings s;
  s.beginGroup(DeezerSettingsPage::kSettingsGroup);
  searchdelay_ = s.value("searchdelay", 1500).toInt();
  albumssearchlimit_ = s.value("albumssearchlimit", 100).toInt();
  songssearchlimit_ = s.value("songssearchlimit", 100).toInt();
  fetchalbums_ = s.value("fetchalbums", false).toBool();
  coversize_ = s.value("coversize", "cover_big").toString();
#if defined(HAVE_DEEZER) || defined(HAVE_DZMEDIA)
  bool preview(false);
#else
  bool preview(true);
#endif
  preview_ = s.value("preview", preview).toBool();
  s.endGroup();

}

void DeezerService::LoadAccessToken() {

  QSettings s;
  s.beginGroup(DeezerSettingsPage::kSettingsGroup);
  if (s.contains("access_token") && s.contains("expiry_time")) {
    access_token_ = s.value("access_token").toString();
    expiry_time_ = s.value("expiry_time").toDateTime();
  }
  s.endGroup();

}

void DeezerService::Logout() {

  access_token_.clear();
  QSettings s;
  s.beginGroup(DeezerSettingsPage::kSettingsGroup);
  s.remove("access_token");
  s.remove("expiry_time");
  s.endGroup();

}

void DeezerService::StartAuthorisation() {

  LocalRedirectServer *server = new LocalRedirectServer(this);
  server->Listen();

  QUrl url = QUrl(kOAuthUrl);
  QUrlQuery url_query;
  //url_query.addQueryItem("response_type", "token");
  url_query.addQueryItem("response_type", "code");
  url_query.addQueryItem("app_id", QString::number(kAppID));
  QUrl redirect_url;
  QUrlQuery redirect_url_query;

  const QString port = QString::number(server->url().port());

  redirect_url = QUrl(kOAuthRedirectUrl);
  redirect_url_query.addQueryItem("port", port);  
  redirect_url.setQuery(redirect_url_query);
  url_query.addQueryItem("redirect_uri", redirect_url.toString());
  url.setQuery(url_query);

  NewClosure(server, SIGNAL(Finished()), this, &DeezerService::RedirectArrived, server, redirect_url);
  QDesktopServices::openUrl(url);

}

void DeezerService::RedirectArrived(LocalRedirectServer *server, QUrl url) {

  server->deleteLater();
  QUrl request_url = server->request_url();
  RequestAccessToken(QUrlQuery(request_url).queryItemValue("code").toUtf8());

}

void DeezerService::RequestAccessToken(const QByteArray &code) {

  typedef QPair<QString, QString> Arg;
  typedef QList<Arg> ArgList;

  typedef QPair<QByteArray, QByteArray> EncodedArg;
  typedef QList<EncodedArg> EncodedArgList;

  ArgList args = ArgList() << Arg("app_id", QString::number(kAppID))
                           << Arg("secret", kSecretKey)
                           << Arg("code", code);

  QUrlQuery url_query;
  for (const Arg &arg : args) {
    EncodedArg encoded_arg(QUrl::toPercentEncoding(arg.first), QUrl::toPercentEncoding(arg.second));
    url_query.addQueryItem(encoded_arg.first, encoded_arg.second);
  }

  QUrl url(kOAuthAccessTokenUrl);
  QNetworkRequest request = QNetworkRequest(url);
  QNetworkReply *reply = network_->post(request, url_query.toString(QUrl::FullyEncoded).toUtf8());
  NewClosure(reply, SIGNAL(finished()), this, SLOT(FetchAccessTokenFinished(QNetworkReply*)), reply);

}

void DeezerService::FetchAccessTokenFinished(QNetworkReply *reply) {

  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    Error(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    return;
  }

  forever {
    QByteArray line = reply->readLine();
    QString str(line);
    QStringList args = str.split("&");
    for (QString arg : args) {
      QStringList params = arg.split("=");
      if (params.count() < 2) continue;
      QString param1 = params.first();
      QString param2 = params[1];
      if (param1 == "access_token") access_token_ = param2;
      else if (param1 == "expires") SetExpiryTime(param2.toInt());
    }
    if (reply->atEnd()) break;
  }

  QSettings s;
  s.beginGroup(DeezerSettingsPage::kSettingsGroup);
  s.setValue("access_token", access_token_);
  s.setValue("expiry_time", expiry_time_);
  s.endGroup();

  emit Authenticated();
  emit LoginSuccess();

}

void DeezerService::SetExpiryTime(int expires_in_seconds) {

  // Set the expiry time with two minutes' grace.
  expiry_time_ = QDateTime::currentDateTime().addSecs(expires_in_seconds - 120);
  qLog(Debug) << "Current oauth access token expires at:" << expiry_time_;

}

QNetworkReply *DeezerService::CreateRequest(const QString &ressource_name, const QList<Param> &params) {

  typedef QPair<QString, QString> Arg;
  typedef QList<Arg> ArgList;

  typedef QPair<QByteArray, QByteArray> EncodedArg;
  typedef QList<EncodedArg> EncodedArgList;

  ArgList args = ArgList() << Arg("access_token", access_token_)
                           << Arg("output", "json")
                           << params;

  QUrlQuery url_query;
  for (const Arg& arg : args) {
    EncodedArg encoded_arg(QUrl::toPercentEncoding(arg.first), QUrl::toPercentEncoding(arg.second));
    url_query.addQueryItem(encoded_arg.first, encoded_arg.second);
  }

  QUrl url(kApiUrl + QString("/") + ressource_name);
  url.setQuery(url_query);
  QNetworkRequest req(url);
  QNetworkReply *reply = network_->get(req);

  //qLog(Debug) << "Deezer: Sending request" << url;

  return reply;

}

QByteArray DeezerService::GetReplyData(QNetworkReply *reply) {

  QByteArray data;

  if (reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
    data = reply->readAll();
  }
  else {
    if (reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      QString failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      Error(failure_reason);
    }
    else {
      // See if there is Json data containing "error" - then use that instead.
      data = reply->readAll();
      QJsonParseError error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &error);
      QString failure_reason;
      if (error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (json_obj.contains("error")) {
          QJsonValue json_value_error = json_obj["error"];
          if (json_value_error.isObject()) {
            QJsonObject json_error = json_value_error.toObject();
            int code = json_error["code"].toInt();
            if (code == 300) Logout();
            QString message = json_error["message"].toString();
            QString type = json_error["type"].toString();
            failure_reason = QString("%1 (%2)").arg(message).arg(code);
          }
          else { failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()); }
        }
        else {
          failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
      }
      else {
        failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      }
      if (reply->error() == QNetworkReply::ContentAccessDenied || reply->error() == QNetworkReply::ContentOperationNotPermittedError || reply->error() == QNetworkReply::AuthenticationRequiredError) {
        // Session is probably expired
        Logout();
        Error(failure_reason);
      }
      else if (reply->error() == QNetworkReply::ContentNotFoundError) { // Ignore this error
        Error(failure_reason);
      }
      else { // Fail
        Error(failure_reason);
      }
    }
    return QByteArray();
  }

  return data;
  
}
  
QJsonObject DeezerService::ExtractJsonObj(QByteArray &data) {

  QJsonParseError error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &error);
  
  //qLog(Debug) << json_doc;

  if (error.error != QJsonParseError::NoError) {
    Error("Reply from server missing Json data.", data);
    return QJsonObject();
  }

  if (json_doc.isNull() || json_doc.isEmpty()) {
    Error("Received empty Json document.", json_doc);
    return QJsonObject();
  }

  if (!json_doc.isObject()) {
    Error("Json document is not an object.", json_doc);
    return QJsonObject();
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    Error("Received empty Json object.", json_doc);
    return QJsonObject();
  }

  //qLog(Debug) << json_obj;

  return json_obj;

}

QJsonValue DeezerService::ExtractData(QByteArray &data) {

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) return QJsonObject();

  if (json_obj.contains("error")) {
    QJsonValue json_value_error = json_obj["error"];
    if (!json_value_error.isObject()) {
      Error("Error missing object", json_obj);
      return QJsonValue();
    }
    QJsonObject json_error = json_value_error.toObject();
    int code = json_error["code"].toInt();
    if (code == 300) Logout();
    QString message = json_error["message"].toString();
    QString type = json_error["type"].toString();
    Error(QString("%1 (%2)").arg(message).arg(code));
    return QJsonValue();
  }

  if (!json_obj.contains("data") && !json_obj.contains("DATA")) {
    Error("Json reply is missing data.", json_obj);
    return QJsonValue();
  }

  QJsonValue json_data;
  if (json_obj.contains("data")) json_data = json_obj["data"];
  else json_data = json_obj["DATA"];

  return json_data;

}

int DeezerService::Search(const QString &text, InternetSearch::SearchBy searchby) {

  pending_search_id_ = next_pending_search_id_;
  pending_search_text_ = text;
  pending_searchby_ = searchby;

  next_pending_search_id_++;

  if (text.isEmpty()) {
    timer_searchdelay_->stop();
    return pending_search_id_;
  }
  timer_searchdelay_->setInterval(searchdelay_);
  timer_searchdelay_->start();

  return pending_search_id_;

}

void DeezerService::StartSearch() {

  if (access_token_.isEmpty()) {
    emit SearchError(pending_search_id_, "Not authenticated.");
    next_pending_search_id_ = 1;
    ShowConfig();
    return;
  }
  ClearSearch();
  search_id_ = pending_search_id_;
  search_text_ = pending_search_text_;

  SendSearch();

}

void DeezerService::CancelSearch() {
  ClearSearch();
}

void DeezerService::ClearSearch() {
  search_id_ = 0;
  search_text_.clear();
  search_error_.clear();
  albums_requested_ = 0;
  albums_received_ = 0;
  requests_album_.clear();
  requests_song_.clear();
  songs_.clear();
}

void DeezerService::SendSearch() {

  emit UpdateStatus("Searching...");

  QList<Param> parameters;
  parameters << Param("q", search_text_);
  QString searchparam;
  switch (pending_searchby_) {
    case InternetSearch::SearchBy_Songs:
      searchparam = "search/track";
      parameters << Param("limit", QString::number(songssearchlimit_));
      break;
    case InternetSearch::SearchBy_Albums:
    default:
      searchparam = "search/album";
      parameters << Param("limit", QString::number(albumssearchlimit_));
      break;
  }

  QNetworkReply *reply = CreateRequest(searchparam, parameters);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(SearchFinished(QNetworkReply*, int)), reply, search_id_);

}

void DeezerService::SearchFinished(QNetworkReply *reply, int id) {

  reply->deleteLater();

  if (id != search_id_) return;

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    CheckFinish();
    return;
  }

  QJsonValue json_value = ExtractData(data);
  if (!json_value.isArray()) {
    CheckFinish();
    return;
  }

  QJsonArray json_data = json_value.toArray();
  if (json_data.isEmpty()) {
    Error("No match.");
    CheckFinish();
    return;
  }

  //qLog(Debug) << json_data;

  for (const QJsonValue &value : json_data) {
    //qLog(Debug) << value;
    if (!value.isObject()) {
      Error("Invalid Json reply, data is not an object.", value);
      continue;
    }
    QJsonObject json_obj = value.toObject();
    //qLog(Debug) << json_obj;

    if (!json_obj.contains("id") || !json_obj.contains("type")) {
      Error("Invalid Json reply, item is missing ID or type.", json_obj);
      continue;
    }

    //int id = json_obj["id"].toInt();
    QString type = json_obj["type"].toString();

    if (!json_obj.contains("artist")) {
      Error("Invalid Json reply, item missing artist.", json_obj);
      continue;
    }
    QJsonValue json_value_artist = json_obj["artist"];
    if (!json_value_artist.isObject()) {
      Error("Invalid Json reply, item artist is not a object.", json_value_artist);
      continue;
    }
    QJsonObject json_artist = json_value_artist.toObject();

    if (!json_artist.contains("name")) {
      Error("Invalid Json reply, artist data missing name.", json_artist);
      continue;
    }
    QString artist = json_artist["name"].toString();
    int album_id(0);
    QString album;
    QString cover;

    if (type == "album") {
      album_id = json_obj["id"].toInt();
      album = json_obj["title"].toString();
      cover = json_obj[coversize_].toString();
    }
    else if (type == "track") {

      if (!json_obj.contains("album")) {
        Error("Invalid Json reply, missing album data.", json_obj);
        continue;
      }
      QJsonValue json_value_album = json_obj["album"];
      if (!json_value_album.isObject()) {
        Error("Invalid Json reply, album data is not an object.", json_value_album);
        continue;
      }
      QJsonObject json_album = json_value_album.toObject();
      if (!json_album.contains("id") || !json_album.contains("title")) {
        Error("Invalid Json reply, album data is missing ID or title.", json_album);
        continue;
      }
      album_id = json_album["id"].toInt();
      album = json_album["title"].toString();
      cover = json_album[coversize_].toString();
      if (!fetchalbums_) {
        Song song = ParseSong(album_id, album, cover, value);
        songs_ << song;
        continue;
      }
    }

    DeezerAlbumContext *album_ctx;
    if (requests_album_.contains(album_id)) {
      album_ctx = requests_album_.value(album_id);
      album_ctx->search_id = search_id_;
      continue;
    }
    album_ctx = CreateAlbum(album_id, artist, album, cover);
    GetAlbum(album_ctx);
    albums_requested_++;
    if (albums_requested_ >= albumssearchlimit_) break;

  }

  if (albums_requested_ > 0) {
    emit UpdateStatus(QString("Retrieving %1 album%2...").arg(albums_requested_).arg(albums_requested_ == 1 ? "" : "s"));
    emit ProgressSetMaximum(albums_requested_);
    emit UpdateProgress(0);
  }

  CheckFinish();

}

DeezerAlbumContext *DeezerService::CreateAlbum(const int album_id, const QString &artist, const QString &album, const QString &cover) {

  DeezerAlbumContext *album_ctx = new DeezerAlbumContext;
  album_ctx->id = album_id;
  album_ctx->artist = artist;
  album_ctx->album = album;
  album_ctx->cover = cover;
  album_ctx->cover_url.setUrl(cover);
  requests_album_.insert(album_id, album_ctx);

  return album_ctx;

 }

void DeezerService::GetAlbum(const DeezerAlbumContext *album_ctx) {

  QList<Param> parameters;
  QNetworkReply *reply = CreateRequest(QString("album/%1/tracks").arg(album_ctx->id), parameters);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(GetAlbumFinished(QNetworkReply*, int, int)), reply, search_id_, album_ctx->id);

}

void DeezerService::GetAlbumFinished(QNetworkReply *reply, int search_id, int album_id) {

  reply->deleteLater();

  if (!requests_album_.contains(album_id)) {
    qLog(Error) << "Deezer: Got reply for cancelled album request: " << album_id;
    CheckFinish();
    return;
  }
  DeezerAlbumContext *album_ctx = requests_album_.value(album_id);

  if (search_id != search_id_) {
    if (album_ctx->search_id == search_id) delete requests_album_.take(album_ctx->id);
    return;
  }

  albums_received_++;
  emit UpdateProgress(albums_received_);

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    delete requests_album_.take(album_ctx->id);
    CheckFinish();
    return;
  }

  QJsonValue json_value = ExtractData(data);
  if (!json_value.isArray()) {
    delete requests_album_.take(album_ctx->id);
    CheckFinish();
    return;
  }

  QJsonArray json_data = json_value.toArray();
  if (json_data.isEmpty()) {
    delete requests_album_.take(album_ctx->id);
    CheckFinish();
    return;
  }

  bool compilation = false;
  bool multidisc = false;
  Song first_song;
  SongList songs;
  for (const QJsonValue &value : json_data) {
    Song song = ParseSong(album_ctx->id, album_ctx->album, album_ctx->cover, value);
    if (!song.is_valid()) continue;
    if (song.disc() >= 2) multidisc = true;
    if (song.is_compilation() || (first_song.is_valid() && song.artist() != first_song.artist())) compilation = true;
    if (!first_song.is_valid()) first_song = song;
    songs << song;
  }
  for (Song &song : songs) {
    if (compilation) song.set_compilation_detected(true);
    if (multidisc) {
      QString album_full(QString("%1 - (Disc %2)").arg(song.album()).arg(song.disc()));
      song.set_album(album_full);
    }
    songs_ << song;
  }

  delete requests_album_.take(album_ctx->id);
  CheckFinish();

}

Song DeezerService::ParseSong(const int album_id, const QString &album, const QString &album_cover, const QJsonValue &value) {

  if (!value.isObject()) {
    Error("Invalid Json reply, track is not an object.", value);
    return Song();
  }
  QJsonObject json_obj = value.toObject();

  //qLog(Debug) << json_obj;

  if (
      !json_obj.contains("id") ||
      !json_obj.contains("title") ||
      !json_obj.contains("artist") ||
      !json_obj.contains("duration") ||
      !json_obj.contains("preview")
    ) {
    Error("Invalid Json reply, track is missing one or more values.", json_obj);
    return Song();
  }

  int song_id = json_obj["id"].toInt();
  QString title = json_obj["title"].toString();
  QJsonValue json_value_artist = json_obj["artist"];
  QVariant q_duration = json_obj["duration"].toVariant();
  int track(0);
  if (json_obj.contains("track_position")) track = json_obj["track_position"].toInt();
  int disc(0);
  if (json_obj.contains("disk_number")) disc = json_obj["disk_number"].toInt();
  QString preview = json_obj["preview"].toString();

  if (!json_value_artist.isObject()) {
    Error("Invalid Json reply, track artist is not an object.", json_value_artist);
    return Song();
  }
  QJsonObject json_artist = json_value_artist.toObject();
  if (!json_artist.contains("name")) {
    Error("Invalid Json reply, track artist is missing name.", json_artist);
    return Song();
  }
  QString artist = json_artist["name"].toString();

  Song song;
  song.set_source(Song::Source_Deezer);
  song.set_id(song_id);
  song.set_album_id(album_id);
  song.set_artist(artist);
  song.set_album(album);
  song.set_title(title);
  song.set_disc(disc);
  song.set_track(track);
  song.set_art_automatic(album_cover);

  QUrl url;
  if (preview_) {
    url.setUrl(preview);
    quint64 duration = (30 * kNsecPerSec);
    song.set_length_nanosec(duration);
  }
  else {
    url.setScheme(url_handler_->scheme());
    url.setPath(QString("track/%1").arg(QString::number(song_id)));
    if (q_duration.isValid()) {
      quint64 duration = q_duration.toULongLong() * kNsecPerSec;
      song.set_length_nanosec(duration);
    }
  }
  song.set_url(url);

  song.set_valid(true);

  return song;

}

bool DeezerService::GetStreamURL(const QUrl &original_url) {

#ifdef HAVE_DZMEDIA
  stream_request_url_ = original_url;
  dzmedia_->GetStreamURL(original_url);
  return true;
#else
  stream_request_url_ = QUrl();
  return false;
#endif

}

#ifdef HAVE_DZMEDIA
void DeezerService::GetStreamURLFinished(const QUrl original_url, const QUrl media_url, const DZMedia::FileType dzmedia_filetype) {

  Song::FileType filetype(Song::FileType_Unknown);

  switch (dzmedia_filetype) {
      case DZMedia::FileType_FLAC:
        filetype = Song::FileType_FLAC;
        break;
      case DZMedia::FileType_MPEG:
        filetype = Song::FileType_MPEG;
        break;
      case DZMedia::FileType_Stream:
        filetype = Song::FileType_Stream;
        break;
      default:
        filetype = Song::FileType_Unknown;
        break;
  }
  stream_request_url_ = QUrl();
  emit StreamURLReceived(original_url, media_url, filetype);

}
#endif

void DeezerService::CheckFinish() {

  if (search_id_ == 0) return;

  if (albums_requested_ <= albums_received_) {
    if (songs_.isEmpty()) {
      if (search_error_.isEmpty()) emit SearchError(search_id_, "Unknown error");
      else emit SearchError(search_id_, search_error_);
    }
    else emit SearchResults(search_id_, songs_);
    ClearSearch();
  }

}

void DeezerService::Error(QString error, QVariant debug) {
  qLog(Error) << "Deezer:" << error;
  if (debug.isValid()) qLog(Debug) << debug;
  if (search_id_ != 0) {
    if (!error.isEmpty()) {
      search_error_ += error;
      search_error_ += "<br />";
    }
    CheckFinish();
  }
  if (!stream_request_url_.isEmpty()) {
    emit StreamURLReceived(stream_request_url_, stream_request_url_, Song::FileType_Stream);
    stream_request_url_ = QUrl();
  }
}
