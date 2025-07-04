/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <QtGlobal>

#include <cstdint>
#include <cstring>
#include <cmath>

#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>
#include <gst/audio/audio.h>

#ifdef Q_OS_UNIX
#  include <pthread.h>
#endif
#ifdef Q_OS_WIN32
#  include <windows.h>
#endif

#include <QObject>
#include <QCoreApplication>
#include <QtConcurrentRun>
#include <QFuture>
#include <QFutureWatcher>
#include <QMutex>
#include <QMutexLocker>
#include <QMetaType>
#include <QByteArray>
#include <QList>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QTimer>
#include <QTimeLine>
#include <QEasingCurve>
#include <QMetaObject>
#include <QUuid>
#include <QVersionNumber>

#include "core/logging.h"
#include "core/signalchecker.h"
#include "constants/timeconstants.h"
#include "constants/backendsettings.h"
#include "gstengine.h"
#include "gstenginepipeline.h"
#include "gstbufferconsumer.h"

using namespace std::chrono_literals;
using namespace Qt::Literals::StringLiterals;

#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wunused-const-variable"
#endif

namespace {

constexpr int GST_PLAY_FLAG_VIDEO = 0x00000001;
constexpr int GST_PLAY_FLAG_AUDIO = 0x00000002;
constexpr int GST_PLAY_FLAG_DOWNLOAD = 0x00000080;
constexpr int GST_PLAY_FLAG_BUFFERING = 0x00000100;
constexpr int GST_PLAY_FLAG_SOFT_VOLUME = 0x00000010;

constexpr int kGstStateTimeoutNanosecs = 10000000;
constexpr std::chrono::milliseconds kFaderFudgeMsec = 2000ms;
constexpr std::chrono::milliseconds kFaderTimeoutMsec = 3000ms;

constexpr int kEqBandCount = 10;
constexpr int kEqBandFrequencies[] = { 60, 170, 310, 600, 1000, 3000, 6000, 12000, 14000, 16000 };

}  // namespace

#ifdef __clang_
#  pragma clang diagnostic pop
#endif

int GstEnginePipeline::sId = 1;

GstEnginePipeline::GstEnginePipeline(QObject *parent)
    : QObject(parent),
      id_(sId++),
      playbin3_support_(false),
      volume_full_range_support_(false),
      playbin3_enabled_(true),
      exclusive_mode_(false),
      volume_enabled_(true),
      fading_enabled_(false),
      strict_ssl_enabled_(false),
      buffer_duration_nanosec_(BackendSettings::kDefaultBufferDuration * kNsecPerMsec),
      buffer_low_watermark_(BackendSettings::kDefaultBufferLowWatermark),
      buffer_high_watermark_(BackendSettings::kDefaultBufferHighWatermark),
      proxy_authentication_(false),
      channels_enabled_(false),
      channels_(0),
      bs2b_enabled_(false),
      stereo_balancer_enabled_(false),
      stereo_balance_(0.0F),
      eq_enabled_(false),
      eq_preamp_(0),
      rg_enabled_(false),
      rg_mode_(0),
      rg_preamp_(0.0),
      rg_fallbackgain_(0.0),
      rg_compression_(true),
      ebur128_loudness_normalization_(false),
      ebur128_loudness_normalizing_gain_db_(0.0),
      segment_start_(0),
      segment_start_received_(false),
      beginning_offset_nanosec_(-1),
      end_offset_nanosec_(-1),
      next_beginning_offset_nanosec_(-1),
      next_end_offset_nanosec_(-1),
      ignore_next_seek_(false),
      ignore_tags_(false),
      pipeline_connected_(false),
      pipeline_active_(false),
      buffering_(false),
      pending_state_(GST_STATE_NULL),
      pending_seek_nanosec_(-1),
      pending_seek_ready_previous_state_(GST_STATE_NULL),
      last_known_position_ns_(0),
      next_uri_set_(false),
      next_uri_need_reset_(false),
      next_uri_reset_(false),
      volume_set_(false),
      volume_internal_(-1.0),
      volume_percent_(100),
      fader_active_(false),
      fader_running_(false),
      fader_use_fudge_timer_(false),
      timer_fader_fudge_(new QTimer(this)),
      timer_fader_timeout_(new QTimer(this)),
      pipeline_(nullptr),
      audiobin_(nullptr),
      audiosink_(nullptr),
      audioqueue_(nullptr),
      audioqueueconverter_(nullptr),
      volume_(nullptr),
      volume_sw_(nullptr),
      volume_fading_(nullptr),
      volume_ebur128_(nullptr),
      audiopanorama_(nullptr),
      equalizer_(nullptr),
      equalizer_preamp_(nullptr),
      eventprobe_(nullptr),
      logged_unsupported_analyzer_format_(false),
      about_to_finish_(false),
      finish_requested_(false),
      finished_(false),
      set_state_in_progress_(0),
      set_state_async_in_progress_(0),
      last_set_state_in_progress_(GST_STATE_VOID_PENDING),
      last_set_state_async_in_progress_(GST_STATE_VOID_PENDING) {

  guint version_major = 0, version_minor = 0;
  gst_plugins_base_version(&version_major, &version_minor, nullptr, nullptr);
  playbin3_support_ = QVersionNumber::compare(QVersionNumber(static_cast<int>(version_major), static_cast<int>(version_minor)), QVersionNumber(1, 24)) >= 0;
  volume_full_range_support_ = QVersionNumber::compare(QVersionNumber(static_cast<int>(version_major), static_cast<int>(version_minor)), QVersionNumber(1, 24)) >= 0;

  eq_band_gains_.reserve(kEqBandCount);
  for (int i = 0; i < kEqBandCount; ++i) eq_band_gains_ << 0;

  timer_fader_fudge_->setSingleShot(true);
  timer_fader_fudge_->setInterval(kFaderFudgeMsec);
  QObject::connect(timer_fader_fudge_, &QTimer::timeout, this, &GstEnginePipeline::FaderFudgeFinished);

  timer_fader_timeout_->setSingleShot(true);
  QObject::connect(timer_fader_timeout_, &QTimer::timeout, this, &GstEnginePipeline::FaderTimelineTimeout);

}

GstEnginePipeline::~GstEnginePipeline() {

  Disconnect();

  if (pipeline_) {

    gst_element_set_state(pipeline_, GST_STATE_NULL);

    GstElement *audiobin = nullptr;
    g_object_get(GST_OBJECT(pipeline_), "audio-sink", &audiobin, nullptr);

    gst_object_unref(GST_OBJECT(pipeline_));
    pipeline_ = nullptr;

    if (audiobin_ && audiobin_ != audiobin) {
      gst_object_unref(GST_OBJECT(audiobin_));
    }
    audiobin_ = nullptr;
  }

  qLog(Debug) << "Pipeline" << id() << "deleted";

}

void GstEnginePipeline::set_output_device(const QString &output, const QVariant &device) {

  output_ = output;
  device_ = device;

}

void GstEnginePipeline::set_playbin3_enabled(const bool playbin3_enabled) {
  playbin3_enabled_ = playbin3_enabled;
}

void GstEnginePipeline::set_exclusive_mode(const bool exclusive_mode) {
  exclusive_mode_ = exclusive_mode;
}

void GstEnginePipeline::set_volume_enabled(const bool enabled) {
  volume_enabled_ = enabled;
}

void GstEnginePipeline::set_stereo_balancer_enabled(const bool enabled) {

  stereo_balancer_enabled_ = enabled;
  if (!enabled) stereo_balance_ = 0.0F;
  if (pipeline_) UpdateStereoBalance();

}

void GstEnginePipeline::set_equalizer_enabled(const bool enabled) {
  eq_enabled_ = enabled;
  if (pipeline_) UpdateEqualizer();
}

void GstEnginePipeline::set_replaygain(const bool enabled, const int mode, const double preamp, const double fallbackgain, const bool compression) {

  rg_enabled_ = enabled;
  rg_mode_ = mode;
  rg_preamp_ = preamp;
  rg_fallbackgain_ = fallbackgain;
  rg_compression_ = compression;

}

void GstEnginePipeline::set_ebur128_loudness_normalization(const bool enabled) {

  ebur128_loudness_normalization_ = enabled;

}

void GstEnginePipeline::set_buffer_duration_nanosec(const quint64 buffer_duration_nanosec) {
  buffer_duration_nanosec_ = buffer_duration_nanosec;
}

void GstEnginePipeline::set_buffer_low_watermark(const double value) {
  buffer_low_watermark_ = value;
}

void GstEnginePipeline::set_buffer_high_watermark(const double value) {
  buffer_high_watermark_ = value;
}

void GstEnginePipeline::set_proxy_settings(const QString &address, const bool authentication, const QString &user, const QString &pass) {

  QMutexLocker l(&mutex_proxy_);
  proxy_address_ = address;
  proxy_authentication_ = authentication;
  proxy_user_ = user;
  proxy_pass_ = pass;

}

void GstEnginePipeline::set_channels(const bool enabled, const int channels) {
  channels_enabled_ = enabled;
  channels_ = channels;
}

void GstEnginePipeline::set_bs2b_enabled(const bool enabled) {
  bs2b_enabled_ = enabled;
}

void GstEnginePipeline::set_strict_ssl_enabled(const bool enabled) {
  strict_ssl_enabled_ = enabled;
}

void GstEnginePipeline::set_fading_enabled(const bool enabled) {
  fading_enabled_ = enabled;
}

#ifdef HAVE_SPOTIFY
void GstEnginePipeline::set_spotify_access_token(const QString &spotify_access_token) {
  QMutexLocker l(&mutex_spotify_access_token_);
  spotify_access_token_ = spotify_access_token;
}
#endif  // HAVE_SPOTIFY

QString GstEnginePipeline::GstStateText(const GstState state) {

  switch (state) {
    case GST_STATE_VOID_PENDING:
      return u"Pending"_s;
    case GST_STATE_NULL:
      return u"Null"_s;
    case GST_STATE_READY:
      return u"Ready"_s;
    case GST_STATE_PAUSED:
      return u"Paused"_s;
    case GST_STATE_PLAYING:
      return u"Playing"_s;
    default:
      return u"Unknown"_s;
  }

}

