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

#include <QSettings>
#include <QVariant>
#include <QString>

#include "settings.h"

Settings::Settings(QObject *parent)
    : QSettings(parent) {}

Settings::Settings(const QString &filename, const Format format, QObject *parent)
    : QSettings(filename, format, parent) {}

// Compatibility with older Qt versions

#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)

void Settings::beginGroup(const char *prefix) {

  QSettings::beginGroup(QLatin1String(prefix));

}

void Settings::beginGroup(const QString &prefix) {

  QSettings::beginGroup(prefix);

}

bool Settings::contains(const char *key) const {

  return QSettings::contains(QLatin1String(key));

}

bool Settings::contains(const QString &key) const {

  return QSettings::contains(key);

}

QVariant Settings::value(const char *key, const QVariant &default_value) const {

  return QSettings::value(QLatin1String(key), default_value);

}

QVariant Settings::value(const QString &key, const QVariant &default_value) const {

  return QSettings::value(key, default_value);

}

void Settings::setValue(const char *key, const QVariant &value) {

  QSettings::setValue(QLatin1String(key), value);

}

void Settings::setValue(const QString &key, const QVariant &value) {

  QSettings::setValue(key, value);

}

int Settings::beginReadArray(const char *prefix) {

  return QSettings::beginReadArray(QLatin1String(prefix));

}

int Settings::beginReadArray(const QString &prefix) {

  return QSettings::beginReadArray(prefix);

}

void Settings::beginWriteArray(const char *prefix, int size) {

  QSettings::beginWriteArray(QLatin1String(prefix), size);

}

void Settings::beginWriteArray(const QString &prefix, int size) {

  QSettings::beginWriteArray(prefix, size);

}

void Settings::remove(const char *key) {

  QSettings::remove(QLatin1String(key));

}

void Settings::remove(const QString &key) {

  QSettings::remove(key);

}

#endif
