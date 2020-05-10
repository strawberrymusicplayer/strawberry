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

#ifndef TIDALCOVERPROVIDER_H
#define TIDALCOVERPROVIDER_H

#include "config.h"

#include <QObject>
#include <QPair>
#include <QSet>
#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QJsonValue>
#include <QJsonObject>

#include "jsoncoverprovider.h"
#include "tidal/tidalservice.h"

class QNetworkAccessManager;
class QNetworkReply;
class Application;

class TidalCoverProvider : public JsonCoverProvider {
  Q_OBJECT

 public:
  explicit TidalCoverProvider(Application *app, QObject *parent = nullptr);
  bool StartSearch(const QString &artist, const QString &album, const QString &title, const int id);
  void CancelSearch(const int id);

  bool IsAuthenticated() const { return service_ && service_->authenticated(); }
  void Deauthenticate() { if (service_) service_->Logout(); }

 private slots:
  void HandleSearchReply(QNetworkReply *reply, const int id);

 private:
  QByteArray GetReplyData(QNetworkReply *reply);
  void Error(const QString &error, const QVariant &debug = QVariant());

 private:
  static const char *kApiUrl;
  static const char *kResourcesUrl;
  static const int kLimit;

  TidalService *service_;
  QNetworkAccessManager *network_;

};

#endif  // TIDALCOVERPROVIDER_H
