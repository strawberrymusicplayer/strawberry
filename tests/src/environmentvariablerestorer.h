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

#ifndef ENVIRONMENTVARIABLERESTORER_H
#define ENVIRONMENTVARIABLERESTORER_H

#include <QtGlobal>
#include <QByteArray>
#include <QString>

// Restores an environment variable as it was when this was created, when it goes out of scope, so also when a test assertion fails.
// The variable is removed again if it was not set.
class EnvironmentVariableRestorer {
 public:
  explicit EnvironmentVariableRestorer(const char *name);
  ~EnvironmentVariableRestorer();

 private:
  Q_DISABLE_COPY_MOVE(EnvironmentVariableRestorer)

  const QByteArray name_;
  const bool was_set_;
#ifdef Q_OS_WIN32
  // The value is kept as a wide character string, since the ANSI code page can't represent all characters.
  const QString value_;
#else
  const QByteArray value_;
#endif
};

#endif  // ENVIRONMENTVARIABLERESTORER_H
