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
  enum State { State_Init, State_Release };

  // the unique request identifier
  int id;

  // the search query
  QString artist;
  QString album;
  QString title;
  int r_count;

  State state;

  CoverSearchResults results;
};
Q_DECLARE_METATYPE(DiscogsCoverSearchContext)

struct DiscogsCoverReleaseContext {
  //enum State { State_Init, State_MastersRequested, State_ReleasesRequested };

  int id;			// the unique request identifier
  int s_id;			// the search request identifier

  // the search query
  //QString artist;
  //QString album;
  //QString title;
  QString resource_url;

  //State state;

  //CoverSearchResults results;
};
Q_DECLARE_METATYPE(DiscogsCoverReleaseContext)

class DiscogsCoverProvider : public CoverProvider {
  Q_OBJECT

 public:
  explicit DiscogsCoverProvider(QObject *parent = nullptr);

  static const char *kUrlSearch;
  static const char *kUrlReleases;

  static const char *kRequestTokenURL;
  static const char *kAuthorizeURL;
  static const char *kAccessTokenURL;

  static const char *kAccessKey;
  static const char *kSecretAccessKey;

  static const char *kAccessKeyB64;
  static const char *kSecretAccessKeyB64;

  bool StartSearch(const QString &artist, const QString &album, int s_id);
  
  void CancelSearch(int id);

 private slots:
  void SearchRequestError(QNetworkReply::NetworkError error, QNetworkReply *reply, int s_id);
  void ReleaseRequestError(QNetworkReply::NetworkError error, QNetworkReply *reply, int s_id, int r_id);
  void HandleSearchReply(QNetworkReply *reply, int s_id);
  void HandleReleaseReply(QNetworkReply *reply, int sa_id, int si_id);

 private:
  QNetworkAccessManager *network_;
  QHash<int, DiscogsCoverSearchContext*> requests_search_;
  QHash<int, DiscogsCoverReleaseContext*> requests_release_;
  
  bool StartRelease(DiscogsCoverSearchContext *s_ctx, int r_id, QString resource_url);

  void SendSearchRequest(DiscogsCoverSearchContext *s_ctx);
  void SendReleaseRequest(DiscogsCoverSearchContext *s_ctx, DiscogsCoverReleaseContext *r_ctx);
  void EndSearch(DiscogsCoverSearchContext *s_ctx, DiscogsCoverReleaseContext *r_ctx);
  void EndSearch(DiscogsCoverSearchContext *s_ctx);

};

#endif  // DISCOGSCOVERPROVIDER_H

