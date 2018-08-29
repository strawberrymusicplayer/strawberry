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

const char *TidalService::kServiceName = "Tidal";
const char *TidalService::kApiUrl = "https://listen.tidal.com/v1";
const char *TidalService::kAuthUrl = "https://listen.tidal.com/v1/login/username";
const char *TidalService::kResourcesUrl = "http://resources.tidal.com";
const char *TidalService::kApiToken = "P5Xbeo5LFvESeDy6";

const int TidalService::kSearchDelayMsec = 1500;
const int TidalService::kSearchAlbumsLimit = 40;
const int TidalService::kSearchTracksLimit = 10;

typedef QPair<QString, QString> Param;

TidalService::TidalService(Application *app, InternetModel *parent)
    : InternetService(kServiceName, app, parent, parent),
      network_(new NetworkAccessManager(this)),
      search_delay_(new QTimer(this)),
      pending_search_id_(0),
      next_pending_search_id_(1),
      search_requests_(0),
      login_sent_(false) {

  search_delay_->setInterval(kSearchDelayMsec);
  search_delay_->setSingleShot(true);
  connect(search_delay_, SIGNAL(timeout()), SLOT(StartSearch()));

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

void TidalService::Login(const QString &username, const QString &password) {
  Login(nullptr, username, password);
}

void TidalService::Login(TidalSearchContext *search_ctx, const QString &username, const QString &password) {

  login_sent_ = true;

  int id = 0;
  if (search_ctx) {
    search_ctx->login_sent = true;
    search_ctx->login_attempts++;
    id = search_ctx->id;
  }

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
  NewClosure(reply, SIGNAL(finished()), this, SLOT(HandleAuthReply(QNetworkReply*, int)), reply, id);

}

void TidalService::HandleAuthReply(QNetworkReply *reply, int id) {

  reply->deleteLater();

  login_sent_ = false;

  TidalSearchContext *search_ctx(nullptr);
  if (id != 0 && requests_search_.contains(id)) {
    search_ctx = requests_search_.value(id);
    search_ctx->login_sent = false;
  }

  //int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if (reply->error() != QNetworkReply::NoError) {
    if (reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      QString failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      if (search_ctx) Error(search_ctx, failure_reason);
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
      if (search_ctx) Error(search_ctx, failure_reason);
      emit LoginFailure(failure_reason);
      return;
    }
  }

  QByteArray data(reply->readAll());
  QJsonParseError error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &error);

  if (error.error != QJsonParseError::NoError) {
    QString failure_reason("Authentication reply from server missing Json data.");
    if (search_ctx) Error(search_ctx, failure_reason);
    emit LoginFailure(failure_reason);
    return;
  }

  if (json_doc.isNull() || json_doc.isEmpty()) {
    QString failure_reason("Authentication reply from server has empty Json document.");
    if (search_ctx) Error(search_ctx, failure_reason);
    emit LoginFailure(failure_reason);
    return;
  }

  if (!json_doc.isObject()) {
    QString failure_reason("Authentication reply from server has Json document that is not an object.");
    if (search_ctx) Error(search_ctx, failure_reason);
    emit LoginFailure(failure_reason);
    return;
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    QString failure_reason("Authentication reply from server has empty Json object.");
    if (search_ctx) Error(search_ctx, failure_reason);
    emit LoginFailure(failure_reason);
    return;
  }

  if ( !json_obj.contains("userId") || !json_obj.contains("sessionId") || !json_obj.contains("countryCode") ) {
    QString failure_reason = tr("Authentication reply from server is missing userId, sessionId or countryCode");
    if (search_ctx) Error(search_ctx, failure_reason);
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

  if (search_ctx) {
    qLog(Debug) << "Tidal: Resuming search";
    SendSearch(search_ctx);
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

QJsonObject TidalService::ExtractJsonObj(TidalSearchContext *search_ctx, QNetworkReply *reply) {

  QByteArray data;

  //int http_status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if (reply->error() == QNetworkReply::NoError) {
    data = reply->readAll();
  }
  else {
    if (reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      QString failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      Error(search_ctx, failure_reason);
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
        if (search_ctx->login_attempts < 1 && !username_.isEmpty() && !password_.isEmpty()) {
          qLog(Error) << "Tidal:" << failure_reason;
          qLog(Error) << "Tidal:" << QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
          qLog(Error) << "Tidal:" << "Attempting to login.";
          Login(search_ctx, username_, password_);
        }
        else {
          Error(search_ctx, failure_reason);
        }
      }
      else if (reply->error() == QNetworkReply::ContentNotFoundError) { // Ignore this error
        qLog(Error) << "Tidal:" << failure_reason;
      }
      else { // Fail
        Error(search_ctx, failure_reason);
      }
    }
    return QJsonObject();
  }

  QJsonParseError error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &error);

  if (error.error != QJsonParseError::NoError) {
    Error(search_ctx, "Reply from server missing Json data.");
    return QJsonObject();
  }

  if (json_doc.isNull() || json_doc.isEmpty()) {
    Error(search_ctx, "Received empty Json document.");
    return QJsonObject();
  }

  if (!json_doc.isObject()) {
    Error(search_ctx, "Json document is not an object.");
    return QJsonObject();
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    Error(search_ctx, "Received empty Json object.");
    return QJsonObject();
  }

  //qLog(Debug) << json_obj;

  return json_obj;

}

