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

#ifndef RADIOSERVICE_H
#define RADIOSERVICE_H

#include <QObject>
#include <QMetaType>
#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QUrl>
#include <QIcon>
#include <QJsonObject>

#include "core/shared_ptr.h"
#include "core/song.h"
#include "radiochannel.h"

class QNetworkReply;

class Application;
class NetworkAccessManager;

class RadioService : public QObject {
  Q_OBJECT

 public:
  explicit RadioService(const Song::Source source, const QString &name, const QIcon &icon, Application *app, SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

  Song::Source source() const { return source_; }
  QString name() const { return name_; }

  virtual void ReloadSettings() {}

  virtual QUrl Homepage() = 0;
  virtual QUrl Donate() = 0;

 signals:
  void NewChannels(const RadioChannelList &channels = RadioChannelList());

 public slots:
  virtual void GetChannels() = 0;

 protected:
  QByteArray ExtractData(QNetworkReply *reply);
  QJsonObject ExtractJsonObj(const QByteArray &data);
  QJsonObject ExtractJsonObj(QNetworkReply *reply);
  void Error(const QString &error, const QVariant &debug = QVariant());

 protected:
  Application *app_;
  SharedPtr<NetworkAccessManager> network_;
  Song::Source source_;
  QString name_;
  QIcon icon_;
};

Q_DECLARE_METATYPE(RadioService*)

#endif  // RADIOSERVICE_H
