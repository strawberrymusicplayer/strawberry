/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, Martin Björklund <mbj4668@gmail.com>
 * Copyright 2016-2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include <algorithm>

#include <QByteArray>
#include <QPair>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QTimer>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QScopeGuard>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "utilities/cryptutils.h"
#include "albumcoverfetcher.h"
#include "jsoncoverprovider.h"
#include "discogscoverprovider.h"

using namespace Qt::Literals::StringLiterals;
using std::make_shared;

namespace {
constexpr char kUrlSearch[] = "https://api.discogs.com/database/search";
constexpr char kAccessKeyB64[] = "dGh6ZnljUGJlZ1NEeXBuSFFxSVk=";
constexpr char kSecretKeyB64[] = "ZkFIcmlaSER4aHhRSlF2U3d0bm5ZVmdxeXFLWUl0UXI=";
constexpr int kRequestsDelay = 1000;
}  // namespace

DiscogsCoverProvider::DiscogsCoverProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonCoverProvider(u"Discogs"_s, false, false, 0.0, false, false, network, parent),
      timer_flush_requests_(new QTimer(this)) {

  timer_flush_requests_->setInterval(kRequestsDelay);
  timer_flush_requests_->setSingleShot(false);
  QObject::connect(timer_flush_requests_, &QTimer::timeout, this, &DiscogsCoverProvider::FlushRequests);

}

DiscogsCoverProvider::~DiscogsCoverProvider() {

  while (!replies_.isEmpty()) {
    QNetworkReply *reply = replies_.takeFirst();
    QObject::disconnect(reply, nullptr, this, nullptr);
    reply->abort();
    reply->deleteLater();
  }

  timer_flush_requests_->stop();
  queue_search_requests_.clear();
  queue_release_requests_.clear();
  requests_search_.clear();

}

bool DiscogsCoverProvider::StartSearch(const QString &artist, const QString &album, const QString &title, const int id) {

  Q_UNUSED(title);

  if (artist.isEmpty() || album.isEmpty()) return false;

  SharedPtr<DiscogsCoverSearchContext> search = make_shared<DiscogsCoverSearchContext>(id, artist, album);

  requests_search_.insert(search->id, search);
  queue_search_requests_.enqueue(search);

  if (!timer_flush_requests_->isActive()) {
    timer_flush_requests_->start();
  }

  return true;

}

void DiscogsCoverProvider::CancelSearch(const int id) {

  if (requests_search_.contains(id)) requests_search_.remove(id);

}

void DiscogsCoverProvider::FlushRequests() {

  if (!queue_release_requests_.isEmpty()) {
    SendReleaseRequest(queue_release_requests_.dequeue());
    return;
  }

  if (!queue_search_requests_.isEmpty()) {
    SendSearchRequest(queue_search_requests_.dequeue());
    return;
  }

  timer_flush_requests_->stop();

}

void DiscogsCoverProvider::SendSearchRequest(SharedPtr<DiscogsCoverSearchContext> search) {

  ParamList params = ParamList() << Param(u"format"_s, u"album"_s)
                                 << Param(u"artist"_s, search->artist.toLower())
                                 << Param(u"release_title"_s, search->album.toLower());

  switch (search->type) {
    case DiscogsCoverType::Master:
      params << Param(u"type"_s, u"master"_s);
      break;
    case DiscogsCoverType::Release:
      params << Param(u"type"_s, u"release"_s);
      break;
  }

  QNetworkReply *reply = CreateRequest(QUrl(QString::fromLatin1(kUrlSearch)), params);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, search]() { HandleSearchReply(reply, search->id); });

}