QJsonArray TidalService::ExtractItems(TidalSearchContext *search_ctx, QNetworkReply *reply) {

  QJsonObject json_obj = ExtractJsonObj(search_ctx, reply);
  if (json_obj.isEmpty()) return QJsonArray();

  if (!json_obj.contains("items")) {
    Error(search_ctx, "Json reply is missing items.");
    return QJsonArray();
  }

  QJsonArray json_items = json_obj["items"].toArray();
  if (json_items.isEmpty()) {
    Error(search_ctx, "No match.");
    return QJsonArray();
  }

  return json_items;

}

int TidalService::Search(const QString &text, TidalSettingsPage::SearchBy searchby) {

  pending_search_id_ = next_pending_search_id_;
  pending_search_ = text;
  pending_searchby_ = searchby;

  next_pending_search_id_++;

  if (text.isEmpty()) {
    search_delay_->stop();
    return pending_search_id_;
  }
  search_delay_->start();

  return pending_search_id_;

}

void TidalService::StartSearch() {

  if (username_.isEmpty() || password_.isEmpty()) {
    emit SearchError(pending_search_id_, "Missing username and/or password.");
    next_pending_search_id_ = 1;
    ShowConfig();
    return;
  }

  TidalSearchContext *search_ctx = CreateSearch(pending_search_id_, pending_search_);
  if (authenticated()) SendSearch(search_ctx);
  else Login(search_ctx, username_, password_);

}

TidalSearchContext *TidalService::CreateSearch(const int search_id, const QString text) {

  TidalSearchContext *search_ctx = new TidalSearchContext;
  search_ctx->id = search_id;
  search_ctx->text = text;
  search_ctx->album_requests = 0;
  search_ctx->song_requests = 0;
  search_ctx->requests_album_.clear();
  search_ctx->requests_song_.clear();
  search_ctx->login_attempts = 0;
  requests_search_.insert(search_id, search_ctx);
  return search_ctx;

}

void TidalService::SendSearch(TidalSearchContext *search_ctx) {

  QList<Param> parameters;
  parameters << Param("query", search_ctx->text);

  QString searchparam;
  switch (pending_searchby_) {
    case TidalSettingsPage::SearchBy_Songs:
      searchparam = "search/tracks";
      parameters << Param("limit", QString::number(kSearchTracksLimit));
      break;
    case TidalSettingsPage::SearchBy_Albums:
    default:
      searchparam = "search/albums";
      parameters << Param("limit", QString::number(kSearchAlbumsLimit));
      break;
  }

  QNetworkReply *reply = CreateRequest(searchparam, parameters);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(SearchFinished(QNetworkReply*, int)), reply, search_ctx->id);

}

