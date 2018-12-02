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
#include "tidalservice.h"
#include "tidalurlhandler.h"
#include "settings/tidalsettingspage.h"

const Song::Source TidalService::kSource = Song::Source_Tidal;
const char *TidalService::kApiUrl = "https://listen.tidal.com/v1";
const char *TidalService::kAuthUrl = "https://listen.tidal.com/v1/login/username";
const char *TidalService::kResourcesUrl = "http://resources.tidal.com";
const char *TidalService::kApiTokenB64 = "UDVYYmVvNUxGdkVTZUR5Ng==";
const int TidalService::kLoginAttempts = 1;
const int TidalService::kTimeResetLoginAttempts = 60000;

typedef QPair<QString, QString> Param;

TidalService::TidalService(Application *app, QObject *parent)
    : InternetService(Song::Source_Tidal, "Tidal", "tidal", app, parent),
      network_(new NetworkAccessManager(this)),
      url_handler_(new TidalUrlHandler(app, this)),
      timer_searchdelay_(new QTimer(this)),
      timer_login_attempt_(new QTimer(this)),
      searchdelay_(1500),
      albumssearchlimit_(1),
      songssearchlimit_(1),
      fetchalbums_(false),
      user_id_(0),
      pending_search_id_(0),
      next_pending_search_id_(1),
      search_id_(0),
      albums_requested_(0),
      albums_received_(0),
      login_sent_(false),
      login_attempts_(0)
  {

  timer_searchdelay_->setSingleShot(true);
  connect(timer_searchdelay_, SIGNAL(timeout()), SLOT(StartSearch()));

  timer_login_attempt_->setSingleShot(true);
  connect(timer_login_attempt_, SIGNAL(timeout()), SLOT(ResetLoginAttempts()));

  connect(this, SIGNAL(Login()), SLOT(SendLogin()));
  connect(this, SIGNAL(Login(QString, QString)), SLOT(SendLogin(QString, QString)));

  app->player()->RegisterUrlHandler(url_handler_);

  ReloadSettings();
  LoadSessionID();

}

TidalService::~TidalService() {}

void TidalService::ShowConfig() {
  app_->OpenSettingsDialogAtPage(SettingsDialog::Page_Tidal);
}

void TidalService::ReloadSettings() {

  QSettings s;
  s.beginGroup(TidalSettingsPage::kSettingsGroup);
  username_ = s.value("username").toString();
  QByteArray password = s.value("password").toByteArray();
  if (password.isEmpty()) password_.clear();
  else password_ = QString::fromUtf8(QByteArray::fromBase64(password));
  quality_ = s.value("quality").toString();
  searchdelay_ = s.value("searchdelay", 1500).toInt();
  albumssearchlimit_ = s.value("albumssearchlimit", 100).toInt();
  songssearchlimit_ = s.value("songssearchlimit", 100).toInt();
  fetchalbums_ = s.value("fetchalbums", false).toBool();
  coversize_ = s.value("coversize", "320x320").toString();
  streamurl_ = s.value("streamurl", "http").toString();
  s.endGroup();

}

void TidalService::LoadSessionID() {

  QSettings s;
  s.beginGroup(TidalSettingsPage::kSettingsGroup);
  if (!s.contains("user_id") ||!s.contains("session_id") || !s.contains("country_code")) return;
  session_id_ = s.value("session_id").toString();
  user_id_ = s.value("user_id").toInt();
  country_code_ = s.value("country_code").toString();
  clientuniquekey_ = Utilities::GetRandomStringWithChars(12).toLower();
  s.endGroup();

}

void TidalService::SendLogin() {
  SendLogin(username_, password_);
}

