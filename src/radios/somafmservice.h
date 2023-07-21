/*
 * Strawberry Music Player
 * Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SOMAFMSERVICE_H
#define SOMAFMSERVICE_H

#include <QObject>
#include <QList>
#include <QUrl>

#include "radioservice.h"
#include "radiochannel.h"

class QNetworkReply;
class NetworkAccessManager;

class SomaFMService : public RadioService {
  Q_OBJECT

 public:
  explicit SomaFMService(Application *app, SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);
  ~SomaFMService();

  QUrl Homepage() override;
  QUrl Donate() override;

  void Abort();

 public slots:
  void GetChannels() override;

 private:
  void GetStreamUrl(const int task_id, const RadioChannel &channel);

 private slots:
  void GetChannelsReply(QNetworkReply *reply, const int task_id);
  void GetStreamUrlsReply(QNetworkReply *reply, const int task_id, RadioChannel channel);

 private:
  static const char *kApiChannelsUrl;
  QList<QNetworkReply*> replies_;
  RadioChannelList channels_;
};

#endif  // SOMAFMSERVICE_H