GstElement *GstEnginePipeline::CreateElement(const QString &factory_name, const QString &name, GstElement *bin, QString &error) const {

  QString unique_name = "pipeline"_L1 + u'-' + QString::number(id()) + u'-' + (name.isEmpty() ? factory_name : name);

  GstElement *element = gst_element_factory_make(factory_name.toUtf8().constData(), unique_name.toUtf8().constData());
  if (!element) {
    qLog(Error) << "GStreamer could not create the element" << factory_name << "with name" << unique_name;
    error = QStringLiteral("GStreamer could not create the element %1 with name %2.").arg(factory_name, unique_name);
  }

  if (bin && element) gst_bin_add(GST_BIN(bin), element);

  return element;

}

void GstEnginePipeline::Disconnect() {

  if (pipeline_) {

    if (fader_) {
      fader_active_ = false;
      fader_running_ = false;
      if (fader_->state() != QTimeLine::State::NotRunning) {
        fader_->stop();
      }
      fader_.reset();
    }

    if (element_added_cb_id_.has_value()) {
      g_signal_handler_disconnect(G_OBJECT(audiobin_), element_added_cb_id_.value());
      element_added_cb_id_.reset();
    }

    if (element_removed_cb_id_.has_value()) {
      g_signal_handler_disconnect(G_OBJECT(audiobin_), element_removed_cb_id_.value());
      element_removed_cb_id_.reset();
    }

    if (pad_added_cb_id_.has_value()) {
      g_signal_handler_disconnect(G_OBJECT(pipeline_), pad_added_cb_id_.value());
      pad_added_cb_id_.reset();
    }

    if (notify_source_cb_id_.has_value()) {
      g_signal_handler_disconnect(G_OBJECT(pipeline_), notify_source_cb_id_.value());
      notify_source_cb_id_.reset();
    }

    if (about_to_finish_cb_id_.has_value()) {
      g_signal_handler_disconnect(G_OBJECT(pipeline_), about_to_finish_cb_id_.value());
      about_to_finish_cb_id_.reset();
    }

    if (notify_volume_cb_id_.has_value()) {
      g_signal_handler_disconnect(G_OBJECT(volume_), notify_volume_cb_id_.value());
      notify_volume_cb_id_.reset();
    }

    if (upstream_events_probe_cb_id_.has_value()) {
      GstPad *pad = gst_element_get_static_pad(eventprobe_, "src");
      if (pad) {
        gst_pad_remove_probe(pad, upstream_events_probe_cb_id_.value());
        gst_object_unref(pad);
      }
      upstream_events_probe_cb_id_.reset();
    }

    if (buffer_probe_cb_id_.has_value()) {
      GstPad *pad = gst_element_get_static_pad(audioqueueconverter_, "src");
      if (pad) {
        gst_pad_remove_probe(pad, buffer_probe_cb_id_.value());
        gst_object_unref(pad);
      }
      buffer_probe_cb_id_.reset();
    }

    {
      GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
      if (bus) {
        gst_bus_remove_watch(bus);
        gst_bus_set_sync_handler(bus, nullptr, nullptr, nullptr);
        gst_object_unref(bus);
      }
    }

  }

}

bool GstEnginePipeline::Finish() {

  qLog(Debug) << "Finishing pipeline" << id();

  finish_requested_ = true;

  Disconnect();

  if (IsStateNull() && set_state_async_in_progress_ == 0 && set_state_in_progress_ == 0) {
    finished_ = true;
  }
  else {
    if (set_state_async_in_progress_ > 0 && last_set_state_async_in_progress_ != GST_STATE_NULL) {
      SetStateAsync(GST_STATE_NULL);
    }
    else if ((!IsStateNull() || set_state_in_progress_ > 0) && last_set_state_in_progress_ != GST_STATE_NULL) {
      SetState(GST_STATE_NULL);
    }
  }

  return finished_.value();

}

bool GstEnginePipeline::InitFromUrl(const QUrl &media_url, const QUrl &stream_url, const QByteArray &gst_url, const qint64 beginning_offset_nanosec, const qint64 end_offset_nanosec, const double ebur128_loudness_normalizing_gain_db, QString &error) {

  {
    QMutexLocker l(&mutex_url_);
    media_url_ = media_url;
    stream_url_ = stream_url;
    gst_url_ = gst_url;
  }

  beginning_offset_nanosec_ = beginning_offset_nanosec;
  end_offset_nanosec_ = end_offset_nanosec;
  ebur128_loudness_normalizing_gain_db_ = ebur128_loudness_normalizing_gain_db;

  const QString playbin_name = playbin3_support_ && playbin3_enabled_ ? u"playbin3"_s : u"playbin"_s;
  qLog(Debug) << "Using" << playbin_name << "for pipeline";
  pipeline_ = CreateElement(playbin_name, u"pipeline"_s, nullptr, error);
  if (!pipeline_) return false;

  pad_added_cb_id_ = CHECKED_GCONNECT(G_OBJECT(pipeline_), "pad-added", &PadAddedCallback, this);
  notify_source_cb_id_ = CHECKED_GCONNECT(G_OBJECT(pipeline_), "source-setup", &SourceSetupCallback, this);
  about_to_finish_cb_id_ = CHECKED_GCONNECT(G_OBJECT(pipeline_), "about-to-finish", &AboutToFinishCallback, this);

  if (!InitAudioBin(error)) return false;

#ifdef Q_OS_WIN32
  if (volume_enabled_ && !volume_ && volume_sw_) {
    SetupVolume(volume_sw_);
  }
#else
  if (volume_enabled_ && !volume_) {
    if (output_ == QLatin1String(GstEngine::kAutoSink)) {
      element_added_cb_id_ = CHECKED_GCONNECT(G_OBJECT(audiobin_), "deep-element-added", &ElementAddedCallback, this);
      element_removed_cb_id_ = CHECKED_GCONNECT(G_OBJECT(audiobin_), "deep-element-removed", &ElementRemovedCallback, this);
    }
    else if (volume_sw_) {
      qLog(Debug) << output_ << "does not have volume, using own volume.";
      SetupVolume(volume_sw_);
    }
  }
#endif

  // Set playbin's sink to be our custom audio-sink.
  g_object_set(GST_OBJECT(pipeline_), "audio-sink", audiobin_, nullptr);

  gint flags = 0;
  g_object_get(G_OBJECT(pipeline_), "flags", &flags, nullptr);
  flags |= GST_PLAY_FLAG_AUDIO;
  flags &= ~GST_PLAY_FLAG_VIDEO;
  flags &= ~GST_PLAY_FLAG_SOFT_VOLUME;
  g_object_set(G_OBJECT(pipeline_), "flags", flags, nullptr);

  {
    QMutexLocker l(&mutex_url_);
    g_object_set(G_OBJECT(pipeline_), "uri", gst_url.constData(), nullptr);
  }

  pipeline_connected_ = true;

  return true;

}

bool GstEnginePipeline::InitAudioBin(QString &error) {

  gst_segment_init(&last_playbin_segment_, GST_FORMAT_TIME);

  // Audio bin
  audiobin_ = gst_bin_new("audiobin");
  if (!audiobin_) return false;

  // Create the sink
  audiosink_ = CreateElement(output_, output_, audiobin_, error);
  if (!audiosink_) {
    return false;
  }

  if (device_.isValid()) {
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(audiosink_), "device")) {
      switch (device_.metaType().id()) {
        case QMetaType::QString:{
          const QString device = device_.toString();
          if (!device.isEmpty()) {
            qLog(Debug) << "Setting device" << device << "for" << output_;
            g_object_set(G_OBJECT(audiosink_), "device", device.toUtf8().constData(), nullptr);
            if (output_ == QLatin1String(GstEngine::kALSASink) && (device.startsWith("hw:"_L1) || device.startsWith("plughw:"_L1))) {
              exclusive_mode_ = true;
            }
          }
          break;
        }
        case QMetaType::QByteArray:{
          QByteArray device = device_.toByteArray();
          if (!device.isEmpty()) {
            qLog(Debug) << "Setting device" << device_ << "for" << output_;
            g_object_set(G_OBJECT(audiosink_), "device", device.constData(), nullptr);
          }
          break;
        }
        case QMetaType::LongLong:{
          qint64 device = device_.toLongLong();
          qLog(Debug) << "Setting device" << device << "for" << output_;
          g_object_set(G_OBJECT(audiosink_), "device", device, nullptr);
          break;
        }
        case QMetaType::Int:{
          int device = device_.toInt();
          qLog(Debug) << "Setting device" << device << "for" << output_;
          g_object_set(G_OBJECT(audiosink_), "device", device, nullptr);
          break;
        }
        case QMetaType::QUuid:{
          QUuid device = device_.toUuid();
          qLog(Debug) << "Setting device" << device << "for" << output_;
          g_object_set(G_OBJECT(audiosink_), "device", device, nullptr);
          break;
        }
        default:
          qLog(Warning) << "Unknown device type" << device_;
          break;
      }
    }
    else if (g_object_class_find_property(G_OBJECT_GET_CLASS(audiosink_), "device-clsid")) {
      switch (device_.metaType().id()) {
        case QMetaType::QString:{
          QString device = device_.toString();
          if (!device.isEmpty()) {
            qLog(Debug) << "Setting device-clsid" << device << "for" << output_;
            g_object_set(G_OBJECT(audiosink_), "device-clsid", device.toUtf8().constData(), nullptr);
          }
          break;
        }
        case QMetaType::QByteArray:{
          QByteArray device = device_.toByteArray();
          if (!device.isEmpty()) {
            qLog(Debug) << "Setting device-clsid" << device_ << "for" << output_;
            g_object_set(G_OBJECT(audiosink_), "device-clsid", device.constData(), nullptr);
          }
          break;
        }
        default:
          qLog(Warning) << "Unknown device clsid" << device_;
          break;
      }
    }
    else if (g_object_class_find_property(G_OBJECT_GET_CLASS(audiosink_), "port-pattern")) {
      switch (device_.metaType().id()) {
        case QMetaType::QString:{
          QString port_pattern = device_.toString();
          if (!port_pattern.isEmpty()) {
            qLog(Debug) << "Setting port pattern" << port_pattern << "for" << output_;
            g_object_set(G_OBJECT(audiosink_), "port-pattern", port_pattern.toUtf8().constData(), nullptr);
          }
          break;
        }

        case QMetaType::QByteArray:{
          QByteArray port_pattern = device_.toByteArray();
          if (!port_pattern.isEmpty()) {
            qLog(Debug) << "Setting port pattern" << port_pattern << "for" << output_;
            g_object_set(G_OBJECT(audiosink_), "port-pattern", port_pattern.constData(), nullptr);
          }
          break;
        }

        default:
          break;

      }
    }

  }

  if (g_object_class_find_property(G_OBJECT_GET_CLASS(audiosink_), "exclusive")) {
    if (exclusive_mode_) {
      qLog(Debug) << "Setting exclusive mode for" << output_;
    }
    g_object_set(G_OBJECT(audiosink_), "exclusive", exclusive_mode_, nullptr);
  }

