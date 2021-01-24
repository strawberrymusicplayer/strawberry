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

#ifndef LASTFMARTISTBIO_H
#define LASTFMARTISTBIO_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QString>

#include "core/song.h"

#include "artistbioprovider.h"

class NetworkAccessManager;
class QNetworkReply;

class LastFMArtistBio : public ArtistBioProvider {
  Q_OBJECT

 public:
  explicit LastFMArtistBio();
  ~LastFMArtistBio();

  void Start(const int id, const Song &song) override;

 private:
  typedef QPair<QString, QString> Param;
  typedef QList<Param> ParamList;

  QNetworkReply *CreateRequest(const ParamList &request_params);
  QByteArray GetReplyData(QNetworkReply *reply);
  QJsonObject ExtractJsonObj(const QByteArray &data);
  void Error(const QString &error, const QVariant &debug = QVariant());

 private slots:
  void RequestFinished(QNetworkReply *reply, const int id);

 private:
  NetworkAccessManager *network_;
  QList<QNetworkReply*> replies_;

};

#endif  // LASTFMARTISTBIO_H