void TidalService::SendLogin(const QString &username, const QString &password) {

  if (search_id_ != 0) emit UpdateStatus("Authenticating...");

  login_sent_ = true;
  login_attempts_++;
  if (timer_login_attempt_->isActive()) timer_login_attempt_->stop();
  timer_login_attempt_->setInterval(kTimeResetLoginAttempts);
  timer_login_attempt_->start();

  typedef QPair<QString, QString> Arg;
  typedef QList<Arg> ArgList;

  typedef QPair<QByteArray, QByteArray> EncodedArg;
  typedef QList<EncodedArg> EncodedArgList;

  ArgList args = ArgList() << Arg("token", QByteArray::fromBase64(kApiTokenB64))
                           << Arg("username", username)
                           << Arg("password", password)
                           << Arg("clientVersion", "2.2.1--7");

  QStringList query_items;
  QUrlQuery url_query;
  for (const Arg &arg : args) {
    EncodedArg encoded_arg(QUrl::toPercentEncoding(arg.first), QUrl::toPercentEncoding(arg.second));
    query_items << QString(encoded_arg.first + "=" + encoded_arg.second);
    url_query.addQueryItem(encoded_arg.first, encoded_arg.second);
  }

  QUrl url(kAuthUrl);
  QNetworkRequest req(url);

  req.setRawHeader("Origin", "http://listen.tidal.com");
  req.setRawHeader("X-Tidal-Token", QByteArray::fromBase64(kApiTokenB64));
  QNetworkReply *reply = network_->post(req, url_query.toString(QUrl::FullyEncoded).toUtf8());
  NewClosure(reply, SIGNAL(finished()), this, SLOT(HandleAuthReply(QNetworkReply*)), reply);

}

void TidalService::HandleAuthReply(QNetworkReply *reply) {

  reply->deleteLater();

  login_sent_ = false;

  if (reply->error() != QNetworkReply::NoError) {
    if (reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      QString failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      Error(failure_reason);
      emit LoginFailure(failure_reason);
      return;
    }
    else {
      // See if there is Json data containing "userMessage" - then use that instead.
      QByteArray data(reply->readAll());
      QJsonParseError error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &error);
      QString failure_reason;
      if (error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("userMessage")) {
          failure_reason = QString("Authentication failure: %1").arg(json_obj["userMessage"].toString());
        }
        else {
          failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
      }
      else {
        failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      }
      Error(failure_reason);
      emit LoginFailure(failure_reason);
      return;
    }
  }

  QByteArray data(reply->readAll());
  QJsonParseError error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &error);

  if (error.error != QJsonParseError::NoError) {
    QString failure_reason("Authentication reply from server missing Json data.");
    Error(failure_reason);
    emit LoginFailure(failure_reason);
    return;
  }

  if (json_doc.isNull() || json_doc.isEmpty()) {
    QString failure_reason("Authentication reply from server has empty Json document.");
    Error(failure_reason);
    emit LoginFailure(failure_reason);
    return;
  }

  if (!json_doc.isObject()) {
    QString failure_reason("Authentication reply from server has Json document that is not an object.");
    Error(failure_reason, json_doc);
    emit LoginFailure(failure_reason);
    return;
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    QString failure_reason("Authentication reply from server has empty Json object.");
    Error(failure_reason, json_doc);
    emit LoginFailure(failure_reason);
    return;
  }

  if ( !json_obj.contains("userId") || !json_obj.contains("sessionId") || !json_obj.contains("countryCode") ) {
    QString failure_reason("Authentication reply from server is missing userId, sessionId or countryCode");
    Error(failure_reason, json_obj);
    emit LoginFailure(failure_reason);
    return;
  }

  country_code_ = json_obj["countryCode"].toString();
  session_id_ = json_obj["sessionId"].toString();
  user_id_ = json_obj["userId"].toInt();
  clientuniquekey_ = Utilities::GetRandomStringWithChars(12).toLower();

  QSettings s;
  s.beginGroup(TidalSettingsPage::kSettingsGroup);
  s.setValue("user_id", user_id_);
  s.setValue("session_id", session_id_);
  s.setValue("country_code", country_code_);
  s.endGroup();

  qLog(Debug) << "Tidal: Login successful" << "user id" << user_id_ << "session id" << session_id_ << "country code" << country_code_;

  if (search_id_ != 0) {
    qLog(Debug) << "Tidal: Resuming search" << search_id_;
    SendSearch();
  }
  if (!stream_request_url_.isEmpty()) {
    qLog(Debug) << "Tidal: Resuming get stream url" << stream_request_url_;
    emit GetStreamURL(stream_request_url_);
  }

  emit LoginSuccess();

}

void TidalService::Logout() {

  user_id_ = 0;
  session_id_.clear();
  country_code_.clear();

  QSettings s;
  s.beginGroup(TidalSettingsPage::kSettingsGroup);
  s.remove("user_id");
  s.remove("session_id");
  s.remove("country_code");
  s.endGroup();

}