void TidalService::SearchFinished(QNetworkReply *reply, int id) {

  reply->deleteLater();

  if (!requests_search_.contains(id)) return;
  TidalSearchContext *search_ctx = requests_search_.value(id);

  QJsonArray json_items = ExtractItems(search_ctx, reply);
  if (json_items.isEmpty()) {
    CheckFinish(search_ctx);
    return;
  }

  //qLog(Debug) << json_items;

  QVector<QString> albums;
  for (const QJsonValue &value : json_items) {
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

    if (search_ctx->requests_album_.contains(album_id)) continue;

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

    search_ctx->requests_album_.insert(album_id, album_id);
    GetAlbum(search_ctx, album_id);
    search_ctx->album_requests++;
    if (search_ctx->album_requests >= kSearchAlbumsLimit) break;
  }

  CheckFinish(search_ctx);

}

void TidalService::GetAlbum(TidalSearchContext *search_ctx, const int album_id) {

  QList<Param> parameters;
  parameters << Param("token", session_id_)
             << Param("soundQuality", quality_);

  QNetworkReply *reply = CreateRequest(QString("albums/%1/tracks").arg(album_id), parameters);

  NewClosure(reply, SIGNAL(finished()), this, SLOT(GetAlbumFinished(QNetworkReply*, int, int)), reply, search_ctx->id, album_id);

}

void TidalService::GetAlbumFinished(QNetworkReply *reply, int search_id, int album_id) {

  reply->deleteLater();

  if (!requests_search_.contains(search_id)) return;
  TidalSearchContext *search_ctx = requests_search_.value(search_id);

  if (!search_ctx->requests_album_.contains(album_id)) return;
  search_ctx->album_requests--;

  QJsonArray json_items = ExtractItems(search_ctx, reply);
  if (json_items.isEmpty()) {
    CheckFinish(search_ctx);
    return;
  }

  bool compilation = false;
  bool multidisc = false;
  Song *first_song(nullptr);
  QList<Song *> songs;
  for (const QJsonValue &value : json_items) {
    Song *song = ParseSong(search_ctx, album_id, value);
    if (!song) continue;
    songs << song;
    if (song->disc() >= 2) multidisc = true;
    if (song->is_compilation() || (first_song && song->artist() != first_song->artist())) compilation = true;
    if (!first_song) first_song = song;
  }
  if (compilation || multidisc) {
    for (Song *song : songs) {
      if (compilation) song->set_compilation_detected(true);
      if (multidisc) {
        QString album_full(QString("%1 - (Disc %2)").arg(song->album()).arg(song->disc()));
        song->set_album(album_full);
      }
    }
  }

  CheckFinish(search_ctx);

}

Song *TidalService::ParseSong(TidalSearchContext *search_ctx, const int album_id, const QJsonValue &value) {

  Song song;

  if (!value.isObject()) {
    qLog(Error) << "Tidal: Invalid Json reply, track is not a object.";
    qLog(Debug) << value;
    return nullptr;
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
    return nullptr;
  }

  QJsonValue json_value_artist = json_obj["artist"];
  QJsonValue json_value_album = json_obj["album"];
  QJsonValue json_duration = json_obj["duration"];
  QJsonArray json_artists = json_obj["artists"].toArray();

  int id = json_obj["id"].toInt();
  QString title = json_obj["title"].toString();
  QString url = json_obj["url"].toString();
  int track = json_obj["trackNumber"].toInt();
  int disc = json_obj["volumeNumber"].toInt();
  bool allow_streaming = json_obj["allowStreaming"].toBool();
  bool stream_ready = json_obj["streamReady"].toBool();

  if (!json_value_artist.isObject()) {
    qLog(Error) << "Tidal: Invalid Json reply, track artist is not a object.";
    qLog(Debug) << json_value_artist;
    return nullptr;
  }
  QJsonObject json_artist = json_value_artist.toObject();
  if (!json_artist.contains("name")) {
    qLog(Error) << "Tidal: Invalid Json reply, track artist is missing name.";
    qLog(Debug) << json_artist;
    return nullptr;
  }
  QString artist = json_artist["name"].toString();

  if (!json_value_album.isObject()) {
    qLog(Error) << "Tidal: Invalid Json reply, track album is not a object.";
    qLog(Debug) << json_value_album;
    return nullptr;
  }
  QJsonObject json_album = json_value_album.toObject();
  if (!json_album.contains("title") || !json_album.contains("cover")) {
    qLog(Error) << "Tidal: Invalid Json reply, track album is missing title or cover.";
    qLog(Debug) << json_album;
    return nullptr;
  }
  QString album = json_album["title"].toString();
  QString cover = json_album["cover"].toString();

  if (!allow_streaming || !stream_ready) {
    qLog(Error) << "Tidal: Skipping song" << artist << album << title << "because allowStreaming is false OR streamReady is false.";
    qLog(Debug) << json_obj;
    return nullptr;
  }

  //qLog(Debug) << "id" << id << "track" << track << "disc" << disc << "title" << title << "album" << album << "artist" << artist << cover << allow_streaming << url;

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

  // Check and see if there is more than 1 artist on the song.
  //int i = 0;
  //for (const QJsonValue &a : json_artists) {
  //i++;
  //qLog(Debug) << a << i;
  //}
  //if (i > 1) song.set_compilation_detected(true);

  cover = cover.replace("-", "/");
  //QUrl cover_url (QString("%1/images/%2/750x750.jpg").arg(kResourcesUrl).arg(cover));
  QUrl cover_url (QString("%1/images/%2/320x320.jpg").arg(kResourcesUrl).arg(cover));
  song.set_art_automatic(cover_url.toEncoded());

  if (search_ctx->requests_song_.contains(id)) return search_ctx->requests_song_.value(id);
  Song *song_new = new Song(song);
  search_ctx->requests_song_.insert(id, song_new);
  search_ctx->song_requests++;
  GetStreamURL(search_ctx, album_id, id);

  return song_new;

}

