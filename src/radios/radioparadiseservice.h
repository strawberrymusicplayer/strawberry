/*
 * Strawberry Music Player
 * Copyright 2021-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef RADIOPARADISESERVICE_H
#define RADIOPARADISESERVICE_H

#include <QObject>
#include <QUrl>

#include "radioservice.h"
#include "radiochannel.h"

class TaskManager;
class NetworkAccessManager;
class QNetworkReply;

class RadioParadiseService : public RadioService {
  Q_OBJECT

 public:
  explicit RadioParadiseService(const SharedPtr<TaskManager> task_manager, const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

  QUrl Homepage() override;
  QUrl Donate() override;

  void Abort();

 public Q_SLOTS:
  void GetChannels() override;

 private Q_SLOTS:
  void GetChannelsReply(QNetworkReply *reply, const int task_id);

 private:
  QList<QNetworkReply*> replies_;
  RadioChannelList channels_;
};

#endif  // RADIOPARADISESERVICE_H