QNetworkReply *DiscogsCoverProvider::CreateRequest(const QUrl &url, const ParamList &params) {

  const ParamList request_params = ParamList() << Param(u"key"_s, QString::fromLatin1(QByteArray::fromBase64(kAccessKeyB64)))
                                               << Param(u"secret"_s, QString::fromLatin1(QByteArray::fromBase64(kSecretKeyB64)))
                                               << params;

  QUrlQuery url_query;
  QStringList query_items;

  // Encode the arguments
  using EncodedParam = QPair<QByteArray, QByteArray>;
  for (const Param &param : request_params) {
    const EncodedParam encoded_param(QUrl::toPercentEncoding(param.first), QUrl::toPercentEncoding(param.second));
    query_items << QString::fromLatin1(encoded_param.first) + QLatin1Char('=') + QString::fromLatin1(encoded_param.second);
    url_query.addQueryItem(QString::fromLatin1(encoded_param.first), QString::fromLatin1(encoded_param.second));
  }

  QUrl request_url(url);
  request_url.setQuery(url_query);

  // Sign the request
  const QByteArray data_to_sign = QStringLiteral("GET\n%1\n%2\n%3").arg(request_url.host(), request_url.path(), query_items.join(u'&')).toUtf8();
  const QByteArray signature(Utilities::HmacSha256(QByteArray::fromBase64(kSecretKeyB64), data_to_sign));

  // Add the signature to the request
  url_query.addQueryItem(u"Signature"_s, QString::fromLatin1(QUrl::toPercentEncoding(QString::fromLatin1(signature.toBase64()))));

  QNetworkRequest network_request(request_url);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = network_->get(network_request);
  replies_ << reply;

  qLog(Debug) << "Discogs: Sending request" << request_url;

  return reply;

}

JsonBaseRequest::JsonObjectResult DiscogsCoverProvider::ParseJsonObject(QNetworkReply *reply) {

  if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
    return ReplyDataResult(ErrorCode::NetworkError, QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
  }

  JsonObjectResult result(ErrorCode::Success);
  result.network_error = reply->error();
  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
    result.http_status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  }

  const QByteArray data = reply->readAll();
  if (!data.isEmpty()) {
    QJsonParseError json_parse_error;
    const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_parse_error);
    if (json_parse_error.error == QJsonParseError::NoError) {
      const QJsonObject json_object = json_document.object();
      if (json_object.contains("message"_L1)) {
        result.error_code = ErrorCode::APIError;
        result.error_message = json_object["message"_L1].toString();
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

  return result;

}

void DiscogsCoverProvider::HandleSearchReply(QNetworkReply *reply, const int id) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (!requests_search_.contains(id)) return;
  SharedPtr<DiscogsCoverSearchContext> search = requests_search_.value(id);

  const QScopeGuard end_search = qScopeGuard([this, search]() { EndSearch(search); });

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject &json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    return;
  }

  QJsonValue value_results;
  if (json_object.contains("results"_L1)) {
    value_results = json_object["results"_L1];
  }
  else if (json_object.contains("message"_L1)) {
    Error(json_object["message"_L1].toString());
    return;
  }
  else {
    Error(u"Json object is missing results."_s, json_object);
    return;
  }

  if (!value_results.isArray()) {
    Error(u"Missing results array."_s, value_results);
    return;
  }

  const QJsonArray array_results = value_results.toArray();
  for (const QJsonValue &value_result : array_results) {

    if (!value_result.isObject()) {
      Error(u"Invalid Json reply, results value is not a object."_s);
      continue;
    }
    const QJsonObject object_result = value_result.toObject();
    if (!object_result.contains("id"_L1) || !object_result.contains("title"_L1) || !object_result.contains("resource_url"_L1)) {
      Error(QStringLiteral("Invalid Json reply, results value object is missing ID, title or resource_url."), object_result);
      continue;
    }
    const quint64 release_id = static_cast<quint64>(object_result["id"_L1].toInt());
    const QUrl resource_url(object_result["resource_url"_L1].toString());
    QString title = object_result["title"_L1].toString();

    if (title.contains(" - "_L1)) {
      QStringList title_splitted = title.split(u" - "_s);
      if (title_splitted.count() == 2) {
        const QString artist = title_splitted.first();
        title = title_splitted.last();
        if (artist.compare(search->artist, Qt::CaseInsensitive) != 0 && title.compare(search->album, Qt::CaseInsensitive) != 0) continue;
      }
    }

    if (!resource_url.isValid()) continue;
    if (search->requests_release_.contains(release_id)) {
      continue;
    }
    StartReleaseRequest(search, release_id, resource_url);
  }

  if (search->requests_release_.count() == 0 && search->type == DiscogsCoverType::Master) {
    search->type = DiscogsCoverType::Release;
    queue_search_requests_.enqueue(search);
  }

}

void DiscogsCoverProvider::StartReleaseRequest(SharedPtr<DiscogsCoverSearchContext> search, const quint64 release_id, const QUrl &url) {

  DiscogsCoverReleaseContext release(search->id, release_id, url);
  search->requests_release_.insert(release_id, release);
  queue_release_requests_.enqueue(release);

  if (!timer_flush_requests_->isActive()) {
    timer_flush_requests_->start();
  }

}

