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

#ifndef LASTFMCOVERPROVIDER_H
#define LASTFMCOVERPROVIDER_H

#include "config.h"

#include <QMap>
#include <QObject>

#include "albumcoverfetcher.h"
#include "coverprovider.h"

#include <QMap>
#include <QObject>

class QNetworkAccessManager;
class QNetworkReply;

// A built-in cover provider which fetches covers from last.fm.
class LastFmCoverProvider : public CoverProvider {
  Q_OBJECT

public:
  explicit LastFmCoverProvider(QObject *parent = nullptr);

  bool StartSearch(const QString &artist, const QString &album, int id);

  static const char *kApiKey;
  static const char *kSecret;

private slots:
  void QueryFinished(QNetworkReply *reply, int id);

private:
  QNetworkAccessManager *network_;
  QMap <QNetworkReply *, int> pending_queries_;

};

#endif // LASTFMCOVERPROVIDER_H

