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
#include <QIcon>

#include "core/song.h"
#include "core/iconloader.h"
#include "playlist/playlistitem.h"
#include "settings/settingsdialog.h"
#include "internetsearch.h"

class Application;
class InternetServices;
class CollectionFilterWidget;

class InternetService : public QObject {
  Q_OBJECT

 public:
  InternetService(Song::Source source, const QString &name, const QString &url_scheme, Application *app, QObject *parent = nullptr);
  virtual ~InternetService() {}
  virtual Song::Source source() const { return source_; }
  virtual QString name() const { return name_; }
  virtual QString url_scheme() const { return url_scheme_; }
  virtual bool has_initial_load_settings() const { return false; }
  virtual void InitialLoadSettings() {}
  virtual void ReloadSettings() {}
  virtual QIcon Icon() { return Song::IconForSource(source_); }
  virtual int Search(const QString &query, InternetSearch::SearchType type) = 0;
  virtual void CancelSearch() = 0;

 public slots:
  virtual void ShowConfig() {}

 protected:
  Application *app_;
 private:
  Song::Source source_;
  QString name_;
  QString url_scheme_;

};
Q_DECLARE_METATYPE(InternetService*);

#endif
