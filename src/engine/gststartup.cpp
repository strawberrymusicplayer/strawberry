/*
 * Strawberry Music Player
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <cstring>
#include <glib.h>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include <QObject>
#include <QMetaObject>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QtConcurrent>
#include <QString>
#include <QDir>
#include <QFile>
#include <QAbstractEventDispatcher>

#include "core/logging.h"
#include "utilities/envutils.h"

#ifdef HAVE_MOODBAR
#  include "ext/gstmoodbar/gstmoodbarplugin.h"
#endif

#include "gststartup.h"

GThread *GstStartup::kGThread = nullptr;

gpointer GstStartup::GLibMainLoopThreadFunc(gpointer) {

  qLog(Info) << "Creating GLib main event loop.";

  GMainLoop *gloop = g_main_loop_new(nullptr, false);
  g_main_loop_run(gloop);
  g_main_loop_unref(gloop);

  return nullptr;

}

GstStartup::GstStartup(QObject *parent) : QObject(parent) {

  initializing_ = QtConcurrent::run(&GstStartup::InitializeGStreamer);

  const QMetaObject *mo = QAbstractEventDispatcher::instance(qApp->thread())->metaObject();
  if (mo && strcmp(mo->className(), "QEventDispatcherGlib") != 0 && strcmp(mo->superClass()->className(), "QEventDispatcherGlib") != 0) {
    kGThread = g_thread_new(nullptr, GstStartup::GLibMainLoopThreadFunc, nullptr);
  }

}

GstStartup::~GstStartup() {
  if (kGThread) g_thread_unref(kGThread);
}

void GstStartup::InitializeGStreamer() {

  SetEnvironment();

  gst_init(nullptr, nullptr);
  gst_pb_utils_init();

#ifdef HAVE_MOODBAR
  gstfastspectrum_register_static();
#endif

#ifdef Q_OS_WIN32
  // Use directsoundsink by default because of buggy wasapi plugin.
  GstRegistry *reg = gst_registry_get();
  if (reg) {
    GstPluginFeature *directsoundsink = gst_registry_lookup_feature(reg, "directsoundsink");
    GstPluginFeature *wasapisink = gst_registry_lookup_feature(reg, "wasapisink");
    if (directsoundsink && wasapisink) {
      gst_plugin_feature_set_rank(directsoundsink, GST_RANK_PRIMARY);
      gst_plugin_feature_set_rank(wasapisink, GST_RANK_SECONDARY);
    }
    if (directsoundsink) gst_object_unref(directsoundsink);
    if (wasapisink) gst_object_unref(wasapisink);
  }
#endif

}

void GstStartup::SetEnvironment() {

#ifdef USE_BUNDLE

  QString app_path = QCoreApplication::applicationDirPath();
  QString bundle_path = app_path + "/" + USE_BUNDLE_DIR;

  QString gio_module_path;
  QString gst_plugin_scanner;
  QString gst_plugin_path;
  QString libsoup_library_path;

#  if defined(Q_OS_WIN32) || defined(Q_OS_MACOS)
  gio_module_path = bundle_path + "/gio-modules";
#  endif
#  if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
  gst_plugin_scanner = bundle_path + "/gst-plugin-scanner";
  gst_plugin_path = bundle_path + "/gstreamer";
#  endif
#  if defined(Q_OS_WIN32)
  gst_plugin_path = bundle_path + "/gstreamer-plugins";
#  endif
#  if defined(Q_OS_MACOS)
  libsoup_library_path = app_path + "/../Frameworks/libsoup-3.0.0.dylib";
#  endif

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
    if (QFile::exists(gst_plugin_scanner)) {
      qLog(Debug) << "Setting GStreamer plugin scanner to" << gst_plugin_scanner;
      Utilities::SetEnv("GST_PLUGIN_SCANNER", gst_plugin_scanner);
    }
    else {
      qLog(Debug) << "GStreamer plugin scanner does not exist:" << gst_plugin_scanner;
    }
  }

  if (!gst_plugin_path.isEmpty()) {
    if (QDir(gst_plugin_path).exists()) {
      qLog(Debug) << "Setting GStreamer plugin path to" << gst_plugin_path;
      Utilities::SetEnv("GST_PLUGIN_PATH", gst_plugin_path);
      // Never load plugins from anywhere else.
      Utilities::SetEnv("GST_PLUGIN_SYSTEM_PATH", gst_plugin_path);
    }
    else {
      qLog(Debug) << "GStreamer plugin path does not exist:" << gst_plugin_path;
    }
  }

  if (!libsoup_library_path.isEmpty()) {
    if (QFile::exists(libsoup_library_path)) {
      Utilities::SetEnv("LIBSOUP3_LIBRARY_PATH", libsoup_library_path);
    }
    else {
      qLog(Debug) << "libsoup path does not exist:" << libsoup_library_path;
    }
  }

#endif  // USE_BUNDLE


#if defined(Q_OS_WIN32) || defined(Q_OS_MACOS)
  QString gst_registry_filename = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QString("/gst-registry-%1-bin").arg(QCoreApplication::applicationVersion());
  qLog(Debug) << "Setting GStreamer registry file to" << gst_registry_filename;
  Utilities::SetEnv("GST_REGISTRY", gst_registry_filename);
#endif

}
