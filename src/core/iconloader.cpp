/*
 * Strawberry Music Player
 * Copyright 2013, 2017-2019, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QDir>
#include <QFile>
#include <QList>
#include <QString>
#include <QIcon>
#include <QSize>
#include <QStandardPaths>
#include <QSettings>

#include "core/logging.h"
#include "settings/appearancesettingspage.h"
#include "iconloader.h"

bool IconLoader::system_icons_ = false;
bool IconLoader::custom_icons_ = false;

void IconLoader::Init() {

  // TODO: Fix use system icons option properly.

  //QSettings s;
  //s.beginGroup(AppearanceSettingsPage::kSettingsGroup);
  //system_icons_ = s.value("system_icons", false).toBool();
  //s.endGroup();

  QDir dir;
  if (dir.exists(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/icons")) {
    custom_icons_ = true;
  }

}

QIcon IconLoader::Load(const QString &name, const int size) {

  QIcon ret;

  if (name.isEmpty()) {
    qLog(Error) << "Icon name is empty!";
    return ret;
  }

  QList<int> sizes;
  sizes.clear();
  if (size == 0) { sizes << 22 << 32 << 48 << 64; }
  else sizes << size;

  if (system_icons_) {
    ret = QIcon::fromTheme(name);
    if (!ret.isNull()) return ret;
    qLog(Warning) << "Couldn't load icon" << name << "from system theme icons.";
  }

  if (custom_icons_) {
    QString custom_icon_path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/icons/%1x%2/%3.png";
    for (int s : sizes) {
      QString filename(custom_icon_path.arg(s).arg(s).arg(name));
      if (QFile::exists(filename)) ret.addFile(filename, QSize(s, s));
    }
    if (!ret.isNull()) return ret;
    qLog(Warning) << "Couldn't load icon" << name << "from custom icons.";
  }

  const QString path(":/icons/%1x%2/%3.png");
  for (int s : sizes) {
    QString filename(path.arg(s).arg(s).arg(name));
    if (QFile::exists(filename)) ret.addFile(filename, QSize(s, s));
  }

  return ret;

}
