/*
 * Strawberry Music Player
 * Copyright 2024-2025, Jonas Kvinge <jonas@jkvinge.net>
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
#include <QString>
#include <QCoreApplication>

#include "settings.h"

using namespace Qt::Literals::StringLiterals;

Settings::Settings(QObject *parent)
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    : QSettings(QCoreApplication::organizationName().toLower(), QCoreApplication::applicationName().toLower(), parent) {}
#else
    : QSettings(parent) {}
#endif

Settings::Settings(const QSettings::Scope scope, QObject *parent)
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    : QSettings(scope, QCoreApplication::organizationName().toLower(), QCoreApplication::applicationName().toLower(), parent) {}
#else
    : QSettings(scope, parent) {}
#endif

Settings::Settings(const QString &filename, const Format format, QObject *parent)
    : QSettings(filename, format, parent) {}