#ifndef Q_OS_WIN32
  if (g_object_class_find_property(G_OBJECT_GET_CLASS(audiosink_), "volume")) {
    qLog(Debug) << output_ << "has volume, enabling volume synchronization.";
    SetupVolume(audiosink_);
  }
#endif

  // Create all the other elements

  audioqueue_ = CreateElement(u"queue2"_s, u"audioqueue"_s, audiobin_, error);
  if (!audioqueue_) {
    return false;
  }

  audioqueueconverter_ = CreateElement(u"audioconvert"_s, u"audioqueueconverter"_s, audiobin_, error);
  if (!audioqueueconverter_) {
    return false;
  }

  GstElement *audiosinkconverter = CreateElement(u"audioconvert"_s, u"audiosinkconverter"_s, audiobin_, error);
  if (!audiosinkconverter) {
    return false;
  }

  // Create the volume element if it's enabled.
  if (volume_enabled_ && !volume_) {
    volume_sw_ = CreateElement(u"volume"_s, u"volume_sw"_s, audiobin_, error);
    if (!volume_sw_) {
      return false;
    }
  }

  if (fading_enabled_) {
    volume_fading_ = CreateElement(u"volume"_s, u"volume_fading"_s, audiobin_, error);
    if (!volume_fading_) {
      return false;
    }
    if (fader_) {
      SetFaderVolume(fader_->currentValue());
    }
  }

  // Create the stereo balancer elements if it's enabled.
  if (stereo_balancer_enabled_) {
    audiopanorama_ = CreateElement(u"audiopanorama"_s, u"audiopanorama"_s, audiobin_, error);
    if (!audiopanorama_) {
      return false;
    }
    // Set the stereo balance.
    g_object_set(G_OBJECT(audiopanorama_), "panorama", stereo_balance_, nullptr);
  }

  // Create the equalizer elements if it's enabled.
  if (eq_enabled_) {
    equalizer_preamp_ = CreateElement(u"volume"_s, u"equalizer_preamp"_s, audiobin_, error);
    if (!equalizer_preamp_) {
      return false;
    }
    equalizer_ = CreateElement(u"equalizer-nbands"_s, u"equalizer_nbands"_s, audiobin_, error);
    if (!equalizer_) {
      return false;
    }
    // Setting the equalizer bands:
    //
    // GStreamer's GstIirEqualizerNBands sets up shelve filters for the first and last bands as corner cases.
    // That was causing the "inverted slider" bug.
    // As a workaround, we create two dummy bands at both ends of the spectrum.
    // This causes the actual first and last adjustable bands to be implemented using band-pass filters.

    g_object_set(G_OBJECT(equalizer_), "num-bands", kEqBandCount + 2, nullptr);

    // Dummy first band (bandwidth 0, cutting below 20Hz):
    GstObject *first_band = GST_OBJECT(gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(equalizer_), 0));
    if (first_band) {
      g_object_set(G_OBJECT(first_band), "freq", 20.0, "bandwidth", 0, "gain", 0.0F, nullptr);
      g_object_unref(G_OBJECT(first_band));
    }

    // Dummy last band (bandwidth 0, cutting over 20KHz):
    GstObject *last_band = GST_OBJECT(gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(equalizer_), kEqBandCount + 1));
    if (last_band) {
      g_object_set(G_OBJECT(last_band), "freq", 20000.0, "bandwidth", 0, "gain", 0.0F, nullptr);
      g_object_unref(G_OBJECT(last_band));
    }

    int last_band_frequency = 0;
    for (int i = 0; i < kEqBandCount; ++i) {
      const int index_in_eq = i + 1;
      GstObject *band = GST_OBJECT(gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(equalizer_), static_cast<guint>(index_in_eq)));
      if (band) {
        const float frequency = static_cast<float>(kEqBandFrequencies[i]);
        const float bandwidth = frequency - static_cast<float>(last_band_frequency);
        last_band_frequency = static_cast<int>(frequency);
        g_object_set(G_OBJECT(band), "freq", frequency, "bandwidth", bandwidth, "gain", 0.0F, nullptr);
        g_object_unref(G_OBJECT(band));
      }

    }  // for

  }

  eventprobe_ = audioqueueconverter_;

  // Create the replaygain elements if it's enabled.
  GstElement *rgvolume = nullptr;
  GstElement *rglimiter = nullptr;
  GstElement *rgconverter = nullptr;
  if (rg_enabled_) {
    rgvolume = CreateElement(u"rgvolume"_s, u"rgvolume"_s, audiobin_, error);
    if (!rgvolume) {
      return false;
    }
    rglimiter = CreateElement(u"rglimiter"_s, u"rglimiter"_s, audiobin_, error);
    if (!rglimiter) {
      return false;
    }
    rgconverter = CreateElement(u"audioconvert"_s, u"rgconverter"_s, audiobin_, error);
    if (!rgconverter) {
      return false;
    }
    eventprobe_ = rgconverter;
    // Set replaygain settings
    g_object_set(G_OBJECT(rgvolume), "album-mode", rg_mode_, nullptr);
    g_object_set(G_OBJECT(rgvolume), "pre-amp", rg_preamp_, nullptr);
    g_object_set(G_OBJECT(rgvolume), "fallback-gain", rg_fallbackgain_, nullptr);
    g_object_set(G_OBJECT(rglimiter), "enabled", static_cast<int>(rg_compression_), nullptr);
  }

  // Create the EBU R 128 loudness normalization volume element if enabled.
  if (ebur128_loudness_normalization_) {
    volume_ebur128_ = CreateElement(u"volume"_s, u"ebur128_volume"_s, audiobin_, error);
    if (!volume_ebur128_) {
      return false;
    }

    UpdateEBUR128LoudnessNormalizingGaindB();

    eventprobe_ = volume_ebur128_;
  }

  GstElement *bs2b = nullptr;
  if (bs2b_enabled_) {
    bs2b = CreateElement(u"bs2b"_s, u"bs2b"_s, audiobin_, error);
    if (!bs2b) {
      return false;
    }
  }

  {  // Create a pad on the outside of the audiobin and connect it to the pad of the first element.
    GstPad *pad = gst_element_get_static_pad(audioqueue_, "sink");
    if (pad) {
      gst_element_add_pad(audiobin_, gst_ghost_pad_new("sink", pad));
      gst_object_unref(pad);
    }
  }

  // Add a data probe on the src pad of the audioconvert element for our scope.
  // We do it here because we want pre-equalized and pre-volume samples so that our visualization are not be affected by them.
  {
    GstPad *pad = gst_element_get_static_pad(eventprobe_, "src");
    if (pad) {
      upstream_events_probe_cb_id_ = gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_EVENT_UPSTREAM, &UpstreamEventsProbeCallback, this, nullptr);
      gst_object_unref(pad);
    }
  }

  // Set the buffer duration.
  // We set this on this queue instead of the playbin because setting it on the playbin only affects network sources.
  // Disable the default buffer and byte limits, so we only buffer based on time.

  g_object_set(G_OBJECT(audioqueue_), "use-buffering", true, nullptr);

  if (buffer_duration_nanosec_ > 0) {
    qLog(Debug) << "Setting buffer duration:" << buffer_duration_nanosec_ << "low watermark:" << buffer_low_watermark_ << "high watermark:" << buffer_high_watermark_;
    g_object_set(G_OBJECT(audioqueue_), "max-size-buffers", 0, nullptr);
    g_object_set(G_OBJECT(audioqueue_), "max-size-bytes", 0, nullptr);
    g_object_set(G_OBJECT(audioqueue_), "max-size-time", buffer_duration_nanosec_, nullptr);
  }
  else {
    qLog(Debug) << "Setting low watermark:" << buffer_low_watermark_ << "high watermark:" << buffer_high_watermark_;
  }

  g_object_set(G_OBJECT(audioqueue_), "low-watermark", buffer_low_watermark_, nullptr);
  g_object_set(G_OBJECT(audioqueue_), "high-watermark", buffer_high_watermark_, nullptr);

  // Link all elements

  if (!gst_element_link(audioqueue_, audioqueueconverter_)) {
    error = u"Failed to link audio queue to audio queue converter."_s;
    return false;
  }

  GstElement *element_link = audioqueueconverter_;  // The next element to link from.

  // Link replaygain elements if enabled.
  if (rg_enabled_ && rgvolume && rglimiter && rgconverter) {
    if (!gst_element_link_many(element_link, rgvolume, rglimiter, rgconverter, nullptr)) {
      error = "Failed to link replaygain volume, limiter and converter elements."_L1;
      return false;
    }
    element_link = rgconverter;
  }

  // Link EBU R 128 loudness normalization volume element if enabled.
  if (ebur128_loudness_normalization_ && volume_ebur128_) {
    GstStaticCaps static_raw_fp_audio_caps = GST_STATIC_CAPS(
      "audio/x-raw,"
      "format = (string) { F32LE, F64LE }");
    GstCaps *raw_fp_audio_caps = gst_static_caps_get(&static_raw_fp_audio_caps);
    if (!gst_element_link_filtered(element_link, volume_ebur128_, raw_fp_audio_caps)) {
      error = "Failed to link EBU R 128 volume element."_L1;
      return false;
    }
    gst_caps_unref(raw_fp_audio_caps);
    element_link = volume_ebur128_;
  }

  // Link equalizer elements if enabled.
  if (eq_enabled_ && equalizer_ && equalizer_preamp_) {
    if (!gst_element_link_many(element_link, equalizer_preamp_, equalizer_, nullptr)) {
      error = "Failed to link equalizer and equalizer preamp elements."_L1;
      return false;
    }
    element_link = equalizer_;
  }

  // Link stereo balancer elements if enabled.
  if (stereo_balancer_enabled_ && audiopanorama_) {
    if (!gst_element_link(element_link, audiopanorama_)) {
      error = "Failed to link audio panorama (stereo balancer)."_L1;
      return false;
    }
    element_link = audiopanorama_;
  }

  // Link software volume element if enabled.
  if (volume_enabled_ && volume_sw_) {
    if (!gst_element_link(element_link, volume_sw_)) {
      error = "Failed to link software volume."_L1;
      return false;
    }
    element_link = volume_sw_;
  }

  // Link fading volume element if enabled.
  if (fading_enabled_ && volume_fading_) {
    if (!gst_element_link(element_link, volume_fading_)) {
      error = "Failed to link fading volume."_L1;
      return false;
    }
    element_link = volume_fading_;
  }

  // Link bs2b element if enabled.
  if (bs2b_enabled_ && bs2b) {
    qLog(Debug) << "Enabling bs2b";
    if (!gst_element_link(element_link, bs2b)) {
      error = "Failed to link bs2b."_L1;
      return false;
    }
    element_link = bs2b;
  }

  if (!gst_element_link(element_link, audiosinkconverter)) {
    error = "Failed to link audio sink converter."_L1;
    return false;
  }

  {
    GstCaps *caps = gst_caps_new_empty_simple("audio/x-raw");
    if (!caps) {
      error = "Failed to create caps for raw audio."_L1;
      return false;
    }
    if (channels_enabled_ && channels_ > 0) {
      qLog(Debug) << "Setting channels to" << channels_;
      gst_caps_set_simple(caps, "channels", G_TYPE_INT, channels_, nullptr);
    }
    const bool link_filtered_result = gst_element_link_filtered(audiosinkconverter, audiosink_, caps);
    gst_caps_unref(caps);
    if (!link_filtered_result) {
      error = "Failed to link audio sink converter to audio sink with filter for "_L1 + output_;
      return false;
    }
  }

  {  // Add probes and handlers.
    GstPad *pad = gst_element_get_static_pad(audioqueueconverter_, "src");
    if (pad) {
      buffer_probe_cb_id_ = gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, BufferProbeCallback, this, nullptr);
      gst_object_unref(pad);
    }
  }

  {
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
    if (bus) {
      gst_bus_set_sync_handler(bus, BusSyncCallback, this, nullptr);
      gst_bus_add_watch(bus, BusWatchCallback, this);
      gst_object_unref(bus);
    }
  }

  logged_unsupported_analyzer_format_ = false;

  return true;

}

