/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, Martin Björklund <mbj4668@gmail.com>
 * Copyright 2016, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QPair>
#include <QVariant>
#include <QString>
#include <QStringBuilder>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>

#include "core/closure.h"
#include "core/logging.h"
#include "core/network.h"
#include "core/utilities.h"
#include "coverprovider.h"
#include "albumcoverfetcher.h"
#include "discogscoverprovider.h"

const char *DiscogsCoverProvider::kUrlSearch = "https://api.discogs.com/database/search";
const char *DiscogsCoverProvider::kUrlReleases = "https://api.discogs.com/releases";

const char *DiscogsCoverProvider::kAccessKeyB64 = "dGh6ZnljUGJlZ1NEeXBuSFFxSVk=";
const char *DiscogsCoverProvider::kSecretKeyB64 = "ZkFIcmlaSER4aHhRSlF2U3d0bm5ZVmdxeXFLWUl0UXI=";

DiscogsCoverProvider::DiscogsCoverProvider(QObject *parent) : CoverProvider("Discogs", false, parent), network_(new NetworkAccessManager(this)) {}

bool DiscogsCoverProvider::StartSearch(const QString &artist, const QString &album, int s_id) {

  DiscogsCoverSearchContext *s_ctx = new DiscogsCoverSearchContext;

  s_ctx->id = s_id;
  s_ctx->artist = artist;
  s_ctx->album = album;
  s_ctx->r_count = 0;
  requests_search_.insert(s_id, s_ctx);
  SendSearchRequest(s_ctx);

  return true;

}

void DiscogsCoverProvider::CancelSearch(int id) {
  delete requests_search_.take(id);
}


bool DiscogsCoverProvider::StartRelease(DiscogsCoverSearchContext *s_ctx, int r_id, QString resource_url) {

  DiscogsCoverReleaseContext *r_ctx = new DiscogsCoverReleaseContext;

  s_ctx->r_count++;

  r_ctx->id = r_id;
  r_ctx->resource_url = resource_url;

  r_ctx->s_id = s_ctx->id;

  requests_release_.insert(r_id, r_ctx);
  SendReleaseRequest(s_ctx, r_ctx);

  return true;

}

void DiscogsCoverProvider::SendSearchRequest(DiscogsCoverSearchContext *s_ctx) {

  typedef QPair<QString, QString> Arg;
  typedef QList<Arg> ArgList;

  typedef QPair<QByteArray, QByteArray> EncodedArg;
  typedef QList<EncodedArg> EncodedArgList;

  ArgList args = ArgList()
  << Arg("key", QByteArray::fromBase64(kAccessKeyB64))
  << Arg("secret", QByteArray::fromBase64(kSecretKeyB64));

  args.append(Arg("type", "release"));
  if (!s_ctx->artist.isEmpty()) {
    args.append(Arg("artist", s_ctx->artist.toLower()));
  }
  if (!s_ctx->album.isEmpty()) {
    args.append(Arg("release_title", s_ctx->album.toLower()));
  }

  QUrlQuery url_query;
  QStringList query_items;

  // Encode the arguments
  for (const Arg &arg : args) {
    EncodedArg encoded_arg(QUrl::toPercentEncoding(arg.first), QUrl::toPercentEncoding(arg.second));
    query_items << QString(encoded_arg.first + "=" + encoded_arg.second);
    url_query.addQueryItem(encoded_arg.first, encoded_arg.second);
  }

  QUrl url(kUrlSearch);
  url.setQuery(url_query);

  // Sign the request
  const QByteArray data_to_sign = QString("GET\n%1\n%2\n%3").arg(url.host(), url.path(), query_items.join("&")).toUtf8();
  const QByteArray signature(Utilities::HmacSha256(QByteArray::fromBase64(kSecretKeyB64), data_to_sign));

  // Add the signature to the request
  url_query.addQueryItem("Signature", QUrl::toPercentEncoding(signature.toBase64()));

  QNetworkReply *reply = network_->get(QNetworkRequest(url));
  NewClosure(reply, SIGNAL(finished()), this, SLOT(HandleSearchReply(QNetworkReply*, int)), reply, s_ctx->id);

}

