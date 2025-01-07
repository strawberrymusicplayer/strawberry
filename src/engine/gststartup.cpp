/*
 * Strawberry Music Player
 * Copyright 2018-2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QCoreApplication>
#include <QString>
#include <QDir>
#include <QFile>

#include "core/logging.h"
#include "core/standardpaths.h"
#include "utilities/envutils.h"

#ifdef HAVE_MOODBAR
#  include "gstfastspectrumplugin.h"
#endif

#include "gststartup.h"

using namespace Qt::Literals::StringLiterals;

namespace GstStartup {

void Initialize() {

  SetEnvironment();

  gst_init(nullptr, nullptr);
  gst_pb_utils_init();

#ifdef HAVE_MOODBAR
  gst_strawberry_fastspectrum_register_static();
#endif

#ifdef Q_OS_WIN32
  // Use directsoundsink as the default sink on Windows.
  // wasapisink does not support device switching and wasapi2sink has issues, see #1227.
  GstRegistry *reg = gst_registry_get();
  if (reg) {
    if (GstPluginFeature *directsoundsink = gst_registry_lookup_feature(reg, "directsoundsink")) {
      gst_plugin_feature_set_rank(directsoundsink, GST_RANK_PRIMARY);
      gst_object_unref(directsoundsink);
    }
    if (GstPluginFeature *wasapisink = gst_registry_lookup_feature(reg, "wasapisink")) {
      gst_plugin_feature_set_rank(wasapisink, GST_RANK_SECONDARY);
      gst_object_unref(wasapisink);
    }
    if (GstPluginFeature *wasapi2sink = gst_registry_lookup_feature(reg, "wasapi2sink")) {
      gst_plugin_feature_set_rank(wasapi2sink, GST_RANK_SECONDARY);
      gst_object_unref(wasapi2sink);
    }
  }
#endif

}

void SetEnvironment() {

#ifdef USE_BUNDLE

  const QString app_path = QCoreApplication::applicationDirPath();

  // Set plugin root path
  QString plugin_root_path;
#  if defined(Q_OS_MACOS)
  plugin_root_path = QDir::cleanPath(app_path + "/../PlugIns"_L1);
#  elif defined(Q_OS_UNIX)
  plugin_root_path = QDir::cleanPath(app_path + "/../plugins"_L1);
#  elif defined(Q_OS_WIN32)
  plugin_root_path = app_path;
#  endif

  // Set GIO module path
  const QString gio_module_path = plugin_root_path + "/gio-modules"_L1;

  // Set GStreamer plugin scanner path
  QString gst_plugin_scanner;
#  if defined(Q_OS_UNIX)
  gst_plugin_scanner = plugin_root_path + "/gst-plugin-scanner"_L1;
#  endif

  // Set GStreamer plugin path
  QString gst_plugin_path;
#  if defined(Q_OS_WIN32)
  gst_plugin_path = plugin_root_path + "/gstreamer-plugins"_L1;
#  else
  gst_plugin_path = plugin_root_path + "/gstreamer"_L1;
#  endif

  if (!gio_module_path.isEmpty()) {
    if (QDir(gio_module_path).exists()) {
      qLog(Debug) << "Setting GIO module path to" << gio_module_path;
      Utilities::SetEnv("GIO_EXTRA_MODULES", gio_module_path);
    }
    else {
      qLog(Error) << "GIO module path" << gio_module_path << "does not exist.";
    }
  }

  if (!gst_plugin_scanner.isEmpty()) {
    if (QFile::exists(gst_plugin_scanner)) {
      qLog(Debug) << "Setting GStreamer plugin scanner to" << gst_plugin_scanner;
      Utilities::SetEnv("GST_PLUGIN_SCANNER", gst_plugin_scanner);
    }
    else {
      qLog(Error) << "GStreamer plugin scanner" << gst_plugin_scanner << "does not exist.";
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
      qLog(Error) << "GStreamer plugin path" << gst_plugin_path << "does not exist.";
    }
  }

#endif  // USE_BUNDLE

#if defined(Q_OS_WIN32) || defined(Q_OS_MACOS)
  QString gst_registry_filename = StandardPaths::WritableLocation(StandardPaths::StandardLocation::AppLocalDataLocation) + QStringLiteral("/gst-registry-%1-bin").arg(QCoreApplication::applicationVersion());
  qLog(Debug) << "Setting GStreamer registry file to" << gst_registry_filename;
  Utilities::SetEnv("GST_REGISTRY", gst_registry_filename);
#endif

}

}  // namespace GstStartup