void GstEnginePipeline::SetupVolume(GstElement *element) {

  if (volume_) {
    qLog(Debug) << "Disonnecting volume notify on" << volume_;
    g_signal_handler_disconnect(G_OBJECT(volume_), notify_volume_cb_id_.value());
    notify_volume_cb_id_.reset();
    volume_ = nullptr;
  }

  qLog(Debug) << "Connecting volume notify on" << element;
  notify_volume_cb_id_ = static_cast<glong>(CHECKED_GCONNECT(G_OBJECT(element), "notify::volume", &NotifyVolumeCallback, this));
  volume_ = element;
  volume_set_ = false;

  // Make sure the unused volume element is set to 1.0.
  if (volume_sw_ && volume_sw_ != volume_) {
    double volume_internal = 1.0;
    g_object_get(G_OBJECT(volume_sw_), "volume", &volume_internal, nullptr);
    if (volume_internal != 1.0) {
      volume_internal = 1.0;
      g_object_set(G_OBJECT(volume_sw_), "volume", volume_internal, nullptr);
    }
  }

}

GstPadProbeReturn GstEnginePipeline::UpstreamEventsProbeCallback(GstPad *pad, GstPadProbeInfo *info, gpointer self) {

  Q_UNUSED(pad)

  GstEnginePipeline *instance = reinterpret_cast<GstEnginePipeline*>(self);

  GstEvent *e = gst_pad_probe_info_get_event(info);

  qLog(Debug) << instance->id() << "event" << GST_EVENT_TYPE_NAME(e);

  switch (GST_EVENT_TYPE(e)) {
    case GST_EVENT_SEGMENT:
      if (!instance->segment_start_received_.value()) {
        // The segment start time is used to calculate the proper offset of data buffers from the start of the stream
        const GstSegment *segment = nullptr;
        gst_event_parse_segment(e, &segment);
        instance->segment_start_ = static_cast<qint64>(segment->start);
        instance->segment_start_received_ = true;
      }
      break;

    default:
      break;
  }

  return GST_PAD_PROBE_OK;

}

void GstEnginePipeline::ElementAddedCallback(GstBin *bin, GstBin *sub_bin, GstElement *element, gpointer self) {

  Q_UNUSED(sub_bin)

  GstEnginePipeline *instance = reinterpret_cast<GstEnginePipeline*>(self);

  gchar *element_name_char = gst_element_get_name(element);
  const QString element_name = QString::fromUtf8(element_name_char);
  g_free(element_name_char);

  if (bin != GST_BIN(instance->audiobin_) || element_name == "fake-audio-sink"_L1 || GST_ELEMENT(gst_element_get_parent(element)) != instance->audiosink_) return;

  GstElement *volume = nullptr;
  if (GST_IS_STREAM_VOLUME(element)) {
    qLog(Debug) << element_name << "has volume, enabling volume synchronization.";
    volume = element;
  }
  else {
    qLog(Debug) << element_name << "does not have volume, using own volume.";
    volume = instance->volume_sw_;
  }

  instance->SetupVolume(volume);
  instance->SetVolume(instance->volume_percent_.value());

}

void GstEnginePipeline::ElementRemovedCallback(GstBin *bin, GstBin *sub_bin, GstElement *element, gpointer self) {

  Q_UNUSED(sub_bin)

  GstEnginePipeline *instance = reinterpret_cast<GstEnginePipeline*>(self);

  if (bin != GST_BIN(instance->audiobin_)) return;

  if (instance->notify_volume_cb_id_.has_value() && element == instance->volume_) {
    qLog(Debug) << "Disconnecting volume notify on" << instance->volume_;
    g_signal_handler_disconnect(G_OBJECT(instance->volume_), instance->notify_volume_cb_id_.value());
    instance->notify_volume_cb_id_.reset();
    instance->volume_ = nullptr;
    instance->volume_set_ = false;
  }

}

void GstEnginePipeline::SourceSetupCallback(GstElement *playbin, GstElement *source, gpointer self) {

  Q_UNUSED(playbin)

  GstEnginePipeline *instance = reinterpret_cast<GstEnginePipeline*>(self);

  {
    QMutexLocker l(&instance->mutex_source_device_);
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(source), "device") && !instance->source_device().isEmpty()) {
      // Gstreamer is not able to handle device in URL (referring to Gstreamer documentation, this might be added in the future).
      // Despite that, for now we include device inside URL: we decompose it during Init and set device here, when this callback is called.
      qLog(Debug) << "Setting device";
      g_object_set(source, "device", instance->source_device().toLocal8Bit().constData(), nullptr);
    }
  }

  if (g_object_class_find_property(G_OBJECT_GET_CLASS(source), "user-agent")) {
    qLog(Debug) << "Setting user-agent";
    QString user_agent = QStringLiteral("%1 %2").arg(QCoreApplication::applicationName(), QCoreApplication::applicationVersion());
    g_object_set(source, "user-agent", user_agent.toUtf8().constData(), nullptr);
  }

  if (g_object_class_find_property(G_OBJECT_GET_CLASS(source), "ssl-strict")) {
    qLog(Debug) << "Turning" << (instance->strict_ssl_enabled_.value() ? "on" : "off") << "strict SSL";
    g_object_set(source, "ssl-strict", instance->strict_ssl_enabled_.value() ? TRUE : FALSE, nullptr);
  }

  {
    QMutexLocker l(&instance->mutex_proxy_);
    if (!instance->proxy_address_.isEmpty() && g_object_class_find_property(G_OBJECT_GET_CLASS(source), "proxy")) {
      qLog(Debug) << "Setting proxy to" << instance->proxy_address_;
      g_object_set(source, "proxy", instance->proxy_address_.toUtf8().constData(), nullptr);
      if (instance->proxy_authentication_ &&
          g_object_class_find_property(G_OBJECT_GET_CLASS(source), "proxy-id") &&
          g_object_class_find_property(G_OBJECT_GET_CLASS(source), "proxy-pw") &&
          !instance->proxy_user_.isEmpty() &&
          !instance->proxy_pass_.isEmpty())
      {
        g_object_set(source, "proxy-id", instance->proxy_user_.toUtf8().constData(), "proxy-pw", instance->proxy_pass_.toUtf8().constData(), nullptr);
      }
    }
  }

#ifdef HAVE_SPOTIFY
  {
    QMutexLocker mutex_locker_url(&instance->mutex_url_);
    if (instance->media_url_.scheme() == u"spotify"_s) {
      if (g_object_class_find_property(G_OBJECT_GET_CLASS(source), "bitrate")) {
        g_object_set(source, "bitrate", 2, nullptr);
      }
      QMutexLocker mutex_locker_spotify_access_token(&instance->mutex_spotify_access_token_);
      if (!instance->spotify_access_token_.isEmpty() && g_object_class_find_property(G_OBJECT_GET_CLASS(source), "access-token")) {
        const QByteArray access_token = instance->spotify_access_token_.toUtf8();
        g_object_set(source, "access-token", access_token.constData(), nullptr);
      }
    }
  }
#endif

  // If the pipeline was buffering we stop that now.
  if (instance->buffering_.value()) {
    qLog(Debug) << "Buffering finished";
    instance->buffering_ = false;
    Q_EMIT instance->BufferingFinished();
    instance->SetStateAsync(GST_STATE_PLAYING);
  }

}

void GstEnginePipeline::NotifyVolumeCallback(GstElement *element, GParamSpec *param_spec, gpointer self) {

  Q_UNUSED(element)
  Q_UNUSED(param_spec)

  GstEnginePipeline *instance = reinterpret_cast<GstEnginePipeline*>(self);

  if (!instance->volume_set_.value()) return;

  double volume_internal = 0.0;
  g_object_get(G_OBJECT(instance->volume_), "volume", &volume_internal, nullptr);

  const uint volume_percent = static_cast<uint>(qBound(0L, lround(volume_internal / 0.01), 100L));
  if (volume_percent != instance->volume_percent_.value()) {
    instance->volume_internal_ = volume_internal;
    instance->volume_percent_ = volume_percent;
    Q_EMIT instance->VolumeChanged(volume_percent);
  }

}

