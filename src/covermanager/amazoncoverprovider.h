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

#ifndef AMAZONCOVERPROVIDER_H
#define AMAZONCOVERPROVIDER_H

#include "config.h"

#include <stdbool.h>

#include <QObject>
#include <QString>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QXmlStreamReader>

#include "coverprovider.h"
#include "albumcoverfetcher.h"

class AmazonCoverProvider : public CoverProvider {
  Q_OBJECT

 public:
  explicit AmazonCoverProvider(QObject *parent = nullptr);

  static const char *kUrl;
  static const char *kAssociateTag;

  static const char *kAccessKeyB64;
  static const char *kSecretAccessKeyB64;

  bool StartSearch(const QString &artist, const QString &album, int id);

 private slots:
  void QueryError(QNetworkReply::NetworkError error, QNetworkReply *reply, int id);
  void QueryFinished(QNetworkReply *reply, int id);

 private:
  void ReadItem(QXmlStreamReader *reader, CoverSearchResults *results);
  void ReadLargeImage(QXmlStreamReader *reader, CoverSearchResults *results);
  QNetworkAccessManager *network_;

};

#endif  // AMAZONCOVERPROVIDER_H
