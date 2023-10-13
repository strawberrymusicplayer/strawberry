/*
 * Strawberry Music Player
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <cstdlib>

#include <QtGlobal>
#include <QByteArray>
#include <QString>
#include <QSettings>

#include "envutils.h"

namespace Utilities {

QString GetEnv(const QString &key) {
  const QByteArray key_data = key.toLocal8Bit();
  return QString::fromLocal8Bit(qgetenv(key_data.constData()));
}

void SetEnv(const char *key, const QString &value) {

#ifdef Q_OS_WIN32
  _putenv(QString("%1=%2").arg(key, value).toLocal8Bit().constData());
#else
  setenv(key, value.toLocal8Bit().constData(), 1);
#endif

}

QString DesktopEnvironment() {

  const QString de = GetEnv("XDG_CURRENT_DESKTOP");
  if (!de.isEmpty()) return de;

  if (!qEnvironmentVariableIsEmpty("KDE_FULL_SESSION"))         return "KDE";
  if (!qEnvironmentVariableIsEmpty("GNOME_DESKTOP_SESSION_ID")) return "Gnome";

  QString session = GetEnv("DESKTOP_SESSION");
  qint64 slash = session.lastIndexOf('/');
  if (slash != -1) {
    QSettings desktop_file(QString("%1.desktop").arg(session), QSettings::IniFormat);
    desktop_file.beginGroup("Desktop Entry");
    QString name = desktop_file.value("DesktopNames").toString();
    desktop_file.endGroup();
    if (!name.isEmpty()) return name;
    session = session.mid(slash + 1);
  }

  if (session == "kde")           return "KDE";
  else if (session == "gnome")    return "Gnome";
  else if (session == "xfce")     return "XFCE";

  return "Unknown";

}

}  // namespace Utilities