void GstEnginePipeline::PadAddedCallback(GstElement *element, GstPad *pad, gpointer self) {

  Q_UNUSED(element)

  GstEnginePipeline *instance = reinterpret_cast<GstEnginePipeline*>(self);

  GstPad *const audiopad = gst_element_get_static_pad(instance->audiobin_, "sink");

  // Link playbin's sink pad to audiobin's src pad.
  if (GST_PAD_IS_LINKED(audiopad)) {
    qLog(Warning) << instance->id() << "audiopad is already linked, unlinking old pad";
    gst_pad_unlink(audiopad, GST_PAD_PEER(audiopad));
  }

  gst_pad_link(pad, audiopad);
  gst_object_unref(audiopad);

  // Offset the timestamps on all the buffers coming out of the playbin so they line up exactly with the end of the last buffer from the old playbin.
  // "Running time" is the time since the last flushing seek.
  GstClockTime running_time = gst_segment_to_running_time(&instance->last_playbin_segment_, GST_FORMAT_TIME, instance->last_playbin_segment_.position);
  gst_pad_set_offset(pad, static_cast<gint64>(running_time));

  // Add a probe to the pad so we can update last_playbin_segment_.
  instance->pad_probe_cb_id_ = gst_pad_add_probe(pad, static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM | GST_PAD_PROBE_TYPE_EVENT_FLUSH), PadProbeCallback, instance, nullptr);

  instance->pipeline_connected_ = true;
  if (instance->pending_seek_nanosec_.value() != -1 && instance->pipeline_active_.value()) {
    QMetaObject::invokeMethod(instance, "Seek", Qt::QueuedConnection, Q_ARG(qint64, instance->pending_seek_nanosec_.value()));
    instance->pending_seek_nanosec_ = -1;
  }

}

GstPadProbeReturn GstEnginePipeline::PadProbeCallback(GstPad *pad, GstPadProbeInfo *info, gpointer self) {

  GstEnginePipeline *instance = reinterpret_cast<GstEnginePipeline*>(self);

  const GstPadProbeType info_type = GST_PAD_PROBE_INFO_TYPE(info);

  if (info_type & GST_PAD_PROBE_TYPE_BUFFER) {
    // The playbin produced a buffer.  Record its end time, so we can offset the buffers produced by the next playbin when transitioning to the next song.
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);

    GstClockTime timestamp = GST_BUFFER_TIMESTAMP(buffer);
    GstClockTime duration = GST_BUFFER_DURATION(buffer);
    if (timestamp == GST_CLOCK_TIME_NONE) {
      timestamp = instance->last_playbin_segment_.position;
    }

    if (duration != GST_CLOCK_TIME_NONE) {
      timestamp += duration;
    }

    instance->last_playbin_segment_.position = timestamp;
  }
  else if (info_type & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) {
    GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
    GstEventType event_type = GST_EVENT_TYPE(event);

    if (event_type == GST_EVENT_SEGMENT) {
      // A new segment started, we need to save this to calculate running time offsets later.
      gst_event_copy_segment(event, &instance->last_playbin_segment_);
    }
    else if (event_type == GST_EVENT_FLUSH_START) {
      // A flushing seek resets the running time to 0, so remove any offset we set on this pad before.
      gst_pad_set_offset(pad, 0);
    }
  }

  return GST_PAD_PROBE_OK;

}

GstPadProbeReturn GstEnginePipeline::BufferProbeCallback(GstPad *pad, GstPadProbeInfo *info, gpointer self) {

  GstEnginePipeline *instance = reinterpret_cast<GstEnginePipeline*>(self);

  QString format;
  int channels = 1;
  int rate = 0;

  GstCaps *caps = gst_pad_get_current_caps(pad);
  if (caps) {
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    if (structure) {
      format = QString::fromUtf8(gst_structure_get_string(structure, "format"));
      gst_structure_get_int(structure, "channels", &channels);
      gst_structure_get_int(structure, "rate", &rate);
    }
    gst_caps_unref(caps);
  }

  GstBuffer *buf = gst_pad_probe_info_get_buffer(info);
  GstBuffer *buf16 = nullptr;

  quint64 start_time = GST_BUFFER_TIMESTAMP(buf) - instance->segment_start_.value();
  quint64 duration = GST_BUFFER_DURATION(buf);
  qint64 end_time = static_cast<qint64>(start_time + duration);

  if (format.startsWith("S16LE"_L1)) {
    instance->logged_unsupported_analyzer_format_ = false;
  }
  else if (format.startsWith("S32LE"_L1)) {

    GstMapInfo map_info;
    gst_buffer_map(buf, &map_info, GST_MAP_READ);

    int32_t *s = reinterpret_cast<int32_t*>(map_info.data);
    int samples = static_cast<int>((map_info.size / sizeof(int32_t)) / channels);
    int buf16_size = samples * static_cast<int>(sizeof(int16_t)) * channels;
    int16_t *d = static_cast<int16_t*>(g_malloc(static_cast<gsize>(buf16_size)));
    memset(d, 0, static_cast<size_t>(buf16_size));
    for (int i = 0; i < (samples * channels); ++i) {
      d[i] = static_cast<int16_t>((s[i] >> 16));
    }
    gst_buffer_unmap(buf, &map_info);
    buf16 = gst_buffer_new_wrapped(d, static_cast<gsize>(buf16_size));
    GST_BUFFER_DURATION(buf16) = GST_FRAMES_TO_CLOCK_TIME(static_cast<guint64>(samples * sizeof(int16_t) / channels), static_cast<guint64>(rate));
    buf = buf16;

    instance->logged_unsupported_analyzer_format_ = false;
  }

  else if (format.startsWith("F32LE"_L1)) {

    GstMapInfo map_info;
    gst_buffer_map(buf, &map_info, GST_MAP_READ);

    float *s = reinterpret_cast<float*>(map_info.data);
    int samples = static_cast<int>((map_info.size / sizeof(float)) / channels);
    int buf16_size = samples * static_cast<int>(sizeof(int16_t)) * channels;
    int16_t *d = static_cast<int16_t*>(g_malloc(static_cast<gsize>(buf16_size)));
    memset(d, 0, static_cast<size_t>(buf16_size));
    for (int i = 0; i < (samples * channels); ++i) {
      float sample_float = (s[i] * static_cast<float>(32768.0));
      d[i] = static_cast<int16_t>(sample_float);
    }
    gst_buffer_unmap(buf, &map_info);
    buf16 = gst_buffer_new_wrapped(d, static_cast<gsize>(buf16_size));
    GST_BUFFER_DURATION(buf16) = GST_FRAMES_TO_CLOCK_TIME(static_cast<guint64>(samples * sizeof(int16_t) / channels), static_cast<guint64>(rate));
    buf = buf16;

    instance->logged_unsupported_analyzer_format_ = false;
  }
  else if (format.startsWith("S24LE"_L1)) {

    GstMapInfo map_info;
    gst_buffer_map(buf, &map_info, GST_MAP_READ);

    int8_t *s24 = reinterpret_cast<int8_t*>(map_info.data);
    int8_t *s24e = s24 + map_info.size;
    int samples = static_cast<int>((map_info.size / sizeof(int8_t)) / channels);
    int buf16_size = samples * static_cast<int>(sizeof(int16_t)) * channels;
    int16_t *s16 = static_cast<int16_t*>(g_malloc(static_cast<gsize>(buf16_size)));
    memset(s16, 0, static_cast<size_t>(buf16_size));
    for (int i = 0; i < (samples * channels); ++i) {
      s16[i] = *(reinterpret_cast<int16_t*>(s24 + 1));
      s24 += 3;
      if (s24 >= s24e) break;
    }
    gst_buffer_unmap(buf, &map_info);
    buf16 = gst_buffer_new_wrapped(s16, static_cast<gsize>(buf16_size));
    GST_BUFFER_DURATION(buf16) = GST_FRAMES_TO_CLOCK_TIME(static_cast<guint64>(samples * sizeof(int16_t) / channels), static_cast<guint64>(rate));
    buf = buf16;

    instance->logged_unsupported_analyzer_format_ = false;
  }
  else if (format.startsWith("S24_32LE"_L1)) {

    GstMapInfo map_info;
    gst_buffer_map(buf, &map_info, GST_MAP_READ);

    int32_t *s32 = reinterpret_cast<int32_t*>(map_info.data);
    int32_t *s32e = s32 + map_info.size;
    int32_t *s32p = s32;
    int samples = static_cast<int>((map_info.size / sizeof(int32_t)) / channels);
    int buf16_size = samples * static_cast<int>(sizeof(int16_t)) * channels;
    int16_t *s16 = static_cast<int16_t*>(g_malloc(static_cast<gsize>(buf16_size)));
    memset(s16, 0, static_cast<size_t>(buf16_size));
    for (int i = 0; i < (samples * channels); ++i) {
      int8_t *s24 = reinterpret_cast<int8_t*>(s32p);
      s16[i] = *(reinterpret_cast<int16_t*>(s24 + 1));
      ++s32p;
      if (s32p > s32e) break;
    }
    gst_buffer_unmap(buf, &map_info);
    buf16 = gst_buffer_new_wrapped(s16, static_cast<gsize>(buf16_size));
    GST_BUFFER_DURATION(buf16) = GST_FRAMES_TO_CLOCK_TIME(static_cast<guint64>(samples * sizeof(int16_t) / channels), static_cast<guint64>(rate));
    buf = buf16;

    instance->logged_unsupported_analyzer_format_ = false;
  }
  else if (!instance->logged_unsupported_analyzer_format_) {
    instance->logged_unsupported_analyzer_format_ = true;
    qLog(Error) << "Unsupported audio format for the analyzer" << format;
  }

  QList<GstBufferConsumer*> consumers;
  {
    QMutexLocker l(&instance->mutex_buffer_consumers_);
    consumers = instance->buffer_consumers_;
  }

  for (GstBufferConsumer *consumer : std::as_const(consumers)) {
    gst_buffer_ref(buf);
    consumer->ConsumeBuffer(buf, instance->id(), format);
  }

  if (buf16) {
    gst_buffer_unref(buf16);
  }

  // Calculate the end time of this buffer so we can stop playback if it's after the end time of this song.
  if (instance->end_offset_nanosec_.value() > 0 && end_time > instance->end_offset_nanosec_.value()) {
    if (instance->HasMatchingNextUrl() && instance->next_beginning_offset_nanosec_.value() == instance->end_offset_nanosec_.value()) {
      // The "next" song is actually the next segment of this file - so cheat and keep on playing, but just tell the Engine we've moved on.
      instance->beginning_offset_nanosec_ = instance->next_beginning_offset_nanosec_;
      instance->end_offset_nanosec_ = instance->next_end_offset_nanosec_;
      instance->next_media_url_.clear();
      instance->next_stream_url_.clear();
      instance->next_gst_url_.clear();
      instance->next_beginning_offset_nanosec_ = 0;
      instance->next_end_offset_nanosec_ = 0;

      // GstEngine will try to seek to the start of the new section, but we're already there so ignore it.
      instance->ignore_next_seek_ = true;
      Q_EMIT instance->EndOfStreamReached(instance->id(), true);
    }
    else {
      // There's no next song
      Q_EMIT instance->EndOfStreamReached(instance->id(), false);
    }
  }

  return GST_PAD_PROBE_OK;

}

