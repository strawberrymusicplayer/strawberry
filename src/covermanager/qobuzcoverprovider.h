/*
 * Strawberry Music Player
 * Copyright 2020-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef QOBUZCOVERPROVIDER_H
#define QOBUZCOVERPROVIDER_H

#include "config.h"

#include <QVariant>
#include <QString>

#include "includes/shared_ptr.h"
#include "jsoncoverprovider.h"

class QNetworkReply;
class NetworkAccessManager;
class QobuzService;

class QobuzCoverProvider : public JsonCoverProvider {
  Q_OBJECT

 public:
  explicit QobuzCoverProvider(const SharedPtr<QobuzService> service, SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

  virtual bool authenticated() const override;

  bool StartSearch(const QString &artist, const QString &album, const QString &title, const int id) override;
  void CancelSearch(const int id) override;
  void ClearSession() override;

 private Q_SLOTS:
  void HandleSearchReply(QNetworkReply *reply, const int id);

 private:
  JsonObjectResult ParseJsonObject(QNetworkReply *reply);
  void Error(const QString &error, const QVariant &debug = QVariant()) override;

 private:
  SharedPtr<QobuzService> service_;
};

#endif  // QOBUZCOVERPROVIDER_H
