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
#include <QPair>
#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QtDebug>

#include "core/application.h"
#include "core/network.h"
#include "core/logging.h"
#include "core/song.h"
#include "core/utilities.h"
#include "dialogs/userpassdialog.h"
#include "albumcoverfetcher.h"
#include "jsoncoverprovider.h"
#include "qobuzcoverprovider.h"

const char *QobuzCoverProvider::kSettingsGroup = "Qobuz";
const char *QobuzCoverProvider::kAuthUrl = "https://www.qobuz.com/api.json/0.2/user/login";
const char *QobuzCoverProvider::kApiUrl = "https://www.qobuz.com/api.json/0.2";
const char *QobuzCoverProvider::kAppID = "OTQyODUyNTY3";
const int QobuzCoverProvider::kLimit = 10;

QobuzCoverProvider::QobuzCoverProvider(Application *app, QObject *parent) : JsonCoverProvider("Qobuz", true, true, 2.0, true, true, app, parent), network_(new NetworkAccessManager(this)) {

  QSettings s;
  s.beginGroup(kSettingsGroup);
  username_ = s.value("username").toString();
  QByteArray password = s.value("password").toByteArray();
  if (password.isEmpty()) password_.clear();
  else password_ = QString::fromUtf8(QByteArray::fromBase64(password));
  user_auth_token_ = s.value("user_auth_token").toString();
  user_id_ = s.value("user_id").toLongLong();
  credential_id_ = s.value("credential_id").toLongLong();
  device_id_ = s.value("device_id").toString();
  s.endGroup();

}

QobuzCoverProvider::~QobuzCoverProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

void QobuzCoverProvider::Authenticate() {

  login_errors_.clear();

  if (username_.isEmpty() || password_.isEmpty()) {
    UserPassDialog dialog;
    if (dialog.exec() == QDialog::Rejected || dialog.username().isEmpty() || dialog.password().isEmpty()) {
      AuthError(tr("Missing username and password."));
      return;
    }
    username_ = dialog.username();
    password_ = dialog.password();
  }

  const ParamList params = ParamList() << Param("app_id", QString::fromUtf8(QByteArray::fromBase64(kAppID)))
                                       << Param("username", username_)
                                       << Param("password", password_)
                                       << Param("device_manufacturer_id", Utilities::MacAddress());

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
  }

  QUrl url(kAuthUrl);
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

  QByteArray query = url_query.toString(QUrl::FullyEncoded).toUtf8();
  QNetworkReply *reply = network_->post(req, query);
  replies_ << reply;
  connect(reply, SIGNAL(sslErrors(QList<QSslError>)), this, SLOT(HandleLoginSSLErrors(QList<QSslError>)));
  connect(reply, &QNetworkReply::finished, [=] { HandleAuthReply(reply); });

}

