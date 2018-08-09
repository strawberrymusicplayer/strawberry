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

#ifndef INTERNETSERVICE_H
#define INTERNETSERVICE_H

#include <QObject>
#include <QStandardItem>
#include <QAction>
#include <QObject>
#include <QList>
#include <QString>
#include <QUrl>

#include "core/song.h"
#include "playlist/playlistitem.h"
#include "settings/settingsdialog.h"

class Application;
class InternetModel;
class CollectionFilterWidget;

class InternetService : public QObject {
  Q_OBJECT

 public:
  InternetService(const QString &name, Application *app, InternetModel *model, QObject *parent = nullptr);
  virtual ~InternetService() {}
  QString name() const { return name_; }
  InternetModel *model() const { return model_; }
  virtual bool has_initial_load_settings() const { return false; }
  virtual void InitialLoadSettings() {}
  virtual void ReloadSettings() {}
  virtual QString Icon() { return QString(); }

 public slots:
  virtual void ShowConfig() {}

 protected:
  Application *app_;
 private:
  InternetModel *model_;
  QString name_;

};
Q_DECLARE_METATYPE(InternetService*);

#endif
