/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include <QObject>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QtConcurrentRun>
#include <QFuture>
#include <QString>
#include <QDir>
#include <QFile>

#include "core/utilities.h"

#ifdef HAVE_MOODBAR
#  include "ext/gstmoodbar/gstmoodbarplugin.h"
#endif

#include "gststartup.h"

GstStartup::GstStartup(QObject *parent) : QObject(parent) {
  initialising_ = QtConcurrent::run(this, &GstStartup::InitialiseGStreamer);
}

GstStartup::~GstStartup() {
  //gst_deinit();
}

void GstStartup::InitialiseGStreamer() {

  SetEnvironment();

  gst_init(nullptr, nullptr);
  gst_pb_utils_init();

#ifdef HAVE_MOODBAR
  gstfastspectrum_register_static();
#endif

}

void GstStartup::SetEnvironment() {

  QString gio_path;
  QString scanner_path;
  QString plugin_path;
  QString registry_filename;

// On Windows and macOS we bundle the gstreamer plugins with strawberry
#ifdef USE_BUNDLE
#if defined(Q_OS_WIN32) || defined(Q_OS_MACOS)
  gio_path = QCoreApplication::applicationDirPath() + "/" + USE_BUNDLE_DIR + "/gio-modules";
#endif
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
  scanner_path = QCoreApplication::applicationDirPath() + "/" + USE_BUNDLE_DIR + "/gst-plugin-scanner";
  plugin_path = QCoreApplication::applicationDirPath() + "/" + USE_BUNDLE_DIR + "/gstreamer";
#endif
#if defined(Q_OS_WIN32)
  plugin_path = QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + "/gstreamer-plugins");
#endif
#endif

#if defined(Q_OS_WIN32) || defined(Q_OS_MACOS)
  registry_filename = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QString("/gst-registry-%1-bin").arg(QCoreApplication::applicationVersion());
#endif

  if (!gio_path.isEmpty()) {
    //Utilities::SetEnv("GIO_MODULE_DIR", gio_path);
    Utilities::SetEnv("GIO_EXTRA_MODULES", gio_path);
  }

  if (!scanner_path.isEmpty()) Utilities::SetEnv("GST_PLUGIN_SCANNER", scanner_path);

  if (!plugin_path.isEmpty()) {
    Utilities::SetEnv("GST_PLUGIN_PATH", plugin_path);
    // Never load plugins from anywhere else.
    Utilities::SetEnv("GST_PLUGIN_SYSTEM_PATH", plugin_path);
  }

  if (!registry_filename.isEmpty()) {
#ifdef Q_OS_WIN32 // Workaround for issue #266 - TLS/SSL support not available; install glib-networking
    QFile registry_file(registry_filename);
    if (registry_file.exists()) {
      registry_file.remove();
    }
#endif
    Utilities::SetEnv("GST_REGISTRY", registry_filename);
  }

  Utilities::SetEnv("PULSE_PROP_media.role", "music");

}

