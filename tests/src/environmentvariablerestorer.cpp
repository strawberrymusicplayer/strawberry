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

#ifdef Q_OS_WIN32
#  include <cstdlib>
#endif

#include <QtGlobal>
#include <QByteArray>
#include <QString>

#include "environmentvariablerestorer.h"

EnvironmentVariableRestorer::EnvironmentVariableRestorer(const char *name)
    : name_(name),
      was_set_(qEnvironmentVariableIsSet(name)),
#ifdef Q_OS_WIN32
      value_(qEnvironmentVariable(name)) {}
#else
      value_(qgetenv(name)) {}
#endif

EnvironmentVariableRestorer::~EnvironmentVariableRestorer() {

  if (was_set_) {
#ifdef Q_OS_WIN32
    // Set the environment with the wide character API, qputenv() uses the ANSI code page.
    const QString env = QString::fromLatin1(name_) + u'=' + value_;
    _wputenv(reinterpret_cast<const wchar_t*>(env.utf16()));
#else
    qputenv(name_.constData(), value_);
#endif
  }
  else {
    qunsetenv(name_.constData());
  }

}
