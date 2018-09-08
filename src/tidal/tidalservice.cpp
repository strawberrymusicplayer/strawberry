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
#include "core/closure.h"
#include "core/logging.h"
#include "core/mergedproxymodel.h"
#include "core/network.h"
#include "core/song.h"
#include "core/iconloader.h"
#include "core/taskmanager.h"
#include "core/timeconstants.h"
#include "core/utilities.h"
#include "internet/internetmodel.h"
#include "tidalservice.h"
#include "tidalsearch.h"
#include "settings/tidalsettingspage.h"

const Song::Source TidalService::kSource = Song::Source_Tidal;
const char *TidalService::kServiceName = "Tidal";
const char *TidalService::kApiUrl = "https://listen.tidal.com/v1";
const char *TidalService::kAuthUrl = "https://listen.tidal.com/v1/login/username";
const char *TidalService::kResourcesUrl = "http://resources.tidal.com";
const char *TidalService::kApiToken = "P5Xbeo5LFvESeDy6";

typedef QPair<QString, QString> Param;

TidalService::TidalService(Application *app, InternetModel *parent)
    : InternetService(kSource, kServiceName, app, parent, parent),
      network_(new NetworkAccessManager(this)),
      timer_searchdelay_(new QTimer(this)),
      searchdelay_(1500),
      albumssearchlimit_(1),
      songssearchlimit_(1),
      fetchalbums_(false),
      pending_search_id_(0),
      next_pending_search_id_(1),
      login_sent_(false)
  {

  timer_searchdelay_->setSingleShot(true);
  connect(timer_searchdelay_, SIGNAL(timeout()), SLOT(StartSearch()));

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
  password_ = s.value("password").toString();
  quality_ = s.value("quality").toString();
  searchdelay_ = s.value("searchdelay", 1500).toInt();
  albumssearchlimit_ = s.value("albumssearchlimit", 40).toInt();
  songssearchlimit_ = s.value("songssearchlimit", 10).toInt();
  fetchalbums_ = s.value("fetchalbums", false).toBool();
  coversize_ = s.value("coversize", "320x320").toString();
  s.endGroup();

}

void TidalService::LoadSessionID() {

  QSettings s;
  s.beginGroup(TidalSettingsPage::kSettingsGroup);
  if (!s.contains("user_id") ||!s.contains("session_id") || !s.contains("country_code")) return;
  session_id_ = s.value("session_id").toString();
  user_id_ = s.value("user_id").toInt();
  country_code_ = s.value("country_code").toString();
  s.endGroup();

}

void TidalService::Login(const QString &username, const QString &password, int search_id) {

  if (search_id != 0) emit UpdateStatus("Authenticating...");

  login_sent_ = true;
  login_attempts_++;

  typedef QPair<QString, QString> Arg;
  typedef QList<Arg> ArgList;

  typedef QPair<QByteArray, QByteArray> EncodedArg;
  typedef QList<EncodedArg> EncodedArgList;

  ArgList args = ArgList() <<Arg("token", kApiToken) << Arg("username", username) << Arg("password", password) << Arg("clientVersion", "2.2.1--7");

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
  QNetworkReply *reply = network_->post(req, url_query.toString(QUrl::FullyEncoded).toUtf8());
  NewClosure(reply, SIGNAL(finished()), this, SLOT(HandleAuthReply(QNetworkReply*, int)), reply, search_id);

}

void TidalService::HandleAuthReply(QNetworkReply *reply, int search_id) {

  reply->deleteLater();

  login_sent_ = false;

  //int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if (reply->error() != QNetworkReply::NoError) {
    if (reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      QString failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      if (search_id != 0) Error(failure_reason);
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
      if (search_id != 0) Error(failure_reason);
      emit LoginFailure(failure_reason);
      return;
    }
  }

  QByteArray data(reply->readAll());
  QJsonParseError error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &error);

  if (error.error != QJsonParseError::NoError) {
    QString failure_reason("Authentication reply from server missing Json data.");
    if (search_id != 0) Error(failure_reason);
    emit LoginFailure(failure_reason);
    return;
  }

  if (json_doc.isNull() || json_doc.isEmpty()) {
    QString failure_reason("Authentication reply from server has empty Json document.");
    if (search_id != 0) Error(failure_reason);
    emit LoginFailure(failure_reason);
    return;
  }

  if (!json_doc.isObject()) {
    QString failure_reason("Authentication reply from server has Json document that is not an object.");
    if (search_id != 0) Error(failure_reason);
    emit LoginFailure(failure_reason);
    return;
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    QString failure_reason("Authentication reply from server has empty Json object.");
    if (search_id != 0) Error(failure_reason);
    emit LoginFailure(failure_reason);
    return;
  }

  if ( !json_obj.contains("userId") || !json_obj.contains("sessionId") || !json_obj.contains("countryCode") ) {
    QString failure_reason = tr("Authentication reply from server is missing userId, sessionId or countryCode");
    if (search_id != 0) Error(failure_reason);
    emit LoginFailure(failure_reason);
    return;
  }

  country_code_ = json_obj["countryCode"].toString();
  session_id_ = json_obj["sessionId"].toString();
  user_id_ = json_obj["userId"].toInt();

  QSettings s;
  s.beginGroup(TidalSettingsPage::kSettingsGroup);
  s.setValue("user_id", user_id_);
  s.setValue("session_id", session_id_);
  s.setValue("country_code", country_code_);
  s.endGroup();

  qLog(Debug) << "Tidal: Login successful" << "user id" << user_id_ << "session id" << session_id_ << "country code" << country_code_;

  if (search_id != 0) {
    qLog(Debug) << "Tidal: Resuming search";
    SendSearch();
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
  QNetworkReply *reply = network_->get(req);

  //qLog(Debug) << "Tidal: Sending request" << url;

  return reply;

}

