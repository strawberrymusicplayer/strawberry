/*
 * Strawberry Music Player
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QObject>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QVariant>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QSslError>
#include <QCryptographicHash>
#include <QJsonValue>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSortFilterProxyModel>

#include "core/logging.h"
#include "core/shared_ptr.h"
#include "core/application.h"
#include "core/player.h"
#include "core/database.h"
#include "core/song.h"
#include "utilities/randutils.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "subsonicservice.h"
#include "subsonicurlhandler.h"
#include "subsonicrequest.h"
#include "subsonicscrobblerequest.h"
#include "settings/settingsdialog.h"
#include "settings/subsonicsettingspage.h"

using std::make_unique;
using std::make_shared;

const Song::Source SubsonicService::kSource = Song::Source::Subsonic;
const char *SubsonicService::kClientName = "Strawberry";
const char *SubsonicService::kApiVersion = "1.11.0";
const char *SubsonicService::kSongsTable = "subsonic_songs";
const char *SubsonicService::kSongsFtsTable = "subsonic_songs_fts";
const int SubsonicService::kMaxRedirects = 3;

SubsonicService::SubsonicService(Application *app, QObject *parent)
    : InternetService(Song::Source::Subsonic, "Subsonic", "subsonic", SubsonicSettingsPage::kSettingsGroup, SettingsDialog::Page::Subsonic, app, parent),
      app_(app),
      url_handler_(new SubsonicUrlHandler(app, this)),
      collection_backend_(nullptr),
      collection_model_(nullptr),
      collection_sort_model_(new QSortFilterProxyModel(this)),
      http2_(false),
      verify_certificate_(false),
      download_album_covers_(true),
      auth_method_(SubsonicSettingsPage::AuthMethod::MD5),
      ping_redirects_(0) {

  app->player()->RegisterUrlHandler(url_handler_);

  // Backend

  collection_backend_ = make_shared<CollectionBackend>();
  collection_backend_->moveToThread(app_->database()->thread());
  collection_backend_->Init(app_->database(), app->task_manager(), Song::Source::Subsonic, kSongsTable, kSongsFtsTable);

  // Model

  collection_model_ = new CollectionModel(collection_backend_, app_, this);
  collection_sort_model_->setSourceModel(collection_model_);
  collection_sort_model_->setSortRole(CollectionModel::Role_SortText);
  collection_sort_model_->setDynamicSortFilter(true);
  collection_sort_model_->setSortLocaleAware(true);
  collection_sort_model_->sort(0);

  SubsonicService::ReloadSettings();

}

SubsonicService::~SubsonicService() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

}

void SubsonicService::Exit() {

  QObject::connect(&*collection_backend_, &CollectionBackend::ExitFinished, this, &SubsonicService::ExitFinished);
  collection_backend_->ExitAsync();

}

void SubsonicService::ShowConfig() {
  app_->OpenSettingsDialogAtPage(SettingsDialog::Page::Subsonic);
}

void SubsonicService::ReloadSettings() {

  QSettings s;
  s.beginGroup(SubsonicSettingsPage::kSettingsGroup);

  server_url_ = s.value("url").toUrl();
  username_ = s.value("username").toString();
  QByteArray password = s.value("password").toByteArray();
  if (password.isEmpty()) password_.clear();
  else password_ = QString::fromUtf8(QByteArray::fromBase64(password));

  http2_ = s.value("http2", false).toBool();
  verify_certificate_ = s.value("verifycertificate", false).toBool();
  download_album_covers_ = s.value("downloadalbumcovers", true).toBool();
  auth_method_ = static_cast<SubsonicSettingsPage::AuthMethod>(s.value("authmethod", static_cast<int>(SubsonicSettingsPage::AuthMethod::MD5)).toInt());

  s.endGroup();

}

void SubsonicService::SendPing() {
  SendPingWithCredentials(server_url_, username_, password_, auth_method_, false);
}

void SubsonicService::SendPingWithCredentials(QUrl url, const QString &username, const QString &password, const SubsonicSettingsPage::AuthMethod auth_method, const bool redirect) {

  if (!network_ || !redirect) {
    network_ = make_unique<QNetworkAccessManager>();
    network_->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    ping_redirects_ = 0;
  }

  using Param = QPair<QString, QString>;
  using ParamList = QList<Param>;

  ParamList params = ParamList() << Param("c", kClientName)
                                 << Param("v", kApiVersion)
                                 << Param("f", "json")
                                 << Param("u", username);

  if (auth_method == SubsonicSettingsPage::AuthMethod::Hex) {
    params << Param("p", QString("enc:" + password.toUtf8().toHex()));
  }
  else {
    const QString salt = Utilities::CryptographicRandomString(20);
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(password.toUtf8());
    md5.addData(salt.toUtf8());
    params << Param("s", salt);
    params << Param("t", md5.result().toHex());
  }

  QUrlQuery url_query(url.query());
  for (const Param &param : params) {
    url_query.addQueryItem(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
  }

  if (!redirect) {
    if (!url.path().isEmpty() && url.path().right(1) == "/") {
      url.setPath(url.path() + QString("rest/ping.view"));
    }
    else {
      url.setPath(url.path() + QString("/rest/ping.view"));
    }
  }

  url.setQuery(url_query);

  QNetworkRequest req(url);

  if (url.scheme() == "https" && !verify_certificate_) {
    QSslConfiguration sslconfig = QSslConfiguration::defaultConfiguration();
    sslconfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    req.setSslConfiguration(sslconfig);
  }

  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  req.setAttribute(QNetworkRequest::Http2AllowedAttribute, http2_);
#endif

  errors_.clear();
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &SubsonicService::HandlePingSSLErrors);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, url, username, password, auth_method]() { HandlePingReply(reply, url, username, password, auth_method); });

  //qLog(Debug) << "Subsonic: Sending request" << url << url.query();

}

void SubsonicService::HandlePingSSLErrors(const QList<QSslError> &ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    errors_ += ssl_error.errorString();
  }

}

void SubsonicService::HandlePingReply(QNetworkReply *reply, const QUrl &url, const QString &username, const QString &password, const SubsonicSettingsPage::AuthMethod auth_method) {

  Q_UNUSED(url);

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
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
          SendPingWithCredentials(redirect_url, username, password, auth_method, true);
          return;
        }
      }

      // See if there is Json data containing "error" - then use that instead.
      QByteArray data = reply->readAll();
      QJsonParseError parse_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &parse_error);
      if (parse_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
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

  if (json_doc.isEmpty()) {
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
  QJsonValue value_response = json_obj["subsonic-response"];
  if (!value_response.isObject()) {
    PingError("Ping reply from server subsonic-response is not an object", value_response);
    return;
  }
  QJsonObject obj_response = value_response.toObject();

  if (obj_response.contains("error")) {
    QJsonValue value_error = obj_response["error"];
    if (!value_error.isObject()) {
      PingError("Authentication error reply from server is not an object", value_error);
      return;
    }
    QJsonObject obj_error = value_error.toObject();
    if (!obj_error.contains("code") || !obj_error.contains("message")) {
      PingError("Authentication error reply from server is missing status or message", json_obj);
      return;
    }
    //int status = obj_error["code"].toInt();
    QString message = obj_error["message"].toString();
    emit TestComplete(false, message);
    emit TestFailure(message);
    return;
  }

  if (!obj_response.contains("status")) {
    PingError("Ping reply from server is missing status", obj_response);
    return;
  }

  QString status = obj_response["status"].toString().toLower();
  QString message = obj_response["message"].toString();

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

void SubsonicService::Scrobble(const QString &song_id, const bool submission, const QDateTime &time) {

  if (!server_url().isValid() || username().isEmpty() || password().isEmpty()) {
    return;
  }

  if (!scrobble_request_) {
    // We're doing requests every 30-240s the whole time, so keep reusing this instance
    scrobble_request_.reset(new SubsonicScrobbleRequest(this, url_handler_, app_), [](SubsonicScrobbleRequest *request) { request->deleteLater(); });
  }

  scrobble_request_->CreateScrobbleRequest(song_id, submission, time);

}

void SubsonicService::ResetSongsRequest() {

  if (songs_request_) {
    QObject::disconnect(&*songs_request_, nullptr, this, nullptr);
    QObject::disconnect(this, nullptr, &*songs_request_, nullptr);
    songs_request_.reset();
  }

}

void SubsonicService::GetSongs() {

  if (!server_url().isValid()) {
    emit SongsResults(SongMap(), tr("Server URL is invalid."));
    return;
  }

  if (username().isEmpty() || password().isEmpty()) {
    emit SongsResults(SongMap(), tr("Missing username or password."));
    return;
  }

  ResetSongsRequest();
  songs_request_.reset(new SubsonicRequest(this, url_handler_, app_), [](SubsonicRequest *request) { request->deleteLater(); });
  QObject::connect(&*songs_request_, &SubsonicRequest::Results, this, &SubsonicService::SongsResultsReceived);
  QObject::connect(&*songs_request_, &SubsonicRequest::UpdateStatus, this, &SubsonicService::SongsUpdateStatus);
  QObject::connect(&*songs_request_, &SubsonicRequest::ProgressSetMaximum, this, &SubsonicService::SongsProgressSetMaximum);
  QObject::connect(&*songs_request_, &SubsonicRequest::UpdateProgress, this, &SubsonicService::SongsUpdateProgress);

  songs_request_->GetAlbums();

}

void SubsonicService::DeleteSongs() {

  collection_backend_->DeleteAllAsync();

}

void SubsonicService::SongsResultsReceived(const SongMap &songs, const QString &error) {

  emit SongsResults(songs, error);

  ResetSongsRequest();

}

void SubsonicService::PingError(const QString &error, const QVariant &debug) {

  if (!error.isEmpty()) errors_ << error;

  QString error_html;
  for (const QString &e : errors_) {
    qLog(Error) << "Subsonic:" << e;
    error_html += e + "<br />";
  }
  if (debug.isValid()) qLog(Debug) << debug;

  emit TestFailure(error_html);
  emit TestComplete(false, error_html);

  errors_.clear();

}