void DiscogsCoverProvider::SendReleaseRequest(DiscogsCoverSearchContext *s_ctx, DiscogsCoverReleaseContext *r_ctx) {

  typedef QPair<QString, QString> Arg;
  typedef QList<Arg> ArgList;

  typedef QPair<QByteArray, QByteArray> EncodedArg;
  typedef QList<EncodedArg> EncodedArgList;

  QUrlQuery url_query;
  QStringList query_items;

  ArgList args = ArgList()
  << Arg("key", QByteArray::fromBase64(kAccessKeyB64))
  << Arg("secret", QByteArray::fromBase64(kSecretKeyB64));
  // Encode the arguments
  for (const Arg &arg : args) {
    EncodedArg encoded_arg(QUrl::toPercentEncoding(arg.first), QUrl::toPercentEncoding(arg.second));
    query_items << QString(encoded_arg.first + "=" + encoded_arg.second);
    url_query.addQueryItem(encoded_arg.first, encoded_arg.second);
  }

  QUrl url(r_ctx->resource_url);

  // Sign the request
  const QByteArray data_to_sign = QString("GET\n%1\n%2\n%3").arg(url.host(), url.path(), query_items.join("&")).toUtf8();
  const QByteArray signature(Utilities::HmacSha256(QByteArray::fromBase64(kSecretKeyB64), data_to_sign));

  // Add the signature to the request
  url_query.addQueryItem("Signature", QUrl::toPercentEncoding(signature.toBase64()));

  url.setQuery(url_query);

  QNetworkReply *reply = network_->get(QNetworkRequest(url));
  NewClosure(reply, SIGNAL(finished()), this, SLOT(HandleReleaseReply(QNetworkReply*, int, int)), reply, s_ctx->id, r_ctx->id);

}

QByteArray DiscogsCoverProvider::GetReplyData(QNetworkReply *reply) {

  QByteArray data;

  if (reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
    data = reply->readAll();
  }
  else {
    if (reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      QString failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      Error(failure_reason);
    }
    else {
      // See if there is Json data containing "message" - then use that instead.
      data = reply->readAll();
      QJsonParseError error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &error);
      QString failure_reason;
      if (error.error == QJsonParseError::NoError && !json_doc.isNull() && !json_doc.isEmpty() && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (json_obj.contains("message")) {
          failure_reason = json_obj["message"].toString();
        }
        else {
          failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
      }
      else {
        failure_reason = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
      }
      Error(failure_reason);
    }
    return QByteArray();
  }

  return data;

}

QJsonObject DiscogsCoverProvider::ExtractJsonObj(const QByteArray &data) {

  QJsonParseError error;
  QJsonDocument json_doc = QJsonDocument::fromJson(data, &error);

  if (error.error != QJsonParseError::NoError) {
    Error("Reply from server missing Json data.", data);
    return QJsonObject();
  }
  if (json_doc.isNull() || json_doc.isEmpty()) {
    Error("Received empty Json document.", json_doc);
    return QJsonObject();
  }
  if (!json_doc.isObject()) {
    Error("Json document is not an object.", json_doc);
    return QJsonObject();
  }
  QJsonObject json_obj = json_doc.object();
  if (json_obj.isEmpty()) {
    Error("Received empty Json object.", json_doc);
    return QJsonObject();
  }

  return json_obj;

}

QJsonValue DiscogsCoverProvider::ExtractData(const QByteArray &data, const QString name, const bool silent) {

  QJsonObject json_obj = ExtractJsonObj(data);
  if (json_obj.isEmpty()) return QJsonObject();

  if (json_obj.contains(name)) {
    QJsonValue json_results = json_obj[name];
    return json_results;
  }
  else if (json_obj.contains("message")) {
    QString message = json_obj["message"].toString();
    Error(QString("%1").arg(message));
  }
  else {
    if (!silent) Error(QString("Json reply is missing \"%1\".").arg(name), json_obj);
  }
  return QJsonValue();

}

