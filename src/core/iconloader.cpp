/*
 * Strawberry Music Player
 * Copyright 2013, 2017-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <utility>

#include <QDir>
#include <QFile>
#include <QList>
#include <QString>
#include <QIcon>
#include <QSize>
#include <QSettings>

#include "logging.h"
#include "standardpaths.h"
#include "settings.h"
#include "includes/iconmapper.h"
#include "iconloader.h"
#include "constants/appearancesettings.h"

using namespace Qt::Literals::StringLiterals;

bool IconLoader::system_icons_ = false;
bool IconLoader::custom_icons_ = false;

void IconLoader::Init() {

#if !defined(Q_OS_MACOS) && !defined(Q_OS_WIN32)
  Settings s;
  s.beginGroup(AppearanceSettings::kSettingsGroup);
  system_icons_ = s.value("system_icons", false).toBool();
  s.endGroup();
#endif

  QDir dir;
  if (dir.exists(StandardPaths::WritableLocation(StandardPaths::StandardLocation::AppLocalDataLocation) + u"/icons"_s)) {
    custom_icons_ = true;
  }

}

QIcon IconLoader::Load(const QString &name, const bool system_icon, const int fixed_size, const int min_size, const int max_size) {

  QIcon ret;

  if (name.isEmpty()) {
    qLog(Error) << "Icon name is empty!";
    return ret;
  }

  QList<int> sizes;
  if (fixed_size == 0) {
    sizes << 22 << 32 << 48 << 64 << 128;
  }
  else {
    sizes << fixed_size;
  }

  if (system_icon && system_icons_) {
    IconMapper::IconProperties icon_prop;
    if (IconMapper::iconmapper_.contains(name)) {
      icon_prop = IconMapper::iconmapper_[name];
    }
    if (min_size != 0) icon_prop.min_size = min_size;
    if (max_size != 0) icon_prop.max_size = max_size;
    if (icon_prop.allow_system_icon) {
      ret = QIcon::fromTheme(name);
      if (ret.isNull()) {
        const QStringList alt_names = icon_prop.names;
        for (const QString &alt_name : alt_names) {
          ret = QIcon::fromTheme(alt_name);
          if (!ret.isNull()) break;
        }
        if (ret.isNull()) {
          qLog(Warning) << "Couldn't load icon" << name << "from system theme icons.";
        }
      }
      if (!ret.isNull()) {
        if (fixed_size != 0 && !ret.availableSizes().contains(QSize(fixed_size, fixed_size))) {
          qLog(Warning) << "Can't use system icon for" << name << "icon does not have fixed size." << fixed_size;
          ret = QIcon();
        }
        else {
          int size_smallest = 0;
          int size_largest = 0;
          const QList<QSize> available_sizes = ret.availableSizes();
          for (const QSize &s : available_sizes) {
            if (s.width() != s.height()) {
              qLog(Warning) << "Can't use system icon for" << name << "icon is not proportional.";
              ret = QIcon();
            }
            if (size_smallest == 0 || s.width() < size_smallest) size_smallest = s.width();
            if (s.width() > size_largest) size_largest = s.width();
          }
          if (size_smallest != 0 && icon_prop.min_size != 0 && size_smallest < icon_prop.min_size) {
            qLog(Warning) << "Can't use system icon for" << name << "icon too small." << size_smallest;
            ret = QIcon();
          }
          else if (size_largest != 0 && icon_prop.max_size != 0 && size_largest > icon_prop.max_size) {
            qLog(Warning) << "Can't use system icon for" << name << "icon too large." << size_largest;
            ret = QIcon();
          }
        }
      }
    }
    if (!ret.isNull()) return ret;
  }

  if (custom_icons_) {
    QString custom_icon_path = StandardPaths::WritableLocation(StandardPaths::StandardLocation::AppLocalDataLocation) + u"/icons/%1x%2/%3.png"_s;
    for (int s : std::as_const(sizes)) {
      QString filename(custom_icon_path.arg(s).arg(s).arg(name));
      if (QFile::exists(filename)) ret.addFile(filename, QSize(s, s));
    }
    if (!ret.isNull()) return ret;
    qLog(Warning) << "Couldn't load icon" << name << "from custom icons.";
  }

  const QString path(u":/icons/%1x%2/%3.png"_s);
  for (int s : std::as_const(sizes)) {
    QString filename(path.arg(s).arg(s).arg(name));
    if (QFile::exists(filename)) ret.addFile(filename, QSize(s, s));
  }

  if (ret.isNull() && !system_icons_ && !custom_icons_) {
    qLog(Error) << "Couldn't load icon" << name;
  }

  return ret;

}