void DiscogsCoverProvider::SendReleaseRequest(const DiscogsCoverReleaseContext &release) {

  QNetworkReply *reply = CreateRequest(release.url);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, release]() { HandleReleaseReply(reply, release.search_id, release.id); });

}

void DiscogsCoverProvider::HandleReleaseReply(QNetworkReply *reply, const int search_id, const quint64 release_id) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (!requests_search_.contains(search_id)) return;
  SharedPtr<DiscogsCoverSearchContext> search = requests_search_.value(search_id);

  if (!search->requests_release_.contains(release_id)) return;
  const DiscogsCoverReleaseContext &release = search->requests_release_.value(release_id);

  const QScopeGuard end_search = qScopeGuard([this, search, release]() { EndSearch(search, release.id); });

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject &json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    return;
  }

  if (!json_object.contains("artists"_L1) || !json_object.contains("title"_L1)) {
    Error(u"Json reply object is missing artists or title."_s, json_object);
    return;
  }

  if (!json_object.contains("images"_L1)) {
    return;
  }

  const QJsonValue value_artists = json_object["artists"_L1];
  if (!value_artists.isArray()) {
    Error(u"Json reply object artists is not a array."_s, value_artists);
    return;
  }
  const QJsonArray array_artists = value_artists.toArray();
  int i = 0;
  QString artist;
  for (const QJsonValue &value_artist : array_artists) {
    if (!value_artist.isObject()) {
      Error(u"Invalid Json reply, atists array value is not a object."_s);
      continue;
    }
    const QJsonObject object_artist = value_artist.toObject();
    if (!object_artist.contains("name"_L1)) {
      Error(u"Invalid Json reply, artists array value object is missing name."_s, object_artist);
      continue;
    }
    artist = object_artist["name"_L1].toString();
    ++i;
    if (artist == search->artist) break;
  }

  if (artist.isEmpty()) {
    return;
  }
  if (i > 1 && artist != search->artist) artist = "Various artists"_L1;

  const QString album = json_object["title"_L1].toString();
  if (artist != search->artist && album != search->album) {
    return;
  }

  const QJsonValue value_images = json_object["images"_L1];
  if (!value_images.isArray()) {
    Error(u"Json images is not an array."_s);
    return;
  }
  const QJsonArray array_images = value_images.toArray();

  if (array_images.isEmpty()) {
    Error(u"Invalid Json reply, images array is empty."_s);
    return;
  }

  for (const QJsonValue &value_image : array_images) {

    if (!value_image.isObject()) {
      Error(u"Invalid Json reply, images array value is not an object."_s);
      continue;
    }
    const QJsonObject obj_image = value_image.toObject();
    if (!obj_image.contains("type"_L1) || !obj_image.contains("resource_url"_L1) || !obj_image.contains("width"_L1) || !obj_image.contains("height"_L1)) {
      Error(u"Invalid Json reply, images array value object is missing type, resource_url, width or height."_s, obj_image);
      continue;
    }
    const QString type = obj_image["type"_L1].toString();
    if (type != "primary"_L1) {
      continue;
    }
    const int width = obj_image["width"_L1].toInt();
    const int height = obj_image["height"_L1].toInt();
    if (width < 300 || height < 300) continue;
    const float aspect_score = static_cast<float>(1.0) - static_cast<float>(std::max(width, height) - std::min(width, height)) / static_cast<float>(std::max(height, width));
    if (aspect_score < 0.85) continue;
    CoverProviderSearchResult result;
    result.artist = artist;
    result.album = album;
    result.image_url = QUrl(obj_image["resource_url"_L1].toString());
    if (result.image_url.isEmpty()) continue;
    search->results.append(result);
  }

  Q_EMIT SearchResults(search->id, search->results);
  search->results.clear();

}

void DiscogsCoverProvider::EndSearch(SharedPtr<DiscogsCoverSearchContext> search, const quint64 release_id) {

  if (search->requests_release_.contains(release_id)) {
    search->requests_release_.remove(release_id);
  }
  if (search->requests_release_.count() <= 0) {
    requests_search_.remove(search->id);
    Q_EMIT SearchFinished(search->id, search->results);
  }

  if (queue_release_requests_.isEmpty() && queue_search_requests_.isEmpty()) {
    timer_flush_requests_->stop();
  }

}

void DiscogsCoverProvider::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "Discogs:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
