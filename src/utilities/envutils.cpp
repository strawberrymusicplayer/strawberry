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
#include <QIODevice>
#include <QFile>
#include <QSettings>

#include "envutils.h"

using namespace Qt::Literals::StringLiterals;

namespace Utilities {

QString GetEnv(const QString &key) {
  const QByteArray key_data = key.toLocal8Bit();
  return QString::fromLocal8Bit(qgetenv(key_data.constData()));
}

void SetEnv(const char *key, const QString &value) {

#ifdef Q_OS_WIN32
  _putenv(QStringLiteral("%1=%2").arg(QLatin1String(key), value).toLocal8Bit().constData());
#else
  setenv(key, value.toLocal8Bit().constData(), 1);
#endif

}

QString DesktopEnvironment() {

  const QString de = GetEnv(u"XDG_CURRENT_DESKTOP"_s);
  if (!de.isEmpty()) return de;

  if (!qEnvironmentVariableIsEmpty("KDE_FULL_SESSION"))         return u"KDE"_s;
  if (!qEnvironmentVariableIsEmpty("GNOME_DESKTOP_SESSION_ID")) return u"Gnome"_s;

  QString session = GetEnv(u"DESKTOP_SESSION"_s);
  qint64 slash = session.lastIndexOf(u'/');
  if (slash != -1) {
    QSettings desktop_file(QStringLiteral("%1.desktop").arg(session), QSettings::IniFormat);
    desktop_file.beginGroup(u"Desktop Entry"_s);
    QString name = desktop_file.value(u"DesktopNames"_s).toString();
    desktop_file.endGroup();
    if (!name.isEmpty()) return name;
    session = session.mid(slash + 1);
  }

  if (session == "kde"_L1)           return u"KDE"_s;
  else if (session == "gnome"_L1)    return u"Gnome"_s;
  else if (session == "xfce"_L1)     return u"XFCE"_s;

  return u"Unknown"_s;

}

bool IsWSL() {

#ifdef Q_OS_LINUX
  // The Windows Subsystem for Linux exposes the Windows kernel version, which contains "microsoft" / "WSL".
  QFile osrelease(u"/proc/sys/kernel/osrelease"_s);
  if (osrelease.open(QIODevice::ReadOnly | QIODevice::Text)) {
    const QString contents = QString::fromLatin1(osrelease.readAll()).toLower();
    osrelease.close();
    if (contents.contains("microsoft"_L1, Qt::CaseInsensitive) || contents.contains("wsl"_L1, Qt::CaseInsensitive)) {
      return true;
    }
  }

  if (!qEnvironmentVariableIsEmpty("WSL_DISTRO_NAME") || !qEnvironmentVariableIsEmpty("WSL_INTEROP")) {
    return true;
  }
#endif

  return false;

}

}  // namespace Utilities
