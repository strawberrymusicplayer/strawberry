/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QVariant>
#include <QString>

#include "includes/shared_ptr.h"
#include "jsoncoverprovider.h"

class NetworkAccessManager;
class QNetworkReply;

class LastFmCoverProvider : public JsonCoverProvider {
  Q_OBJECT

 public:
  explicit LastFmCoverProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

  bool authentication_required() const override { return true; }

  bool StartSearch(const QString &artist, const QString &album, const QString &title, const int id) override;

 private Q_SLOTS:
  void QueryFinished(QNetworkReply *reply, const int id, const QString &type);

 protected:
  JsonObjectResult ParseJsonObject(QNetworkReply *reply);

 private:
  enum class LastFmImageSize {
    Unknown,
    Small = 34,
    Medium = 64,
    Large = 174,
    ExtraLarge = 300
  };

  static LastFmImageSize ImageSizeFromString(const QString &size);
  void Error(const QString &error, const QVariant &debug = QVariant()) override;
};

#endif  // LASTFMCOVERPROVIDER_H
