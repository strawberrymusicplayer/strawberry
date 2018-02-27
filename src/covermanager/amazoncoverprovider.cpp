/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2013, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QDateTime>
#include <QNetworkReply>
#include <QStringList>
#include <QXmlStreamReader>
#include <QUrlQuery>

#include "amazoncoverprovider.h"

#include "core/closure.h"
#include "core/logging.h"
#include "core/network.h"
#include "core/utilities.h"

const char *AmazonCoverProvider::kUrl = "http://ecs.amazonaws.com/onca/xml";
const char *AmazonCoverProvider::kAssociateTag = "jonas052-20";

const char *AmazonCoverProvider::kAccessKeyB64 = "QUtJQUozQ1dIQ0RWSVlYN1JMTFE=";
const char *AmazonCoverProvider::kSecretAccessKeyB64 = "TjFZU3F2c2hJZDVtUGxKVW1Ka0kvc2E1WkZESG9TYy9ZWkgxYWdJdQ==";

AmazonCoverProvider::AmazonCoverProvider(QObject *parent) : CoverProvider("Amazon", parent), network_(new NetworkAccessManager(this)) {}

bool AmazonCoverProvider::StartSearch(const QString &artist, const QString &album, int id) {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;
  
  typedef QPair<QString, QString> Arg;
  typedef QList<Arg> ArgList;

  typedef QPair<QByteArray, QByteArray> EncodedArg;
  typedef QList<EncodedArg> EncodedArgList;
  
  // Must be sorted by parameter name
  ArgList args = ArgList()
                << Arg("AWSAccessKeyId", QByteArray::fromBase64(kAccessKeyB64))
		//<< Arg("AWSAccessKeyId", kAccessKey)
                << Arg("AssociateTag", kAssociateTag)
                << Arg("Keywords", artist + " " + album)
                << Arg("Operation", "ItemSearch")
                << Arg("ResponseGroup", "Images")
                << Arg("SearchIndex", "All")
                << Arg("Service", "AWSECommerceService")
                << Arg("Timestamp", QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss.zzzZ"))
                << Arg("Version", "2009-11-01");

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

  NewClosure(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(QueryError(QNetworkReply::NetworkError, QNetworkReply*, int)), reply, id);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(QueryFinished(QNetworkReply*, int)), reply, id);

  return true;
}

void AmazonCoverProvider::QueryError(QNetworkReply::NetworkError error, QNetworkReply *reply, int id) {

  //qLog(Debug) << __PRETTY_FUNCTION__;


}

void AmazonCoverProvider::QueryFinished(QNetworkReply *reply, int id) {
    
  //qLog(Debug) << __PRETTY_FUNCTION__;
    
  reply->deleteLater();
  
  QString data=(QString)reply->readAll();
  
  //qDebug() << data;

  CoverSearchResults results;

  QXmlStreamReader reader(data);
  while (!reader.atEnd()) {
    if (reader.readNext() == QXmlStreamReader::EndDocument) break;
    if (reader.tokenType() == QXmlStreamReader::Invalid	) { qLog(Debug) << reader.error() << reader.errorString(); break; }
    //qLog(Debug) << reader.tokenType() << reader.name();
    if (reader.tokenType() == QXmlStreamReader::StartElement && reader.name() == "Item") {
      ReadItem(&reader, &results);
    }
  }

  emit SearchFinished(id, results);

}

void AmazonCoverProvider::ReadItem(QXmlStreamReader *reader, CoverSearchResults *results) {

  //qLog(Debug) << __PRETTY_FUNCTION__ << "name: " << reader->name() << " text: " << reader->text();

  while (!reader->atEnd()) {
    switch (reader->readNext()) {
      case QXmlStreamReader::StartElement:
        if (reader->name() == "LargeImage") {
          ReadLargeImage(reader, results);
        }
        else {
          reader->skipCurrentElement();
        }
        break;

      case QXmlStreamReader::EndElement:
        return;

      default:
        break;
    }
  }
}

void AmazonCoverProvider::ReadLargeImage(QXmlStreamReader *reader, CoverSearchResults *results) {

  //qLog(Debug) << __PRETTY_FUNCTION__;

  while (!reader->atEnd()) {
    switch (reader->readNext()) {
      case QXmlStreamReader::StartElement:
        if (reader->name() == "URL") {
          CoverSearchResult result;
          result.image_url = QUrl(reader->readElementText());
          results->append(result);
        }
        else {
          reader->skipCurrentElement();
        }
        break;

      case QXmlStreamReader::EndElement:
        return;

      default:
        break;
    }
  }
}

