/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 * Copyright (C) <2009> Sebastian Dröge <sebastian.droege@collabora.co.uk>
 * Copyright (C) <2018-2024> Jonas Kvinge <jonas@jkvinge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

// Adapted from gstspectrum for Clementine with the following changes:
//   - Uses fftw instead of kiss fft (2x faster).
//   - Hardcoded to 1 channel (use an audioconvert element to do the work
//     instead, simplifies this code a lot).
//   - Send output via a callback instead of GST messages (less overhead).
//   - Removed all properties except interval and band.

#ifndef GST_STRAWBERRY_FASTSPECTRUM_H
#define GST_STRAWBERRY_FASTSPECTRUM_H

#include <functional>

#include <gst/gst.h>
#include <gst/audio/gstaudiofilter.h>
#include <fftw3.h>

G_BEGIN_DECLS

#define GST_TYPE_STRAWBERRY_FASTSPECTRUM            (gst_strawberry_fastspectrum_get_type())
#define GST_STRAWBERRY_FASTSPECTRUM(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_FASTSPECTRUM, GstStrawberryFastSpectrum))
#define GST_IS_STRAWBERRY_FASTSPECTRUM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_FASTSPECTRUM))
#define GST_STRAWBERRY_FASTSPECTRUM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_FASTSPECTRUM, GstStrawberryFastSpectrumClass))
#define GST_IS_STRAWBERRY_FASTSPECTRUM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_FASTSPECTRUM))

typedef void (*GstStrawberryFastSpectrumInputData)(const guint8 *in, double *out, guint64 len, double max_value, guint op, guint nfft);

using GstStrawberryFastSpectrumOutputCallback = std::function<void(double *magnitudes, int size)>;

struct GstStrawberryFastSpectrum {
  GstAudioFilter parent;

  // Properties
  guint64 interval;            // How many nanoseconds between emits
  guint64 frames_per_interval; // How many frames per interval
  guint64 frames_todo;
  guint bands;                 // Number of spectrum bands
  gboolean multi_channel;      // Send separate channel results

  guint64 num_frames;          // Frame count (1 sample per channel) since last emit
  guint64 num_fft;             // Number of FFTs since last emit
  GstClockTime message_ts;     // Starttime for next message

  // <private>
  bool channel_data_initialized;
  double *input_ring_buffer;
  double *fft_input;
  fftw_complex *fft_output;
  double *spect_magnitude;
  fftw_plan plan;

  guint input_pos;
  guint64 error_per_interval;
  guint64 accumulated_error;

  GMutex lock;

  GstStrawberryFastSpectrumInputData input_data;
  GstStrawberryFastSpectrumOutputCallback output_callback;
};

struct GstStrawberryFastSpectrumClass {
  GstAudioFilterClass parent_class;
  GMutex fftw_lock;
};

GType gst_strawberry_fastspectrum_get_type(void);

G_END_DECLS

#endif  // GST_STRAWBERRY_FASTSPECTRUM_H
