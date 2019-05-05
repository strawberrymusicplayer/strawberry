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
const char *TidalService::kApiUrl = "https://api.tidalhifi.com/v1";
const char *TidalService::kAuthUrl = "https://api.tidalhifi.com/v1/login/username";
const char *TidalService::kResourcesUrl = "http://resources.tidal.com";
const char *TidalService::kApiTokenB64 = "UDVYYmVvNUxGdkVTZUR5Ng==";
const int TidalService::kLoginAttempts = 1;
const int TidalService::kTimeResetLoginAttempts = 60000;

TidalService::TidalService(Application *app, QObject *parent)
    : InternetService(Song::Source_Tidal, "Tidal", "tidal", app, parent),
      app_(app),
      network_(new NetworkAccessManager(this)),
      url_handler_(new TidalUrlHandler(app, this)),
      timer_search_delay_(new QTimer(this)),
      timer_login_attempt_(new QTimer(this)),
      search_delay_(1500),
      artistssearchlimit_(1),
      albumssearchlimit_(1),
      songssearchlimit_(1),
      fetchalbums_(false),
      user_id_(0),
      pending_search_id_(0),
      next_pending_search_id_(1),
      search_id_(0),
      artist_search_(false),
      artist_albums_requested_(0),
      artist_albums_received_(0),
      album_songs_requested_(0),
      album_songs_received_(0),
      login_sent_(false),
      login_attempts_(0)
  {

  timer_search_delay_->setSingleShot(true);
  connect(timer_search_delay_, SIGNAL(timeout()), SLOT(StartSearch()));

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
  search_delay_ = s.value("searchdelay", 1500).toInt();
  artistssearchlimit_ = s.value("artistssearchlimit", 5).toInt();
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

  if (search_id_ != 0) emit UpdateStatus(tr("Authenticating..."));

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

  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
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
  for (QUrl url : queue_stream_url_) {
    qLog(Debug) << "Tidal: Resuming get stream url" << url;
    GetStreamURL(url);
  }
  queue_stream_url_.clear();

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

QByteArray TidalService::GetReplyData(QNetworkReply *reply, QString &error, const bool sendlogin) {

  QByteArray data;

  if (reply->error() == QNetworkReply::NoError) {
    data = reply->readAll();
  }
  else {
    if (reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      error = Error(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    }
    else {
      // See if there is Json data containing "userMessage" - then use that instead.
      data = reply->readAll();
      QJsonParseError parse_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &parse_error);
      int status = 0;
      int sub_status = 0;
      QString failure_reason;
      if (parse_error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("status") && json_obj.contains("userMessage")) {
          status = json_obj["status"].toInt();
          sub_status = json_obj["subStatus"].toInt();
          QString user_message = json_obj["userMessage"].toString();
          failure_reason = QString("%1 (%2) (%3)").arg(user_message).arg(status).arg(sub_status);
        }
      }
      if (failure_reason.isEmpty()) {
        failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      }
      if (status == 401 && sub_status == 6001) {  // User does not have a valid session
        Logout();
        if (sendlogin && login_attempts_ < kLoginAttempts && !username_.isEmpty() && !password_.isEmpty()) {
          qLog(Error) << "Tidal:" << failure_reason;
          qLog(Info) << "Tidal:" << "Attempting to login.";
          emit Login();
        }
        else {
          error = Error(failure_reason);
        }
      }
      else { // Fail
        error = Error(failure_reason);
      }
    }
    return QByteArray();
  }

  return data;

}

QJsonObject TidalService::ExtractJsonObj(QByteArray &data, QString &error) {

  QJsonParseError json_error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);

  if (json_error.error != QJsonParseError::NoError) {
    error = Error("Reply from server missing Json data.", data);
    return QJsonObject();
  }

  if (json_doc.isNull() || json_doc.isEmpty()) {
    error = Error("Received empty Json document.", data);
    return QJsonObject();
  }

  if (!json_doc.isObject()) {
    error = Error("Json document is not an object.", json_doc);
    return QJsonObject();
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    error = Error("Received empty Json object.", json_doc);
    return QJsonObject();
  }

  return json_obj;

}

