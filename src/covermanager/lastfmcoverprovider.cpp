/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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
#include <QString>
#include <QStringBuilder>
#include <QUrl>
#include <QNetworkReply>

#include "core/closure.h"
#include "core/network.h"
#include "coverprovider.h"
#include "albumcoverfetcher.h"
#include "lastfmcompat.h"
#include "lastfmcoverprovider.h"

const char *LastFmCoverProvider::kApiKey = "211990b4c96782c05d1536e7219eb56e";
const char *LastFmCoverProvider::kSecret = "80fd738f49596e9709b1bf9319c444a8";

LastFmCoverProvider::LastFmCoverProvider(QObject *parent) : CoverProvider("last.fm", true, parent), network_(new NetworkAccessManager(this)) {
  lastfm::ws::ApiKey = kApiKey;
  lastfm::ws::SharedSecret = kSecret;
  lastfm::setNetworkAccessManager(network_);
}

bool LastFmCoverProvider::StartSearch(const QString &artist, const QString &album, int id) {

  QMap<QString, QString> params;
  params["method"] = "album.search";
  params["album"] = album + " " + artist;

  QNetworkReply *reply = lastfm::ws::post(params);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(QueryFinished(QNetworkReply*, int)), reply, id);

  return true;
}

void LastFmCoverProvider::QueryFinished(QNetworkReply *reply, int id) {

  reply->deleteLater();

  CoverSearchResults results;

  lastfm::XmlQuery query(lastfm::compat::EmptyXmlQuery());
  if (lastfm::compat::ParseQuery(reply->readAll(), &query)) {
    // parse the list of search results
    QList<lastfm::XmlQuery> elements = query["results"]["albummatches"].children("album");

    for (const lastfm::XmlQuery &element : elements) {
      CoverSearchResult result;
      result.description = element["artist"].text() + " - " + element["name"].text();
      result.image_url = QUrl(element["image size=extralarge"].text());
      if (result.image_url.isEmpty()) continue;
      results << result;
    }
  }
  else {
    // Drop through and emit an empty list of results.
  }

  emit SearchFinished(id, results);
}
