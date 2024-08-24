/*
 * Strawberry Music Player
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef OVHLYRICSPROVIDER_H
#define OVHLYRICSPROVIDER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QVariant>
#include <QString>

#include "core/shared_ptr.h"
#include "jsonlyricsprovider.h"
#include "lyricssearchrequest.h"

class QNetworkReply;
class NetworkAccessManager;

class OVHLyricsProvider : public JsonLyricsProvider {
  Q_OBJECT

 public:
  explicit OVHLyricsProvider(SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);
  ~OVHLyricsProvider() override;

 private:
  void Error(const QString &error, const QVariant &debug = QVariant()) override;

 protected Q_SLOTS:
  void StartSearch(const int id, const LyricsSearchRequest &request) override;

 private Q_SLOTS:
  void HandleSearchReply(QNetworkReply *reply, const int id, const LyricsSearchRequest &request);

 private:
  QList<QNetworkReply*> replies_;
};

#endif  // OVHLYRICSPROVIDER_H
