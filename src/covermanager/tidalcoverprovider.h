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

#include <stdbool.h>

#include <QObject>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>

#include "coverprovider.h"
#include "tidal/tidalservice.h"

class Application;

class TidalCoverProvider : public CoverProvider {
  Q_OBJECT

 public:
  explicit TidalCoverProvider(Application *app, QObject *parent = nullptr);
  void SetService(TidalService *service);
  void ReloadSettings();
  bool StartSearch(const QString &artist, const QString &album, int id);
  void CancelSearch(int id);

 private slots:
  void HandleSearchReply(QNetworkReply *reply, int id);

 private:
  typedef QPair<QString, QString> Param;
  static const char *kApiUrl;
  static const char *kResourcesUrl;
  static const char *kApiTokenB64;
  static const int kLimit;

  QNetworkReply *CreateRequest(const QString &ressource_name, const QList<Param> &params_supplied);
  QByteArray GetReplyData(QNetworkReply *reply, QString &error);
  QJsonObject ExtractJsonObj(QByteArray &data, QString &error);
  QJsonValue ExtractItems(QByteArray &data, QString &error);
  QJsonValue ExtractItems(QJsonObject &json_obj, QString &error);
  QString Error(QString error, QVariant debug = QVariant());

  TidalService *service_;
  QNetworkAccessManager *network_;

};

#endif  // TIDALCOVERPROVIDER_H