void DiscogsCoverProvider::HandleSearchReply(QNetworkReply *reply, int s_id) {

  reply->deleteLater();

  if (!requests_search_.contains(s_id)) {
    Error(QString("Got reply for cancelled request: %1").arg(s_id));
    return;
  }
  DiscogsCoverSearchContext *s_ctx = requests_search_.value(s_id);

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    EndSearch(s_ctx);
    return;
  }

  QJsonValue json_value = ExtractData(data, "results");
  if (!json_value.isArray()) {
    EndSearch(s_ctx);
    return;
  }

  QJsonArray json_results = json_value.toArray();
  if (json_results.isEmpty()) {
    Error("Json array is empty.");
    EndSearch(s_ctx);
    return;
  }

  int i = 0;
  for (const QJsonValue &value : json_results) {
    if (!value.isObject()) {
      Error("Invalid Json reply, data is not an object.", value);
      continue;
    }
    QJsonObject json_obj = value.toObject();
    if (!json_obj.contains("id") || !json_obj.contains("title") || !json_obj.contains("resource_url")) {
      Error("Invalid Json reply, value is missing ID, title or resource_url.", json_obj);
      continue;
    }
    int r_id = json_obj["id"].toInt();
    QString title = json_obj["title"].toString();
    QString resource_url = json_obj["resource_url"].toString();
    if (resource_url.isEmpty()) continue;
    StartRelease(s_ctx, r_id, resource_url);
    i++;
  }

  if (i <= 0) {
    Error("Ending search with no results.");
    EndSearch(s_ctx);
  }

}

void DiscogsCoverProvider::HandleReleaseReply(QNetworkReply *reply, int s_id, int r_id) {

  reply->deleteLater();

  if (!requests_release_.contains(r_id)) {
    Error(QString("Got reply for cancelled request: %1 %2").arg(s_id).arg(r_id));
    return;
  }
  DiscogsCoverReleaseContext *r_ctx = requests_release_.value(r_id);

  if (!requests_search_.contains(s_id)) {
    Error(QString("Got reply for cancelled request: %1 %2").arg(s_id).arg(r_id));
    EndSearch(r_ctx);
    return;
  }
  DiscogsCoverSearchContext *s_ctx = requests_search_.value(s_id);

  QByteArray data = GetReplyData(reply);
  if (data.isEmpty()) {
    EndSearch(s_ctx);
    return;
  }

  QJsonValue json_value = ExtractData(data, "images", true);
  if (!json_value.isArray()) {
    EndSearch(s_ctx, r_ctx);
    return;
  }

  QJsonArray json_images = json_value.toArray();
  if (json_images.isEmpty()) {
    Error("Json array is empty.");
    EndSearch(s_ctx, r_ctx);
    return;
  }

  int i = 0;
  for (const QJsonValue &value : json_images) {
    if (!value.isObject()) {
      Error("Invalid Json reply, value is not an object.", value);
      continue;
    }
    QJsonObject json_obj = value.toObject();
    if (!json_obj.contains("type") || !json_obj.contains("resource_url")) {
      Error("Invalid Json reply, value is missing ID or resource_url.", json_obj);
      continue;
    }
    CoverSearchResult cover_result;
    cover_result.description = s_ctx->title;
    QString type = json_obj["type"].toString();
    i++;
    if (type != "primary") {
      continue;
    }
    cover_result.image_url = QUrl(json_obj["resource_url"].toString());
    if (cover_result.image_url.isEmpty()) continue;
    s_ctx->results.append(cover_result);
  }
  if (i <= 0) {
    Error("Ending search with no results.");
  }

  EndSearch(s_ctx, r_ctx);

}

void DiscogsCoverProvider::EndSearch(DiscogsCoverSearchContext *s_ctx, DiscogsCoverReleaseContext *r_ctx) {

  delete requests_release_.take(r_ctx->id);
  s_ctx->r_count--;
  if (s_ctx->r_count <= 0) EndSearch(s_ctx);

}

void DiscogsCoverProvider::EndSearch(DiscogsCoverSearchContext *s_ctx) {

  requests_search_.remove(s_ctx->id);
  emit SearchFinished(s_ctx->id, s_ctx->results);
  delete s_ctx;

}

void DiscogsCoverProvider::EndSearch(DiscogsCoverReleaseContext* r_ctx) {
  delete requests_release_.take(r_ctx->id);
}

void DiscogsCoverProvider::Error(QString error, QVariant debug) {
  qLog(Error) << "Discogs:" << error;
  if (debug.isValid()) qLog(Debug) << debug;
}
