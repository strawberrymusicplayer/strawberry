/*
 * Strawberry Music Player
 * Copyright 2020-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef MUSIXMATCHCOVERPROVIDER_H
#define MUSIXMATCHCOVERPROVIDER_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QVariant>
#include <QString>

#include "core/shared_ptr.h"
#include "jsoncoverprovider.h"
#include "providers/musixmatchprovider.h"

class QNetworkReply;
class NetworkAccessManager;

class MusixmatchCoverProvider : public JsonCoverProvider, MusixmatchProvider {
  Q_OBJECT

 public:
  explicit MusixmatchCoverProvider(Application *app, SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);
  ~MusixmatchCoverProvider() override;

  bool StartSearch(const QString &artist, const QString &album, const QString &title, const int id) override;
  void CancelSearch(const int id) override;

 private:
  void Error(const QString &error, const QVariant &debug = QVariant()) override;

 private slots:
  void HandleSearchReply(QNetworkReply *reply, const int id, const QString &artist, const QString &album);

 private:
  QList<QNetworkReply*> replies_;
};

#endif  // MUSIXMATCHCOVERPROVIDER_H
