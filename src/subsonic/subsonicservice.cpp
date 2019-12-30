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

#include <memory>

#include <QObject>
#include <QByteArray>
#include <QPair>
#include <QList>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslError>
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
#include "internet/internetsearch.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "subsonicservice.h"
#include "subsonicurlhandler.h"
#include "subsonicrequest.h"
#include "settings/subsonicsettingspage.h"

using std::shared_ptr;

const Song::Source SubsonicService::kSource = Song::Source_Subsonic;
const char *SubsonicService::kClientName = "Strawberry";
const char *SubsonicService::kApiVersion = "1.11.0";
const char *SubsonicService::kSongsTable = "subsonic_songs";
const char *SubsonicService::kSongsFtsTable = "subsonic_songs_fts";
const int SubsonicService::kMaxRedirects = 3;

SubsonicService::SubsonicService(Application *app, QObject *parent)
    : InternetService(Song::Source_Subsonic, "Subsonic", "subsonic", app, parent),
      app_(app),
      network_(new QNetworkAccessManager),
      url_handler_(new SubsonicUrlHandler(app, this)),
      collection_backend_(nullptr),
      collection_model_(nullptr),
      collection_sort_model_(new QSortFilterProxyModel(this)),
      verify_certificate_(false),
      download_album_covers_(true),
      ping_redirects_(0)
  {

  app->player()->RegisterUrlHandler(url_handler_);

  // Backend

  collection_backend_ = new CollectionBackend();
  collection_backend_->moveToThread(app_->database()->thread());
  collection_backend_->Init(app_->database(), Song::Source_Subsonic, kSongsTable, QString(), QString(), kSongsFtsTable);

  // Model

  collection_model_ = new CollectionModel(collection_backend_, app_, this);
  collection_sort_model_->setSourceModel(collection_model_);
  collection_sort_model_->setSortRole(CollectionModel::Role_SortText);
  collection_sort_model_->setDynamicSortFilter(true);
  collection_sort_model_->setSortLocaleAware(true);
  collection_sort_model_->sort(0);

  ReloadSettings();

}

SubsonicService::~SubsonicService() {
  collection_backend_->deleteLater();
}

void SubsonicService::Exit() {

  connect(collection_backend_, SIGNAL(ExitFinished()), this, SIGNAL(ExitFinished()));
  collection_backend_->ExitAsync();

}

void SubsonicService::ShowConfig() {
  app_->OpenSettingsDialogAtPage(SettingsDialog::Page_Subsonic);
}

void SubsonicService::ReloadSettings() {

  QSettings s;
  s.beginGroup(SubsonicSettingsPage::kSettingsGroup);

  server_url_ = s.value("url").toUrl();
  username_ = s.value("username").toString();
  QByteArray password = s.value("password").toByteArray();
  if (password.isEmpty()) password_.clear();
  else password_ = QString::fromUtf8(QByteArray::fromBase64(password));

  verify_certificate_ = s.value("verifycertificate", false).toBool();
  download_album_covers_ = s.value("downloadalbumcovers", true).toBool();

  s.endGroup();

}

void SubsonicService::SendPing() {
  SendPing(server_url_, username_, password_);
}

void SubsonicService::SendPing(QUrl url, const QString &username, const QString &password, const bool redirect) {

  if (!redirect) {
    network_.reset(new QNetworkAccessManager);
    ping_redirects_ = 0;
  }

  const ParamList params = ParamList() << Param("c", kClientName)
                                       << Param("v", kApiVersion)
                                       << Param("f", "json")
                                       << Param("u", username)
                                       << Param("p", QString("enc:" + password.toUtf8().toHex()));

  QUrlQuery url_query(url.query());
  for (const Param &param : params) {
    EncodedParam encoded_param(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
    if (!url_query.hasQueryItem(encoded_param.first)) {
      url_query.addQueryItem(encoded_param.first, encoded_param.second);
    }
  }

  if (!redirect) {
    if (!url.path().isEmpty() && url.path().right(1) == "/") {
      url.setPath(url.path() + QString("rest/ping.view"));
    }
    else
      url.setPath(url.path() + QString("/rest/ping.view"));
  }

  url.setQuery(url_query);

  QNetworkRequest req(url);

  if (url.scheme() == "https" && !verify_certificate_) {
    QSslConfiguration sslconfig = QSslConfiguration::defaultConfiguration();
    sslconfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    req.setSslConfiguration(sslconfig);
  }

  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

  errors_.clear();
  QNetworkReply *reply = network_->get(req);
  connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(HandlePingSSLErrors(QList<QSslError>)));
  NewClosure(reply, SIGNAL(finished()), this, SLOT(HandlePingReply(QNetworkReply*, QUrl, QString, QString)), reply, url, username, password);

  //qLog(Debug) << "Subsonic: Sending request" << url << query;

}

void SubsonicService::HandlePingSSLErrors(QList<QSslError> ssl_errors) {

  for (QSslError &ssl_error : ssl_errors) {
    errors_ += ssl_error.errorString();
  }

}

