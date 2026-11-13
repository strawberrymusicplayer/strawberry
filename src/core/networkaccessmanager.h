/*
 * Strawberry Music Player
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef NETWORKACCESSMANAGER_H
#define NETWORKACCESSMANAGER_H

#include "config.h"

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkInformation>
#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QVersionNumber>

class QIODevice;
class QNetworkReply;

class NetworkAccessManager : public QNetworkAccessManager {
  Q_OBJECT

 public:
  explicit NetworkAccessManager(QObject *parent = nullptr);
  ~NetworkAccessManager() override;

 protected:
  QNetworkReply *createRequest(Operation op, const QNetworkRequest &network_request, QIODevice *outgoing_data) override;

 private Q_SLOTS:
  void ReachabilityChanged(const QNetworkInformation::Reachability reachability);
  void ClearCacheIfIdle();
  void ReplyDone(QObject *reply);
  void ReplyDeleted(QObject *reply);

 private:
  // Qt 6.5.10, 6.8.5, 6.9.2 and later cancel the saving of a reply in the cache when the reply is deleted.
  static bool QtCancelsSavingOfDeletedReply(const QVersionNumber &qt_version);
  void ReplyFinished(QNetworkReply *reply);
  bool SavingFinished(QNetworkReply *reply) const;
  void CancelUnusedSavings();

  friend class ThreadSafeNetworkDiskCacheNetworkTest;

  QHash<QObject*, QList<QMetaObject::Connection>> running_replies_;
  // All the replies until they are deleted, with the connection to their destroyed signal.
  QHash<QObject*, QMetaObject::Connection> replies_;
  bool clear_cache_pending_;
  // Before Qt 6.5.10, 6.8.5 and 6.9.2, a reply which failed before it got all the data never cancels its saving in the cache, so it's cancelled when the reply has finished.
  bool cancel_savings_of_finished_replies_;
};

#endif  // NETWORKACCESSMANAGER_H
