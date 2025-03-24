/* GStreamer
 * Copyright (C) <1999> Erik Walthinsen <omega@cse.ogi.edu>
 *               <2006,2011> Stefan Kost <ensonic@users.sf.net>
 *               <2007-2009> Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *               <2018-2024> Jonas Kvinge <jonas@jkvinge.net>
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

#include <cstring>
#include <cmath>

#include <glib.h>

#include <gst/gst.h>
#include <gst/audio/gstaudiofilter.h>

#include <fftw3.h>

#include "gstfastspectrum.h"

GST_DEBUG_CATEGORY_STATIC(gst_strawberry_fastspectrum_debug);

namespace {

// Spectrum properties
constexpr auto DEFAULT_INTERVAL = (GST_SECOND / 10);
constexpr auto DEFAULT_BANDS = 128;

enum {
  PROP_0,
  PROP_INTERVAL,
  PROP_BANDS
};

}  // namespace

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4514)
#endif
G_DEFINE_TYPE(GstStrawberryFastSpectrum, gst_strawberry_fastspectrum, GST_TYPE_AUDIO_FILTER)
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#ifdef _MSC_VER
#pragma warning(pop)
#endif

static void gst_strawberry_fastspectrum_finalize(GObject *object);
static void gst_strawberry_fastspectrum_set_property(GObject *object, const guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_strawberry_fastspectrum_get_property(GObject *object, const guint prop_id, GValue *value, GParamSpec *pspec);
static gboolean gst_strawberry_fastspectrum_start(GstBaseTransform *transform);
static gboolean gst_strawberry_fastspectrum_stop(GstBaseTransform *transform);
static GstFlowReturn gst_strawberry_fastspectrum_transform_ip(GstBaseTransform *transform, GstBuffer *buffer);
static gboolean gst_strawberry_fastspectrum_setup(GstAudioFilter *audio_filter, const GstAudioInfo *audio_info);

static void gst_strawberry_fastspectrum_class_init(GstStrawberryFastSpectrumClass *klass) {

  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
  GstBaseTransformClass *transform_class = GST_BASE_TRANSFORM_CLASS(klass);
  GstAudioFilterClass *filter_class = GST_AUDIO_FILTER_CLASS(klass);

  gobject_class->set_property = gst_strawberry_fastspectrum_set_property;
  gobject_class->get_property = gst_strawberry_fastspectrum_get_property;
  gobject_class->finalize = gst_strawberry_fastspectrum_finalize;

  transform_class->start = GST_DEBUG_FUNCPTR(gst_strawberry_fastspectrum_start);
  transform_class->stop = GST_DEBUG_FUNCPTR(gst_strawberry_fastspectrum_stop);
  transform_class->transform_ip = GST_DEBUG_FUNCPTR(gst_strawberry_fastspectrum_transform_ip);
  transform_class->passthrough_on_same_caps = TRUE;

  filter_class->setup = GST_DEBUG_FUNCPTR(gst_strawberry_fastspectrum_setup);

  g_object_class_install_property(gobject_class, PROP_INTERVAL, g_param_spec_uint64("interval", "Interval", "Interval of time between message posts (in nanoseconds)", 1, G_MAXUINT64, DEFAULT_INTERVAL, static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property(gobject_class, PROP_BANDS, g_param_spec_uint("bands", "Bands", "Number of frequency bands", 0, G_MAXUINT, DEFAULT_BANDS, static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  GST_DEBUG_CATEGORY_INIT(gst_strawberry_fastspectrum_debug, "spectrum", 0, "audio spectrum analyser element");

  gst_element_class_set_static_metadata(element_class,
    "Fast spectrum analyzer using FFTW",
    "Filter/Analyzer/Audio",
    "Run an FFT on the audio signal, output spectrum data",
    "Erik Walthinsen <omega@cse.ogi.edu>, "
    "Stefan Kost <ensonic@users.sf.net>, "
    "Sebastian Dröge <sebastian.droege@collabora.co.uk>, "
    "Jonas Kvinge <jonas@jkvinge.net>");

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
  GstCaps *caps = gst_caps_from_string(GST_AUDIO_CAPS_MAKE("{ S16LE, S24LE, S32LE, F32LE, F64LE }") ", layout = (string) interleaved, channels = 1");
#else
  GstCaps *caps = gst_caps_from_string(GST_AUDIO_CAPS_MAKE("{ S16BE, S24BE, S32BE, F32BE, F64BE }") ", layout = (string) interleaved, channels = 1");
#endif

  gst_audio_filter_class_add_pad_templates(filter_class, caps);
  gst_caps_unref(caps);

  g_mutex_init(&klass->fftw_lock);

}

static void gst_strawberry_fastspectrum_init(GstStrawberryFastSpectrum *fastspectrum) {

  fastspectrum->interval = DEFAULT_INTERVAL;
  fastspectrum->bands = DEFAULT_BANDS;

  fastspectrum->channel_data_initialized = false;

  g_mutex_init(&fastspectrum->lock);

}

static void gst_strawberry_fastspectrum_alloc_channel_data(GstStrawberryFastSpectrum *fastspectrum) {

  const guint bands = fastspectrum->bands;
  const guint nfft = 2 * bands - 2;

  fastspectrum->input_ring_buffer = new double[nfft];
  fastspectrum->fft_input = reinterpret_cast<double*>(fftw_malloc(sizeof(double) * nfft));
  fastspectrum->fft_output = reinterpret_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex) * (nfft / 2 + 1)));

  fastspectrum->spect_magnitude = new double[bands] {};

  GstStrawberryFastSpectrumClass *klass = reinterpret_cast<GstStrawberryFastSpectrumClass*>(G_OBJECT_GET_CLASS(fastspectrum));
  {
    g_mutex_lock(&klass->fftw_lock);
    fastspectrum->plan = fftw_plan_dft_r2c_1d(static_cast<int>(nfft), fastspectrum->fft_input, fastspectrum->fft_output, FFTW_ESTIMATE);
    g_mutex_unlock(&klass->fftw_lock);
  }
  fastspectrum->channel_data_initialized = true;

}

static void gst_strawberry_fastspectrum_free_channel_data(GstStrawberryFastSpectrum *fastspectrum) {

  GstStrawberryFastSpectrumClass *klass = reinterpret_cast<GstStrawberryFastSpectrumClass*>(G_OBJECT_GET_CLASS(fastspectrum));

  if (fastspectrum->channel_data_initialized) {
    {
      g_mutex_lock(&klass->fftw_lock);
      fftw_destroy_plan(fastspectrum->plan);
      g_mutex_unlock(&klass->fftw_lock);
    }
    fftw_free(fastspectrum->fft_input);
    fftw_free(fastspectrum->fft_output);
    delete[] fastspectrum->input_ring_buffer;
    delete[] fastspectrum->spect_magnitude;

    fastspectrum->channel_data_initialized = false;
  }

}

static void gst_strawberry_fastspectrum_flush(GstStrawberryFastSpectrum *fastspectrum) {

  fastspectrum->num_frames = 0;
  fastspectrum->num_fft = 0;
  fastspectrum->accumulated_error = 0;

}

static void gst_strawberry_fastspectrum_reset_state(GstStrawberryFastSpectrum *fastspectrum) {

  GST_DEBUG_OBJECT(fastspectrum, "resetting state");

  gst_strawberry_fastspectrum_free_channel_data(fastspectrum);
  gst_strawberry_fastspectrum_flush(fastspectrum);

}

static void gst_strawberry_fastspectrum_finalize(GObject *object) {

  GstStrawberryFastSpectrum *fastspectrum = reinterpret_cast<GstStrawberryFastSpectrum*>(object);

  gst_strawberry_fastspectrum_reset_state(fastspectrum);
  g_mutex_clear(&fastspectrum->lock);

  G_OBJECT_CLASS(gst_strawberry_fastspectrum_parent_class)->finalize(object);

}

static void gst_strawberry_fastspectrum_set_property(GObject *object, const guint prop_id, const GValue *value, GParamSpec *pspec) {

  GstStrawberryFastSpectrum *filter = reinterpret_cast<GstStrawberryFastSpectrum*>(object);

  switch (prop_id) {
    case PROP_INTERVAL: {
      const guint64 interval = g_value_get_uint64(value);
      g_mutex_lock(&filter->lock);
      if (filter->interval != interval) {
        filter->interval = interval;
        gst_strawberry_fastspectrum_reset_state(filter);
      }
      g_mutex_unlock(&filter->lock);
      break;
    }
    case PROP_BANDS: {
      const guint bands = g_value_get_uint(value);
      g_mutex_lock(&filter->lock);
      if (filter->bands != bands) {
        filter->bands = bands;
        gst_strawberry_fastspectrum_reset_state(filter);
      }
      g_mutex_unlock(&filter->lock);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }

}

static void gst_strawberry_fastspectrum_get_property(GObject *object, const guint prop_id, GValue *value, GParamSpec *pspec) {

  GstStrawberryFastSpectrum *fastspectrum = reinterpret_cast<GstStrawberryFastSpectrum*>(object);

  switch (prop_id) {
    case PROP_INTERVAL:
      g_value_set_uint64(value, fastspectrum->interval);
      break;
    case PROP_BANDS:
      g_value_set_uint(value, fastspectrum->bands);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }

}

static gboolean gst_strawberry_fastspectrum_start(GstBaseTransform *transform) {

  GstStrawberryFastSpectrum *fastspectrum = reinterpret_cast<GstStrawberryFastSpectrum*>(transform);

  gst_strawberry_fastspectrum_reset_state(fastspectrum);

  return TRUE;

}

static gboolean gst_strawberry_fastspectrum_stop(GstBaseTransform *transform) {

  GstStrawberryFastSpectrum *fastspectrum = reinterpret_cast<GstStrawberryFastSpectrum*>(transform);

  gst_strawberry_fastspectrum_reset_state(fastspectrum);

  return TRUE;

}

// Mixing data readers

static void gst_strawberry_fastspectrum_input_data_mixed_float(const guint8 *_in, double *out, const guint64 len, const double max_value, guint op, const guint nfft) {

  (void) max_value;

  const gfloat *in = reinterpret_cast<const gfloat*>(_in);
  guint ip = 0;

  for (guint64 j = 0; j < len; j++) {
    out[op] = in[ip++];
    op = (op + 1) % nfft;
  }

}

static void gst_strawberry_fastspectrum_input_data_mixed_double(const guint8 *_in, double *out, const guint64 len, const double max_value, guint op, const guint nfft) {

  (void) max_value;

  const gdouble *in = reinterpret_cast<const gdouble*>(_in);
  guint ip = 0;

  for (guint64 j = 0; j < len; j++) {
    out[op] = in[ip++];
    op = (op + 1) % nfft;
  }

}

static void gst_strawberry_fastspectrum_input_data_mixed_int32_max(const guint8 *_in, double *out, const guint64 len, const double max_value, guint op, const guint nfft) {

  const gint32 *in = reinterpret_cast<const gint32*>(_in);
  guint ip = 0;

  for (guint64 j = 0; j < len; j++) {
    out[op] = in[ip++] / max_value;
    op = (op + 1) % nfft;
  }

}

static void gst_strawberry_fastspectrum_input_data_mixed_int24_max(const guint8 *_in, double *out, const guint64 len, const double max_value, guint op, const guint nfft) {

  for (guint64 j = 0; j < len; j++) {
#if G_BYTE_ORDER == G_BIG_ENDIAN
    guint32 value = GST_READ_UINT24_BE(_in);
#else
    guint32 value = GST_READ_UINT24_LE(_in);
#endif
    if (value & 0x00800000) {
      value |= 0xff000000;
    }

    out[op] = value / max_value;
    op = (op + 1) % nfft;
    _in += 3;
  }

}

static void gst_strawberry_fastspectrum_input_data_mixed_int16_max(const guint8 *_in, double *out, const guint64 len, const double max_value, guint op, const guint nfft) {

  const gint16 *in = reinterpret_cast<const gint16*>(_in);
  guint ip = 0;

  for (guint64 j = 0; j < len; j++) {
    out[op] = in[ip++] / max_value;
    op = (op + 1) % nfft;
  }

}

static gboolean gst_strawberry_fastspectrum_setup(GstAudioFilter *audio_filter, const GstAudioInfo *audio_info) {

  GstStrawberryFastSpectrum *fastspectrum = reinterpret_cast<GstStrawberryFastSpectrum*>(audio_filter);
  GstStrawberryFastSpectrumInputData input_data = nullptr;

  g_mutex_lock(&fastspectrum->lock);
  switch (GST_AUDIO_INFO_FORMAT(audio_info)) {
    case GST_AUDIO_FORMAT_S16:
      input_data = gst_strawberry_fastspectrum_input_data_mixed_int16_max;
      break;
    case GST_AUDIO_FORMAT_S24:
      input_data = gst_strawberry_fastspectrum_input_data_mixed_int24_max;
      break;
    case GST_AUDIO_FORMAT_S32:
      input_data = gst_strawberry_fastspectrum_input_data_mixed_int32_max;
      break;
    case GST_AUDIO_FORMAT_F32:
      input_data = gst_strawberry_fastspectrum_input_data_mixed_float;
      break;
    case GST_AUDIO_FORMAT_F64:
      input_data = gst_strawberry_fastspectrum_input_data_mixed_double;
      break;
    default:
      g_assert_not_reached();
      break;
  }
  fastspectrum->input_data = input_data;

  gst_strawberry_fastspectrum_reset_state(fastspectrum);
  g_mutex_unlock(&fastspectrum->lock);

  return TRUE;

}

static void gst_strawberry_fastspectrum_run_fft(GstStrawberryFastSpectrum *fastspectrum, const guint input_pos) {

  const guint bands = fastspectrum->bands;
  const guint nfft = 2 * bands - 2;

  for (guint i = 0; i < nfft; i++) {
    fastspectrum->fft_input[i] = fastspectrum->input_ring_buffer[(input_pos + i) % nfft];
  }

  // Should be safe to execute the same plan multiple times in parallel.
  fftw_execute(fastspectrum->plan);

  // Calculate magnitude in db
  for (guint i = 0; i < bands; i++) {
    gdouble value = fastspectrum->fft_output[i][0] * fastspectrum->fft_output[i][0];
    value += fastspectrum->fft_output[i][1] * fastspectrum->fft_output[i][1];
    value /= nfft * nfft;
    fastspectrum->spect_magnitude[i] += value;
  }

}

static GstFlowReturn gst_strawberry_fastspectrum_transform_ip(GstBaseTransform *transform, GstBuffer *buffer) {

  GstStrawberryFastSpectrum *fastspectrum = reinterpret_cast<GstStrawberryFastSpectrum*>(transform);

  const guint rate = static_cast<guint>(GST_AUDIO_FILTER_RATE(fastspectrum));
  const guint bps = static_cast<guint>(GST_AUDIO_FILTER_BPS(fastspectrum));
  const guint64 bpf = static_cast<guint64>(GST_AUDIO_FILTER_BPF(fastspectrum));
  const double max_value = static_cast<double>((1UL << ((bps << 3) - 1)) - 1);
  const guint bands = fastspectrum->bands;
  const guint nfft = 2 * bands - 2;

  g_mutex_lock(&fastspectrum->lock);

  GstMapInfo map;
  gst_buffer_map(buffer, &map, GST_MAP_READ);
  const guint8 *data = map.data;
  gsize size = map.size;

  GST_LOG_OBJECT(fastspectrum, "input size: %" G_GSIZE_FORMAT " bytes", size);

  if (GST_BUFFER_IS_DISCONT(buffer)) {
    GST_DEBUG_OBJECT(fastspectrum, "Discontinuity detected -- flushing");
    gst_strawberry_fastspectrum_flush(fastspectrum);
  }

  // If we don't have a FFT context yet (or it was reset due to parameter changes) get one and allocate memory for everything
  if (!fastspectrum->channel_data_initialized) {
    GST_DEBUG_OBJECT(fastspectrum, "allocating for bands %u", bands);

    gst_strawberry_fastspectrum_alloc_channel_data(fastspectrum);

    // Number of sample frames we process before posting a message interval is in ns
    fastspectrum->frames_per_interval = gst_util_uint64_scale(fastspectrum->interval, rate, GST_SECOND);
    fastspectrum->frames_todo = fastspectrum->frames_per_interval;
    // Rounding error for frames_per_interval in ns, aggregated it in accumulated_error
    fastspectrum->error_per_interval = (fastspectrum->interval * rate) % GST_SECOND;
    if (fastspectrum->frames_per_interval == 0) {
      fastspectrum->frames_per_interval = 1;
    }

    GST_INFO_OBJECT(fastspectrum, "interval %" GST_TIME_FORMAT ", fpi %" G_GUINT64_FORMAT ", error %" GST_TIME_FORMAT, GST_TIME_ARGS(fastspectrum->interval), fastspectrum->frames_per_interval, GST_TIME_ARGS(fastspectrum->error_per_interval));

    fastspectrum->input_pos = 0;

    gst_strawberry_fastspectrum_flush(fastspectrum);
  }

  if (fastspectrum->num_frames == 0) {
    fastspectrum->message_ts = GST_BUFFER_TIMESTAMP(buffer);
  }

  guint input_pos = fastspectrum->input_pos;
  GstStrawberryFastSpectrumInputData input_data = fastspectrum->input_data;

  while (size >= bpf) {
    // Run input_data for a chunk of data
    guint64 fft_todo = nfft - (fastspectrum->num_frames % nfft);
    guint64 msg_todo = fastspectrum->frames_todo - fastspectrum->num_frames;
    GST_LOG_OBJECT(fastspectrum, "message frames todo: %" G_GUINT64_FORMAT ", fft frames todo: %" G_GUINT64_FORMAT ", input frames %" G_GSIZE_FORMAT, msg_todo, fft_todo, static_cast<gsize>(size / bpf));
    guint64 block_size = msg_todo;
    if (block_size > (size / bpf)) {
      block_size = (size / bpf);
    }
    if (block_size > fft_todo) {
      block_size = fft_todo;
    }

    // Move the current frames into our ringbuffers
    input_data(data, fastspectrum->input_ring_buffer, block_size, max_value, input_pos, nfft);

    data += block_size * bpf;
    size -= block_size * bpf;
    input_pos = (input_pos + block_size) % nfft;
    fastspectrum->num_frames += block_size;

    gboolean have_full_interval = (fastspectrum->num_frames == fastspectrum->frames_todo);

    GST_LOG_OBJECT(fastspectrum, "size: %" G_GSIZE_FORMAT ", do-fft = %d, do-message = %d", size, (fastspectrum->num_frames % nfft == 0), have_full_interval);

    // If we have enough frames for an FFT or we have all frames required for the interval and we haven't run a FFT, then run an FFT
    if ((fastspectrum->num_frames % nfft == 0) || (have_full_interval && !fastspectrum->num_fft)) {
      gst_strawberry_fastspectrum_run_fft(fastspectrum, input_pos);
      fastspectrum->num_fft++;
    }

    // Do we have the FFTs for one interval?
    if (have_full_interval) {
      GST_DEBUG_OBJECT(fastspectrum, "nfft: %u frames: %" G_GUINT64_FORMAT " fpi: %" G_GUINT64_FORMAT " error: %" GST_TIME_FORMAT, nfft, fastspectrum->num_frames, fastspectrum->frames_per_interval, GST_TIME_ARGS(fastspectrum->accumulated_error));

      fastspectrum->frames_todo = fastspectrum->frames_per_interval;
      if (fastspectrum->accumulated_error >= GST_SECOND) {
        fastspectrum->accumulated_error -= GST_SECOND;
        fastspectrum->frames_todo++;
      }
      fastspectrum->accumulated_error += fastspectrum->error_per_interval;

      if (fastspectrum->output_callback) {
        // Calculate average
        for (guint i = 0; i < fastspectrum->bands; i++) {
          fastspectrum->spect_magnitude[i] /= static_cast<double>(fastspectrum->num_fft);
        }

        fastspectrum->output_callback(fastspectrum->spect_magnitude, static_cast<int>(fastspectrum->bands));

        // Reset spectrum accumulators
        memset(fastspectrum->spect_magnitude, 0, fastspectrum->bands * sizeof(double));
      }

      if (GST_CLOCK_TIME_IS_VALID(fastspectrum->message_ts)) {
        fastspectrum->message_ts += gst_util_uint64_scale(fastspectrum->num_frames, GST_SECOND, rate);
      }

      fastspectrum->num_frames = 0;
      fastspectrum->num_fft = 0;
    }
  }

  fastspectrum->input_pos = input_pos;

  gst_buffer_unmap(buffer, &map);
  g_mutex_unlock(&fastspectrum->lock);

  g_assert(size == 0);

  return GST_FLOW_OK;

}