QJsonValue TidalService::ExtractItems(QByteArray &data, QString &error) {

  QJsonObject json_obj = ExtractJsonObj(data, error);
  if (json_obj.isEmpty()) return QJsonValue();
  return ExtractItems(json_obj, error);

}

QJsonValue TidalService::ExtractItems(QJsonObject &json_obj, QString &error) {

  if (!json_obj.contains("items")) {
    error = Error("Json reply is missing items.", json_obj);
    return QJsonArray();
  }
  QJsonValue json_items = json_obj["items"];
  return json_items;

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

  if (username_.isEmpty() || password_.isEmpty()) {
    emit SearchError(pending_search_id_, tr("Missing username and/or password."));
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
  artist_search_ = false;
  artist_albums_requested_ = 0;
  artist_albums_received_ = 0;
  album_songs_requested_ = 0;
  album_songs_received_ = 0;
  requests_artist_albums_.clear();
  requests_album_songs_.clear();
  songs_.clear();

}

void TidalService::SendSearch() {

  emit UpdateStatus(tr("Searching..."));

  switch (pending_search_type_) {
    case InternetSearch::SearchType_Artists:
      SendArtistsSearch();
      break;
    case InternetSearch::SearchType_Albums:
      SendAlbumsSearch();
      break;
    case InternetSearch::SearchType_Songs:
      SendSongsSearch();
      break;
    default:
      Error("Invalid search type.");
      break;
  }

}

void TidalService::SendArtistsSearch() {

  artist_search_ = true;

  QList<Param> parameters;
  parameters << Param("query", search_text_);
  parameters << Param("limit", QString::number(artistssearchlimit_));
  QNetworkReply *reply = CreateRequest("search/artists", parameters);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(ArtistsReceived(QNetworkReply*, int)), reply, search_id_);

}

void TidalService::SendAlbumsSearch() {

  QList<Param> parameters;
  parameters << Param("query", search_text_);
  parameters << Param("limit", QString::number(albumssearchlimit_));
  QNetworkReply *reply = CreateRequest("search/albums", parameters);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(AlbumsReceived(QNetworkReply*, int, int)), reply, search_id_, 0);

}

void TidalService::SendSongsSearch() {

  QList<Param> parameters;
  parameters << Param("query", search_text_);
  parameters << Param("limit", QString::number(songssearchlimit_));
  QNetworkReply *reply = CreateRequest("search/tracks", parameters);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(AlbumsReceived(QNetworkReply*, int, int)), reply, search_id_, 0);

}

void TidalService::ArtistsReceived(QNetworkReply *reply, int search_id) {

  reply->deleteLater();

  if (search_id != search_id_) return;

  QString error;

  QByteArray data = GetReplyData(reply, error, true);
  if (data.isEmpty()) {
    artist_search_ = false;
    CheckFinish();
    return;
  }

  QJsonValue json_value = ExtractItems(data, error);
  if (!json_value.isArray()) {
    artist_search_ = false;
    CheckFinish();
    return;
  }
  QJsonArray json_items = json_value.toArray();
  if (json_items.isEmpty()) {
    artist_search_ = false;
    Error(tr("No match."));
    return;
  }

  for (const QJsonValue &value : json_items) {
    if (!value.isObject()) {
      qLog(Error) << "Tidal: Invalid Json reply, item not a object.";
      qLog(Debug) << value;
      continue;
    }
    QJsonObject json_obj = value.toObject();

    if (!json_obj.contains("id") || !json_obj.contains("name")) {
      qLog(Error) << "Tidal: Invalid Json reply, item missing type or album.";
      qLog(Debug) << json_obj;
      continue;
    }

    int artist_id = json_obj["id"].toInt();
    if (requests_artist_albums_.contains(artist_id)) continue;
    requests_artist_albums_.append(artist_id);
    GetAlbums(artist_id);
    artist_albums_requested_++;
    if (artist_albums_requested_ >= artistssearchlimit_) break;

  }

  if (artist_albums_requested_ > 0) {
    if (artist_albums_requested_ == 1) emit UpdateStatus(tr("Retrieving albums for %1 artist...").arg(artist_albums_requested_));
    else emit UpdateStatus(tr("Retrieving albums for %1 artists...").arg(artist_albums_requested_));
    emit ProgressSetMaximum(artist_albums_requested_);
    emit UpdateProgress(0);
  }

  CheckFinish();

}

