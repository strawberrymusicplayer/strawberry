/*
 * Strawberry Music Player
 * Copyright 2024-2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QScopeGuard>

#include "includes/shared_ptr.h"
#include "core/networkaccessmanager.h"
#include "core/logging.h"
#include "core/oauthenticator.h"
#include "albumcoverfetcher.h"
#include "jsoncoverprovider.h"
#include "opentidalcoverprovider.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kSettingsGroup[] = "OpenTidal";
constexpr char kOAuthAccessTokenUrl[] = "https://auth.tidal.com/v1/oauth2/token";
constexpr char kApiUrl[] = "https://openapi.tidal.com/v2";
constexpr char kApiClientIdB64[] = "RHBwV3FpTEM4ZFJSV1RJaQ==";
constexpr char kApiClientSecretB64[] = "cGk0QmxpclZXQWlteWpBc0RnWmZ5RmVlRzA2b3E1blVBVTljUW1IdFhDST0=";
constexpr char kContentTypeHeader[] = "application/vnd.api+json";
constexpr int kSearchLimit = 6;
constexpr const int kRequestsDelay = 300;
}  // namespace

using std::make_shared;

OpenTidalCoverProvider::OpenTidalCoverProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonCoverProvider(u"OpenTidal"_s, true, false, 2.5, true, false, network, parent),
      oauth_(new OAuthenticator(network, this)),
      timer_flush_requests_(new QTimer(this)),
      login_in_progress_(false) {

  oauth_->set_settings_group(QLatin1String(kSettingsGroup));
  oauth_->set_type(OAuthenticator::Type::Client_Credentials);
  oauth_->set_access_token_url(QUrl(QLatin1String(kOAuthAccessTokenUrl)));
  oauth_->set_client_id(QString::fromLatin1(QByteArray::fromBase64(kApiClientIdB64)));
  oauth_->set_client_secret(QString::fromLatin1(QByteArray::fromBase64(kApiClientSecretB64)));
  oauth_->set_use_local_redirect_server(false);
  oauth_->set_random_port(false);
  QObject::connect(oauth_, &OAuthenticator::AuthenticationFinished, this, &OpenTidalCoverProvider::OAuthFinished);

  timer_flush_requests_->setInterval(kRequestsDelay);
  timer_flush_requests_->setSingleShot(false);
  QObject::connect(timer_flush_requests_, &QTimer::timeout, this, &OpenTidalCoverProvider::FlushRequests);

  oauth_->LoadSession();

}

bool OpenTidalCoverProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  if (artist.isEmpty() || album.isEmpty()) return false;

  if (!oauth_->authenticated() && !login_in_progress_ && (last_login_attempt_.isValid() && (QDateTime::currentSecsSinceEpoch() - last_login_attempt_.toSecsSinceEpoch()) < 120)) {
    return false;
  }

  search_requests_queue_.enqueue(make_shared<QueuedSearchRequest>(make_shared<SearchRequest>(id, artist, album, title)));

  if (!timer_flush_requests_->isActive()) {
    timer_flush_requests_->start();
  }

  return true;

}

void OpenTidalCoverProvider::CancelSearch(const int id) {
  Q_UNUSED(id);
}

void OpenTidalCoverProvider::FlushRequests() {

  if (!oauth_->authenticated()) {
    LoginCheck();
    return;
  }

  if (!artwork_requests_queue_.isEmpty()) {
    QueuedArtworkRequestPtr queued_artwork_request = artwork_requests_queue_.dequeue();
    SendArtworkRequest(queued_artwork_request->search, queued_artwork_request->albumcover, queued_artwork_request->artwork);
    return;
  }

  if (!albumcover_requests_queue_.isEmpty()) {
    QueuedAlbumCoverRequestPtr queued_albumcover_request = albumcover_requests_queue_.dequeue();
    SendAlbumCoverRequest(queued_albumcover_request->search, queued_albumcover_request->albumcover);
    return;
  }

  if (!search_requests_queue_.isEmpty()) {
    QueuedSearchRequestPtr queued_search_request = search_requests_queue_.dequeue();
    SendSearchRequest(queued_search_request->search);
    return;
  }

  timer_flush_requests_->stop();

}

void OpenTidalCoverProvider::LoginCheck() {

  if (!oauth_->authenticated() && !login_in_progress_ && (!last_login_attempt_.isValid() || QDateTime::currentSecsSinceEpoch() - last_login_attempt_.toSecsSinceEpoch() > 120)) {
    Login();
  }

}

void OpenTidalCoverProvider::Login() {

  qLog(Debug) << "Authenticating...";

  login_in_progress_ = true;

  oauth_->Authenticate();

}

void OpenTidalCoverProvider::OAuthFinished(const bool success, const QString &error) {

  login_in_progress_ = false;

  if (success) {
    qLog(Debug) << "OpenTidal: Authentication successful";
    last_login_attempt_ = QDateTime();
    if (!timer_flush_requests_->isActive()) {
      timer_flush_requests_->start();
    }
  }
  else {
    qLog(Debug) << "OpenTidal: Authentication failed" << error;
    last_login_attempt_ = QDateTime::currentDateTime();
  }

}

JsonBaseRequest::JsonObjectResult OpenTidalCoverProvider::ParseJsonObject(QNetworkReply *reply) {

  if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
    return ReplyDataResult(ErrorCode::NetworkError, QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
  }

  JsonObjectResult result(ErrorCode::Success);
  result.network_error = reply->error();
  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
    result.http_status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  }

  const QByteArray data = reply->readAll();
  bool clear_session = false;
  if (!data.isEmpty()) {
    QJsonParseError json_parse_error;
    const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_parse_error);
    if (json_parse_error.error == QJsonParseError::NoError) {
      const QJsonObject json_object = json_document.object();
      if (json_object.contains("errors"_L1) && json_object["errors"_L1].isArray()) {
        const QJsonArray array_errors = json_object["errors"_L1].toArray();
        for (const auto &value : array_errors) {
          if (!value.isObject()) continue;
          const QJsonObject object_error = value.toObject();
          if (!object_error.contains("category"_L1) || !object_error.contains("code"_L1) || !object_error.contains("detail"_L1)) {
            continue;
          }
          const QString category = object_error["category"_L1].toString();
          const QString code = object_error["code"_L1].toString();
          const QString detail = object_error["detail"_L1].toString();
          result.error_code = ErrorCode::APIError;
          result.error_message = QStringLiteral("%1 (%2) (%3)").arg(category, code, detail);
          if (category == "AUTHENTICATION_ERROR"_L1) {
            clear_session = true;
          }
        }
      }
      else {
        result.json_object = json_document.object();
      }
    }
    else {
      result.error_code = ErrorCode::ParseError;
      result.error_message = json_parse_error.errorString();
    }
  }

  if (result.error_code != ErrorCode::APIError) {
    if (reply->error() != QNetworkReply::NoError) {
      result.error_code = ErrorCode::NetworkError;
      result.error_message = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
    }
    else if (result.http_status_code != 200) {
      result.error_code = ErrorCode::HttpError;
      result.error_message = QStringLiteral("Received HTTP code %1").arg(result.http_status_code);
    }
  }

  if (reply->error() == QNetworkReply::AuthenticationRequiredError || clear_session) {
    oauth_->ClearSession();
  }

  return result;

}

void OpenTidalCoverProvider::SendSearchRequest(SearchRequestPtr search_request) {

  QString query = search_request->artist;
  if (!search_request->album.isEmpty()) {
    if (!query.isEmpty()) query.append(u' ');
    query.append(search_request->album);
  }
  else if (!search_request->title.isEmpty()) {
    if (!query.isEmpty()) query.append(u' ');
    query.append(search_request->title);
  }

  QUrlQuery url_query;
  url_query.addQueryItem(u"countryCode"_s, u"US"_s);
  url_query.addQueryItem(u"limit"_s, QString::number(kSearchLimit));
  url_query.addQueryItem(u"include"_s, u"albums"_s);
  QUrl url(QLatin1String(kApiUrl) + "/searchResults/"_L1 + QString::fromUtf8(QUrl::toPercentEncoding(query)));
  url.setQuery(url_query);
  QNetworkRequest network_request(url);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  network_request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String(kContentTypeHeader));
  if (oauth_->authenticated()) {
    network_request.setRawHeader("Authorization", oauth_->authorization_header());
  }

  QNetworkReply *reply = network_->get(network_request);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, search_request]() { HandleSearchReply(reply, search_request); });

}

void OpenTidalCoverProvider::HandleSearchReply(QNetworkReply *reply, SearchRequestPtr search_request) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const QScopeGuard search_finished = qScopeGuard([this, search_request]() {
    if (!search_request->finished && search_request->albumcover_requests.isEmpty()) {
      Q_EMIT SearchFinished(search_request->id, search_request->results);
      search_request->finished = true;
    }
  });

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    if (login_in_progress_) {
      search_requests_queue_.prepend(make_shared<QueuedSearchRequest>(search_request));
    }
    return;
  }
  const QJsonObject &json_object = json_object_result.json_object;
  if (!json_object.contains("included"_L1) || !json_object["included"_L1].isArray()) {
    return;
  }
  const QJsonArray array_included = json_object["included"_L1].toArray();
  if (array_included.isEmpty()) {
    return;
  }
  for (const auto &value : array_included) {
    if (!value.isObject()) {
      continue;
    }
    const QJsonObject object = value.toObject();
    const QString id = object["id"_L1].toString();
    const QString type = object["type"_L1].toString();
    if (type == "albums"_L1) {
      QString title;
      if (object.contains("attributes"_L1)) {
        const QJsonObject attributes = object["attributes"_L1].toObject();
        if (attributes.contains("title"_L1)) {
          title = attributes["title"_L1].toString();
        }
      }
      AddAlbumCoverRequest(search_request, id, title);
    }
  }

}

void OpenTidalCoverProvider::AddAlbumCoverRequest(SearchRequestPtr search_request, const QString &album_id, const QString &album_title) {

  AlbumCoverRequestPtr albumcover_request = make_shared<AlbumCoverRequest>(album_id, album_title);
  search_request->albumcover_requests << albumcover_request;
  albumcover_requests_queue_.enqueue(make_shared<QueuedAlbumCoverRequest>(search_request, albumcover_request));

  if (!timer_flush_requests_->isActive()) {
    timer_flush_requests_->start();
  }

}

void OpenTidalCoverProvider::SendAlbumCoverRequest(SearchRequestPtr search_request, AlbumCoverRequestPtr albumcover_request) {

  QUrlQuery url_query;
  url_query.addQueryItem(u"countryCode"_s, u"US"_s);
  QUrl url(QLatin1String(kApiUrl) + QLatin1String("/albums/%1/relationships/coverArt"_L1).arg(albumcover_request->album_id));
  url.setQuery(url_query);
  QNetworkRequest network_request(url);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  network_request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String(kContentTypeHeader));
  if (oauth_->authenticated()) {
    network_request.setRawHeader("Authorization", oauth_->authorization_header());
  }

  QNetworkReply *reply = network_->get(network_request);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, search_request, albumcover_request]() { HandleAlbumCoverReply(reply, search_request, albumcover_request); });

}

void OpenTidalCoverProvider::HandleAlbumCoverReply(QNetworkReply *reply, SearchRequestPtr search_request, AlbumCoverRequestPtr albumcover_request) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const QScopeGuard search_finished = qScopeGuard([this, search_request, albumcover_request]() {
    if (albumcover_request->artwork_requests.isEmpty()) {
      search_request->albumcover_requests.removeAll(albumcover_request);
    }
    if (!search_request->finished && search_request->albumcover_requests.isEmpty()) {
      Q_EMIT SearchFinished(search_request->id, search_request->results);
      search_request->finished = true;
    }
  });

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    if (login_in_progress_) {
      albumcover_requests_queue_.prepend(make_shared<QueuedAlbumCoverRequest>(search_request, albumcover_request));
    }
    return;
  }
  const QJsonObject &json_object = json_object_result.json_object;
  if (!json_object.contains("data"_L1) || !json_object["data"_L1].isArray()) {
    return;
  }
  const QJsonArray array_data = json_object["data"_L1].toArray();
  if (array_data.isEmpty()) {
    return;
  }
  for (const auto &value : array_data) {
    if (!value.isObject()) {
      continue;
    }
    const QJsonObject object = value.toObject();
    if (!object.contains("id"_L1) || !object.contains("type"_L1)) {
      continue;
    }
    const QString id = object["id"_L1].toString();
    const QString type = object["type"_L1].toString();
    if (type == "artworks"_L1) {
      AddArtworkRequest(search_request, albumcover_request, id);
    }
  }

}

void OpenTidalCoverProvider::AddArtworkRequest(SearchRequestPtr search_request, AlbumCoverRequestPtr albumcover_request, const QString &artwork_id) {

  ArtworkRequestPtr artwork_request = make_shared<ArtworkRequest>(artwork_id);
  albumcover_request->artwork_requests << artwork_request;
  artwork_requests_queue_.enqueue(make_shared<QueuedArtworkRequest>(search_request, albumcover_request, artwork_request));

  if (!timer_flush_requests_->isActive()) {
    timer_flush_requests_->start();
  }

}

void OpenTidalCoverProvider::SendArtworkRequest(SearchRequestPtr search_request, AlbumCoverRequestPtr albumcover_request, ArtworkRequestPtr artwork_request) {

  QUrlQuery url_query;
  url_query.addQueryItem(u"countryCode"_s, u"US"_s);
  QUrl url(QLatin1String(kApiUrl) + QLatin1String("/artworks/%1").arg(artwork_request->artwork_id));
  url.setQuery(url_query);
  QNetworkRequest network_request(url);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  network_request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String(kContentTypeHeader));
  if (oauth_->authenticated()) {
    network_request.setRawHeader("Authorization", oauth_->authorization_header());
  }

  QNetworkReply *reply = network_->get(network_request);
  replies_ << reply;
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, search_request, albumcover_request, artwork_request]() { HandleArtworkReply(reply, search_request, albumcover_request, artwork_request); });

}

void OpenTidalCoverProvider::HandleArtworkReply(QNetworkReply *reply, SearchRequestPtr search_request, AlbumCoverRequestPtr albumcover_request, ArtworkRequestPtr artwork_request) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const QScopeGuard search_finished = qScopeGuard([this, search_request, albumcover_request, artwork_request]() {
    albumcover_request->artwork_requests.removeAll(artwork_request);
    if (albumcover_request->artwork_requests.isEmpty()) {
      search_request->albumcover_requests.removeAll(albumcover_request);
    }
    if (!search_request->finished && search_request->albumcover_requests.isEmpty()) {
      Q_EMIT SearchFinished(search_request->id, search_request->results);
      search_request->finished = true;
    }
  });

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    if (login_in_progress_) {
      artwork_requests_queue_.prepend(make_shared<QueuedArtworkRequest>(search_request, albumcover_request, artwork_request));
    }
    return;
  }
  const QJsonObject &json_object = json_object_result.json_object;
  if (!json_object.contains("data"_L1) || !json_object["data"_L1].isObject()) {
    return;
  }
  const QJsonObject object_data = json_object["data"_L1].toObject();
  if (!object_data.contains("attributes"_L1) || !object_data["attributes"_L1].isObject()) {
    return;
  }
  const QJsonObject object_attributes = object_data["attributes"_L1].toObject();
  if (!object_attributes.contains("files"_L1) || !object_attributes["files"_L1].isArray()) {
    return;
  }
  const QJsonArray array_files = object_attributes["files"_L1].toArray();
  int i = 0;
  for (const auto &value_file : array_files) {
    if (!value_file.isObject()) {
      continue;
    }
    const QJsonObject object_file = value_file.toObject();
    if (!object_file.contains("href"_L1) || !object_file["href"_L1].isString()) {
      continue;
    }
    if (!object_file.contains("meta"_L1) || !object_file["meta"_L1].isObject()) {
      continue;
    }
    const QString href = object_file["href"_L1].toString();
    const QJsonObject object_meta = object_file["meta"_L1].toObject();
    if (!object_meta.contains("width"_L1) || !object_meta.contains("height"_L1)) {
      continue;
    }
    const int width = object_meta["width"_L1].toInt();
    const int height = object_meta["height"_L1].toInt();
    const QUrl url(href);
    if (!url.isValid() || width < 640 || height < 640) continue;
    CoverProviderSearchResult cover_result;
    cover_result.artist = search_request->artist;
    cover_result.album = albumcover_request->album_title;
    cover_result.image_url = url;
    cover_result.image_size = QSize(width, height);
    cover_result.number = ++i;
    search_request->results << cover_result;
  }

}

void OpenTidalCoverProvider::FinishAllSearches() {

  while (!search_requests_queue_.isEmpty()) {
    QueuedSearchRequestPtr queued_search_request = search_requests_queue_.dequeue();
    SearchRequestPtr search_request = queued_search_request->search;
    search_request->albumcover_requests.clear();
    if (!search_request->finished) {
      Q_EMIT SearchFinished(search_request->id, CoverProviderSearchResults());
      search_request->finished = true;
    }
  }

  timer_flush_requests_->stop();

}

void OpenTidalCoverProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "OpenTidal:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
