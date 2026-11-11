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

#include <cstring>

#include <gst/gst.h>

#include <QJniEnvironment>
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>

#include "core/logging.h"

#include "gststartup.h"
#include "gstqfilesrc.h"

namespace GstStartup {

namespace {

// The GStreamer factory type flags are macros with C-style casts.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
constexpr GstElementFactoryListType kAudioDecoderType = GST_ELEMENT_FACTORY_TYPE_DECODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO;
#pragma GCC diagnostic pop

}  // namespace

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

void RegisterAndroidFileSource() {
  if (!RegisterGstQFileSource()) {
    qLog(Fatal) << "Could not register Android document audio source";
  }
}

void LowerAndroidMediaDecoderRanks() {

  // androidmedia registers Android's software audio codecs with a rank above GStreamer's own decoders.
  // Lower them below those, so they are only used for formats without a GStreamer decoder, such as AAC.
  GList *decoders = gst_element_factory_list_get_elements(kAudioDecoderType, GST_RANK_NONE);
  for (GList *item = decoders; item; item = item->next) {
    GstPluginFeature *feature = static_cast<GstPluginFeature*>(item->data);
    const gchar *plugin_name = gst_plugin_feature_get_plugin_name(feature);
    if (plugin_name && strcmp(plugin_name, "androidmedia") == 0 && gst_plugin_feature_get_rank(feature) > GST_RANK_SECONDARY) {
      gst_plugin_feature_set_rank(feature, GST_RANK_SECONDARY);
    }
  }
  gst_plugin_feature_list_free(decoders);

}

}  // namespace GstStartup