void TidalService::GetAlbums(const int artist_id, const int offset) {

  QList<Param> parameters;
  if (offset > 0) parameters << Param("offset", QString::number(offset));
  QNetworkReply *reply = CreateRequest(QString("artists/%1/albums").arg(artist_id), parameters);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(AlbumsReceived(QNetworkReply*, int, int, int)), reply, search_id_, artist_id, offset);

}

void TidalService::AlbumsReceived(QNetworkReply *reply, int search_id, int artist_id, int offset_requested) {

  reply->deleteLater();

  if (search_id != search_id_) return;

  if (artist_search_) {
    if (!requests_artist_albums_.contains(artist_id)) return;
    artist_albums_received_++;
    emit UpdateProgress(artist_albums_received_);
  }

  QString error;

  QByteArray data = GetReplyData(reply, error, (artist_id == 0));
  if (data.isEmpty()) {
    AlbumsFinished(artist_id, offset_requested);
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data, error);
  if (json_obj.isEmpty()) {
    AlbumsFinished(artist_id, offset_requested);
    return;
  }

  int limit = 0;
  int total_albums = 0;
  if (artist_search_) {  // This was a list of albums by artist
    if (!json_obj.contains("limit") ||
        !json_obj.contains("offset") ||
        !json_obj.contains("totalNumberOfItems") ||
        !json_obj.contains("items")) {
      AlbumsFinished(artist_id, offset_requested);
      Error("Json object missing values.", json_obj);
      return;
    }
    limit = json_obj["limit"].toInt();
    int offset = json_obj["offset"].toInt();
    total_albums = json_obj["totalNumberOfItems"].toInt();
    if (offset != offset_requested) {
      AlbumsFinished(artist_id, offset_requested, total_albums, limit);
      Error(QString("Offset returned does not match offset requested! %1 != %2").arg(offset).arg(offset_requested));
      return;
    }
  }

  QJsonValue json_value = ExtractItems(json_obj, error);
  if (!json_value.isArray()) {
    AlbumsFinished(artist_id, offset_requested, total_albums, limit);
    return;
  }
  QJsonArray json_items = json_value.toArray();
  if (json_items.isEmpty()) {
    if (!artist_search_) Error("No match.");
    AlbumsFinished(artist_id, offset_requested, total_albums, limit);
    return;
  }

  int albums = 0;
  for (const QJsonValue &value : json_items) {
     albums++;
    if (!value.isObject()) {
      qLog(Error) << "Tidal: Invalid Json reply, item not a object.";
      qLog(Debug) << value;
      continue;
    }
    QJsonObject json_obj = value.toObject();

    int album_id = 0;
    QString album;
    if (json_obj.contains("type")) {  // This was a albums search
      if (!json_obj.contains("id") || !json_obj.contains("title")) {
        qLog(Error) << "Tidal: Invalid Json reply, item is missing ID or title.";
        qLog(Debug) << json_obj;
        continue;
      }
      album_id = json_obj["id"].toInt();
      album = json_obj["title"].toString();
    }
    else if (json_obj.contains("album")) {  // This was a tracks search
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

    if (requests_album_songs_.contains(album_id)) continue;

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

    requests_album_songs_.insert(album_id, artist);
    album_songs_requested_++;
    if (album_songs_requested_ >= albumssearchlimit_) break;
  }

  AlbumsFinished(artist_id, offset_requested, total_albums, limit, albums);

}

void TidalService::AlbumsFinished(const int artist_id, const int offset_requested, const int total_albums, const int limit, const int albums) {

  if (artist_search_) {  // This is a artist search.
    if (albums > limit) {
      Error("Albums returned does not match limit returned!");
    }
    int offset_next = offset_requested + albums;
    if (album_songs_requested_ < albumssearchlimit_ && offset_next < total_albums) {
      GetAlbums(artist_id, offset_next);
      artist_albums_requested_++;
    }
    else if (artist_albums_received_ >= artist_albums_requested_) {  // Artist search is finished.
      artist_search_ = false;
    }
  }

  if (!artist_search_) {
    // Get songs for the albums.
    QHashIterator<int, QString> i(requests_album_songs_);
    while (i.hasNext()) {
      i.next();
      GetSongs(i.key());
    }

    if (album_songs_requested_ > 0) {
      if (album_songs_requested_ == 1) emit UpdateStatus(tr("Retrieving songs for %1 album...").arg(album_songs_requested_));
      else emit UpdateStatus(tr("Retrieving songs for %1 albums...").arg(album_songs_requested_));
      emit ProgressSetMaximum(album_songs_requested_);
      emit UpdateProgress(0);
    }
  }

  CheckFinish();

}

void TidalService::GetSongs(const int album_id) {

  QList<Param> parameters;
  QNetworkReply *reply = CreateRequest(QString("albums/%1/tracks").arg(album_id), parameters);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(SongsReceived(QNetworkReply*, int, int)), reply, search_id_, album_id);

}

