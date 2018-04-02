/*
 * Strawberry Music Player
 * This code was part of Clementine.
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

//#ifdef HAVE_GIO
//#undef signals  // Clashes with GIO, and not needed in this file
#include <gio/gio.h>

#include <QApplication>
#include <QCoreApplication>
#include <QString>

#include "core/logging.h"

//namespace {

void ScanGIOModulePath() {
  QString gio_module_path;

#if defined(Q_OS_WIN32)
  gio_module_path = QCoreApplication::applicationDirPath() + "/gio-modules";
#endif

  if (!gio_module_path.isEmpty()) {
    qLog(Debug) << "Adding GIO module path:" << gio_module_path;
    QByteArray bytes = gio_module_path.toLocal8Bit();
    g_io_modules_scan_all_in_directory(bytes.data());
  }
}

//}  // namespace
//#endif  // HAVE_GIO
