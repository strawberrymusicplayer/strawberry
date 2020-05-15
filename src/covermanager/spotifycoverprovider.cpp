/*
 * Strawberry Music Player
 * Copyright 2020, Jonas Kvinge <jonas@jkvinge.net>
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

#include <algorithm>

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslError>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QDesktopServices>
#include <QMessageBox>
#include <QtDebug>

#include "core/application.h"
#include "core/network.h"
#include "core/logging.h"
#include "core/song.h"
#include "core/utilities.h"
#include "core/timeconstants.h"
#include "internet/localredirectserver.h"
#include "albumcoverfetcher.h"
#include "jsoncoverprovider.h"
#include "spotifycoverprovider.h"

const char *SpotifyCoverProvider::kSettingsGroup = "Spotify";
const char *SpotifyCoverProvider::kOAuthAuthorizeUrl = "https://accounts.spotify.com/authorize";
const char *SpotifyCoverProvider::kOAuthAccessTokenUrl = "https://accounts.spotify.com/api/token";
const char *SpotifyCoverProvider::kOAuthRedirectUrl = "http://localhost:63111/";
const char *SpotifyCoverProvider::kClientIDB64 = "ZTZjY2Y2OTQ5NzY1NGE3NThjOTAxNWViYzdiMWQzMTc=";
const char *SpotifyCoverProvider::kClientSecretB64 = "N2ZlMDMxODk1NTBlNDE3ZGI1ZWQ1MzE3ZGZlZmU2MTE=";
const char *SpotifyCoverProvider::kApiUrl = "https://api.spotify.com/v1";
const int SpotifyCoverProvider::kLimit = 10;

SpotifyCoverProvider::SpotifyCoverProvider(Application *app, QObject *parent) : JsonCoverProvider("Spotify", true, true, 2.5, true, true, app, parent), network_(new NetworkAccessManager(this)), server_(nullptr), expires_in_(0), login_time_(0) {

  refresh_login_timer_.setSingleShot(true);
  connect(&refresh_login_timer_, SIGNAL(timeout()), SLOT(RequestAccessToken()));

  QSettings s;
  s.beginGroup(kSettingsGroup);
  access_token_ = s.value("access_token").toString();
  refresh_token_ = s.value("refresh_token").toString();
  expires_in_ = s.value("expires_in").toLongLong();
  login_time_ = s.value("login_time").toLongLong();
  s.endGroup();

  if (!refresh_token_.isEmpty()) {
    qint64 time = expires_in_ - (QDateTime::currentDateTime().toTime_t() - login_time_);
    if (time < 6) time = 6;
    refresh_login_timer_.setInterval(time * kMsecPerSec);
    refresh_login_timer_.start();
  }

}

SpotifyCoverProvider::~SpotifyCoverProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

void SpotifyCoverProvider::Authenticate() {

  QUrl redirect_url(kOAuthRedirectUrl);

  if (!server_) {
    server_ = new LocalRedirectServer(this);
    server_->set_https(false);
    int port = redirect_url.port();
    int port_max = port + 10;
    bool success = false;
    forever {
      server_->set_port(port);
      if (server_->Listen()) { success = true; break; }
      ++port;
      if (port > port_max) break;
    }
    if (!success) {
      AuthError(server_->error());
      server_->deleteLater();
      server_ = nullptr;
      return;
    }
    connect(server_, SIGNAL(Finished()), this, SLOT(RedirectArrived()));
  }

  code_verifier_ = Utilities::CryptographicRandomString(44);
  code_challenge_ = QString(QCryptographicHash::hash(code_verifier_.toUtf8(), QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding));
  if (code_challenge_.lastIndexOf(QChar('=')) == code_challenge_.length() - 1) {
    code_challenge_.chop(1);
  }

  const ParamList params = ParamList() << Param("client_id", QByteArray::fromBase64(kClientIDB64))
                                       << Param("response_type", "code") 
                                       << Param("redirect_uri", redirect_url.toString())
                                       << Param("state", code_challenge_);
                                       

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
  }

  QUrl url(kOAuthAuthorizeUrl);
  url.setQuery(url_query);

  const bool result = QDesktopServices::openUrl(url);
  if (!result) {
    QMessageBox messagebox(QMessageBox::Information, tr("Spotify Authentication"), tr("Please open this URL in your browser") + QString(":<br /><a href=\"%1\">%1</a>").arg(url.toString()), QMessageBox::Ok);
    messagebox.setTextFormat(Qt::RichText);
    messagebox.exec();
  }

}

void SpotifyCoverProvider::Deauthenticate() {

  access_token_.clear();
  refresh_token_.clear();
  expires_in_ = 0;
  login_time_ = 0;

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.remove("access_token");
  s.remove("refresh_token");
  s.remove("expires_in");
  s.remove("login_time");
  s.endGroup();

  refresh_login_timer_.stop();

}

void SpotifyCoverProvider::RedirectArrived() {

  if (!server_) return;

  if (server_->error().isEmpty()) {
    QUrl url = server_->request_url();
    if (url.isValid()) {
      QUrlQuery url_query(url);
      if (url_query.hasQueryItem("error")) {
        AuthError(QUrlQuery(url).queryItemValue("error"));
      }
      else if (url_query.hasQueryItem("code") && url_query.hasQueryItem("state")) {
        qLog(Debug) << "Spotify: Authorization URL Received" << url;
        QString code = url_query.queryItemValue("code");
        QString state = url_query.queryItemValue("state");
        QUrl redirect_url(kOAuthRedirectUrl);
        redirect_url.setPort(server_->url().port());
        RequestAccessToken(code, redirect_url);
      }
      else {
        AuthError(tr("Redirect missing token code or state!"));
      }
    }
    else {
      AuthError(tr("Received invalid reply from web browser."));
    }
  }
  else {
    AuthError(server_->error());
  }

  server_->close();
  server_->deleteLater();
  server_ = nullptr;

}

void SpotifyCoverProvider::RequestAccessToken(const QString code, const QUrl redirect_url) {

  refresh_login_timer_.stop();

  ParamList params = ParamList() << Param("client_id", QByteArray::fromBase64(kClientIDB64))
                                 << Param("client_secret", QByteArray::fromBase64(kClientSecretB64));

  if (!code.isEmpty() && !redirect_url.isEmpty()) {
    params << Param("grant_type", "authorization_code");
    params << Param("code", code);
    params << Param("redirect_uri", redirect_url.toString());
  }
  else if (!refresh_token_.isEmpty() && is_enabled()) {
    params << Param("grant_type", "refresh_token");
    params << Param("refresh_token", refresh_token_);
  }
  else {
    return;
  }

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
  }

  QUrl new_url(kOAuthAccessTokenUrl);
  QNetworkRequest req = QNetworkRequest(new_url);
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  QString auth_header_data = QByteArray::fromBase64(kClientIDB64) + QString(":") + QByteArray::fromBase64(kClientSecretB64);
  req.setRawHeader("Authorization", "Basic " + auth_header_data.toUtf8().toBase64());

  QByteArray query = url_query.toString(QUrl::FullyEncoded).toUtf8();

  QNetworkReply *reply = network_->post(req, query);
  replies_ << reply;
  connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(HandleLoginSSLErrors(QList<QSslError>)));
  connect(reply, &QNetworkReply::finished, [=] { AccessTokenRequestFinished(reply); });

}

void SpotifyCoverProvider::HandleLoginSSLErrors(QList<QSslError> ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    login_errors_ += ssl_error.errorString();
  }

}

void SpotifyCoverProvider::AccessTokenRequestFinished(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      AuthError(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
      return;
    }
    else {
      // See if there is Json data containing "error" and "error_description" then use that instead.
      QByteArray data = reply->readAll();
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      if (json_error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("error") && json_obj.contains("error_description")) {
          QString error = json_obj["error"].toString();
          QString error_description = json_obj["error_description"].toString();
          login_errors_ << QString("Authentication failure: %1 (%2)").arg(error).arg(error_description);
        }
      }
      if (login_errors_.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
          login_errors_ << QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
        else {
          login_errors_ << QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        }
      }
      AuthError();
      return;
    }
  }

  QByteArray data = reply->readAll();

  QJsonParseError json_error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);

  if (json_error.error != QJsonParseError::NoError) {
    Error(QString("Failed to parse Json data in authentication reply: %1").arg(json_error.errorString()));
    return;
  }

  if (json_doc.isEmpty()) {
    AuthError("Authentication reply from server has empty Json document.");
    return;
  }

  if (!json_doc.isObject()) {
    AuthError("Authentication reply from server has Json document that is not an object.", json_doc);
    return;
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    AuthError("Authentication reply from server has empty Json object.", json_doc);
    return;
  }

  if (!json_obj.contains("access_token") || !json_obj.contains("expires_in")) {
    AuthError("Authentication reply from server is missing access token or expires in.", json_obj);
    return;
  }

  access_token_ = json_obj["access_token"].toString();
  if (json_obj.contains("refresh_token")) {
    refresh_token_ = json_obj["refresh_token"].toString();
  }
  expires_in_ = json_obj["expires_in"].toInt();
  login_time_ = QDateTime::currentDateTime().toTime_t();

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("access_token", access_token_);
  s.setValue("refresh_token", refresh_token_);
  s.setValue("expires_in", expires_in_);
  s.setValue("login_time", login_time_);
  s.endGroup();

  if (expires_in_ > 0) {
    refresh_login_timer_.setInterval(expires_in_ * kMsecPerSec);
    refresh_login_timer_.start();
  }

  qLog(Debug) << "Spotify: Authentication was successful, got access token" << access_token_ << "expires in" << expires_in_;

  emit AuthenticationComplete(true);
  emit AuthenticationSuccess();

}

bool SpotifyCoverProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  if (access_token_.isEmpty()) return false;

  QString type;
  QString query;
  QString extract;
  if (album.isEmpty()) {
    type = "track";
    query = artist + " " + title;
    extract = "tracks";
  }
  else {
    type = "album";
    query = artist + " " + album;
    extract = "albums";
  }

  ParamList params = ParamList() << Param("q", query)
                                 << Param("type", type)
                                 << Param("limit", QString::number(kLimit));

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
  }

  QUrl url(kApiUrl + QString("/search"));
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  req.setRawHeader("Authorization", "Bearer " + access_token_.toUtf8());

  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  connect(reply, &QNetworkReply::finished, [=] { HandleSearchReply(reply, id, extract); });

  return true;

}

void SpotifyCoverProvider::CancelSearch(const int id) { Q_UNUSED(id); }

QByteArray SpotifyCoverProvider::GetReplyData(QNetworkReply *reply) {

  QByteArray data;

  if (reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
    data = reply->readAll();
  }
  else {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      Error(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    }
    else {
      data = reply->readAll();
      QJsonParseError parse_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &parse_error);
      QString error;
      if (parse_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("error") && json_obj["error"].isObject()) {
          QJsonObject obj_error = json_obj["error"].toObject();
          if (obj_error.contains("status") && obj_error.contains("message")) {
            int status = obj_error["status"].toInt();
            QString message = obj_error["message"].toString();
            error = QString("%1 (%2)").arg(message).arg(status);
            if (status == 401) access_token_.clear();
          }
        }
      }
      if (error.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
          if (reply->error() == 204) access_token_.clear();
          error = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
        else {
          error = QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        }
      }
      Error(error);
    }
    return QByteArray();
  }

  return data;

}

void SpotifyCoverProvider::HandleSearchReply(QNetworkReply *reply, const int id, const QString &extract) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    emit SearchFinished(id, CoverSearchResults());
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    emit SearchFinished(id, CoverSearchResults());
    return;
  }

  if (!json_obj.contains(extract) || !json_obj[extract].isObject()) {
    Error(QString("Json object is missing %1 object.").arg(extract), json_obj);
    emit SearchFinished(id, CoverSearchResults());
    return;
  }
  json_obj = json_obj[extract].toObject();

  if (!json_obj.contains("items") || !json_obj["items"].isArray()) {
    Error(QString("%1 object is missing items array.").arg(extract), json_obj);
    emit SearchFinished(id, CoverSearchResults());
    return;
  }

  QJsonArray array_items = json_obj["items"].toArray();
  if (array_items.isEmpty()) {
    emit SearchFinished(id, CoverSearchResults());
    return;
  }

  CoverSearchResults results;
  for (const QJsonValue &value_item : array_items) {

    if (!value_item.isObject()) {
      continue;
    }
    QJsonObject obj_item = value_item.toObject();

    QJsonObject obj_album = obj_item;
    if (obj_item.contains("album") && obj_item["album"].isObject()) {
      obj_album = obj_item["album"].toObject();
    }

    if (!obj_album.contains("artists") || !obj_album.contains("name") || !obj_album.contains("images") || !obj_album["artists"].isArray() || !obj_album["images"].isArray()) {
      continue;
    }
    QJsonArray array_artists = obj_album["artists"].toArray();
    QJsonArray array_images = obj_album["images"].toArray();
    QString album = obj_album["name"].toString();

    QStringList artists;
    for (const QJsonValue &value_artist : array_artists) {
      if (!value_artist.isObject()) continue;
      QJsonObject obj_artist = value_artist.toObject();
      if (!obj_artist.contains("name")) continue;
      artists << obj_artist["name"].toString();
    }

    for (const QJsonValue &value_image : array_images) {
      if (!value_image.isObject()) continue;
      QJsonObject obj_image = value_image.toObject();
      if (!obj_image.contains("url") || !obj_image.contains("width") || !obj_image.contains("height")) continue;
      int width = obj_image["width"].toInt();
      int height = obj_image["height"].toInt();
      if (width < 300 || height < 300) continue;
      QUrl url(obj_image["url"].toString());
      CoverSearchResult result;
      result.album = album;
      result.image_url = url;
      if (!artists.isEmpty()) result.artist = artists.first();
      results << result;
    }

  }
  emit SearchFinished(id, results);

}

void SpotifyCoverProvider::AuthError(const QString &error, const QVariant &debug) {

  if (!error.isEmpty()) login_errors_ << error;

  for (const QString &e : login_errors_) Error(e);
  if (debug.isValid()) qLog(Debug) << debug;

  emit AuthenticationFailure(login_errors_);
  emit AuthenticationComplete(false, login_errors_);

  login_errors_.clear();

}

void SpotifyCoverProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Spotify:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