void TidalService::SongsReceived(QNetworkReply *reply, int search_id, int album_id) {

  reply->deleteLater();

  if (search_id != search_id_) return;
  if (!requests_album_songs_.contains(album_id)) return;
  QString album_artist = requests_album_songs_[album_id];

  album_songs_received_++;
  if (!artist_search_) {
    emit UpdateProgress(album_songs_received_);
  }

  QString error;

  QByteArray data = GetReplyData(reply, error, false);
  if (data.isEmpty()) {
    CheckFinish();
    return;
  }

  QJsonValue json_value = ExtractItems(data, error);
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
  SongList songs;
  for (const QJsonValue &value : json_items) {
    Song song = ParseSong(album_id, value, album_artist);
    if (!song.is_valid()) continue;
    if (song.disc() >= 2) multidisc = true;
    if (song.is_compilation()) compilation = true;
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

Song TidalService::ParseSong(const int album_id_requested, const QJsonValue &value, QString album_artist) {

  if (!value.isObject()) {
    qLog(Error) << "Tidal: Invalid Json reply, track is not a object.";
    qLog(Debug) << value;
    return Song();
  }
  QJsonObject json_obj = value.toObject();

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
      !json_obj.contains("volumeNumber") ||
      !json_obj.contains("copyright")
    ) {
    qLog(Error) << "Tidal: Invalid Json reply, track is missing one or more values.";
    qLog(Debug) << json_obj;
    return Song();
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
  QString copyright = json_obj["copyright"].toString();

  if (!json_value_artist.isObject()) {
    qLog(Error) << "Tidal: Invalid Json reply, track artist is not a object.";
    qLog(Debug) << json_value_artist;
    return Song();
  }
  QJsonObject json_artist = json_value_artist.toObject();
  if (!json_artist.contains("name")) {
    qLog(Error) << "Tidal: Invalid Json reply, track artist is missing name.";
    qLog(Debug) << json_artist;
    return Song();
  }
  QString artist = json_artist["name"].toString();

  if (!json_value_album.isObject()) {
    qLog(Error) << "Tidal: Invalid Json reply, track album is not a object.";
    qLog(Debug) << json_value_album;
    return Song();
  }
  QJsonObject json_album = json_value_album.toObject();
  if (!json_album.contains("id") || !json_album.contains("title") || !json_album.contains("cover")) {
    qLog(Error) << "Tidal: Invalid Json reply, track album is missing id, title or cover.";
    qLog(Debug) << json_album;
    return Song();
  }
  int album_id = json_album["id"].toInt();
  if (album_id_requested != 0 && album_id_requested != album_id) {
    qLog(Error) << "Tidal: Invalid Json reply, track album id is wrong.";
    qLog(Debug) << json_album;
    return Song();
  }
  QString album = json_album["title"].toString();
  QString cover = json_album["cover"].toString();

  if (!allow_streaming) {
    qLog(Error) << "Tidal: Song" << artist << album << title << "is not allowStreaming";
  }

  if (!stream_ready) {
    qLog(Error) << "Tidal: Song" << artist << album << title << "is not streamReady.";
  }

  //qLog(Debug) << "id" << id << "track" << track << "disc" << disc << "title" << title << "album" << album << "artist" << artist << cover << allow_streaming << url;

  title.remove(Song::kTitleRemoveMisc);

  Song song;
  song.set_source(Song::Source_Tidal);
  song.set_id(song_id);
  song.set_album_id(album_id);
  if (album_artist != artist) song.set_albumartist(album_artist);
  song.set_album(album);
  song.set_artist(artist);
  song.set_title(title);
  song.set_track(track);
  song.set_disc(disc);
  song.set_comment(copyright);

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

  if (login_sent_) {
    queue_stream_url_ << url;
    return;
  }

  int song_id = url.path().toInt();
  requests_stream_url_.insert(song_id, url);

  QList<Param> parameters;
  parameters << Param("soundQuality", quality_);

  QNetworkReply *reply = CreateRequest(QString("tracks/%1/streamUrl").arg(song_id), parameters);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(StreamURLReceived(QNetworkReply*, int, QUrl)), reply, song_id, url);

}

void TidalService::StreamURLReceived(QNetworkReply *reply, const int song_id, const QUrl original_url) {

  reply->deleteLater();

  if (!requests_stream_url_.contains(song_id)) return;
  requests_stream_url_.remove(song_id);

  QString error;

  QByteArray data = GetReplyData(reply, error, true);
  if (data.isEmpty()) {
    if (login_sent_) {
      queue_stream_url_ << original_url;
      return;
    }
    emit StreamURLFinished(original_url, original_url, Song::FileType_Stream, error);
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data, error);
  if (json_obj.isEmpty()) {
    emit StreamURLFinished(original_url, original_url, Song::FileType_Stream, error);
    return;
  }

  if (!json_obj.contains("url") || !json_obj.contains("codec")) {
    error = Error("Invalid Json reply, stream missing url or codec.", json_obj);
    emit StreamURLFinished(original_url, original_url, Song::FileType_Stream, error);
    return;
  }

  QUrl new_url(json_obj["url"].toString());
  QString codec(json_obj["codec"].toString().toLower());
  Song::FileType filetype(Song::FiletypeByExtension(codec));
  if (filetype == Song::FileType_Unknown) {
    qLog(Debug) << "Tidal: Unknown codec" << codec;
    filetype = Song::FileType_Stream;
  }

  if (new_url.scheme() != streamurl_) new_url.setScheme(streamurl_);

  emit StreamURLFinished(original_url, new_url, filetype);

}

void TidalService::CheckFinish() {

  if (search_id_ == 0) return;

  if (!login_sent_ && !artist_search_ && artist_albums_requested_ <= artist_albums_received_ && album_songs_requested_ <= album_songs_received_) {
    if (songs_.isEmpty()) {
      if (search_error_.isEmpty()) emit SearchError(search_id_, "Unknown error");
      else emit SearchError(search_id_, search_error_);
    }
    else emit SearchResults(search_id_, songs_);
    ClearSearch();
  }

}

QString TidalService::LoginError(QString error, QVariant debug) {

  emit LoginFailure(error);

  for (QUrl url : queue_stream_url_) {
    emit StreamURLFinished(url, url, Song::FileType_Stream, error);
  }
  queue_stream_url_.clear();

  Error(error);

  return error;

}

QString TidalService::Error(QString error, QVariant debug) {

  qLog(Error) << "Tidal:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

  if (search_id_ != 0) {
    if (!error.isEmpty()) {
      search_error_ += error;
      search_error_ += "<br />";
    }
    CheckFinish();
  }

  return error;

}
