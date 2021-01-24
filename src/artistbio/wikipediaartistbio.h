/*
 * Strawberry Music Player
 * Copyright 2020, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef WIKIPEDIAARTISTBIO_H
#define WIKIPEDIAARTISTBIO_H

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QSslError>
#include <QJsonObject>

#include "artistbioprovider.h"

class QNetworkReply;
class CountdownLatch;
class NetworkAccessManager;

class WikipediaArtistBio : public ArtistBioProvider {
  Q_OBJECT

 public:
  explicit WikipediaArtistBio();
  ~WikipediaArtistBio();

  void Start(const int id, const Song &song) override;

 private:
  typedef QPair<QString, QString> Param;
  typedef QList<Param> ParamList;

  QNetworkReply *CreateRequest(QList<Param> &params);
  QByteArray GetReplyData(QNetworkReply *reply);
  QJsonObject ExtractJsonObj(const QByteArray &data);
  void GetArticle(const int id, const QString &artist, CountdownLatch* latch);
  void GetImageTitles(const int id, const QString &artist, CountdownLatch* latch);
  void GetImage(const int id, const QString &title, CountdownLatch *latch);
  QList<QUrl> ExtractImageUrls(QJsonObject json_obj);

 private slots:
  void HandleSSLErrors(QList<QSslError> ssl_errors);
  void GetArticleReply(QNetworkReply *reply, const int id, CountdownLatch *latch);
  void GetImageTitlesFinished(QNetworkReply *reply, const int id, CountdownLatch *latch);
  void GetImageFinished(QNetworkReply *reply, const int id, CountdownLatch *latch);

 private:
  static const char *kApiUrl;
  static const int kMinimumImageSize;

  NetworkAccessManager *network_;
  QList<QNetworkReply*> replies_;

};

#endif  // WIKIPEDIAARTISTBIO_H
