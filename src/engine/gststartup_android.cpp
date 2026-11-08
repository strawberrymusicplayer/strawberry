/*
 * Strawberry Music Player
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

// Android-only parts of GstStartup, kept apart from gststartup.cpp to reduce conflicts when merging upstream.

#include <QJniEnvironment>
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>

#include "core/logging.h"

#include "gststartup.h"

namespace GstStartup {

void InitializeAndroid() {

  // Call through QJniEnvironment to detect a Java exception, which QJniObject would only print as a generic warning.
  QJniEnvironment env;
  jclass gstreamer_class = env.findClass("org/freedesktop/gstreamer/GStreamer");
  jmethodID gstreamer_init = gstreamer_class ? env.findStaticMethod(gstreamer_class, "init", "(Landroid/content/Context;)V") : nullptr;
  if (gstreamer_init) {
    env->CallStaticVoidMethod(gstreamer_class, gstreamer_init, QNativeInterface::QAndroidApplication::context().object());
  }
  if (!gstreamer_init || env.checkAndClearExceptions()) {
    qLog(Error) << "Could not initialize GStreamer for Android, audio playback may not work";
  }

}

}  // namespace GstStartup
