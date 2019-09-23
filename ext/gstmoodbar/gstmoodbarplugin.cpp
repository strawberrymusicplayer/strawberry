/* This file was part of Clementine.
   Copyright 2014, David Sansome <me@davidsansome.com>

   Strawberry is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Strawberry is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <gst/gst.h>

#include "gstfastspectrum.h"
#include "gstmoodbarplugin.h"

namespace {

static gboolean gst_moodbar_plugin_init(GstPlugin* plugin) {

  if (!gst_element_register(plugin, "fastspectrum", GST_RANK_NONE, GST_TYPE_FASTSPECTRUM)) {
    return FALSE;
  }

  return TRUE;

}

}  // namespace

int gstfastspectrum_register_static() {

  return gst_plugin_register_static(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "fastspectrum",
    "Fast spectrum analyzer for generating Moodbars",
    gst_moodbar_plugin_init,
    "0.1",
    "GPL",
    "FastSpectrum",
    "FastSpectrum",
    "https://www.strawberrymusicplayer.org");
}