void TidalService::GetStreamURL(TidalSearchContext *search_ctx, const int album_id, const int song_id) {

  QList<Param> parameters;
  parameters << Param("token", session_id_)
             << Param("soundQuality", quality_);

  QNetworkReply *reply = CreateRequest(QString("tracks/%1/streamUrl").arg(song_id), parameters);

  NewClosure(reply, SIGNAL(finished()), this, SLOT(GetStreamURLFinished(QNetworkReply*, int, int)), reply, search_ctx->id, song_id);

}

void TidalService::GetStreamURLFinished(QNetworkReply *reply, const int search_id, const int song_id) {

  reply->deleteLater();

  if (!requests_search_.contains(search_id)) return;
  TidalSearchContext *search_ctx = requests_search_.value(search_id);

  if (!search_ctx->requests_song_.contains(song_id)) {
    CheckFinish(search_ctx);
    return;
  }
  Song *song = search_ctx->requests_song_.value(song_id);

  search_ctx->song_requests--;

  QJsonObject json_obj = ExtractJsonObj(search_ctx, reply);
  if (json_obj.isEmpty()) {
    delete search_ctx->requests_song_.take(song_id);
    CheckFinish(search_ctx);
    return;
  }

  if (!json_obj.contains("url") || !json_obj.contains("codec")) {
    qLog(Error) << "Tidal: Invalid Json reply, stream missing url or codec.";
    qLog(Debug) << json_obj;
    delete search_ctx->requests_song_.take(song_id);
    CheckFinish(search_ctx);
    return;
  }

  song->set_url(QUrl(json_obj["url"].toString()));
  song->set_valid(true);
  QString codec = json_obj["codec"].toString();
  if (codec == "AAC") song->set_filetype(Song::Type_MP4);
  else qLog(Debug) << "Tidal codec" << codec;

  //qLog(Debug) << song->artist() << song->album() << song->title() << song->url() << song->filetype();

  search_ctx->songs << *song;

  delete search_ctx->requests_song_.take(song_id);

  CheckFinish(search_ctx);

}

void TidalService::CheckFinish(TidalSearchContext *search_ctx) {

  if (!search_ctx->login_sent && search_ctx->album_requests <= 0 && search_ctx->song_requests <= 0) {
    if (search_ctx->songs.isEmpty()) emit SearchError(search_ctx->id, search_ctx->error);
    else emit SearchResults(search_ctx->id, search_ctx->songs);
    delete requests_search_.take(search_ctx->id);
  }

}

void TidalService::Error(TidalSearchContext *search_ctx, QString error, QString debug) {
  qLog(Error) << "Tidal:" << error;
  if (!debug.isEmpty()) qLog(Debug) << debug;
  if (search_ctx) {
    search_ctx->error = error;
    CheckFinish(search_ctx);
  }
}
