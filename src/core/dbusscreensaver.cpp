/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include "config.h"

#include <QCoreApplication>
#include <QObject>
#include <QDBusInterface>
#include <QDBusReply>
#include <QString>

#include "dbusscreensaver.h"

DBusScreensaver::DBusScreensaver(const QString &service, const QString &path, const QString &interface)
    : service_(service), path_(path), interface_(interface) {}

void DBusScreensaver::Inhibit() {

  QDBusInterface gnome_screensaver("org.gnome.ScreenSaver", "/", "org.gnome.ScreenSaver");
  QDBusReply<quint32> reply = gnome_screensaver.call("Inhibit", QCoreApplication::applicationName(), QObject::tr("Visualizations"));
  if (reply.isValid()) {
    cookie_ = reply.value();
  }

}

void DBusScreensaver::Uninhibit() {

  QDBusInterface gnome_screensaver("org.gnome.ScreenSaver", "/", "org.gnome.ScreenSaver");
  gnome_screensaver.call("UnInhibit", cookie_);

}
