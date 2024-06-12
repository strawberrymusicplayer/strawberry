/*
 * Strawberry Music Player
 * Copyright 2020-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include "core/shared_ptr.h"
#include "core/application.h"
#include "core/networkaccessmanager.h"
#include "core/logging.h"
#include "core/settings.h"
#include "utilities/randutils.h"
#include "utilities/timeconstants.h"
#include "internet/localredirectserver.h"
#include "albumcoverfetcher.h"
#include "jsoncoverprovider.h"
#include "spotifycoverprovider.h"

namespace {
constexpr char kSettingsGroup[] = "Spotify";
constexpr char kOAuthAuthorizeUrl[] = "https://accounts.spotify.com/authorize";
constexpr char kOAuthAccessTokenUrl[] = "https://accounts.spotify.com/api/token";
constexpr char kOAuthRedirectUrl[] = "http://localhost:63111/";
constexpr char kClientIDB64[] = "ZTZjY2Y2OTQ5NzY1NGE3NThjOTAxNWViYzdiMWQzMTc=";
constexpr char kClientSecretB64[] = "N2ZlMDMxODk1NTBlNDE3ZGI1ZWQ1MzE3ZGZlZmU2MTE=";
constexpr char kApiUrl[] = "https://api.spotify.com/v1";
constexpr int kLimit = 10;
}  // namespace

SpotifyCoverProvider::SpotifyCoverProvider(Application *app, SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonCoverProvider(QStringLiteral("Spotify"), true, true, 2.5, true, true, app, network, parent),
      server_(nullptr),
      expires_in_(0),
      login_time_(0) {

  refresh_login_timer_.setSingleShot(true);
  QObject::connect(&refresh_login_timer_, &QTimer::timeout, this, &SpotifyCoverProvider::RequestNewAccessToken);

  Settings s;
  s.beginGroup(kSettingsGroup);
  access_token_ = s.value("access_token").toString();
  refresh_token_ = s.value("refresh_token").toString();
  expires_in_ = s.value("expires_in").toLongLong();
  login_time_ = s.value("login_time").toLongLong();
  s.endGroup();

  if (!refresh_token_.isEmpty()) {
    qint64 time = static_cast<qint64>(expires_in_) - (QDateTime::currentDateTime().toSecsSinceEpoch() - static_cast<qint64>(login_time_));
    if (time < 1) time = 1;
    refresh_login_timer_.setInterval(static_cast<int>(time * kMsecPerSec));
    refresh_login_timer_.start();
  }

}

SpotifyCoverProvider::~SpotifyCoverProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

void SpotifyCoverProvider::Authenticate() {

  QUrl redirect_url(QString::fromLatin1(kOAuthRedirectUrl));

  if (!server_) {
    server_ = new LocalRedirectServer(this);
    int port = redirect_url.port();
    int port_max = port + 10;
    bool success = false;
    forever {
      server_->set_port(port);
      if (server_->Listen()) {
        success = true;
        break;
      }
      ++port;
      if (port > port_max) break;
    }
    if (!success) {
      AuthError(server_->error());
      server_->deleteLater();
      server_ = nullptr;
      return;
    }
    QObject::connect(server_, &LocalRedirectServer::Finished, this, &SpotifyCoverProvider::RedirectArrived);
  }

  code_verifier_ = Utilities::CryptographicRandomString(44);
  code_challenge_ = QString::fromLatin1(QCryptographicHash::hash(code_verifier_.toUtf8(), QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding));
  if (code_challenge_.lastIndexOf(QLatin1Char('=')) == code_challenge_.length() - 1) {
    code_challenge_.chop(1);
  }

  const ParamList params = ParamList() << Param(QStringLiteral("client_id"), QString::fromLatin1(QByteArray::fromBase64(kClientIDB64)))
                                       << Param(QStringLiteral("response_type"), QStringLiteral("code"))
                                       << Param(QStringLiteral("redirect_uri"), redirect_url.toString())
                                       << Param(QStringLiteral("state"), code_challenge_);

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QUrl url(QString::fromLatin1(kOAuthAuthorizeUrl));
  url.setQuery(url_query);

  const bool result = QDesktopServices::openUrl(url);
  if (!result) {
    QMessageBox messagebox(QMessageBox::Information, tr("Spotify Authentication"), tr("Please open this URL in your browser") + QStringLiteral(":<br /><a href=\"%1\">%1</a>").arg(url.toString()), QMessageBox::Ok);
    messagebox.setTextFormat(Qt::RichText);
    messagebox.exec();
  }

}

void SpotifyCoverProvider::Deauthenticate() {

  access_token_.clear();
  refresh_token_.clear();
  expires_in_ = 0;
  login_time_ = 0;

  Settings s;
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
      if (url_query.hasQueryItem(QStringLiteral("error"))) {
        AuthError(QUrlQuery(url).queryItemValue(QStringLiteral("error")));
      }
      else if (url_query.hasQueryItem(QStringLiteral("code")) && url_query.hasQueryItem(QStringLiteral("state"))) {
        qLog(Debug) << "Spotify: Authorization URL Received" << url;
        QString code = url_query.queryItemValue(QStringLiteral("code"));
        QUrl redirect_url(QString::fromLatin1(kOAuthRedirectUrl));
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

void SpotifyCoverProvider::RequestAccessToken(const QString &code, const QUrl &redirect_url) {

  refresh_login_timer_.stop();

  ParamList params = ParamList() << Param(QStringLiteral("client_id"), QLatin1String(kClientIDB64))
                                 << Param(QStringLiteral("client_secret"), QLatin1String(kClientSecretB64));

  if (!code.isEmpty() && !redirect_url.isEmpty()) {
    params << Param(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    params << Param(QStringLiteral("code"), code);
    params << Param(QStringLiteral("redirect_uri"), redirect_url.toString());
  }
  else if (!refresh_token_.isEmpty() && is_enabled()) {
    params << Param(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    params << Param(QStringLiteral("refresh_token"), refresh_token_);
  }
  else {
    return;
  }

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QUrl new_url(QString::fromLatin1(kOAuthAccessTokenUrl));
  QNetworkRequest req(new_url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
  QString auth_header_data = QString::fromLatin1(QByteArray::fromBase64(kClientIDB64)) + QLatin1Char(':') + QString::fromLatin1(QByteArray::fromBase64(kClientSecretB64));
  req.setRawHeader("Authorization", "Basic " + auth_header_data.toUtf8().toBase64());

  QByteArray query = url_query.toString(QUrl::FullyEncoded).toUtf8();

  QNetworkReply *reply = network_->post(req, query);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &SpotifyCoverProvider::HandleLoginSSLErrors);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { AccessTokenRequestFinished(reply); });

}

void SpotifyCoverProvider::HandleLoginSSLErrors(const QList<QSslError> &ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    login_errors_ += ssl_error.errorString();
  }

}

void SpotifyCoverProvider::AccessTokenRequestFinished(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      AuthError(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
      return;
    }
    else {
      // See if there is Json data containing "error" and "error_description" then use that instead.
      QByteArray data = reply->readAll();
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      if (json_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains(QLatin1String("error")) && json_obj.contains(QLatin1String("error_description"))) {
          QString error = json_obj[QLatin1String("error")].toString();
          QString error_description = json_obj[QLatin1String("error_description")].toString();
          login_errors_ << QStringLiteral("Authentication failure: %1 (%2)").arg(error, error_description);
        }
      }
      if (login_errors_.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
          login_errors_ << QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
        else {
          login_errors_ << QStringLiteral("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
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
    Error(QStringLiteral("Failed to parse Json data in authentication reply: %1").arg(json_error.errorString()));
    return;
  }

  if (json_doc.isEmpty()) {
    AuthError(QStringLiteral("Authentication reply from server has empty Json document."));
    return;
  }

  if (!json_doc.isObject()) {
    AuthError(QStringLiteral("Authentication reply from server has Json document that is not an object."), json_doc);
    return;
  }

  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    AuthError(QStringLiteral("Authentication reply from server has empty Json object."), json_doc);
    return;
  }

  if (!json_obj.contains(QLatin1String("access_token")) || !json_obj.contains(QLatin1String("expires_in"))) {
    AuthError(QStringLiteral("Authentication reply from server is missing access token or expires in."), json_obj);
    return;
  }

  access_token_ = json_obj[QLatin1String("access_token")].toString();
  if (json_obj.contains(QLatin1String("refresh_token"))) {
    refresh_token_ = json_obj[QLatin1String("refresh_token")].toString();
  }
  expires_in_ = json_obj[QLatin1String("expires_in")].toInt();
  login_time_ = QDateTime::currentDateTime().toSecsSinceEpoch();

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("access_token", access_token_);
  s.setValue("refresh_token", refresh_token_);
  s.setValue("expires_in", expires_in_);
  s.setValue("login_time", login_time_);
  s.endGroup();

  if (expires_in_ > 0) {
    refresh_login_timer_.setInterval(static_cast<int>(expires_in_ * kMsecPerSec));
    refresh_login_timer_.start();
  }

  qLog(Debug) << "Spotify: Authentication was successful, login expires in" << expires_in_;

  emit AuthenticationComplete(true);
  emit AuthenticationSuccess();

}

bool SpotifyCoverProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  if (access_token_.isEmpty()) return false;

  if (artist.isEmpty() && album.isEmpty() && title.isEmpty()) return false;

  QString type;
  QString extract;
  QString query = artist;
  if (album.isEmpty() && !title.isEmpty()) {
    type = QLatin1String("track");
    extract = QLatin1String("tracks");
    if (!query.isEmpty()) query.append(QLatin1Char(' '));
    query.append(title);
  }
  else {
    type = QLatin1String("album");
    extract = QLatin1String("albums");
    if (!album.isEmpty()) {
      if (!query.isEmpty()) query.append(QLatin1Char(' '));
      query.append(album);
    }
  }

  ParamList params = ParamList() << Param(QStringLiteral("q"), query)
                                 << Param(QStringLiteral("type"), type)
                                 << Param(QStringLiteral("limit"), QString::number(kLimit));

  QUrlQuery url_query;
  for (const Param &param : params) {
    url_query.addQueryItem(QString::fromLatin1(QUrl::toPercentEncoding(param.first)), QString::fromLatin1(QUrl::toPercentEncoding(param.second)));
  }

  QUrl url(QLatin1String(kApiUrl) + QStringLiteral("/search"));
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
  req.setRawHeader("Authorization", "Bearer " + access_token_.toUtf8());

  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, id, extract]() { HandleSearchReply(reply, id, extract); });

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
      Error(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    }
    else {
      data = reply->readAll();
      QJsonParseError parse_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &parse_error);
      QString error;
      if (parse_error.error == QJsonParseError::NoError && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains(QLatin1String("error")) && json_obj[QLatin1String("error")].isObject()) {
          QJsonObject obj_error = json_obj[QLatin1String("error")].toObject();
          if (obj_error.contains(QLatin1String("status")) && obj_error.contains(QLatin1String("message"))) {
            int status = obj_error[QLatin1String("status")].toInt();
            QString message = obj_error[QLatin1String("message")].toString();
            error = QStringLiteral("%1 (%2)").arg(message).arg(status);
            if (status == 401) access_token_.clear();
          }
        }
      }
      if (error.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
          if (reply->error() == 204) access_token_.clear();
          error = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
        else {
          error = QStringLiteral("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
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
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    emit SearchFinished(id, CoverProviderSearchResults());
    return;
  }

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) {
    emit SearchFinished(id, CoverProviderSearchResults());
    return;
  }

  if (!json_obj.contains(extract) || !json_obj[extract].isObject()) {
    Error(QStringLiteral("Json object is missing %1 object.").arg(extract), json_obj);
    emit SearchFinished(id, CoverProviderSearchResults());
    return;
  }
  json_obj = json_obj[extract].toObject();

  if (!json_obj.contains(QLatin1String("items")) || !json_obj[QLatin1String("items")].isArray()) {
    Error(QStringLiteral("%1 object is missing items array.").arg(extract), json_obj);
    emit SearchFinished(id, CoverProviderSearchResults());
    return;
  }

  QJsonArray array_items = json_obj[QLatin1String("items")].toArray();
  if (array_items.isEmpty()) {
    emit SearchFinished(id, CoverProviderSearchResults());
    return;
  }

  CoverProviderSearchResults results;
  for (const QJsonValueRef value_item : array_items) {

    if (!value_item.isObject()) {
      continue;
    }
    QJsonObject obj_item = value_item.toObject();

    QJsonObject obj_album = obj_item;
    if (obj_item.contains(QLatin1String("album")) && obj_item[QLatin1String("album")].isObject()) {
      obj_album = obj_item[QLatin1String("album")].toObject();
    }

    if (!obj_album.contains(QLatin1String("artists")) || !obj_album.contains(QLatin1String("name")) || !obj_album.contains(QLatin1String("images")) || !obj_album[QLatin1String("artists")].isArray() || !obj_album[QLatin1String("images")].isArray()) {
      continue;
    }
    QJsonArray array_artists = obj_album[QLatin1String("artists")].toArray();
    QJsonArray array_images = obj_album[QLatin1String("images")].toArray();
    QString album = obj_album[QLatin1String("name")].toString();

    QStringList artists;
    for (const QJsonValueRef value_artist : array_artists) {
      if (!value_artist.isObject()) continue;
      QJsonObject obj_artist = value_artist.toObject();
      if (!obj_artist.contains(QLatin1String("name"))) continue;
      artists << obj_artist[QLatin1String("name")].toString();
    }

    for (const QJsonValueRef value_image : array_images) {
      if (!value_image.isObject()) continue;
      QJsonObject obj_image = value_image.toObject();
      if (!obj_image.contains(QLatin1String("url")) || !obj_image.contains(QLatin1String("width")) || !obj_image.contains(QLatin1String("height"))) continue;
      int width = obj_image[QLatin1String("width")].toInt();
      int height = obj_image[QLatin1String("height")].toInt();
      if (width < 300 || height < 300) continue;
      QUrl url(obj_image[QLatin1String("url")].toString());
      CoverProviderSearchResult result;
      result.album = album;
      result.image_url = url;
      result.image_size = QSize(width, height);
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
