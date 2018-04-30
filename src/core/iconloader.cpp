/*
 * Strawberry Music Player
 * Copyright 2013, Jonas Kvinge <jonas@strawbs.net>
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

#include <QFile>
#include <QList>
#include <QString>
#include <QIcon>
#include <QSize>
#include <QtDebug>

#include "core/logging.h"
#include "iconloader.h"

QList<int> IconLoader::sizes_;
QString IconDefault(":/icons/64x64/strawberry.png");

void IconLoader::Init() {

  sizes_.clear();
  sizes_ << 22 << 32 << 48 << 64;

  if (!QFile::exists(IconDefault)) {
    qLog(Error) << "Default icon does not exist" << IconDefault;
  }

}

QIcon IconLoader::Load(const QString &name) {

  QIcon ret;

  if (name.isEmpty()) {
    qLog(Warning) << "Icon name is empty!";
    ret.addFile(IconDefault, QSize(64, 64));
    return ret;
  }

  const QString path(":icons/%1x%2/%3.png");
  for (int size : sizes_) {
    QString filename(path.arg(size).arg(size).arg(name));
    if (QFile::exists(filename)) ret.addFile(filename, QSize(size, size));
  }

  // Load icon from system theme only if it hasn't been found
  if (ret.isNull()) {
    ret = QIcon::fromTheme(name);
    if (!ret.isNull()) return ret;
    qLog(Warning) << "Couldn't load icon" << name;
  }

  if (ret.isNull()) {
    ret.addFile(IconDefault, QSize(64, 64));
  }

  return ret;

}