QJsonObject TidalService::ExtractJsonObj(QNetworkReply *reply) {

  QByteArray data;

  //int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
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
        if (login_attempts_ < 1 && !username_.isEmpty() && !password_.isEmpty()) {
          qLog(Error) << "Tidal:" << failure_reason;
          qLog(Error) << "Tidal:" << QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
          qLog(Error) << "Tidal:" << "Attempting to login.";
          Login(username_, password_);
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
    return QJsonObject();
  }

  QJsonParseError error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &error);

  if (error.error != QJsonParseError::NoError) {
    Error("Reply from server missing Json data.");
    return QJsonObject();
  }

  if (json_doc.isNull() || json_doc.isEmpty()) {
    Error("Received empty Json document.");
    return QJsonObject();
  }

  if (!json_doc.isObject()) {
    Error("Json document is not an object.");
    return QJsonObject();
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    Error("Received empty Json object.");
    return QJsonObject();
  }

  //qLog(Debug) << json_obj;

  return json_obj;

}

QJsonArray TidalService::ExtractItems(QNetworkReply *reply) {

  QJsonObject json_obj = ExtractJsonObj(reply);
  if (json_obj.isEmpty()) return QJsonArray();

  if (!json_obj.contains("items")) {
    Error("Json reply is missing items.");
    return QJsonArray();
  }

  QJsonArray json_items = json_obj["items"].toArray();
  if (json_items.isEmpty()) {
    Error("No match.");
    return QJsonArray();
  }

  return json_items;

}

int TidalService::Search(const QString &text, TidalSettingsPage::SearchBy searchby) {

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
  else Login(username_, password_);

}

void TidalService::CancelSearch() {
  ClearSearch();
}
void TidalService::ClearSearch() {
  search_id_ = 0;
  search_text_ = QString();
  search_error_ = QString();
  albums_requested_ = 0;
  songs_requested_ = 0;
  albums_received_ = 0;
  songs_received_ = 0;
  requests_album_.clear();
  requests_song_.clear();
  login_attempts_ = 0;
  songs_.clear();
}

