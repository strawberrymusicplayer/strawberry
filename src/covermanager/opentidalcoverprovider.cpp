/*
 * Strawberry Music Player
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>

#include "core/shared_ptr.h"
#include "core/application.h"
#include "core/networkaccessmanager.h"
#include "core/logging.h"
#include "core/settings.h"
#include "utilities/timeconstants.h"
#include "albumcoverfetcher.h"
#include "jsoncoverprovider.h"
#include "opentidalcoverprovider.h"

namespace {
constexpr char kSettingsGroup[] = "OpenTidal";
constexpr char kAuthUrl[] = "https://auth.tidal.com/v1/oauth2/token";
constexpr char kApiUrl[] = "https://openapi.tidal.com";
constexpr char kApiClientIdB64[] = "RHBwV3FpTEM4ZFJSV1RJaQ==";
constexpr char kApiClientSecretB64[] = "cGk0QmxpclZXQWlteWpBc0RnWmZ5RmVlRzA2b3E1blVBVTljUW1IdFhDST0=";
constexpr int kLimit = 10;
constexpr const int kRequestsDelay = 1000;
}  // namespace

using std::make_shared;

OpenTidalCoverProvider::OpenTidalCoverProvider(Application *app, SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonCoverProvider(QStringLiteral("OpenTidal"), true, false, 2.5, true, false, app, network, parent),
      login_timer_(new QTimer(this)),
      timer_flush_requests_(new QTimer(this)),
      login_in_progress_(false),
      have_login_(false),
      login_time_(0),
      expires_in_(0) {

  login_timer_->setSingleShot(true);
  QObject::connect(login_timer_, &QTimer::timeout, this, &OpenTidalCoverProvider::Login);

  timer_flush_requests_->setInterval(kRequestsDelay);
  timer_flush_requests_->setSingleShot(false);
  QObject::connect(timer_flush_requests_, &QTimer::timeout, this, &OpenTidalCoverProvider::FlushRequests);

  LoadSession();

}

OpenTidalCoverProvider::~OpenTidalCoverProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

}

bool OpenTidalCoverProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  if (artist.isEmpty() || album.isEmpty()) return false;

  if (!have_login_ && !login_in_progress_ && QDateTime::currentDateTime().toSecsSinceEpoch() - last_login_attempt_.toSecsSinceEpoch() < 120) {
    return false;
  }

  SearchRequestPtr search_request = make_shared<SearchRequest>(id, artist, album, title);
  search_requests_queue_ << search_request;

  if (!timer_flush_requests_->isActive()) {
    timer_flush_requests_->start();
  }

  return true;

}

void OpenTidalCoverProvider::CancelSearch(const int id) {
  Q_UNUSED(id);
}

void OpenTidalCoverProvider::LoadSession() {

  Settings s;
  s.beginGroup(kSettingsGroup);
  token_type_ = s.value("token_type").toString();
  access_token_ = s.value("access_token").toString();
  expires_in_ = s.value("expires_in", 0).toLongLong();
  login_time_ = s.value("login_time", 0).toLongLong();
  s.endGroup();

  if (!token_type_.isEmpty() && !access_token_.isEmpty() && login_time_ > 0 && expires_in_ > 0) {
    have_login_ = true;
  }

  qint64 time = expires_in_ - (QDateTime::currentDateTime().toSecsSinceEpoch() - login_time_) - 30;
  if (time < 2) time = 2000;
  login_timer_->setInterval(static_cast<int>(time * kMsecPerSec));
  login_timer_->start();

}

void OpenTidalCoverProvider::FlushRequests() {

  if (!have_login_) {
    if (!login_in_progress_) {
      Login();
    }
    return;
  }

  if (!search_requests_queue_.isEmpty()) {
    SendSearchRequest(search_requests_queue_.dequeue());
    return;
  }

  timer_flush_requests_->stop();

}

void OpenTidalCoverProvider::Login() {

  have_login_ = false;
  login_in_progress_ = true;
  last_login_attempt_ = QDateTime::currentDateTime();

  QUrl url(QString::fromLatin1(kAuthUrl));
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setRawHeader("Authorization", "Basic " + QByteArray(QByteArray::fromBase64(kApiClientIdB64) + ":" + QByteArray::fromBase64(kApiClientSecretB64)).toBase64());
  QUrlQuery url_query;
  url_query.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("client_credentials"));
  QNetworkReply *reply = network_->post(req, url_query.toString(QUrl::FullyEncoded).toUtf8());
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::sslErrors, this, &OpenTidalCoverProvider::HandleLoginSSLErrors);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() { LoginFinished(reply); });

}

void OpenTidalCoverProvider::HandleLoginSSLErrors(const QList<QSslError> &ssl_errors) {

  for (const QSslError &ssl_error : ssl_errors) {
    qLog(Error) << "OpenTidal:" << ssl_error.errorString();
  }

}

void OpenTidalCoverProvider::LoginFinished(QNetworkReply *reply) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  login_in_progress_ = false;
  last_login_attempt_ = QDateTime();

  QJsonObject json_obj = GetJsonObject(reply);
  if (json_obj.isEmpty()) {
    FinishAllSearches();
    return;
  }

  if (!json_obj.contains(QStringLiteral("access_token")) ||
      !json_obj.contains(QStringLiteral("token_type")) ||
      !json_obj.contains(QStringLiteral("expires_in")) ||
      !json_obj[QStringLiteral("access_token")].isString() ||
      !json_obj[QStringLiteral("token_type")].isString()) {
    qLog(Error) << "OpenTidal: Invalid login reply.";
    FinishAllSearches();
    return;
  }

  have_login_ = true;
  token_type_ = json_obj[QStringLiteral("token_type")].toString();
  access_token_ = json_obj[QStringLiteral("access_token")].toString();
  login_time_ = QDateTime::currentDateTime().toSecsSinceEpoch();
  expires_in_ = json_obj[QStringLiteral("expires_in")].toInt();

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("token_type", token_type_);
  s.setValue("access_token", access_token_);
  s.setValue("expires_in", expires_in_);
  s.setValue("login_time", login_time_);
  s.endGroup();

  if (expires_in_ <= 300) {
    expires_in_ = 300;
  }

  expires_in_ -= 30;

  login_timer_->setInterval(static_cast<int>(expires_in_ * kMsecPerSec));
  login_timer_->start();

  if (!timer_flush_requests_->isActive()) {
    timer_flush_requests_->start();
  }

}

QJsonObject OpenTidalCoverProvider::ExtractJsonObj(const QByteArray &data) {

  QJsonParseError json_parse_error;
  const QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_parse_error);
  if (json_parse_error.error != QJsonParseError::NoError) {
    qLog(Error) << "OpenTidal:" << json_parse_error.errorString();
    return QJsonObject();
  }
  if (!json_doc.isObject()) {
    return QJsonObject();
  }
  return json_doc.object();

}

QJsonObject OpenTidalCoverProvider::GetJsonObject(QNetworkReply *reply) {

  if (reply->error() != QNetworkReply::NoError) {
    qLog(Error) << "OpenTidal:" << reply->errorString() << reply->error();
    return QJsonObject();
  }

  const int http_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  if (http_code != 200 && http_code != 207) {
    qLog(Error) << "OpenTidal: Received HTTP code" << http_code;
    const QByteArray data = reply->readAll();
    if (data.isEmpty()) {
      return QJsonObject();
    }
    QJsonObject json_obj = ExtractJsonObj(data);
    if (json_obj.contains(QStringLiteral("errors")) && json_obj[QStringLiteral("errors")].isArray()) {
      QJsonArray array = json_obj[QStringLiteral("errors")].toArray();
      for (const QJsonValue &value : array) {
        if (!value.isObject()) continue;
        QJsonObject obj = value.toObject();
        if (!obj.contains(QStringLiteral("category")) ||
            !obj.contains(QStringLiteral("code")) ||
            !obj.contains(QStringLiteral("detail"))) {
          continue;
        }
        QString category = obj[QStringLiteral("category")].toString();
        QString code = obj[QStringLiteral("code")].toString();
        QString detail = obj[QStringLiteral("detail")].toString();
        qLog(Error) << "OpenTidal:" << category << code << detail;
      }
    }
    return QJsonObject();
  }

  const QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    return QJsonObject();
  }

  return ExtractJsonObj(data);

}

void OpenTidalCoverProvider::SendSearchRequest(SearchRequestPtr search_request) {

  QString query = search_request->artist;
  if (!search_request->album.isEmpty()) {
    if (!query.isEmpty()) query.append(QLatin1Char(' '));
    query.append(search_request->album);
  }
  else if (!search_request->title.isEmpty()) {
    if (!query.isEmpty()) query.append(QLatin1Char(' '));
    query.append(search_request->title);
  }

  QUrlQuery url_query;
  url_query.addQueryItem(QStringLiteral("query"), QString::fromUtf8(QUrl::toPercentEncoding(query)));
  url_query.addQueryItem(QStringLiteral("limit"), QString::number(kLimit));
  url_query.addQueryItem(QStringLiteral("countryCode"), QStringLiteral("US"));
  QUrl url(QLatin1String(kApiUrl) + QStringLiteral("/search"));
  url.setQuery(url_query);
  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/vnd.tidal.v1+json"));
  req.setRawHeader("Authorization", token_type_.toUtf8() + " " + access_token_.toUtf8());

  QNetworkReply *reply = network_->get(req);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, search_request]() { HandleSearchReply(reply, search_request); });

}

void OpenTidalCoverProvider::HandleSearchReply(QNetworkReply *reply, SearchRequestPtr search_request) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  QJsonObject json_obj = GetJsonObject(reply);
  if (json_obj.isEmpty()) {
    emit SearchFinished(search_request->id, CoverProviderSearchResults());
    return;
  }

  if (!json_obj.contains(QStringLiteral("albums")) || !json_obj[QStringLiteral("albums")].isArray()) {
    qLog(Debug) << "OpenTidal: Json object is missing albums.";
    emit SearchFinished(search_request->id, CoverProviderSearchResults());
    return;
  }

  QJsonArray array_albums = json_obj[QStringLiteral("albums")].toArray();
  if (array_albums.isEmpty()) {
    emit SearchFinished(search_request->id, CoverProviderSearchResults());
    return;
  }

  CoverProviderSearchResults results;
  int i = 0;
  for (const QJsonValueRef value_album : array_albums) {

    if (!value_album.isObject()) {
      qLog(Debug) << "OpenTidal: Invalid Json reply: Albums array value is not a object.";
      continue;
    }
    QJsonObject obj_album = value_album.toObject();

    if (!obj_album.contains(QStringLiteral("resource")) || !obj_album[QStringLiteral("resource")].isObject()) {
      qLog(Debug) << "OpenTidal: Invalid Json reply: Albums array album is missing resource object.";
      continue;
    }
    QJsonObject obj_resource = obj_album[QStringLiteral("resource")].toObject();

    if (!obj_resource.contains(QStringLiteral("artists")) || !obj_resource[QStringLiteral("artists")].isArray()) {
      qLog(Debug) << "OpenTidal: Invalid Json reply: Resource is missing artists array.";
      continue;
    }

    if (!obj_resource.contains(QStringLiteral("title")) || !obj_resource[QStringLiteral("title")].isString()) {
      qLog(Debug) << "OpenTidal: Invalid Json reply: Resource is missing title.";
      continue;
    }

    if (!obj_resource.contains(QStringLiteral("imageCover")) || !obj_resource[QStringLiteral("imageCover")].isArray()) {
      qLog(Debug) << "OpenTidal: Invalid Json reply: Resource is missing imageCover array.";
      continue;
    }

    QString artist;
    const QString album = obj_resource[QStringLiteral("title")].toString();

    QJsonArray array_artists = obj_resource[QStringLiteral("artists")].toArray();
    for (const QJsonValueRef value_artist : array_artists) {
      if (!value_artist.isObject()) {
        continue;
      }
      QJsonObject obj_artist = value_artist.toObject();
      if (!obj_artist.contains(QStringLiteral("name"))) {
        continue;
      }
      artist = obj_artist[QStringLiteral("name")].toString();
      break;
    }

    QJsonArray array_covers = obj_resource[QStringLiteral("imageCover")].toArray();
    for (const QJsonValueRef value_cover : array_covers) {
      if (!value_cover.isObject()) {
        continue;
      }
      QJsonObject obj_cover = value_cover.toObject();
      if (!obj_cover.contains(QStringLiteral("url")) || !obj_cover.contains(QStringLiteral("width")) || !obj_cover.contains(QStringLiteral("height"))) {
        continue;
      }
      const QUrl url(obj_cover[QStringLiteral("url")].toString());
      const int width = obj_cover[QStringLiteral("width")].toInt();
      const int height = obj_cover[QStringLiteral("height")].toInt();
      if (!url.isValid()) continue;
      if (width < 640 || height < 640) continue;
      CoverProviderSearchResult cover_result;
      cover_result.artist = artist;
      cover_result.album = Song::AlbumRemoveDiscMisc(album);
      cover_result.image_url = url;
      cover_result.image_size = QSize(width, height);
      cover_result.number = ++i;
      results << cover_result;
    }
  }

  emit SearchFinished(search_request->id, results);

}

void OpenTidalCoverProvider::FinishAllSearches() {

  timer_flush_requests_->stop();

  while (!search_requests_queue_.isEmpty()) {
    SearchRequestPtr search_request = search_requests_queue_.dequeue();
    emit SearchFinished(search_request->id, CoverProviderSearchResults());
  }

}

void OpenTidalCoverProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Tidal:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
