/*
 * Strawberry Music Player
 * Copyright 2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QString>
#include <QDir>
#include <QCoreApplication>

#include "standardpaths.h"

using namespace Qt::StringLiterals;

void StandardPaths::AppendOrganizationAndApplication(QString &path) {

  const QString organization_name = QCoreApplication::organizationName().toLower();
  if (!organization_name.isEmpty()) {
    path += u'/' + organization_name;
  }
  const QString application_name = QCoreApplication::applicationName().toLower();
  if (!application_name.isEmpty()) {
    path += u'/' + application_name;
  }

}

QString StandardPaths::WritableLocation(const StandardLocation type) {

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
  switch (type) {
    case StandardLocation::CacheLocation:
    case StandardLocation::GenericCacheLocation:{
      QString cache_location = qEnvironmentVariable("XDG_CACHE_HOME");
      if (!cache_location.startsWith(u'/')) {
        cache_location.clear();
      }
      if (cache_location.isEmpty()) {
        cache_location = QDir::homePath() + "/.cache"_L1;
      }
      if (type == QStandardPaths::CacheLocation) {
        AppendOrganizationAndApplication(cache_location);
      }
      return cache_location;
    }
    case StandardLocation::AppDataLocation:
    case StandardLocation::AppLocalDataLocation:
    case StandardLocation::GenericDataLocation:{
      QString data_location = qEnvironmentVariable("XDG_DATA_HOME");
      if (!data_location.startsWith(u'/')) {
        data_location.clear();
      }
      if (data_location.isEmpty()) {
        data_location = QDir::homePath() + "/.local/share"_L1;
      }
      if (type == StandardLocation::AppDataLocation || type == StandardLocation::AppLocalDataLocation) {
        AppendOrganizationAndApplication(data_location);
      }
      return data_location;
    }
    default:
      break;
  }
#endif

  return QStandardPaths::writableLocation(type);

}
