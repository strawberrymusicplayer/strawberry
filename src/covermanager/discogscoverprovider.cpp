/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

//#include <qjson/parser.h>
#include <QJson/Parser>

#include <QVariant>
#include <QNetworkReply>
#include <QXmlStreamReader>
#include <QStringList>
#include <QUrlQuery>

#include "discogscoverprovider.h"

#include "core/closure.h"
#include "core/logging.h"
#include "core/network.h"
#include "core/utilities.h"

const char *DiscogsCoverProvider::kUrl = "https://api.discogs.com/database/search";

const char *DiscogsCoverProvider::kAccessKeyB64 = "dGh6ZnljUGJlZ1NEeXBuSFFxSVk=";
const char *DiscogsCoverProvider::kSecretAccessKeyB64 = "ZkFIcmlaSER4aHhRSlF2U3d0bm5ZVmdxeXFLWUl0UXI=";

DiscogsCoverProvider::DiscogsCoverProvider(QObject *parent) : CoverProvider("Discogs", parent), network_(new NetworkAccessManager(this)) {}

bool DiscogsCoverProvider::StartSearch(const QString &artist, const QString &album, int id) {

  //qLog(Debug) << __PRETTY_FUNCTION__;
  
  DiscogsCoverSearchContext *ctx = new DiscogsCoverSearchContext;
  ctx->id = id;
  ctx->artist = artist;
  ctx->album = album;
  ctx->state = DiscogsCoverSearchContext::State_Init;
  pending_requests_.insert(id, ctx);
  SendSearchRequest(ctx);
  
  return true;
  
}
  
void DiscogsCoverProvider::SendSearchRequest(DiscogsCoverSearchContext *ctx) {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;
  
  typedef QPair<QString, QString> Arg;
  typedef QList<Arg> ArgList;

  typedef QPair<QByteArray, QByteArray> EncodedArg;
  typedef QList<EncodedArg> EncodedArgList;

  QString type;
  
  switch (ctx->state) {
    case DiscogsCoverSearchContext::State_Init:
      type = "master";
      ctx->state = DiscogsCoverSearchContext::State_MastersRequested;
      break;
    case DiscogsCoverSearchContext::State_MastersRequested:
      type = "release";
      ctx->state = DiscogsCoverSearchContext::State_ReleasesRequested;
      break;
    default:
      EndSearch(ctx);
      return;
  }
  
  ArgList args = ArgList()
  << Arg("key", QByteArray::fromBase64(kAccessKeyB64))
  << Arg("secret", QByteArray::fromBase64(kSecretAccessKeyB64));
  
  if (!ctx->artist.isEmpty()) {
    args.append(Arg("artist", ctx->artist.toLower()));
  }
  if (!ctx->album.isEmpty()) {
    args.append(Arg("release_title", ctx->album.toLower()));
  }
  args.append(Arg("type", type));

  QUrlQuery url_query;
  QUrl url(kUrl);
  QStringList query_items;

  // Encode the arguments
  for (const Arg &arg : args) {
    EncodedArg encoded_arg(QUrl::toPercentEncoding(arg.first), QUrl::toPercentEncoding(arg.second));
    query_items << QString(encoded_arg.first + "=" + encoded_arg.second);
    url_query.addQueryItem(encoded_arg.first, encoded_arg.second);
  }

  // Sign the request
  const QByteArray data_to_sign = QString("GET\n%1\n%2\n%3").arg(url.host(), url.path(), query_items.join("&")).toLatin1();
  //const QByteArray signature(Utilities::HmacSha256(kSecretAccessKey, data_to_sign));
  const QByteArray signature(Utilities::HmacSha256(QByteArray::fromBase64(kSecretAccessKeyB64), data_to_sign));

  // Add the signature to the request
  url_query.addQueryItem("Signature", QUrl::toPercentEncoding(signature.toBase64()));

  url.setQuery(url_query);
  QNetworkReply *reply = network_->get(QNetworkRequest(url));

  NewClosure(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(QueryError(QNetworkReply::NetworkError, QNetworkReply*, int)), reply, ctx->id);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(HandleSearchReply(QNetworkReply*, int)), reply, ctx->id);

  return true;

}

void DiscogsCoverProvider::HandleSearchReply(QNetworkReply *reply, int id) {

  //qLog(Debug) << __PRETTY_FUNCTION__;
  
  //QString text(reply->readAll());
  //qLog(Debug) << text;

  reply->deleteLater();

  DiscogsCoverSearchContext *ctx;
  if (!pending_requests_.contains(id)) {
    // the request was cancelled while we were waiting for the reply
    qLog(Debug) << "Discogs: got reply for cancelled request" << id;
    return;
  }
  ctx = pending_requests_.value(id);

  QJson::Parser parser;
  bool ok;
  bool found = false;
  QVariantMap reply_map = parser.parse(reply, &ok).toMap();

  if (!ok || !reply_map.contains("results")) {
    // this is an error; either parse error or bad response from the server
    EndSearch(ctx);
    return;
  }

  QVariantList results = reply_map["results"].toList();

  for (const QVariant &result : results) {
    QVariantMap result_map = result.toMap();
    // In order to use less round-trips, we cheat here.  Instead of
    // following the "resource_url", and then scan all images in the
    // resource, we go directly to the largest primary image by
    // constructing the primary image's url from the thmub's url.
    if (result_map.contains("thumb")) {
      CoverSearchResult cover_result;
      cover_result.image_url = QUrl(result_map["thumb"].toString().replace("R-90-", "R-"));
      if (result_map.contains("title")) {
        cover_result.description = result_map["title"].toString();
      }
      ctx->results.append(cover_result);
      found = true;
    }
  }
  if (found) {
    EndSearch(ctx);
    return;
  }

  // otherwise, no results
  switch (ctx->state) {
    case DiscogsCoverSearchContext::State_MastersRequested:
      // search again, this time for releases
      SendSearchRequest(ctx);
      break;
    default:
      EndSearch(ctx);
      break;
  }

}

void DiscogsCoverProvider::CancelSearch(int id) {
    
  //qLog(Debug) << __PRETTY_FUNCTION__ << id;

  delete pending_requests_.take(id);
}

void DiscogsCoverProvider::QueryError(QNetworkReply::NetworkError error, QNetworkReply *reply, int id) {

  //qLog(Debug) << __PRETTY_FUNCTION__;

}

void DiscogsCoverProvider::EndSearch(DiscogsCoverSearchContext *ctx) {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;

  (void)pending_requests_.remove(ctx->id);
  emit SearchFinished(ctx->id, ctx->results);
  delete ctx;

}
