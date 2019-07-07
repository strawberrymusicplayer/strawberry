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

#ifndef DISCOGSCOVERPROVIDER_H
#define DISCOGSCOVERPROVIDER_H

#include "config.h"

#include <stdbool.h>

#include <QObject>
#include <QHash>
#include <QMetaType>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "coverprovider.h"
#include "albumcoverfetcher.h"

class Application;

// This struct represents a single search-for-cover request. It identifies and describes the request.
struct DiscogsCoverSearchContext {
  DiscogsCoverSearchContext() : id(-1), r_count(0) {}

  // The unique request identifier
  int id;

  // The search query
  QString artist;
  QString album;
  int r_count;

  CoverSearchResults results;
};
Q_DECLARE_METATYPE(DiscogsCoverSearchContext)

// This struct represents a single release request. It identifies and describes the request.
struct DiscogsCoverReleaseContext {
  DiscogsCoverReleaseContext() : id(-1) {}

  int id;			// The unique request identifier
  int s_id;			// The search request identifier

  QString resource_url;
};
Q_DECLARE_METATYPE(DiscogsCoverReleaseContext)

class DiscogsCoverProvider : public CoverProvider {
  Q_OBJECT

 public:
  explicit DiscogsCoverProvider(Application *app, QObject *parent = nullptr);

  bool StartSearch(const QString &artist, const QString &album, int s_id);

  void CancelSearch(int id);

 private slots:
  void HandleSearchReply(QNetworkReply *reply, int s_id);
  void HandleReleaseReply(QNetworkReply *reply, int s_id, int r_id);

 private:
  static const char *kUrlSearch;
  static const char *kUrlReleases;
  static const char *kAccessKeyB64;
  static const char *kSecretKeyB64;

  QNetworkAccessManager *network_;
  QHash<int, DiscogsCoverSearchContext*> requests_search_;
  QHash<int, DiscogsCoverReleaseContext*> requests_release_;

  bool StartRelease(DiscogsCoverSearchContext *s_ctx, int r_id, QString &resource_url);

  void SendSearchRequest(DiscogsCoverSearchContext *s_ctx);
  void SendReleaseRequest(DiscogsCoverSearchContext *s_ctx, DiscogsCoverReleaseContext *r_ctx);
  QByteArray GetReplyData(QNetworkReply *reply);
  QJsonObject ExtractJsonObj(const QByteArray &data);
  QJsonValue ExtractData(const QByteArray &data, const QString name, const bool silent = false);
  void EndSearch(DiscogsCoverSearchContext *s_ctx, DiscogsCoverReleaseContext *r_ctx);
  void EndSearch(DiscogsCoverSearchContext *s_ctx);
  void EndSearch(DiscogsCoverReleaseContext *r_ctx);
  void Error(QString error, QVariant debug = QVariant());

};

#endif  // DISCOGSCOVERPROVIDER_H
