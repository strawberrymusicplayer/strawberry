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
#include <QtConcurrent>
#include <QFuture>
#include <QString>
#include <QDir>
#include <QFile>

#include "core/logging.h"
#include "core/utilities.h"

#ifdef HAVE_MOODBAR
#  include "ext/gstmoodbar/gstmoodbarplugin.h"
#endif

#include "gststartup.h"

GstStartup::GstStartup(QObject *parent) : QObject(parent) {
  initialising_ = QtConcurrent::run([=]{ InitialiseGStreamer(); });
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

  QString bundle_path = QCoreApplication::applicationDirPath();

#ifdef USE_BUNDLE_DIR
  QString bundle_dir = USE_BUNDLE_DIR;
  if (!bundle_dir.isEmpty()) {
    bundle_path.append("/" + bundle_dir);
  }
#endif

  QString gio_module_path;
  QString gst_plugin_scanner;
  QString gst_plugin_path;
  QString gst_registry_filename;

#ifdef USE_BUNDLE
#if defined(Q_OS_WIN32) || defined(Q_OS_MACOS)
  gio_module_path = bundle_path + "/gio-modules";
#endif
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
  gst_plugin_scanner = bundle_path + "/gst-plugin-scanner";
  gst_plugin_path = bundle_path + "/gstreamer";
#endif
#if defined(Q_OS_WIN32)
  //gst_plugin_scanner = bundle_path + "/gst-plugin-scanner.exe";
  gst_plugin_path = bundle_path + "/gstreamer-plugins";
#endif
#endif

#if defined(Q_OS_WIN32) || defined(Q_OS_MACOS)
  gst_registry_filename = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QString("/gst-registry-%1-bin").arg(QCoreApplication::applicationVersion());
#endif

  if (!gio_module_path.isEmpty()) {
    if (QDir(gio_module_path).exists()) {
      qLog(Debug) << "Setting GIO module path to" << gio_module_path;
      Utilities::SetEnv("GIO_EXTRA_MODULES", gio_module_path);
    }
    else {
      qLog(Debug) << "GIO module path does not exist:" << gio_module_path;
    }
  }

  if (!gst_plugin_scanner.isEmpty()) {
    if (QFile(gst_plugin_scanner).exists()) {
      qLog(Debug) << "Setting GST plugin scanner to" << gst_plugin_scanner;
      Utilities::SetEnv("GST_PLUGIN_SCANNER", gst_plugin_scanner);
    }
    else {
      qLog(Debug) << "GST plugin scanner does not exist:" << gst_plugin_scanner;
    }
  }

  if (!gst_plugin_path.isEmpty()) {
    if (QDir(gst_plugin_path).exists()) {
      qLog(Debug) << "Setting GST plugin path to" << gst_plugin_path;
      Utilities::SetEnv("GST_PLUGIN_PATH", gst_plugin_path);
      // Never load plugins from anywhere else.
      Utilities::SetEnv("GST_PLUGIN_SYSTEM_PATH", gst_plugin_path);
    }
    else {
      qLog(Debug) << "GST plugin path does not exist:" << gst_plugin_path;
    }
  }

  if (!gst_registry_filename.isEmpty()) {
    qLog(Debug) << "Setting GST registry file to" << gst_registry_filename;
    Utilities::SetEnv("GST_REGISTRY", gst_registry_filename);
  }

  Utilities::SetEnv("PULSE_PROP_media.role", "music");

}
