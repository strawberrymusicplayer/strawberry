/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QByteArray>
#include <QString>
#include <QCoreApplication>
#include <QSysInfo>

#include "envutils.h"
#include "useragent.h"

using namespace Qt::Literals::StringLiterals;

namespace Utilities {

const QByteArray &UserAgent() {

  static const QByteArray user_agent = "%1/%2 (%3 %4; +https://www.strawberrymusicplayer.org)"_L1.arg(QCoreApplication::applicationName(), QCoreApplication::applicationVersion(), Utilities::OSName(), QSysInfo::currentCpuArchitecture()).toUtf8();

  return user_agent;

}

}  // namespace Utilities