void SubsonicService::HandlePingReply(QNetworkReply *reply, const QUrl &url, const QString &username, const QString &password) {

  Q_UNUSED(url);

  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      PingError(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
      return;
    }
    else {

      // Check for a valid redirect first.
      if (
          (
          reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 301 ||
          reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 302 ||
          reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 307
          )
          &&
          ping_redirects_ <= kMaxRedirects
      )
      {
        QUrl redirect_url = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        if (!redirect_url.isEmpty()) {
          ++ping_redirects_;
          qLog(Debug) << "Redirecting ping request to" << redirect_url.toString(QUrl::RemoveQuery);
          SendPing(redirect_url, username, password, true);
          return;
        }
      }

      // See if there is Json data containing "error" - then use that instead.
      QByteArray data = reply->readAll();
      QJsonParseError parse_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &parse_error);
      if (parse_error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("error")) {
          QJsonValue json_error = json_obj["error"];
          if (json_error.isObject()) {
            json_obj = json_error.toObject();
            if (!json_obj.isEmpty() && json_obj.contains("code") && json_obj.contains("message")) {
              int code = json_obj["code"].toInt();
              QString message = json_obj["message"].toString();
              errors_ << QString("%1 (%2)").arg(message).arg(code);
            }
          }
        }
      }
      if (errors_.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
          errors_ << QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
        else {
          errors_ << QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        }
      }
      PingError();
      return;
    }
  }

  errors_.clear();

  QByteArray data(reply->readAll());

  QJsonParseError json_error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);

  if (json_error.error != QJsonParseError::NoError) {
    PingError("Ping reply from server missing Json data.");
    return;
  }

  if (json_doc.isNull() || json_doc.isEmpty()) {
    PingError("Ping reply from server has empty Json document.");
    return;
  }

  if (!json_doc.isObject()) {
    PingError("Ping reply from server has Json document that is not an object.", json_doc);
    return;
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    PingError("Ping reply from server has empty Json object.", json_doc);
    return;
  }

  if (!json_obj.contains("subsonic-response")) {
    PingError("Ping reply from server is missing subsonic-response", json_obj);
    return;
  }
  QJsonValue json_response = json_obj["subsonic-response"];
  if (!json_response.isObject()) {
    PingError("Ping reply from server subsonic-response is not an object", json_response);
    return;
  }
  
  json_obj = json_response.toObject();
  
  if (json_obj.contains("error")) {
    QJsonValue json_error = json_obj["error"];
    if (!json_error.isObject()) {
      PingError("Authentication error reply from server is not an object", json_response);
      return;
    }
    json_obj = json_error.toObject();
    if (!json_obj.contains("code") || !json_obj.contains("message")) {
      PingError("Authentication error reply from server is missing status or message", json_obj);
      return;
    }
    //int status = json_obj["code"].toInt();
    QString message = json_obj["message"].toString();
    emit TestComplete(false, message);
    emit TestFailure(message);
    return;
  }

  if (!json_obj.contains("status")) {
    PingError("Ping reply from server is missing status", json_obj);
    return;
  }

  QString status = json_obj["status"].toString().toLower();
  QString message = json_obj["message"].toString();

  if (status == "failed") {
    emit TestComplete(false, message);
    emit TestFailure(message);
    return;
  }
  else if (status == "ok") {
    emit TestComplete(true);
    emit TestSuccess();
    return;
  }
  else {
    PingError("Ping reply status from server is unknown", json_obj);
    return;
  }

}

void SubsonicService::CheckConfiguration() {

  if (server_url_.isEmpty()) {
    emit TestComplete(false, "Missing Subsonic server url.");
    return;
  }
  if (username_.isEmpty()) {
    emit TestComplete(false, "Missing Subsonic username.");
    return;
  }
  if (password_.isEmpty()) {
    emit TestComplete(false, "Missing Subsonic password.");
    return;
  }

}

void SubsonicService::ResetSongsRequest() {

  if (songs_request_.get()) {  // WARNING: Don't disconnect everything. NewClosure() relies on destroyed()!!!
    disconnect(songs_request_.get(), 0, this, 0);
    disconnect(this, 0, songs_request_.get(), 0);
    songs_request_.reset();
  }

}

void SubsonicService::GetSongs() {

  if (!server_url().isValid()) {
    emit SongsResults(SongList(), tr("Server URL is invalid."));
    return;
  }

  if (username().isEmpty() || password().isEmpty()) {
    emit SongsResults(SongList(), tr("Missing username or password."));
    return;
  }

  ResetSongsRequest();
  songs_request_.reset(new SubsonicRequest(this, url_handler_, app_, this));
  connect(songs_request_.get(), SIGNAL(Results(const SongList&, const QString&)), SLOT(SongsResultsReceived(const SongList&, const QString&)));
  connect(songs_request_.get(), SIGNAL(UpdateStatus(const QString&)), SIGNAL(SongsUpdateStatus(const QString&)));
  connect(songs_request_.get(), SIGNAL(ProgressSetMaximum(const int)), SIGNAL(SongsProgressSetMaximum(const int)));
  connect(songs_request_.get(), SIGNAL(UpdateProgress(const int)), SIGNAL(SongsUpdateProgress(const int)));

  songs_request_->GetAlbums();

}

void SubsonicService::SongsResultsReceived(const SongList &songs, const QString &error) {

  emit SongsResults(songs, error);

}

void SubsonicService::PingError(const QString &error, const QVariant &debug) {

  if (!error.isEmpty()) errors_ << error;

  QString error_html;
  for (const QString &error : errors_) {
    qLog(Error) << "Subsonic:" << error;
    error_html += error + "<br />";
  }
  if (debug.isValid()) qLog(Debug) << debug;

  emit TestFailure(error_html);
  emit TestComplete(false, error_html);

  errors_.clear();

}