void GstEnginePipeline::AboutToFinishCallback(GstPlayBin *playbin, gpointer self) {

  Q_UNUSED(playbin)

  GstEnginePipeline *instance = reinterpret_cast<GstEnginePipeline*>(self);

  {
    QMutexLocker l(&instance->mutex_url_);
    qLog(Debug) << "Stream from URL" << instance->gst_url_ << "about to finish.";
  }

  // When playing GME files it seems playbin3 emits about-to-finish early
  // This stops us from skipping when the song has just started.
  if (instance->position() == 0) {
    return;
  }

  instance->about_to_finish_ = true;

  if (instance->HasNextUrl() && !instance->next_uri_set_.value()) {
    instance->SetNextUrl();
  }

  Q_EMIT instance->AboutToFinish();

}

GstBusSyncReply GstEnginePipeline::BusSyncCallback(GstBus *bus, GstMessage *msg, gpointer self) {

  Q_UNUSED(bus)

  GstEnginePipeline *instance = reinterpret_cast<GstEnginePipeline*>(self);

  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS:
      Q_EMIT instance->EndOfStreamReached(instance->id(), false);
      break;

    case GST_MESSAGE_TAG:
      instance->TagMessageReceived(msg);
      break;

    case GST_MESSAGE_ERROR:
      instance->ErrorMessageReceived(msg);
      break;

    case GST_MESSAGE_ELEMENT:
      instance->ElementMessageReceived(msg);
      break;

    case GST_MESSAGE_STATE_CHANGED:
      instance->StateChangedMessageReceived(msg);
      break;

    case GST_MESSAGE_BUFFERING:
      instance->BufferingMessageReceived(msg);
      break;

    case GST_MESSAGE_STREAM_STATUS:
      instance->StreamStatusMessageReceived(msg);
      break;

    case GST_MESSAGE_STREAM_START:
      instance->StreamStartMessageReceived();
      break;

    default:
      break;
  }

  return GST_BUS_PASS;

}

gboolean GstEnginePipeline::BusWatchCallback(GstBus *bus, GstMessage *msg, gpointer self) {

  Q_UNUSED(bus)

  GstEnginePipeline *instance = reinterpret_cast<GstEnginePipeline*>(self);

  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR:
      instance->ErrorMessageReceived(msg);
      break;

    case GST_MESSAGE_TAG:
      instance->TagMessageReceived(msg);
      break;

    case GST_MESSAGE_STATE_CHANGED:
      instance->StateChangedMessageReceived(msg);
      break;

    default:
      break;
  }

  return TRUE;

}

void GstEnginePipeline::StreamStatusMessageReceived(GstMessage *msg) {

  GstStreamStatusType type = GST_STREAM_STATUS_TYPE_CREATE;
  GstElement *owner = nullptr;
  gst_message_parse_stream_status(msg, &type, &owner);

  if (type == GST_STREAM_STATUS_TYPE_CREATE) {
    const GValue *val = gst_message_get_stream_status_object(msg);
    if (G_VALUE_TYPE(val) == GST_TYPE_TASK) {
      GstTask *task = static_cast<GstTask*>(g_value_get_object(val));
      gst_task_set_enter_callback(task, &TaskEnterCallback, this, nullptr);
    }
  }

}

void GstEnginePipeline::StreamStartMessageReceived() {

  if (next_uri_set_.value()) {
    next_uri_set_ = false;
    next_uri_reset_ = false;
    about_to_finish_ = false;
    {
      QMutexLocker lock_url(&mutex_url_);
      QMutexLocker lock_next_url(&mutex_next_url_);
      qLog(Debug) << "Stream changed from URL" << gst_url_ << "to" << next_gst_url_;
      media_url_ = next_media_url_;
      stream_url_ = next_stream_url_;
      gst_url_ = next_gst_url_;
      next_stream_url_.clear();
      next_media_url_.clear();
      next_gst_url_.clear();
    }
    beginning_offset_nanosec_ = next_beginning_offset_nanosec_;
    end_offset_nanosec_ = next_end_offset_nanosec_;
    next_beginning_offset_nanosec_ = 0;
    next_end_offset_nanosec_ = 0;

    Q_EMIT EndOfStreamReached(id(), true);
  }

}

void GstEnginePipeline::TaskEnterCallback(GstTask *task, GThread *thread, gpointer self) {

  Q_UNUSED(task)
  Q_UNUSED(thread)
  Q_UNUSED(self)

#ifdef Q_OS_UNIX
  sched_param param{};
  memset(&param, 0, sizeof(param));
  param.sched_priority = 40;
  pthread_setschedparam(pthread_self(), SCHED_RR, &param);
#endif

#ifdef Q_OS_WIN32
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
#endif

}

void GstEnginePipeline::ElementMessageReceived(GstMessage *msg) {

  const GstStructure *structure = gst_message_get_structure(msg);

  if (gst_structure_has_name(structure, "redirect")) {
    const char *uri = gst_structure_get_string(structure, "new-location");

    // Set the redirect URL.  In mmssrc redirect messages come during the initial state change to PLAYING, so callers can pick up this URL after the state change has failed.
    QMutexLocker l(&mutex_redirect_url_);
    redirect_url_ = uri;
  }

}

void GstEnginePipeline::ErrorMessageReceived(GstMessage *msg) {

  GError *error = nullptr;
  gchar *debugs = nullptr;

  gst_message_parse_error(msg, &error, &debugs);
  GQuark domain = error->domain;
  int code = error->code;
  QString message = QString::fromLocal8Bit(error->message);
  QString debugstr = QString::fromLocal8Bit(debugs);
  g_error_free(error);
  g_free(debugs);

  if (pipeline_active_.value() && next_uri_set_.value() && (domain == GST_CORE_ERROR || domain == GST_RESOURCE_ERROR || domain == GST_STREAM_ERROR)) {
    // A track is still playing and the next uri is not playable. We ignore the error here so it can play until the end.
    // But there is no message send to the bus when the current track finishes, we have to add an EOS ourself.
    qLog(Info) << "Ignoring error" << domain << code << message << debugstr << "when loading next track";
    GstPad *pad = gst_element_get_static_pad(audiobin_, "sink");
    gst_pad_send_event(pad, gst_event_new_eos());
    gst_object_unref(pad);
    return;
  }

  qLog(Error) << __FUNCTION__ << "ID:" << id() << "Domain:" << domain << "Code:" << code << "Error:" << message;
  qLog(Error) << __FUNCTION__ << "ID:" << id() << "Domain:" << domain << "Code:" << code << "Debug:" << debugstr;

  {
    QMutexLocker l(&mutex_redirect_url_);
    if (!redirect_url_.isEmpty() && debugstr.contains("A redirect message was posted on the bus and should have been handled by the application."_L1)) {
      // mmssrc posts a message on the bus *and* makes an error message when it wants to do a redirect.
      // We handle the message, but now we have to ignore the error too.
      return;
    }
  }

#ifdef Q_OS_WIN32
  // Ignore non-error received for directsoundsink: "IDirectSoundBuffer_GetStatus The operation completed successfully"
  if (code == GST_RESOURCE_ERROR_OPEN_WRITE && message.contains(QLatin1String("IDirectSoundBuffer_GetStatus The operation completed successfully."))) {
    return;
  }
#endif

  Q_EMIT Error(id(), static_cast<int>(domain), code, message, debugstr);

}

void GstEnginePipeline::TagMessageReceived(GstMessage *msg) {

  if (ignore_tags_.value()) return;

  GstTagList *taglist = nullptr;
  gst_message_parse_tag(msg, &taglist);

  EngineMetadata engine_metadata;
  engine_metadata.type = EngineMetadata::Type::Current;
  {
    QMutexLocker l(&mutex_url_);
    engine_metadata.media_url = media_url_;
    engine_metadata.stream_url = stream_url_;
  }
  engine_metadata.title = ParseStrTag(taglist, GST_TAG_TITLE);
  engine_metadata.artist = ParseStrTag(taglist, GST_TAG_ARTIST);
  engine_metadata.comment = ParseStrTag(taglist, GST_TAG_COMMENT);
  engine_metadata.album = ParseStrTag(taglist, GST_TAG_ALBUM);
  engine_metadata.bitrate = static_cast<int>(ParseUIntTag(taglist, GST_TAG_BITRATE) / 1000);
  engine_metadata.lyrics = ParseStrTag(taglist, GST_TAG_LYRICS);

  if (!engine_metadata.title.isEmpty() && engine_metadata.artist.isEmpty() && engine_metadata.album.isEmpty()) {
    QStringList title_splitted;
    if (engine_metadata.title.contains(" - "_L1)) {
      title_splitted = engine_metadata.title.split(u" - "_s);
    }
    else if (engine_metadata.title.contains(u'~')) {
      title_splitted = engine_metadata.title.split(u'~');
    }
    if (!title_splitted.isEmpty() && title_splitted.count() >= 2) {
      int i = 0;
      for (const QString &title_part : std::as_const(title_splitted)) {
        ++i;
        switch (i) {
          case 1:
            engine_metadata.artist = title_part.trimmed();
            break;
          case 2:
            engine_metadata.title = title_part.trimmed();
            break;
          case 3:
            engine_metadata.album = title_part.trimmed();
            break;
          default:
            break;
        }
      }
    }
  }

  gst_tag_list_unref(taglist);

  Q_EMIT MetadataFound(id(), engine_metadata);

}