void TidalService::SendSearch() {

  emit UpdateStatus("Searching...");

  QList<Param> parameters;
  parameters << Param("query", search_text_);

  QString searchparam;
  switch (pending_searchby_) {
    case TidalSettingsPage::SearchBy_Songs:
      searchparam = "search/tracks";
      parameters << Param("limit", QString::number(songssearchlimit_));
      break;
    case TidalSettingsPage::SearchBy_Albums:
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

  QJsonArray json_items = ExtractItems(reply);
  if (json_items.isEmpty()) {
    CheckFinish();
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
        requests_song_.insert(song.id(), song);
        songs_requested_++;
        GetStreamURL(0, song.id());
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

    //qLog(Debug) << "Tidal:" << artist << album << quality;

    QString artist_album(QString("%1-%2").arg(artist).arg(album));
    if (albums.contains(artist_album)) {
      qLog(Debug) << "Tidal: Skipping duplicate album" << artist << album << quality;
      continue;
    }
    albums.insert(0, artist_album);

    requests_album_.insert(album_id, album_id);
    GetAlbum(album_id);
    albums_requested_++;
    if (albums_requested_ >= albumssearchlimit_) break;
  }

  if (albums_requested_ > 0) {
    emit UpdateStatus(QString("Retriving %1 album%2...").arg(albums_requested_).arg(albums_requested_ == 1 ? "" : "s"));
    emit ProgressSetMaximum(albums_requested_);
    emit UpdateProgress(0);
  }
  else if (songs_requested_ > 0) {
    emit UpdateStatus(QString("Retriving %1 song%2...").arg(songs_requested_).arg(songs_requested_ == 1 ? "" : "s"));
    emit ProgressSetMaximum(songs_requested_);
    emit UpdateProgress(songs_received_);
  }

  CheckFinish();

}

void TidalService::GetAlbum(const int album_id) {

  QList<Param> parameters;
  parameters << Param("token", session_id_)
             << Param("soundQuality", quality_);

  QNetworkReply *reply = CreateRequest(QString("albums/%1/tracks").arg(album_id), parameters);

  NewClosure(reply, SIGNAL(finished()), this, SLOT(GetAlbumFinished(QNetworkReply*, int, int)), reply, search_id_, album_id);

}

void TidalService::GetAlbumFinished(QNetworkReply *reply, int search_id, int album_id) {

  reply->deleteLater();

  if (search_id != search_id_) return;
  if (!requests_album_.contains(album_id)) return;
  albums_received_++;
  emit UpdateProgress(albums_received_);

  QJsonArray json_items = ExtractItems(reply);
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
    requests_song_.insert(song.id(), song);
    songs_requested_++;
    GetStreamURL(album_id, song.id());
  }

  if (albums_requested_ <= albums_received_) {
    emit UpdateStatus(QString("Retriving %1 song%2...").arg(songs_requested_).arg(songs_requested_ == 1 ? "" : "s"));
    emit ProgressSetMaximum(songs_requested_);
    emit UpdateProgress(songs_received_);
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
  if (requests_song_.contains(song_id)) return requests_song_.value(song_id);

  QString title = json_obj["title"].toString();
  QString url = json_obj["url"].toString();
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
  song.set_bitrate(0);
  song.set_samplerate(0);
  song.set_bitdepth(0);

  QVariant q_duration = json_duration.toVariant();
  if (q_duration.isValid()) {
    quint64 duration = q_duration.toULongLong() * kNsecPerSec;
    song.set_length_nanosec(duration);
  }

  cover = cover.replace("-", "/");
  QUrl cover_url (QString("%1/images/%2/%3.jpg").arg(kResourcesUrl).arg(cover).arg(coversize_));
  song.set_art_automatic(cover_url.toEncoded());
  song.set_valid(true);

  return song;

}

void TidalService::GetStreamURL(const int album_id, const int song_id) {

  QList<Param> parameters;
  parameters << Param("token", session_id_)
             << Param("soundQuality", quality_);

  QNetworkReply *reply = CreateRequest(QString("tracks/%1/streamUrl").arg(song_id), parameters);

  NewClosure(reply, SIGNAL(finished()), this, SLOT(GetStreamURLFinished(QNetworkReply*, int, int)), reply, search_id_, song_id);

}

void TidalService::GetStreamURLFinished(QNetworkReply *reply, const int search_id, const int song_id) {

  reply->deleteLater();

  if (search_id != search_id_) return;

  if (!requests_song_.contains(song_id)) {
    CheckFinish();
    return;
  }
  Song song = requests_song_.value(song_id);
  songs_received_++;

  if (albums_requested_ <= albums_received_) {
    emit UpdateProgress(songs_received_);
  }

  QJsonObject json_obj = ExtractJsonObj(reply);
  if (json_obj.isEmpty()) {
    requests_song_.remove(song_id);
    CheckFinish();
    return;
  }

  if (!json_obj.contains("url") || !json_obj.contains("codec")) {
    qLog(Error) << "Tidal: Invalid Json reply, stream missing url or codec.";
    qLog(Debug) << json_obj;
    requests_song_.remove(song_id);
    CheckFinish();
    return;
  }

  song.set_url(QUrl(json_obj["url"].toString()));

  QString codec = json_obj["codec"].toString().toLower();
  song.set_filetype(Song::FiletypeByExtension(codec));
  if (song.filetype() == Song::FileType_Unknown) {
    qLog(Debug) << "Tidal: Unknown codec" << codec;
    song.set_filetype(Song::FileType_Stream);
  }

  song.set_valid(true);

  //qLog(Debug) << song.artist() << song.album() << song.title() << song.url() << song.filetype();

  songs_ << song;

  requests_song_.remove(song_id);

  CheckFinish();

}

void TidalService::CheckFinish() {

  if (!login_sent_ && albums_requested_ <= albums_received_ && songs_requested_ <= songs_received_) {
    if (songs_.isEmpty()) emit SearchError(search_id_, search_error_);
    else emit SearchResults(search_id_, songs_);
    ClearSearch();
  }

}

void TidalService::Error(QString error, QString debug) {
  qLog(Error) << "Tidal:" << error;
  if (!debug.isEmpty()) qLog(Debug) << debug;
  search_error_ = error;
  CheckFinish();
}
