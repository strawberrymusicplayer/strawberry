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

#ifndef DISCOGSCOVERPROVIDER_H
#define DISCOGSCOVERPROVIDER_H

#include "config.h"

#include <QXmlStreamReader>
#include <QNetworkReply>

#include "coverprovider.h"

class QNetworkAccessManager;

// This struct represents a single search-for-cover request. It identifies and describes the request.
struct DiscogsCoverSearchContext {
  enum State { State_Init, State_MastersRequested, State_ReleasesRequested };

  // the unique request identifier
  int id;

  // the search query
  QString artist;
  QString album;

  State state;

  CoverSearchResults results;
};
Q_DECLARE_METATYPE(DiscogsCoverSearchContext)

class DiscogsCoverProvider : public CoverProvider {
  Q_OBJECT

 public:
  explicit DiscogsCoverProvider(QObject *parent = nullptr);

  static const char *kUrl;

  static const char *kRequestTokenURL;
  static const char *kAuthorizeURL;
  static const char *kAccessTokenURL;

  static const char *kAccessKey;
  static const char *kSecretAccessKey;

  static const char *kAccessKeyB64;
  static const char *kSecretAccessKeyB64;

  bool StartSearch(const QString &artist, const QString &album, int id);
  void CancelSearch(int id);

 private slots:
  void QueryError(QNetworkReply::NetworkError error, QNetworkReply *reply, int id);
  void HandleSearchReply(QNetworkReply* reply, int id);

 private:
  QNetworkAccessManager *network_;
  QHash<int, DiscogsCoverSearchContext*> pending_requests_;

  void SendSearchRequest(DiscogsCoverSearchContext *ctx);
  void ReadItem(QXmlStreamReader *reader, CoverSearchResults *results);
  void ReadLargeImage(QXmlStreamReader *reader, CoverSearchResults *results);
  void QueryFinished(QNetworkReply *reply, int id);
  void EndSearch(DiscogsCoverSearchContext *ctx);

};

#endif  // DISCOGSCOVERPROVIDER_H