void TidalService::ResetLoginAttempts() {
  login_attempts_ = 0;
}

QNetworkReply *TidalService::CreateRequest(const QString &ressource_name, const QList<Param> &params) {

  typedef QPair<QString, QString> Arg;
  typedef QList<Arg> ArgList;

  typedef QPair<QByteArray, QByteArray> EncodedArg;
  typedef QList<EncodedArg> EncodedArgList;

  ArgList args = ArgList() << params
                           << Arg("sessionId", session_id_)
                           << Arg("countryCode", country_code_);

  QStringList query_items;
  QUrlQuery url_query;
  for (const Arg& arg : args) {
    EncodedArg encoded_arg(QUrl::toPercentEncoding(arg.first), QUrl::toPercentEncoding(arg.second));
    query_items << QString(encoded_arg.first + "=" + encoded_arg.second);
    url_query.addQueryItem(encoded_arg.first, encoded_arg.second);
  }

  QUrl url(kApiUrl + QString("/") + ressource_name);
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setRawHeader("Origin", "http://listen.tidal.com");
  req.setRawHeader("X-Tidal-SessionId", session_id_.toUtf8());
  QNetworkReply *reply = network_->get(req);

  //qLog(Debug) << "Tidal: Sending request" << url;

  return reply;

}

QByteArray TidalService::GetReplyData(QNetworkReply *reply, const bool sendlogin) {

  QByteArray data;

  if (reply->error() == QNetworkReply::NoError) {
    data = reply->readAll();
  }
  else {
    if (reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      QString failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      Error(failure_reason);
    }
    else {
      // See if there is Json data containing "userMessage" - then use that instead.
      data = reply->readAll();
      QJsonParseError error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &error);
      QString failure_reason;
      if (error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("userMessage")) {
          failure_reason = json_obj["userMessage"].toString();
        }
        else {
          failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
      }
      else {
        failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      }
      if (reply->error() == QNetworkReply::ContentAccessDenied || reply->error() == QNetworkReply::ContentOperationNotPermittedError || reply->error() == QNetworkReply::AuthenticationRequiredError) {
        // Session is probably expired, attempt to login once
        Logout();
        if (sendlogin && login_attempts_ < kLoginAttempts && !username_.isEmpty() && !password_.isEmpty()) {
          qLog(Error) << "Tidal:" << failure_reason;
          qLog(Error) << "Tidal:" << QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
          qLog(Error) << "Tidal:" << "Attempting to login.";
          emit Login();
        }
        else {
          Error(failure_reason);
        }
      }
      else if (reply->error() == QNetworkReply::ContentNotFoundError) { // Ignore this error
        qLog(Error) << "Tidal:" << failure_reason;
      }
      else { // Fail
        Error(failure_reason);
      }
    }
    return QByteArray();
  }

  return data;

}

QJsonObject TidalService::ExtractJsonObj(QByteArray &data) {

  QJsonParseError error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &error);
  
  //qLog(Debug) << json_doc;

  if (error.error != QJsonParseError::NoError) {
    Error("Reply from server missing Json data.", data);
    return QJsonObject();
  }

  if (json_doc.isNull() || json_doc.isEmpty()) {
    Error("Received empty Json document.", data);
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

QJsonValue TidalService::ExtractItems(QByteArray &data) {

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) return QJsonArray();

  if (!json_obj.contains("items")) {
    Error("Json reply is missing items.", json_obj);
    return QJsonArray();
  }
  QJsonValue json_items = json_obj["items"];

  return json_items;

}

int TidalService::Search(const QString &text, InternetSearch::SearchBy searchby) {

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

void TidalService::StartSearch() {

  if (username_.isEmpty() || password_.isEmpty()) {
    emit SearchError(pending_search_id_, "Missing username and/or password.");
    next_pending_search_id_ = 1;
    ShowConfig();
    return;
  }
  ClearSearch();
  search_id_ = pending_search_id_;
  search_text_ = pending_search_text_;

  if (authenticated()) SendSearch();
  else emit Login(username_, password_);

}

void TidalService::CancelSearch() {
  ClearSearch();
}

void TidalService::ClearSearch() {
  search_id_ = 0;
  search_text_.clear();
  search_error_.clear();
  albums_requested_ = 0;
  albums_received_ = 0;
  requests_album_.clear();
  requests_song_.clear();
  songs_.clear();
}

void TidalService::SendSearch() {

  emit UpdateStatus("Searching...");

  QList<Param> parameters;
  parameters << Param("query", search_text_);

  QString searchparam;
  switch (pending_searchby_) {
    case InternetSearch::SearchBy_Songs:
      searchparam = "search/tracks";
      parameters << Param("limit", QString::number(songssearchlimit_));
      break;
    case InternetSearch::SearchBy_Albums:
    default:
      searchparam = "search/albums";
      parameters << Param("limit", QString::number(albumssearchlimit_));
      break;
  }

  QNetworkReply *reply = CreateRequest(searchparam, parameters);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(SearchFinished(QNetworkReply*, int)), reply, search_id_);

}

