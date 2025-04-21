/*
 * Strawberry Music Player
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/database.h"
#include "core/song.h"
#include "core/settings.h"
#include "core/urlhandlers.h"
#include "utilities/randutils.h"
#include "collection/collectionbackend.h"
#include "collection/collectionmodel.h"
#include "subsonicservice.h"
#include "subsonicurlhandler.h"
#include "subsonicrequest.h"
#include "subsonicscrobblerequest.h"
#include "constants/subsonicsettings.h"

using namespace Qt::Literals::StringLiterals;
using std::make_unique;
using std::make_shared;

const Song::Source SubsonicService::kSource = Song::Source::Subsonic;
const char *SubsonicService::kClientName = "Strawberry";
const char *SubsonicService::kApiVersion = "1.11.0";

namespace {
constexpr char kSongsTable[] = "subsonic_songs";
constexpr int kMaxRedirects = 3;
}  // namespace

SubsonicService::SubsonicService(const SharedPtr<TaskManager> task_manager,
                                 const SharedPtr<Database> database,
                                 const SharedPtr<UrlHandlers> url_handlers,
                                 const SharedPtr<AlbumCoverLoader> albumcover_loader,
                                 QObject *parent)
    : StreamingService(Song::Source::Subsonic, u"Subsonic"_s, u"subsonic"_s, QLatin1String(SubsonicSettings::kSettingsGroup), parent),
      url_handler_(new SubsonicUrlHandler(this)),
      collection_backend_(nullptr),
      collection_model_(nullptr),
      http2_(false),
      verify_certificate_(false),
      download_album_covers_(true),
      use_album_id_for_album_covers_(false),
      auth_method_(SubsonicSettings::AuthMethod::MD5),
      ping_redirects_(0) {

  url_handlers->Register(url_handler_);

  collection_backend_ = make_shared<CollectionBackend>();
  collection_backend_->moveToThread(database->thread());
  collection_backend_->Init(database, task_manager, Song::Source::Subsonic, QLatin1String(kSongsTable));
  collection_model_ = new CollectionModel(collection_backend_, albumcover_loader, this);

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

void SubsonicService::ReloadSettings() {

  Settings s;
  s.beginGroup(SubsonicSettings::kSettingsGroup);

  server_url_ = s.value(SubsonicSettings::kUrl).toUrl();
  username_ = s.value(SubsonicSettings::kUsername).toString();
  QByteArray password = s.value(SubsonicSettings::kPassword).toByteArray();
  if (password.isEmpty()) password_.clear();
  else password_ = QString::fromUtf8(QByteArray::fromBase64(password));

  http2_ = s.value(SubsonicSettings::kHTTP2, false).toBool();
  verify_certificate_ = s.value(SubsonicSettings::kVerifyCertificate, false).toBool();
  download_album_covers_ = s.value(SubsonicSettings::kDownloadAlbumCovers, true).toBool();
  use_album_id_for_album_covers_ = s.value(SubsonicSettings::kUseAlbumIdForAlbumCovers, false).toBool();
  auth_method_ = static_cast<SubsonicSettings::AuthMethod>(s.value(SubsonicSettings::kAuthMethod, static_cast<int>(SubsonicSettings::AuthMethod::MD5)).toInt());

  s.endGroup();

}

void SubsonicService::SendPing() {
  SendPingWithCredentials(server_url_, username_, password_, auth_method_, false);
}

void SubsonicService::SendPingWithCredentials(QUrl url, const QString &username, const QString &password, const SubsonicSettings::AuthMethod auth_method, const bool redirect) {

  if (!network_ || !redirect) {
    network_ = make_unique<QNetworkAccessManager>();
    network_->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    ping_redirects_ = 0;
  }

  using Param = QPair<QString, QString>;
  using ParamList = QList<Param>;

  ParamList params = ParamList() << Param(u"c"_s, QLatin1String(kClientName))
                                 << Param(u"v"_s, QLatin1String(kApiVersion))
                                 << Param(u"f"_s, u"json"_s)
                                 << Param(u"u"_s, username);

  if (auth_method == SubsonicSettings::AuthMethod::Hex) {
    params << Param(u"p"_s, u"enc:"_s + QString::fromLatin1(password.toUtf8().toHex()));
  }
  else {
    const QString salt = Utilities::CryptographicRandomString(20);
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(password.toUtf8());
    md5.addData(salt.toUtf8());
    params << Param(u"s"_s, salt);
    params << Param(u"t"_s, QString::fromLatin1(md5.result().toHex()));
  }

  QUrlQuery url_query(url.query());
  for (const Param &param : std::as_const(params)) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  if (!redirect) {
    if (!url.path().isEmpty() && url.path().right(1) == u'/') {
      url.setPath(url.path() + "rest/ping.view"_L1);
    }
    else {
      url.setPath(url.path() + "/rest/ping.view"_L1);
    }
  }

  url.setQuery(url_query);

  QNetworkRequest network_request(url);

  if (url.scheme() == "https"_L1 && !verify_certificate_) {
    QSslConfiguration sslconfig = QSslConfiguration::defaultConfiguration();
    sslconfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    network_request.setSslConfiguration(sslconfig);
  }

  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  network_request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_s);
  network_request.setAttribute(QNetworkRequest::Http2AllowedAttribute, http2_);

  errors_.clear();
  QNetworkReply *reply = network_->get(network_request);
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

void SubsonicService::HandlePingReply(QNetworkReply *reply, const QUrl &url, const QString &username, const QString &password, const SubsonicSettings::AuthMethod auth_method) {

  Q_UNUSED(url);

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      PingError(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
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
      const QByteArray data = reply->readAll();
      QJsonParseError parse_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &parse_error);
      if (parse_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("error"_L1)) {
          QJsonValue json_error = json_obj["error"_L1];
          if (json_error.isObject()) {
            json_obj = json_error.toObject();
            if (!json_obj.isEmpty() && json_obj.contains("code"_L1) && json_obj.contains("message"_L1)) {
              int code = json_obj["code"_L1].toInt();
              QString message = json_obj["message"_L1].toString();
              errors_ << QStringLiteral("%1 (%2)").arg(message).arg(code);
            }
          }
        }
      }
      if (errors_.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
          errors_ << QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
        else {
          errors_ << QStringLiteral("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
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
    PingError(u"Ping reply from server missing Json data."_s);
    return;
  }

  if (json_doc.isEmpty()) {
    PingError(u"Ping reply from server has empty Json document."_s);
    return;
  }

  if (!json_doc.isObject()) {
    PingError(u"Ping reply from server has Json document that is not an object."_s, json_doc);
    return;
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    PingError(u"Ping reply from server has empty Json object."_s, json_doc);
    return;
  }

  if (!json_obj.contains("subsonic-response"_L1)) {
    PingError(u"Ping reply from server is missing subsonic-response"_s, json_obj);
    return;
  }
  QJsonValue value_response = json_obj["subsonic-response"_L1];
  if (!value_response.isObject()) {
    PingError(u"Ping reply from server subsonic-response is not an object"_s, value_response);
    return;
  }
  QJsonObject obj_response = value_response.toObject();

  if (obj_response.contains("error"_L1)) {
    QJsonValue value_error = obj_response["error"_L1];
    if (!value_error.isObject()) {
      PingError(u"Authentication error reply from server is not an object"_s, value_error);
      return;
    }
    QJsonObject obj_error = value_error.toObject();
    if (!obj_error.contains("code"_L1) || !obj_error.contains("message"_L1)) {
      PingError(u"Authentication error reply from server is missing status or message"_s, json_obj);
      return;
    }
    //int status = obj_error["code"].toInt();
    QString message = obj_error["message"_L1].toString();
    Q_EMIT TestComplete(false, message);
    Q_EMIT TestFailure(message);
    return;
  }

  if (!obj_response.contains("status"_L1)) {
    PingError(u"Ping reply from server is missing status"_s, obj_response);
    return;
  }

  QString status = obj_response["status"_L1].toString().toLower();
  QString message = obj_response["message"_L1].toString();

  if (status == "failed"_L1) {
    Q_EMIT TestComplete(false, message);
    Q_EMIT TestFailure(message);
    return;
  }
  if (status == "ok"_L1) {
    Q_EMIT TestComplete(true);
    Q_EMIT TestSuccess();
    return;
  }

  PingError(u"Ping reply status from server is unknown"_s, json_obj);

}

void SubsonicService::CheckConfiguration() {

  if (server_url_.isEmpty()) {
    Q_EMIT TestComplete(false, u"Missing Subsonic server url."_s);
    return;
  }
  if (username_.isEmpty()) {
    Q_EMIT TestComplete(false, u"Missing Subsonic username."_s);
    return;
  }
  if (password_.isEmpty()) {
    Q_EMIT TestComplete(false, u"Missing Subsonic password."_s);
    return;
  }

}

void SubsonicService::Scrobble(const QString &song_id, const bool submission, const QDateTime &time) {

  if (!server_url().isValid() || username().isEmpty() || password().isEmpty()) {
    return;
  }

  if (!scrobble_request_) {
    // We're doing requests every 30-240s the whole time, so keep reusing this instance
    scrobble_request_.reset(new SubsonicScrobbleRequest(this, url_handler_), [](SubsonicScrobbleRequest *request) { request->deleteLater(); });
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
    Q_EMIT SongsResults(SongMap(), tr("Server URL is invalid."));
    return;
  }

  if (username().isEmpty() || password().isEmpty()) {
    Q_EMIT SongsResults(SongMap(), tr("Missing username or password."));
    return;
  }

  ResetSongsRequest();
  songs_request_.reset(new SubsonicRequest(this, url_handler_), [](SubsonicRequest *request) { request->deleteLater(); });
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

  Q_EMIT SongsResults(songs, error);

  ResetSongsRequest();

}

void SubsonicService::PingError(const QString &error, const QVariant &debug) {

  if (!error.isEmpty()) errors_ << error;

  QString error_html;
  for (const QString &e : std::as_const(errors_)) {
    qLog(Error) << "Subsonic:" << e;
    error_html += e + "<br />"_L1;
  }
  if (debug.isValid()) qLog(Debug) << debug;

  Q_EMIT TestFailure(error_html);
  Q_EMIT TestComplete(false, error_html);

  errors_.clear();

}
