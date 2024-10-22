/*
 * Strawberry Music Player
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef URLHANDLERS_H
#define URLHANDLERS_H

#include "config.h"

#include <QObject>
#include <QMap>
#include <QString>
#include <QUrl>

class UrlHandler;

class UrlHandlers : public QObject {
  Q_OBJECT

 public:
  explicit UrlHandlers(QObject *parent = nullptr);

  void Register(UrlHandler *url_handler);
  void Unregister(UrlHandler *url_handler);
  void Destroyed(QObject *object);

  bool CanHandle(const QString &scheme) const;
  bool CanHandle(const QUrl &url) const;

  UrlHandler *GetUrlHandler(const QString &scheme) const;
  UrlHandler *GetUrlHandler(const QUrl &url) const;

 Q_SIGNALS:
  void Registered(UrlHandler *url_handler);
  void UnRegistered(UrlHandler *url_handler);

 private:
  QMap<QString, UrlHandler*> url_handlers_;
};

#endif  // URLHANDLERS_H
