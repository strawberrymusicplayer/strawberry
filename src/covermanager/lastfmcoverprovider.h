/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QJsonObject>

#include "jsoncoverprovider.h"

class QNetworkAccessManager;
class QNetworkReply;
class Application;

class LastFmCoverProvider : public JsonCoverProvider {
  Q_OBJECT

 public:
  explicit LastFmCoverProvider(Application *app, QObject *parent = nullptr);
  ~LastFmCoverProvider();

  bool StartSearch(const QString &artist, const QString &album, const QString &title, const int id);

 private slots:
  void QueryFinished(QNetworkReply *reply, const int id, const QString &type);

 private:
  enum LastFmImageSize {
    Unknown,
    Small,
    Medium,
    Large,
    ExtraLarge
  };

  QByteArray GetReplyData(QNetworkReply *reply);
  LastFmImageSize ImageSizeFromString(const QString &size);
  void Error(const QString &error, const QVariant &debug = QVariant());

 private:
  static const char *kUrl;
  static const char *kApiKey;
  static const char *kSecret;

  QNetworkAccessManager *network_;
  QList<QNetworkReply*> replies_;

};

#endif  // LASTFMCOVERPROVIDER_H