void TidalService::SearchFinished(QNetworkReply *reply, int id) {

  reply->deleteLater();

  if (id != search_id_) return;

  QByteArray data = GetReplyData(reply, true);
  if (data.isEmpty()) {
    CheckFinish();
    return;
  }
  QJsonValue json_value = ExtractItems(data);
  if (!json_value.isArray()) {
    CheckFinish();
    return;
  }
  QJsonArray json_items = json_value.toArray();
  if (json_items.isEmpty()) {
    Error("No match.");
    return;
  }

  //qLog(Debug) << json_items;

  QVector<QString> albums;
  for (const QJsonValue &value : json_items) {
    //qLog(Debug) << value;
    if (!value.isObject()) {
      qLog(Error) << "Tidal: Invalid Json reply, item not a object.";
      qLog(Debug) << value;
      continue;
    }
    QJsonObject json_obj = value.toObject();
    //qLog(Debug) << json_obj;
    int album_id(0);
    QString album("");
    if (json_obj.contains("type")) {
      // This was a albums search
      if (!json_obj.contains("id") || !json_obj.contains("title")) {
        qLog(Error) << "Tidal: Invalid Json reply, item is missing ID or title.";
        qLog(Debug) << json_obj;
        continue;
      }
      album_id = json_obj["id"].toInt();
      album = json_obj["title"].toString();
    }
    else if (json_obj.contains("album")) {
      // This was a tracks search
      if (!fetchalbums_) {
        Song song = ParseSong(0, value);
        songs_ << song;
        continue;
      }
      QJsonValue json_value_album = json_obj["album"];
      if (!json_value_album.isObject()) {
        qLog(Error) << "Tidal: Invalid Json reply, item album is not a object.";
        qLog(Debug) << json_value_album;
        continue;
      }
      QJsonObject json_album = json_value_album.toObject();
      if (!json_album.contains("id") || !json_album.contains("title")) {
        qLog(Error) << "Tidal: Invalid Json reply, item album is missing ID or title.";
        qLog(Debug) << json_album;
        continue;
      }
      album_id = json_album["id"].toInt();
      album = json_album["title"].toString();
    }
    else {
      qLog(Error) << "Tidal: Invalid Json reply, item missing type or album.";
      qLog(Debug) << json_obj;
      continue;
    }

    if (requests_album_.contains(album_id)) continue;

    if (!json_obj.contains("artist") || !json_obj.contains("title") || !json_obj.contains("audioQuality")) {
      qLog(Error) << "Tidal: Invalid Json reply, item missing artist, title or audioQuality.";
      qLog(Debug) << json_obj;
      continue;
    }
    QJsonValue json_value_artist = json_obj["artist"];
    if (!json_value_artist.isObject()) {
      qLog(Error) << "Tidal: Invalid Json reply, item artist is not a object.";
      qLog(Debug) << json_value_artist;
      continue;
    }
    QJsonObject json_artist = json_value_artist.toObject();
    if (!json_artist.contains("name")) {
      qLog(Error) << "Tidal: Invalid Json reply, item artist missing name.";
      qLog(Debug) << json_artist;
      continue;
    }
    QString artist = json_artist["name"].toString();

    QString quality = json_obj["audioQuality"].toString();
    QString copyright = json_obj["copyright"].toString();

    //qLog(Debug) << "Tidal:" << artist << album << quality << copyright;

    QString artist_album(QString("%1-%2").arg(artist).arg(album));
    if (albums.contains(artist_album)) {
      qLog(Debug) << "Tidal: Skipping duplicate album" << artist << album << quality << copyright;
      continue;
    }
    albums.insert(0, artist_album);

    requests_album_.insert(album_id, album_id);
    GetAlbum(album_id);
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

void TidalService::GetAlbum(const int album_id) {

  QList<Param> parameters;
  QNetworkReply *reply = CreateRequest(QString("albums/%1/tracks").arg(album_id), parameters);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(GetAlbumFinished(QNetworkReply*, int, int)), reply, search_id_, album_id);

}

void TidalService::GetAlbumFinished(QNetworkReply *reply, int search_id, int album_id) {

  reply->deleteLater();

  if (search_id != search_id_) return;
  if (!requests_album_.contains(album_id)) return;
  albums_received_++;
  emit UpdateProgress(albums_received_);

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    CheckFinish();
    return;
  }

  QJsonValue json_value = ExtractItems(data);
  if (!json_value.isArray()) {
    CheckFinish();
    return;
  }
  
  QJsonArray json_items = json_value.toArray();
  if (json_items.isEmpty()) {
    CheckFinish();
    return;
  }

  bool compilation = false;
  bool multidisc = false;
  Song first_song;
  SongList songs;
  for (const QJsonValue &value : json_items) {
    Song song = ParseSong(album_id, value);
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

  CheckFinish();

}

Song TidalService::ParseSong(const int album_id_requested, const QJsonValue &value) {

  Song song;

  if (!value.isObject()) {
    qLog(Error) << "Tidal: Invalid Json reply, track is not a object.";
    qLog(Debug) << value;
    return song;
  }
  QJsonObject json_obj = value.toObject();

  //qLog(Debug) << json_obj;

  if (
      !json_obj.contains("album") ||
      !json_obj.contains("allowStreaming") ||
      !json_obj.contains("artist") ||
      !json_obj.contains("artists") ||
      !json_obj.contains("audioQuality") ||
      !json_obj.contains("duration") ||
      !json_obj.contains("id") ||
      !json_obj.contains("streamReady") ||
      !json_obj.contains("title") ||
      !json_obj.contains("trackNumber") ||
      !json_obj.contains("url") ||
      !json_obj.contains("volumeNumber")
    ) {
    qLog(Error) << "Tidal: Invalid Json reply, track is missing one or more values.";
    qLog(Debug) << json_obj;
    return song;
  }

  QJsonValue json_value_artist = json_obj["artist"];
  QJsonValue json_value_album = json_obj["album"];
  QJsonValue json_duration = json_obj["duration"];
  QJsonArray json_artists = json_obj["artists"].toArray();

  int song_id = json_obj["id"].toInt();

  QString title = json_obj["title"].toString();
  QString urlstr = json_obj["url"].toString();
  int track = json_obj["trackNumber"].toInt();
  int disc = json_obj["volumeNumber"].toInt();
  bool allow_streaming = json_obj["allowStreaming"].toBool();
  bool stream_ready = json_obj["streamReady"].toBool();

  if (!json_value_artist.isObject()) {
    qLog(Error) << "Tidal: Invalid Json reply, track artist is not a object.";
    qLog(Debug) << json_value_artist;
    return song;
  }
  QJsonObject json_artist = json_value_artist.toObject();
  if (!json_artist.contains("name")) {
    qLog(Error) << "Tidal: Invalid Json reply, track artist is missing name.";
    qLog(Debug) << json_artist;
    return song;
  }
  QString artist = json_artist["name"].toString();

  if (!json_value_album.isObject()) {
    qLog(Error) << "Tidal: Invalid Json reply, track album is not a object.";
    qLog(Debug) << json_value_album;
    return song;
  }
  QJsonObject json_album = json_value_album.toObject();
  if (!json_album.contains("id") || !json_album.contains("title") || !json_album.contains("cover")) {
    qLog(Error) << "Tidal: Invalid Json reply, track album is missing id, title or cover.";
    qLog(Debug) << json_album;
    return song;
  }
  int album_id = json_album["id"].toInt();
  if (album_id_requested != 0 && album_id_requested != album_id) {
    qLog(Error) << "Tidal: Invalid Json reply, track album id is wrong.";
    qLog(Debug) << json_album;
    return song;
  }
  QString album = json_album["title"].toString();
  QString cover = json_album["cover"].toString();

  if (!allow_streaming || !stream_ready) {
    qLog(Error) << "Tidal: Skipping song" << artist << album << title << "because allowStreaming is false OR streamReady is false.";
    //qLog(Debug) << json_obj;
    return song;
  }

  //qLog(Debug) << "id" << id << "track" << track << "disc" << disc << "title" << title << "album" << album << "artist" << artist << cover << allow_streaming << url;

  song.set_source(Song::Source_Tidal);
  song.set_id(song_id);
  song.set_album_id(album_id);
  song.set_artist(artist);
  song.set_album(album);
  song.set_title(title);
  song.set_track(track);
  song.set_disc(disc);

  QVariant q_duration = json_duration.toVariant();
  if (q_duration.isValid()) {
    quint64 duration = q_duration.toULongLong() * kNsecPerSec;
    song.set_length_nanosec(duration);
  }

  cover = cover.replace("-", "/");
  QUrl cover_url (QString("%1/images/%2/%3.jpg").arg(kResourcesUrl).arg(cover).arg(coversize_));
  song.set_art_automatic(cover_url.toEncoded());

  QUrl url;
  url.setScheme(url_handler_->scheme());
  url.setPath(QString::number(song_id));
  song.set_url(url);

  song.set_valid(true);

  return song;

}

void TidalService::GetStreamURL(const QUrl &url) {

  stream_request_url_ = url;
  int song_id = url.path().toInt();
  requests_song_.insert(song_id, url);

  QList<Param> parameters;
  parameters << Param("soundQuality", quality_);

  QNetworkReply *reply = CreateRequest(QString("tracks/%1/streamUrl").arg(song_id), parameters);

  NewClosure(reply, SIGNAL(finished()), this, SLOT(GetStreamURLFinished(QNetworkReply*, int, QUrl)), reply, song_id, url);

}

void TidalService::GetStreamURLFinished(QNetworkReply *reply, const int song_id, const QUrl original_url) {

  reply->deleteLater();
  if (requests_song_.contains(song_id)) requests_song_.remove(song_id);
  if (original_url != stream_request_url_) return;

  QByteArray data = GetReplyData(reply, true);
  if (data.isEmpty()) {
    if (!stream_request_url_.isEmpty() && !login_sent_) {
      emit StreamURLFinished(original_url, Song::FileType_Stream);
      stream_request_url_ = QUrl();
    }
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    if (!stream_request_url_.isEmpty() && !login_sent_) {
      emit StreamURLFinished(original_url, Song::FileType_Stream);
      stream_request_url_ = QUrl();
    }
    return;
  }

  if (!json_obj.contains("url") || !json_obj.contains("codec")) {
    qLog(Error) << "Tidal: Invalid Json reply, stream missing url or codec.";
    qLog(Debug) << json_obj;
    emit StreamURLFinished(original_url, Song::FileType_Stream);
    stream_request_url_ = QUrl();
    return;
  }

  stream_request_url_ = QUrl();

  QUrl new_url(json_obj["url"].toString());
  QString codec(json_obj["codec"].toString().toLower());
  Song::FileType filetype(Song::FiletypeByExtension(codec));
  if (filetype == Song::FileType_Unknown) {
    qLog(Debug) << "Tidal: Unknown codec" << codec;
    filetype = Song::FileType_Stream;
  }

  if (new_url.scheme() != streamurl_) new_url.setScheme(streamurl_);

  emit StreamURLFinished(new_url, filetype);

}

void TidalService::CheckFinish() {

  if (search_id_ == 0) return;

  if (!login_sent_ && albums_requested_ <= albums_received_) {
    if (songs_.isEmpty()) {
      if (search_error_.isEmpty()) emit SearchError(search_id_, "Unknown error");
      else emit SearchError(search_id_, search_error_);
    }
    else emit SearchResults(search_id_, songs_);
    ClearSearch();
  }

}

void TidalService::Error(QString error, QVariant debug) {
  qLog(Error) << "Tidal:" << error;
  if (debug.isValid()) qLog(Debug) << debug;
  if (search_id_ != 0) {
    if (!error.isEmpty()) {
      search_error_ += error;
      search_error_ += "<br />";
    }
    CheckFinish();
  }
  if (!stream_request_url_.isEmpty() && !login_sent_) {
    emit StreamURLFinished(stream_request_url_, Song::FileType_Stream);
    stream_request_url_ = QUrl();
  }
}
