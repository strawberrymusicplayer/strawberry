/*
 * Strawberry Music Player
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

#include <glib.h>
#include <gst/gst.h>

#include "gstfastspectrum.h"
#include "gstfastspectrumplugin.h"

static gboolean gst_strawberry_fastspectrum_plugin_init(GstPlugin *plugin) {

  GstRegistry *reg = gst_registry_get();
  if (reg) {
    GstPluginFeature *fastspectrum = gst_registry_lookup_feature(reg, "strawberry-fastspectrum");
    if (fastspectrum) {
      gst_object_unref(fastspectrum);
      return TRUE;
    }
  }

  return gst_element_register(plugin, "strawberry-fastspectrum", GST_RANK_NONE, GST_TYPE_STRAWBERRY_FASTSPECTRUM);

}

int gst_strawberry_fastspectrum_register_static() {

  return gst_plugin_register_static(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "strawberry-fastspectrum",
    "Fast spectrum analyzer for generating Moodbars",
    gst_strawberry_fastspectrum_plugin_init,
    "0.1",
    "GPL",
    "FastSpectrum",
    "gst-strawberry-fastspectrum",
    "https://www.strawberrymusicplayer.org");
}
