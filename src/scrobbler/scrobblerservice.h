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

#ifndef SCROBBLERSERVICE_H
#define SCROBBLERSERVICE_H

#include "config.h"

#include <QObject>
#include <QPair>
#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QJsonObject>

class Application;
class Song;

class ScrobblerService : public QObject {
  Q_OBJECT

 public:
  explicit ScrobblerService(const QString &name, Application *app, QObject *parent);

  QString name() const { return name_; }

  virtual void ReloadSettings() = 0;

  virtual bool IsEnabled() const { return false; }
  virtual bool IsAuthenticated() const { return false; }

  virtual void UpdateNowPlaying(const Song &song) = 0;
  virtual void ClearPlaying() = 0;
  virtual void Scrobble(const Song &song) = 0;
  virtual void Love() {}
  virtual void Error(const QString &error, const QVariant &debug = QVariant()) = 0;

  virtual void DoSubmit() = 0;
  virtual void Submitted() = 0;
  virtual bool IsSubmitted() const { return false; }

  typedef QPair<QString, QString> Param;
  typedef QPair<QByteArray, QByteArray> EncodedParam;
  typedef QList<Param> ParamList;

  QJsonObject ExtractJsonObj(const QByteArray &data, const bool ignore_empty = false);

 public slots:
  virtual void Submit() = 0;
  virtual void WriteCache() = 0;

 signals:
  void ErrorMessage(QString);

 private:
  QString name_;

};

#endif  // SCROBBLERSERVICE_H