void QobuzCoverProvider::HandleAuthReply(QNetworkReply *reply) {

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
      // See if there is Json data containing "status", "code" and "message" - then use that instead.
      QByteArray data(reply->readAll());
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      if (json_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("status") && json_obj.contains("code") && json_obj.contains("message")) {
          QString status = json_obj["status"].toString();
          int code = json_obj["code"].toInt();
          QString message = json_obj["message"].toString();
          login_errors_ << QString("%1 (%2)").arg(message).arg(code);
          if (code == 401) {
            username_.clear();
            password_.clear();
          }
        }
      }
      if (login_errors_.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
          login_errors_ << QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
          if(reply->error() == QNetworkReply::AuthenticationRequiredError) {
            username_.clear();
            password_.clear();
          }
        }
        else {
          login_errors_ << QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        }
      }
      AuthError();
      return;
    }
  }

  login_errors_.clear();

  QByteArray data = reply->readAll();
  QJsonParseError json_error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);

  if (json_error.error != QJsonParseError::NoError) {
    AuthError("Authentication reply from server missing Json data.");
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

  if (!json_obj.contains("user_auth_token")) {
    AuthError("Authentication reply from server is missing user_auth_token", json_obj);
    return;
  }
  user_auth_token_ = json_obj["user_auth_token"].toString();

  if (!json_obj.contains("user")) {
    AuthError("Authentication reply from server is missing user", json_obj);
    return;
  }
  QJsonValue json_user = json_obj["user"];
  if (!json_user.isObject()) {
    AuthError("Authentication reply user is not a object", json_obj);
    return;
  }
  QJsonObject json_obj_user = json_user.toObject();

  if (!json_obj_user.contains("id")) {
    AuthError("Authentication reply from server is missing user id", json_obj_user);
    return;
  }
  user_id_ = json_obj_user["id"].toVariant().toLongLong();

  if (!json_obj_user.contains("device")) {
    AuthError("Authentication reply from server is missing user device", json_obj_user);
    return;
  }
  QJsonValue json_device = json_obj_user["device"];
  if (!json_device.isObject()) {
    AuthError("Authentication reply from server user device is not a object", json_device);
    return;
  }
  QJsonObject json_obj_device = json_device.toObject();

  if (!json_obj_device.contains("device_manufacturer_id")) {
    AuthError("Authentication reply from server device is missing device_manufacturer_id", json_obj_device);
    return;
  }
  device_id_ = json_obj_device["device_manufacturer_id"].toString();

  if (!json_obj_user.contains("credential")) {
    AuthError("Authentication reply from server is missing user credential", json_obj_user);
    return;
  }
  QJsonValue json_credential = json_obj_user["credential"];
  if (!json_credential.isObject()) {
    AuthError("Authentication reply from serve userr credential is not a object", json_device);
    return;
  }
  QJsonObject json_obj_credential = json_credential.toObject();

  if (!json_obj_credential.contains("id")) {
    AuthError("Authentication reply user credential from server is missing user credential id", json_obj_device);
    return;
  }
  //credential_id_ = json_obj_credential["id"].toInt();

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("username", username_);
  s.setValue("password", password_.toUtf8().toBase64());
  s.setValue("user_auth_token", user_auth_token_);
  s.setValue("user_id", user_id_);
  s.setValue("credential_id", credential_id_);
  s.setValue("device_id", device_id_);
  s.endGroup();

  qLog(Debug) << "Qobuz: Login successful" << "user id" << user_id_ << "user auth token" << user_auth_token_ << "device id" << device_id_;

  emit AuthenticationComplete(true);
  emit AuthenticationSuccess();

}

void QobuzCoverProvider::Deauthenticate() {

  user_auth_token_.clear();
  user_id_ = 0;
  credential_id_ = 0;
  device_id_.clear();

  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.remove("user_auth_token");
  s.remove("user_id");
  s.remove("credential_id");
  s.remove("device_id");
  s.endGroup();

}

void QobuzCoverProvider::HandleLoginSSLErrors(QList<QSslError> ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    login_errors_ += ssl_error.errorString();
  }

}

bool QobuzCoverProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  if (artist.isEmpty() && album.isEmpty() && title.isEmpty()) return false;

  QString resource;
  QString query = artist;
  if (album.isEmpty() && !title.isEmpty()) {
    resource = "track/search";
    if (!query.isEmpty()) query.append(" ");
    query.append(title);
  }
  else {
    resource = "album/search";
    if (!album.isEmpty()) {
      if (!query.isEmpty()) query.append(" ");
      query.append(album);
    }
  }

  ParamList params = ParamList() << Param("query", query)
                                 << Param("limit", QString::number(kLimit))
                                 << Param("app_id", QString::fromUtf8(QByteArray::fromBase64(kAppID)));

  std::sort(params.begin(), params.end());

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
  }

  QUrl url(kApiUrl + QString("/") + resource);
  url.setQuery(url_query);

  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
  req.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  req.setRawHeader("X-App-Id", kAppID);
  req.setRawHeader("X-User-Auth-Token", user_auth_token_.toUtf8());
  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  connect(reply, &QNetworkReply::finished, [=] { HandleSearchReply(reply, id); });

  return true;

}

void QobuzCoverProvider::CancelSearch(const int id) { Q_UNUSED(id); }