QString GstEnginePipeline::ParseStrTag(GstTagList *list, const char *tag) {

  gchar *data = nullptr;
  bool success = gst_tag_list_get_string(list, tag, &data);

  QString ret;
  if (success && data) {
    ret = QString::fromUtf8(data);
    g_free(data);
  }
  return ret.trimmed();

}

guint GstEnginePipeline::ParseUIntTag(GstTagList *list, const char *tag) {

  guint data = 0;
  bool success = gst_tag_list_get_uint(list, tag, &data);

  guint ret = 0;
  if (success && data) ret = data;
  return ret;

}

void GstEnginePipeline::StateChangedMessageReceived(GstMessage *msg) {

  if (!pipeline_ || msg->src != GST_OBJECT(pipeline_)) {
    // We only care about state changes of the whole pipeline.
    return;
  }

  GstState old_state = GST_STATE_NULL, new_state = GST_STATE_NULL, pending = GST_STATE_NULL;
  gst_message_parse_state_changed(msg, &old_state, &new_state, &pending);

  qLog(Debug) << "Pipeline state changed from" << GstStateText(old_state) << "to" << GstStateText(new_state);

  const bool pipeline_active = new_state == GST_STATE_PAUSED || new_state == GST_STATE_PLAYING;
  if (pipeline_active != pipeline_active_.value()) {
    pipeline_active_ = pipeline_active;
    qLog(Debug) << "Pipeline is" << (pipeline_active ? "active" : "inactive");
  }

  if (new_state == GST_STATE_NULL && !finished_.value() && finish_requested_.value()) {
    finished_ = true;
    Q_EMIT Finished();
    return;
  }

  if (pipeline_connected_.value() && pipeline_active_.value() && !volume_set_.value()) {
    SetVolume(volume_percent_.value());
  }

  if (next_uri_set_.value() && next_uri_need_reset_.value() && new_state == GST_STATE_READY && pending_seek_nanosec_.value() != -1) {
    qLog(Debug) << "Reverting next uri and going to pause state.";
    next_uri_set_ = false;
    {
      QMutexLocker l(&mutex_url_);
      g_object_set(G_OBJECT(pipeline_), "uri", gst_url_.constData(), nullptr);
    }
    next_uri_need_reset_ = false;
    next_uri_reset_ = true;
    SetStateAsync(GST_STATE_PAUSED);
    return;
  }

  if (pipeline_active_.value() && !buffering_.value() && !next_uri_need_reset_.value()) {
    if (pending_seek_nanosec_.value() != -1) {
      ProcessPendingSeek(new_state);
    }
    else if (pending_state_.value() != GST_STATE_NULL) {
      SetStateAsync(pending_state_.value());
      pending_state_ = GST_STATE_NULL;
    }
    if (fader_ && fader_active_.value() && !fader_running_.value() && new_state == GST_STATE_PLAYING) {
      qLog(Debug) << "Resuming fader";
      ResumeFaderAsync();
    }
  }

}

void GstEnginePipeline::BufferingMessageReceived(GstMessage *msg) {

  // Only handle buffering messages from the queue2 element in audiobin - not the one that's created automatically by playbin.
  if (GST_ELEMENT(GST_MESSAGE_SRC(msg)) != audioqueue_) {
    return;
  }

  int percent = 0;
  gst_message_parse_buffering(msg, &percent);

  const GstState current_state = state();

  if (percent < 100 && !buffering_.value()) {
    qLog(Debug) << "Buffering started";
    buffering_ = true;
    Q_EMIT BufferingStarted();
    if (current_state == GST_STATE_PLAYING) {
      SetStateAsync(GST_STATE_PAUSED);
      if (pending_state_.value() == GST_STATE_NULL) {
        pending_state_ = current_state;
      }
    }
  }
  else if (percent == 100 && buffering_.value()) {
    qLog(Debug) << "Buffering finished";
    buffering_ = false;
    Q_EMIT BufferingFinished();
    if (pending_seek_nanosec_.value() != -1 && !next_uri_need_reset_.value()) {
      ProcessPendingSeek(state());
    }
    else if (pending_state_.value() != GST_STATE_NULL) {
      SetStateAsync(pending_state_.value());
      pending_state_ = GST_STATE_NULL;
    }
  }
  else if (buffering_.value()) {
    Q_EMIT BufferingProgress(percent);
  }

}

GstState GstEnginePipeline::state() const {

  GstState s = GST_STATE_NULL, sp = GST_STATE_NULL;
  if (!pipeline_ || gst_element_get_state(pipeline_, &s, &sp, kGstStateTimeoutNanosecs) == GST_STATE_CHANGE_FAILURE) {
    return GST_STATE_NULL;
  }

  return s;

}

qint64 GstEnginePipeline::length() const {

  gint64 value = 0;
  if (pipeline_) gst_element_query_duration(pipeline_, GST_FORMAT_TIME, &value);

  return value;

}

qint64 GstEnginePipeline::position() const {

  if (pipeline_active_.value()) {
    gint64 current_position = 0;
    if (gst_element_query_position(pipeline_, GST_FORMAT_TIME, &current_position)) {
      last_known_position_ns_ = current_position;
    }
  }

  return last_known_position_ns_;

}

bool GstEnginePipeline::IsStateNull() const {

  if (!pipeline_) return true;

  GstState s = GST_STATE_NULL, sp = GST_STATE_NULL;
  return gst_element_get_state(pipeline_, &s, &sp, kGstStateTimeoutNanosecs) == GST_STATE_CHANGE_SUCCESS && s == GST_STATE_NULL;

}

void GstEnginePipeline::SetStateAsync(const GstState state) {

  last_set_state_async_in_progress_ = state;
  ++set_state_async_in_progress_;

  QMetaObject::invokeMethod(this, "SetStateAsyncSlot", Qt::QueuedConnection, Q_ARG(GstState, state));

}

void GstEnginePipeline::SetStateAsyncSlot(const GstState state) {

  last_set_state_async_in_progress_ = GST_STATE_VOID_PENDING;
  --set_state_async_in_progress_;

  SetState(state);

}

QFuture<GstStateChangeReturn> GstEnginePipeline::SetState(const GstState state) {

  qLog(Debug) << "Setting pipeline" << id() << "state to" << GstStateText(state);

  last_set_state_in_progress_ = state;
  ++set_state_in_progress_;

  QFutureWatcher<GstStateChangeReturn> *watcher = new QFutureWatcher<GstStateChangeReturn>();
  QObject::connect(watcher, &QFutureWatcher<GstStateChangeReturn>::finished, this, [this, watcher, state]() {
    const GstStateChangeReturn state_change_return = watcher->result();
    watcher->deleteLater();
    SetStateFinishedSlot(state, state_change_return);
  });
  QFuture<GstStateChangeReturn> future = QtConcurrent::run(&set_state_threadpool_, &gst_element_set_state, pipeline_, state);
  watcher->setFuture(future);

  return future;

}

void GstEnginePipeline::SetStateFinishedSlot(const GstState state, const GstStateChangeReturn state_change_return) {

  last_set_state_in_progress_ = GST_STATE_VOID_PENDING;
  --set_state_in_progress_;

  switch (state_change_return) {
    case GST_STATE_CHANGE_SUCCESS:
    case GST_STATE_CHANGE_ASYNC:
    case GST_STATE_CHANGE_NO_PREROLL:
      qLog(Debug) << "Pipeline" << id() << "state successfully set to" << GstStateText(state);
      Q_EMIT SetStateFinished(state_change_return);
      if (!finished_.value() && finish_requested_.value() && set_state_async_in_progress_ == 0 && set_state_in_progress_ == 0) {
        finished_ = true;
        Q_EMIT Finished();
      }
      break;
    case GST_STATE_CHANGE_FAILURE:
      qLog(Error) << "Failed to set pipeline to state" << GstStateText(state);
      break;
  }

}

QFuture<GstStateChangeReturn> GstEnginePipeline::Play(const bool pause, const quint64 offset_nanosec) {

  if (offset_nanosec != 0) {
    pending_seek_nanosec_ = static_cast<qint64>(offset_nanosec);
  }

  if (!pause) {
    pending_state_ = GST_STATE_PLAYING;
  }

  return SetState(GST_STATE_PAUSED);

}

bool GstEnginePipeline::Seek(const qint64 nanosec) {

  if (ignore_next_seek_.value()) {
    ignore_next_seek_ = false;
    return true;
  }

  if (next_uri_set_.value() || next_uri_reset_.value()) {
    qLog(Debug) << "Seek to" << nanosec << "requested, but next uri is set, adding to pending seek to revert next uri.";
    pending_seek_nanosec_ = nanosec;
    if (!next_uri_need_reset_.value() && !next_uri_reset_.value()) {
      next_uri_need_reset_ = true;
      pending_seek_ready_previous_state_ = state();
      SetState(GST_STATE_READY);
    }
    return true;
  }

  if (!pipeline_connected_.value() || !pipeline_active_.value()) {
    qLog(Debug) << "Seek to" << nanosec << "requested, but pipeline is not active, adding to pending seek.";
    pending_seek_nanosec_ = nanosec;
    return true;
  }

  pending_seek_nanosec_ = -1;
  last_known_position_ns_ = nanosec;

  qLog(Debug) << "Seeking to" << nanosec;

  const bool success = gst_element_seek_simple(pipeline_, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, nanosec);

  if (success) {
    qLog(Debug) << "Seek succeeded";
    if (pending_state_.value() != GST_STATE_NULL) {
      qLog(Debug) << "Setting state from pending state" << GstStateText(pending_state_.value());
      SetState(pending_state_.value());
      pending_state_ = GST_STATE_NULL;
    }
  }

  return success;

}

void GstEnginePipeline::SeekAsync(const qint64 nanosec) {

  QMetaObject::invokeMethod(this, "Seek", Qt::QueuedConnection, Q_ARG(qint64, nanosec));

}

void GstEnginePipeline::SeekDelayed(const qint64 nanosec) {

  QMetaObject::invokeMethod(this, [this, nanosec]() {
    QTimer::singleShot(100, this, [this, nanosec]() { Seek(nanosec); });
  }, Qt::QueuedConnection);

}

