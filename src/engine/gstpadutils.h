/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef GSTPADUTILS_H
#define GSTPADUTILS_H

#include <glib.h>
#include <gst/gst.h>

// Returns true when the pad has audio caps.
// Decodebin and uridecodebin can add pads for video, for example cover art, and subtitles, which should not be linked to the audio converter.
inline bool GstPadIsAudio(GstPad *pad) {

  GstCaps *caps = gst_pad_get_current_caps(pad);
  if (!caps) {
    caps = gst_pad_query_caps(pad, nullptr);
  }
  if (!caps) return false;

  bool is_audio = false;
  if (gst_caps_get_size(caps) > 0) {
    const GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *name = structure ? gst_structure_get_name(structure) : nullptr;
    is_audio = name && g_str_has_prefix(name, "audio/");
  }
  gst_caps_unref(caps);

  return is_audio;

}

#endif  // GSTPADUTILS_H
