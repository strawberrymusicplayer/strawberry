/*
 * Strawberry Music Player
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

#include "version.h"

#include "gmock_include.h"

#ifdef GUI
#  include <QApplication>
#else
#  include <QCoreApplication>
#endif
#include <QString>
#include <QTemporaryDir>
#include <QSettings>
#include <QDebug>

#include "logging_env.h"
#include "metatypes_env.h"
#include "resources_env.h"

using namespace Qt::StringLiterals;

int main(int argc, char **argv) {

  testing::InitGoogleMock(&argc, argv);

  testing::AddGlobalTestEnvironment(new MetatypesEnvironment);
#ifdef GUI
  QApplication a(argc, argv);
#else
  QCoreApplication a(argc, argv);
#endif

  // Use the same application name and version as the application, the version is part of the user agent sent by the network tests.
  QCoreApplication::setApplicationName(u"Strawberry"_s);
  QCoreApplication::setApplicationVersion(QStringLiteral(STRAWBERRY_VERSION_DISPLAY));

  // Keep settings written by the tests out of the users own configuration.
  // Every test binary shares the application name set above, so give each process its own directory to keep them from writing to the same settings when run in parallel.
  // The directory is removed again when this goes out of scope.
  QTemporaryDir settings_dir;
  if (!settings_dir.isValid()) {
    qCritical() << "Failed to create temporary directory for settings:" << settings_dir.errorString();
    return 1;
  }

  // Settings uses the native format on Unix, where setPath() works, but the default format on Windows and macOS, where setPath() is documented to have no effect for the native format.
  // Changing the default format to INI is what makes the redirect take effect on those platforms.
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings_dir.path());
  QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settings_dir.path());

  testing::AddGlobalTestEnvironment(new ResourcesEnvironment);
  testing::AddGlobalTestEnvironment(new LoggingEnvironment);

  return RUN_ALL_TESTS();

}