void GstEnginePipeline::ProcessPendingSeek(const GstState state) {

  if (pending_seek_nanosec_.value() == -1) return;

  if (next_uri_reset_.value()) {
    if (state != GST_STATE_PAUSED) {
      return;
    }
    if (pending_seek_ready_previous_state_.value() == GST_STATE_NULL) {
      pending_seek_ready_previous_state_ = GST_STATE_PLAYING;
    }
    qLog(Debug) << "Next uri is reset, seeking and going back to" << GstStateText(pending_seek_ready_previous_state_.value());
    if (pending_seek_ready_previous_state_.value() != GST_STATE_PAUSED) {
      pending_state_ = pending_seek_ready_previous_state_.value();
    }
    pending_seek_ready_previous_state_ = GST_STATE_NULL;
    next_uri_reset_ = false;
    SeekDelayed(pending_seek_nanosec_.value());
  }
  else {
    SeekAsync(pending_seek_nanosec_.value());
  }

  pending_seek_nanosec_ = -1;

}

void GstEnginePipeline::SetVolume(const uint volume_percent) {

  if (volume_) {
    const double volume_internal = static_cast<double>(volume_percent) * 0.01;
    if (!volume_set_.value() || volume_internal != volume_internal_.value()) {
      volume_internal_ = volume_internal;
      g_object_set(G_OBJECT(volume_), "volume", volume_internal, nullptr);
      if (pipeline_active_.value()) {
        volume_set_ = true;
      }
    }
  }

  volume_percent_ = volume_percent;

}

void GstEnginePipeline::SetStereoBalance(const float value) {

  stereo_balance_ = value;
  UpdateStereoBalance();

}

void GstEnginePipeline::UpdateStereoBalance() {

  if (audiopanorama_) {
    g_object_set(G_OBJECT(audiopanorama_), "panorama", stereo_balance_, nullptr);
  }

}

void GstEnginePipeline::SetEqualizerParams(const int preamp, const QList<int> &band_gains) {

  eq_preamp_ = preamp;
  eq_band_gains_ = band_gains;
  UpdateEqualizer();

}

void GstEnginePipeline::UpdateEqualizer() {

  if (!equalizer_ || !equalizer_preamp_) return;

  // Update band gains
  for (int i = 0; i < kEqBandCount; ++i) {
    float gain = eq_enabled_ ? static_cast<float>(eq_band_gains_.value(i)) : static_cast<float>(0.0);
    if (gain < 0) {
      gain *= 0.24F;
    }
    else {
      gain *= 0.12F;
    }

    const int index_in_eq = i + 1;
    // Offset because of the first dummy band we created.
    GstObject *band = GST_OBJECT(gst_child_proxy_get_child_by_index(GST_CHILD_PROXY(equalizer_), static_cast<guint>(index_in_eq)));
    g_object_set(G_OBJECT(band), "gain", gain, nullptr);
    g_object_unref(G_OBJECT(band));
  }

  // Update preamp
  float preamp = 1.0F;
  if (eq_enabled_) preamp = static_cast<float>(eq_preamp_ + 100) * 0.01F;  // To scale from 0.0 to 2.0

  g_object_set(G_OBJECT(equalizer_preamp_), "volume", preamp, nullptr);

}

void GstEnginePipeline::SetEBUR128LoudnessNormalizingGain_dB(const double ebur128_loudness_normalizing_gain_db) {

  ebur128_loudness_normalizing_gain_db_ = ebur128_loudness_normalizing_gain_db;
  UpdateEBUR128LoudnessNormalizingGaindB();

}

void GstEnginePipeline::UpdateEBUR128LoudnessNormalizingGaindB() {

  if (volume_ebur128_) {
    auto dB_to_mult = [](const double gain_dB) { return std::pow(10., gain_dB / 20.); };

    g_object_set(G_OBJECT(volume_ebur128_), volume_full_range_support_ ? "volume-full-range" : "volume", dB_to_mult(ebur128_loudness_normalizing_gain_db_), nullptr);
  }

}

void GstEnginePipeline::StartFader(const qint64 duration_nanosec, const QTimeLine::Direction direction, const QEasingCurve::Type shape, const bool use_fudge_timer) {

  fader_active_ = true;

  const qint64 duration_msec = duration_nanosec / kNsecPerMsec;

  // If there's already another fader running then start from the same time that one was already at.
  qint64 start_time = direction == QTimeLine::Direction::Forward ? 0 : duration_msec;
  if (fader_ && fader_->state() == QTimeLine::State::Running) {
    if (duration_msec == fader_->duration()) {
      start_time = fader_->currentTime();
    }
    else {
      // Calculate the position in the new fader with the same value from the old fader, so no volume jumps appear
      qreal time = static_cast<qreal>(duration_msec) * (static_cast<qreal>(fader_->currentTime()) / static_cast<qreal>(fader_->duration()));
      start_time = qRound(time);
    }
  }

  fader_.reset(new QTimeLine(static_cast<int>(duration_msec)), [](QTimeLine *timeline) {
    if (timeline->state() != QTimeLine::State::NotRunning) {
      timeline->stop();
    }
    timeline->deleteLater();
  });
  QObject::connect(&*fader_, &QTimeLine::valueChanged, this, &GstEnginePipeline::SetFaderVolume);
  QObject::connect(&*fader_, &QTimeLine::stateChanged, this, &GstEnginePipeline::FaderTimelineStateChanged);
  QObject::connect(&*fader_, &QTimeLine::finished, this, &GstEnginePipeline::FaderTimelineFinished);
  fader_->setDirection(direction);
  fader_->setEasingCurve(shape);
  fader_->setCurrentTime(static_cast<int>(start_time));

  timer_fader_timeout_->setInterval(std::chrono::milliseconds(duration_msec) + kFaderTimeoutMsec);
  timer_fader_timeout_->start();

  timer_fader_fudge_->stop();
  fader_use_fudge_timer_ = use_fudge_timer;

  SetFaderVolume(fader_->currentValue());

  qLog(Debug) << "Pipeline" << id() << "with state" << GstStateText(state()) << "set to fade from" << fader_->currentValue() << "time" << start_time << "direction" << (direction == QTimeLine::Direction::Forward ? "forward" : "backward");

  if (pipeline_active_.value()) {
    fader_->resume();
  }

}

void GstEnginePipeline::SetFaderVolume(const qreal volume) {

  if (volume_fading_) {
    g_object_set(G_OBJECT(volume_fading_), "volume", volume, nullptr);
  }

}

void GstEnginePipeline::ResumeFaderAsync() {

  if (fader_active_.value() && !fader_running_.value()) {
    QMetaObject::invokeMethod(&*fader_, &QTimeLine::resume, Qt::QueuedConnection);
  }

}

void GstEnginePipeline::FaderTimelineStateChanged(const QTimeLine::State state) {

  fader_running_ = state == QTimeLine::State::Running;

}

void GstEnginePipeline::FaderTimelineFinished() {

  qLog(Debug) << "Pipeline" << id() << "finished fading";

  fader_active_ = false;
  fader_running_ = false;

  fader_.reset();

  timer_fader_timeout_->stop();

  // Wait a little while longer before emitting the finished signal (and probably destroying the pipeline) to account for delays in the audio server/driver.
  timer_fader_fudge_->setInterval(fader_use_fudge_timer_ ? kFaderFudgeMsec : 250ms);
  timer_fader_fudge_->start();

}

void GstEnginePipeline::FaderTimelineTimeout() {

  qLog(Debug) << "Pipeline" << id() << "fading timed out";

  if (volume_fading_) {
    qLog(Debug) << "Pipeline" << id() << "setting volume" << (fader_->direction() == QTimeLine::Direction::Forward ? 1.0 : 0.0);
    g_object_set(G_OBJECT(volume_fading_), "volume", fader_->direction() == QTimeLine::Direction::Forward ? 1.0 : 0.0, nullptr);
  }

  FaderTimelineFinished();

}

void GstEnginePipeline::FaderFudgeFinished() {

  qLog(Debug) << "Pipeline" << id() << "fading fudge finished";

  Q_EMIT FaderFinished(id());

}

bool GstEnginePipeline::HasNextUrl() const {

  QMutexLocker l(&mutex_next_url_);
  return next_stream_url_.isValid();

}

bool GstEnginePipeline::HasMatchingNextUrl() const {

  QMutexLocker mutex_locker_url(&mutex_url_);
  QMutexLocker mutex_locker_next_url(&mutex_next_url_);
  return next_stream_url_.isValid() && next_stream_url_ == stream_url_;

}

void GstEnginePipeline::PrepareNextUrl(const QUrl &media_url, const QUrl &stream_url, const QByteArray &gst_url, const qint64 beginning_offset_nanosec, const qint64 end_offset_nanosec) {

  {
    QMutexLocker l(&mutex_next_url_);
    next_media_url_ = media_url;
    next_stream_url_ = stream_url;
    next_gst_url_ = gst_url;
  }

  next_beginning_offset_nanosec_ = beginning_offset_nanosec;
  next_end_offset_nanosec_ = end_offset_nanosec;

  if (about_to_finish_.value()) {
    SetNextUrl();
  }

}

void GstEnginePipeline::SetNextUrl() {

  if (about_to_finish_.value() && HasNextUrl() && !next_uri_set_.value()) {
    // Set the next uri. When the current song ends it will be played automatically and a STREAM_START message is send to the bus.
    // When the next uri is not playable an error message is send when the pipeline goes to PLAY (or PAUSE) state or immediately if it is currently in PLAY state.
    next_uri_set_ = true;
    {
      QMutexLocker l(&mutex_next_url_);
      qLog(Debug) << "Setting next URL to" << next_gst_url_;
      g_object_set(G_OBJECT(pipeline_), "uri", next_gst_url_.constData(), nullptr);
    }
    about_to_finish_ = false;
  }

}

void GstEnginePipeline::SetSourceDevice(const QString &device) {

  QMutexLocker l(&mutex_source_device_);
  source_device_ = device;

}

void GstEnginePipeline::AddBufferConsumer(GstBufferConsumer *consumer) {
  QMutexLocker l(&mutex_buffer_consumers_);
  buffer_consumers_ << consumer;
}

void GstEnginePipeline::RemoveBufferConsumer(GstBufferConsumer *consumer) {
  QMutexLocker l(&mutex_buffer_consumers_);
  buffer_consumers_.removeAll(consumer);
}

void GstEnginePipeline::RemoveAllBufferConsumers() {
  QMutexLocker l(&mutex_buffer_consumers_);
  buffer_consumers_.clear();
}