QByteArray QobuzCoverProvider::GetReplyData(QNetworkReply *reply) {

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
      // See if there is Json data containing "status", "code" and "message" - then use that instead.
      data = reply->readAll();
      QString error;
      QJsonParseError parse_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &parse_error);
      if (parse_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("status") && json_obj.contains("code") && json_obj.contains("message")) {
          QString status = json_obj["status"].toString();
          int code = json_obj["code"].toInt();
          QString message = json_obj["message"].toString();
          error = QString("%1 (%2)").arg(message).arg(code);
        }
      }
      if (error.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
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

void QobuzCoverProvider::HandleSearchReply(QNetworkReply *reply, const int id) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  CoverSearchResults results;

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    emit SearchFinished(id, results);
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    emit SearchFinished(id, results);
    return;
  }

  QJsonValue value_type;
  if (json_obj.contains("albums")) {
    value_type = json_obj["albums"];
  }
  else if (json_obj.contains("tracks")) {
    value_type = json_obj["tracks"];
  }
  else {
    Error("Json reply is missing albums and tracks object.", json_obj);
    emit SearchFinished(id, results);
    return;
  }

  if (!value_type.isObject()) {
    Error("Json albums or tracks is not a object.", value_type);
    emit SearchFinished(id, results);
    return;
  }
  QJsonObject obj_type = value_type.toObject();

  if (!obj_type.contains("items")) {
    Error("Json albums or tracks object does not contain items.", obj_type);
    emit SearchFinished(id, results);
    return;
  }
  QJsonValue value_items = obj_type["items"];

  if (!value_items.isArray()) {
    Error("Json albums or track object items is not a array.", value_items);
    emit SearchFinished(id, results);
    return;
  }
  QJsonArray array_items = value_items.toArray();

  for (const QJsonValue &value : array_items) {

    if (!value.isObject()) {
      Error("Invalid Json reply, value in items is not a object.", value);
      continue;
    }
    QJsonObject item_obj = value.toObject();

    QJsonObject obj_album;
    if (item_obj.contains("album")) {
      if (!item_obj["album"].isObject()) {
        Error("Invalid Json reply, items album is not a object.", item_obj);
        continue;
      }
      obj_album = item_obj["album"].toObject();
    }
    else {
      obj_album = item_obj;
    }

    if (!obj_album.contains("artist") || !obj_album.contains("image") || !obj_album.contains("title")) {
      Error("Invalid Json reply, item is missing artist, title or image.", obj_album);
      continue;
    }

    QString album = obj_album["title"].toString();

    // Artist
    QJsonValue value_artist = obj_album["artist"];
    if (!value_artist.isObject()) {
      Error("Invalid Json reply, items (album) artist is not a object.", value_artist);
      continue;
    }
    QJsonObject obj_artist = value_artist.toObject();
    if (!obj_artist.contains("name")) {
      Error("Invalid Json reply, items (album) artist is missing name.", obj_artist);
      continue;
    }
    QString artist = obj_artist["name"].toString();

    // Image
    QJsonValue value_image = obj_album["image"];
    if (!value_image.isObject()) {
      Error("Invalid Json reply, items (album) image is not a object.", value_image);
      continue;
    }
    QJsonObject obj_image = value_image.toObject();
    if (!obj_image.contains("large")) {
      Error("Invalid Json reply, items (album) image is missing large.", obj_image);
      continue;
    }
    QUrl cover_url(obj_image["large"].toString());

    album = album.remove(Song::kAlbumRemoveDisc);
    album = album.remove(Song::kAlbumRemoveMisc);

    CoverSearchResult cover_result;
    cover_result.artist = artist;
    cover_result.album = album;
    cover_result.image_url = cover_url;
    cover_result.image_size = QSize(600, 600);
    results << cover_result;

  }
  emit SearchFinished(id, results);

}

void QobuzCoverProvider::AuthError(const QString &error, const QVariant &debug) {

  if (!error.isEmpty()) login_errors_ << error;

  for (const QString &e : login_errors_) Error(e);
  if (debug.isValid()) qLog(Debug) << debug;

  emit AuthenticationFailure(login_errors_);
  emit AuthenticationComplete(false, login_errors_);

  login_errors_.clear();

}

void QobuzCoverProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Qobuz:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
