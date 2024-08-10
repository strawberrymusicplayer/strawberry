/***************************************************************************
 *   Copyright (C) 2003-2005 by Mark Kretschmann <markey@web.de>           *
 *   Copyright (C) 2005 by Jakub Stachowski <qbast@go2.pl>                 *
 *   Copyright (C) 2006 Paul Cifarelli <paul@cifarelli.net>                *
 *   Copyright (C) 2017-2021 Jonas Kvinge <jonas@jkvinge.net>              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Steet, Fifth Floor, Boston, MA  02111-1307, USA.          *
 ***************************************************************************/

#include "config.h"

#include <cmath>
#include <algorithm>
#include <optional>
#include <utility>
#include <memory>

#include <glib.h>
#include <glib-object.h>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include <QtGlobal>
#include <QFuture>
#include <QFutureWatcher>
#include <QTimer>
#include <QList>
#include <QByteArray>
#include <QChar>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QTimeLine>
#include <QEasingCurve>
#include <QMetaObject>
#include <QTimerEvent>

#include "core/shared_ptr.h"
#include "core/logging.h"
#include "core/taskmanager.h"
#include "core/signalchecker.h"
#include "utilities/timeconstants.h"
#include "enginebase.h"
#include "gstengine.h"
#include "gstenginepipeline.h"
#include "gstbufferconsumer.h"
#include "enginemetadata.h"

using std::make_shared;

const char *GstEngine::kAutoSink = "autoaudiosink";
const char *GstEngine::kALSASink = "alsasink";

namespace {
constexpr char kOpenALSASink[] = "openalsink";
constexpr char kOSSSink[] = "osssink";
constexpr char kOSS4Sink[] = "oss4sink";
constexpr char kJackAudioSink[] = "jackaudiosink";
constexpr char kPulseSink[] = "pulsesink";
constexpr char kA2DPSink[] = "a2dpsink";
constexpr char kAVDTPSink[] = "avdtpsink";
constexpr char InterAudiosink[] = "interaudiosink";
constexpr char kDirectSoundSink[] = "directsoundsink";
constexpr char kOSXAudioSink[] = "osxaudiosink";
constexpr char kWASAPISink[] = "wasapisink";
constexpr int kDiscoveryTimeoutS = 10;
constexpr qint64 kTimerIntervalNanosec = 1000 * kNsecPerMsec;  // 1s
constexpr qint64 kPreloadGapNanosec = 8000 * kNsecPerMsec;     // 8s
constexpr qint64 kSeekDelayNanosec = 100 * kNsecPerMsec;       // 100msec
}  // namespace

GstEngine::GstEngine(SharedPtr<TaskManager> task_manager, QObject *parent)
    : EngineBase(parent),
      task_manager_(task_manager),
      gst_startup_(nullptr),
      discoverer_(nullptr),
      buffering_task_id_(-1),
      latest_buffer_(nullptr),
      stereo_balancer_enabled_(false),
      stereo_balance_(0.0F),
      equalizer_enabled_(false),
      equalizer_preamp_(0),
      seek_timer_(new QTimer(this)),
      waiting_to_seek_(false),
      seek_pos_(0),
      timer_id_(-1),
      has_faded_out_to_pause_(false),
      scope_chunk_(0),
      have_new_buffer_(false),
      scope_chunks_(0),
      discovery_finished_cb_id_(-1),
      discovery_discovered_cb_id_(-1),
      delayed_state_(State::Empty),
      delayed_state_pause_(false),
      delayed_state_offset_nanosec_(0) {

  seek_timer_->setSingleShot(true);
  seek_timer_->setInterval(kSeekDelayNanosec / kNsecPerMsec);
  QObject::connect(seek_timer_, &QTimer::timeout, this, &GstEngine::SeekNow);

  GstEngine::ReloadSettings();

}

GstEngine::~GstEngine() {

  EnsureInitialized();
  current_pipeline_.reset();

  if (latest_buffer_) {
    gst_buffer_unref(latest_buffer_);
    latest_buffer_ = nullptr;
  }

  if (discoverer_) {

    if (discovery_discovered_cb_id_ != -1) {
      g_signal_handler_disconnect(G_OBJECT(discoverer_), discovery_discovered_cb_id_);
    }
    if (discovery_finished_cb_id_ != -1) {
      g_signal_handler_disconnect(G_OBJECT(discoverer_), discovery_finished_cb_id_);
    }

    gst_discoverer_stop(discoverer_);
    g_object_unref(discoverer_);
    discoverer_ = nullptr;

  }

}

bool GstEngine::Init() {

  return true;

}

EngineBase::State GstEngine::state() const {

  if (!current_pipeline_) return stream_url_.isEmpty() ? State::Empty : State::Idle;

  switch (current_pipeline_->state()) {
    case GST_STATE_NULL:
      return State::Empty;
    case GST_STATE_READY:
      return State::Idle;
    case GST_STATE_PLAYING:
      return State::Playing;
    case GST_STATE_PAUSED:
      return State::Paused;
    default:
      return State::Empty;
  }

}

void GstEngine::StartPreloading(const QUrl &media_url, const QUrl &stream_url, const bool force_stop_at_end, const qint64 beginning_nanosec, const qint64 end_nanosec) {

  EnsureInitialized();

  const QByteArray gst_url = FixupUrl(stream_url);

  // No crossfading, so we can just queue the new URL in the existing pipeline and get gapless playback (hopefully)
  if (current_pipeline_) {
    current_pipeline_->PrepareNextUrl(media_url, stream_url, gst_url, beginning_nanosec, force_stop_at_end ? end_nanosec : 0);
    // Add request to discover the stream
    if (discoverer_ && media_url.scheme() != QStringLiteral("spotify")) {
      if (!gst_discoverer_discover_uri_async(discoverer_, gst_url.constData())) {
        qLog(Error) << "Failed to start stream discovery for" << gst_url;
      }
    }
  }

}

bool GstEngine::Load(const QUrl &media_url, const QUrl &stream_url, const EngineBase::TrackChangeFlags change, const bool force_stop_at_end, const quint64 beginning_nanosec, const qint64 end_nanosec, const std::optional<double> ebur128_integrated_loudness_lufs) {

  EnsureInitialized();

  EngineBase::Load(media_url, stream_url, change, force_stop_at_end, beginning_nanosec, end_nanosec, ebur128_integrated_loudness_lufs);

  const QByteArray gst_url = FixupUrl(stream_url);

  bool crossfade = current_pipeline_ && ((crossfade_enabled_ && change & EngineBase::TrackChangeType::Manual) || (autocrossfade_enabled_ && change & EngineBase::TrackChangeType::Auto) || ((crossfade_enabled_ || autocrossfade_enabled_) && change & EngineBase::TrackChangeType::Intro));

  if (change & EngineBase::TrackChangeType::Auto && change & EngineBase::TrackChangeType::SameAlbum && !crossfade_same_album_) {
    crossfade = false;
  }

  if (!crossfade && current_pipeline_ && current_pipeline_->stream_url() == stream_url && change & EngineBase::TrackChangeType::Auto) {
    // We're not crossfading, and the pipeline is already playing the URI we want, so just do nothing.
    current_pipeline_->SetEBUR128LoudnessNormalizingGain_dB(ebur128_loudness_normalizing_gain_db_);
    return true;
  }

  GstEnginePipelinePtr pipeline = CreatePipeline(media_url, stream_url, gst_url, force_stop_at_end ? end_nanosec : 0, ebur128_loudness_normalizing_gain_db_);
  if (!pipeline) return false;

  if (crossfade && current_pipeline_ && !ExclusivePipelineActive() && !fadeout_pipelines_.contains(current_pipeline_->id())) {
    StartFadeout();
  }

  BufferingFinished();

  GstEnginePipelinePtr old_pipeline = current_pipeline_;
  current_pipeline_ = pipeline;
  if (old_pipeline && !fadeout_pipelines_.contains(old_pipeline->id())) {
    FinishPipeline(old_pipeline);
  }

  SetVolume(volume_);
  SetStereoBalance(stereo_balance_);
  SetEqualizerParameters(equalizer_preamp_, equalizer_gains_);

  // Maybe fade in this track
  if (crossfade && !ExclusivePipelineActive()) {
    current_pipeline_->StartFader(fadeout_duration_nanosec_, QTimeLine::Forward);
  }

  // Setting up stream discoverer
  if (!discoverer_) {
    discoverer_ = gst_discoverer_new(kDiscoveryTimeoutS * GST_SECOND, nullptr);
    if (discoverer_) {
      discovery_discovered_cb_id_ = CHECKED_GCONNECT(G_OBJECT(discoverer_), "discovered", &StreamDiscovered, this);
      discovery_finished_cb_id_ = CHECKED_GCONNECT(G_OBJECT(discoverer_), "finished", &StreamDiscoveryFinished, this);
      gst_discoverer_start(discoverer_);
    }
  }

  // Add request to discover the stream
  if (discoverer_ && media_url.scheme() != QStringLiteral("spotify")) {
    if (!gst_discoverer_discover_uri_async(discoverer_, gst_url.constData())) {
      qLog(Error) << "Failed to start stream discovery for" << gst_url;
    }
  }

  return true;

}

bool GstEngine::Play(const bool pause, const quint64 offset_nanosec) {

  EnsureInitialized();

  if (!current_pipeline_ || current_pipeline_->is_buffering()) return false;

  if (ExclusivePipelineActive()) {
    qLog(Debug) << "Delaying play because a exclusive pipeline is already active...";
    delayed_state_ = pause ? State::Paused : State::Playing;
    delayed_state_pause_ = pause;
    delayed_state_offset_nanosec_ = offset_nanosec;
    return true;
  }

  if (fadeout_pause_pipeline_) {
    StopFadeoutPause();
  }

  delayed_state_ = State::Empty;
  delayed_state_pause_ = false;
  delayed_state_offset_nanosec_ = 0;

  QFutureWatcher<GstStateChangeReturn> *watcher = new QFutureWatcher<GstStateChangeReturn>();
  const int pipeline_id = current_pipeline_->id();
  QObject::connect(watcher, &QFutureWatcher<GstStateChangeReturn>::finished, this, [this, watcher, pipeline_id, pause, offset_nanosec]() {
    const GstStateChangeReturn ret = watcher->result();
    watcher->deleteLater();
    PlayDone(ret, pause, offset_nanosec, pipeline_id);
  });
  QFuture<GstStateChangeReturn> future = current_pipeline_->Play(pause, offset_nanosec);
  watcher->setFuture(future);

  return true;

}

void GstEngine::Stop(const bool stop_after) {

  StopTimers();

  delayed_state_ = State::Empty;
  delayed_state_pause_ = false;
  delayed_state_offset_nanosec_ = 0;

  media_url_.clear();
  stream_url_.clear();  // To ensure we return Empty from state()
  beginning_nanosec_ = end_nanosec_ = 0;

  // Check if we started a fade out. If it isn't finished yet and the user pressed stop, we cancel the fader and just stop the playback.
  if (fadeout_pause_pipeline_) {
    StopFadeoutPause();
  }

  if (current_pipeline_) {
    if (fadeout_enabled_ && !stop_after && !ExclusivePipelineActive()) {
      StartFadeout();
      current_pipeline_ = GstEnginePipelinePtr();
    }
    else {
      GstEnginePipelinePtr old_pipeline = current_pipeline_;
      current_pipeline_ = GstEnginePipelinePtr();
      FinishPipeline(old_pipeline);
    }
  }

  BufferingFinished();

  emit StateChanged(State::Empty);

}

void GstEngine::Pause() {

  if (!current_pipeline_ || current_pipeline_->is_buffering()) return;

  delayed_state_ = State::Empty;
  delayed_state_pause_ = false;
  delayed_state_offset_nanosec_ = 0;

  if (fadeout_pause_pipeline_) {
    return;
  }

  if (current_pipeline_->state() == GST_STATE_PLAYING) {
    if (fadeout_pause_enabled_ && !ExclusivePipelineActive()) {
      StartFadeoutPause();
    }
    else {
      current_pipeline_->SetStateAsync(GST_STATE_PAUSED);
      emit StateChanged(State::Paused);
      StopTimers();
    }
  }

}

void GstEngine::Unpause() {

  if (!current_pipeline_ || current_pipeline_->is_buffering()) return;

  if (current_pipeline_->state() == GST_STATE_PAUSED) {

    // Check if we faded out last time. If yes, fade in no matter what the settings say.
    // If we pause with fadeout, deactivate fadeout and resume playback, the player would be muted if not faded in.
    if (has_faded_out_to_pause_ && !ExclusivePipelineActive()) {
      QObject::disconnect(&*current_pipeline_, &GstEnginePipeline::FaderFinished, nullptr, nullptr);
      current_pipeline_->StartFader(fadeout_pause_duration_nanosec_, QTimeLine::Forward, QEasingCurve::Linear, false);
      has_faded_out_to_pause_ = false;
    }

    current_pipeline_->SetStateAsync(GST_STATE_PLAYING);

    emit StateChanged(State::Playing);

    StartTimers();
  }

}

void GstEngine::Seek(const quint64 offset_nanosec) {

  if (!current_pipeline_) return;

  seek_pos_ = beginning_nanosec_ + offset_nanosec;
  waiting_to_seek_ = true;

  if (!seek_timer_->isActive()) {
    SeekNow();
    seek_timer_->start();  // Stop us from seeking again for a little while
  }

}

void GstEngine::SetVolumeSW(const uint volume) {
  if (current_pipeline_) current_pipeline_->SetVolume(volume);
}

qint64 GstEngine::position_nanosec() const {

  if (!current_pipeline_) return 0;

  const qint64 result = current_pipeline_->position() - static_cast<qint64>(beginning_nanosec_);
  return std::max(0LL, result);

}

qint64 GstEngine::length_nanosec() const {

  if (!current_pipeline_) return 0;

  const qint64 result = end_nanosec_ - static_cast<qint64>(beginning_nanosec_);

  if (result > 0) {
    return result;
  }
  else {
    // Get the length from the pipeline if we don't know.
    return current_pipeline_->length();
  }

}

const EngineBase::Scope &GstEngine::scope(const int chunk_length) {

  // The new buffer could have a different size
  if (have_new_buffer_) {
    if (latest_buffer_) {
      scope_chunks_ = ceil((static_cast<double>(GST_BUFFER_DURATION(latest_buffer_) / static_cast<double>(chunk_length * kNsecPerMsec))));
    }

    // if the buffer is shorter than the chunk length
    if (scope_chunks_ <= 0) {
      scope_chunks_ = 1;
    }

    scope_chunk_ = 0;
    have_new_buffer_ = false;
  }

  if (latest_buffer_) {
    UpdateScope(chunk_length);
  }

  return scope_;

}

EngineBase::OutputDetailsList GstEngine::GetOutputsList() const {

  const_cast<GstEngine*>(this)->EnsureInitialized();

  OutputDetailsList outputs;

  GstRegistry *registry = gst_registry_get();
  GList *const features = gst_registry_get_feature_list(registry, GST_TYPE_ELEMENT_FACTORY);
  for (GList *future = features; future; future = g_list_next(future)) {
    GstElementFactory *factory = GST_ELEMENT_FACTORY(future->data);
    const QString metadata = QString::fromUtf8(gst_element_factory_get_metadata(factory, GST_ELEMENT_METADATA_KLASS));
    const QString name = QString::fromUtf8(gst_plugin_feature_get_name(future->data));
    const QStringList classes = metadata.split(QLatin1Char('/'));
    if (classes.contains(QLatin1String("Audio"), Qt::CaseInsensitive) && (classes.contains(QLatin1String("Sink"), Qt::CaseInsensitive) || (classes.contains(QLatin1String("Source"), Qt::CaseInsensitive) && name.contains(QLatin1String("sink"))))) {
      QString description = QString::fromUtf8(gst_element_factory_get_metadata(factory, GST_ELEMENT_METADATA_DESCRIPTION));
      if (name == QLatin1String("wasapi2sink") && description == QLatin1String("Stream audio to an audio capture device through WASAPI")) {
        description.append(QLatin1Char('2'));
      }
      else if (name == QLatin1String("pipewiresink") && description == QLatin1String("Send video to PipeWire")) {
        description = QLatin1String("Send audio to PipeWire");
      }
      OutputDetails output;
      output.name = name;
      output.description = description;
      if (output.name == QLatin1String(kAutoSink)) output.iconname = QLatin1String("soundcard");
      else if (output.name == QLatin1String(kALSASink) || output.name == QLatin1String(kOSS4Sink)) output.iconname = QLatin1String("alsa");
      else if (output.name == QLatin1String(kJackAudioSink)) output.iconname = QLatin1String("jack");
      else if (output.name == QLatin1String(kPulseSink)) output.iconname = QLatin1String("pulseaudio");
      else if (output.name == QLatin1String(kA2DPSink) || output.name == QLatin1String(kAVDTPSink)) output.iconname = QLatin1String("bluetooth");
      else output.iconname = QLatin1String("soundcard");
      outputs << output;
    }
  }

  gst_plugin_feature_list_free(features);

  return outputs;

}

bool GstEngine::ValidOutput(const QString &output) {

  EnsureInitialized();

  const OutputDetailsList output_details = GetOutputsList();
  return std::any_of(output_details.begin(), output_details.end(), [output](const OutputDetails &output_detail) { return output_detail.name == output; });

}

bool GstEngine::CustomDeviceSupport(const QString &output) const {
  return output == QLatin1String(kALSASink) || output == QLatin1String(kOpenALSASink) || output == QLatin1String(kOSSSink) || output == QLatin1String(kOSS4Sink) || output == QLatin1String(kPulseSink) || output == QLatin1String(kA2DPSink) || output == QLatin1String(kAVDTPSink) || output == QLatin1String(kJackAudioSink);
}

bool GstEngine::ALSADeviceSupport(const QString &output) const {
  return output == QLatin1String(kALSASink);
}

bool GstEngine::ExclusiveModeSupport(const QString &output) const {
  return output == QLatin1String(kWASAPISink);
}

void GstEngine::ReloadSettings() {

  EngineBase::ReloadSettings();

  if (output_.isEmpty()) output_ = QLatin1String(kAutoSink);

}

void GstEngine::ConsumeBuffer(GstBuffer *buffer, const int pipeline_id, const QString &format) {

  // Schedule this to run in the GUI thread.  The buffer gets added to the queue and unreffed by UpdateScope.
  if (!QMetaObject::invokeMethod(this, "AddBufferToScope", Q_ARG(GstBuffer*, buffer), Q_ARG(int, pipeline_id), Q_ARG(QString, format))) {
    qLog(Warning) << "Failed to invoke AddBufferToScope on GstEngine";
    gst_buffer_unref(buffer);
  }

}

void GstEngine::SetStereoBalancerEnabled(const bool enabled) {

  stereo_balancer_enabled_ = enabled;
  if (current_pipeline_) current_pipeline_->set_stereo_balancer_enabled(enabled);

}

void GstEngine::SetStereoBalance(const float value) {

  stereo_balance_ = value;
  if (current_pipeline_) current_pipeline_->SetStereoBalance(value);

}

void GstEngine::SetEqualizerEnabled(const bool enabled) {

  equalizer_enabled_ = enabled;
  if (current_pipeline_) current_pipeline_->set_equalizer_enabled(enabled);

}

void GstEngine::SetEqualizerParameters(const int preamp, const QList<int> &band_gains) {

  equalizer_preamp_ = preamp;
  equalizer_gains_ = band_gains;

  if (current_pipeline_) current_pipeline_->SetEqualizerParams(preamp, band_gains);

}

void GstEngine::AddBufferConsumer(GstBufferConsumer *consumer) {

  buffer_consumers_ << consumer;
  if (current_pipeline_) current_pipeline_->AddBufferConsumer(consumer);

}

void GstEngine::RemoveBufferConsumer(GstBufferConsumer *consumer) {

  buffer_consumers_.removeAll(consumer);
  if (current_pipeline_) current_pipeline_->RemoveBufferConsumer(consumer);

}

void GstEngine::timerEvent(QTimerEvent *e) {

  if (e->timerId() != timer_id_) return;

  if (current_pipeline_ && !about_to_end_emitted_) {
    const qint64 current_length = length_nanosec();
    // Only if we know the length of the current stream...
    if (current_length > 0) {
      const qint64 current_position = position_nanosec();
      const qint64 remaining = current_length - current_position;
      const qint64 fudge = kTimerIntervalNanosec + 100 * kNsecPerMsec;  // Mmm fudge
      const qint64 gap = static_cast<qint64>(buffer_duration_nanosec_) + (autocrossfade_enabled_ ? fadeout_duration_nanosec_ : kPreloadGapNanosec);
      // Emit TrackAboutToEnd when we're a few seconds away from finishing
      if (remaining < gap + fudge) {
        qLog(Debug) << "Stream from URL" << media_url_.toString() << "about to end in" << remaining / kNsecPerSec << "seconds. Fuge:" << fudge / kNsecPerMsec << "+" << "Gap:" << gap / kNsecPerMsec;
        EmitAboutToFinish();
      }
    }
  }

}

void GstEngine::EndOfStreamReached(const int pipeline_id, const bool has_next_track) {

  if (!current_pipeline_ || current_pipeline_->id() != pipeline_id) {
    return;
  }

  if (!has_next_track) {
    current_pipeline_.reset();
    BufferingFinished();
  }

  emit TrackEnded();

}

void GstEngine::HandlePipelineError(const int pipeline_id, const int domain, const int error_code, const QString &message, const QString &debugstr) {

  if (!current_pipeline_ || current_pipeline_->id() != pipeline_id) return;

  qLog(Error) << "GStreamer error:" << domain << error_code << message;

  current_pipeline_.reset();
  BufferingFinished();
  emit StateChanged(State::Error);

  if (
      (domain == static_cast<int>(GST_RESOURCE_ERROR) && (
        error_code == static_cast<int>(GST_RESOURCE_ERROR_NOT_FOUND) ||
        error_code == static_cast<int>(GST_RESOURCE_ERROR_OPEN_READ) ||
        error_code == static_cast<int>(GST_RESOURCE_ERROR_NOT_AUTHORIZED)
      ))
      || (domain == static_cast<int>(GST_STREAM_ERROR))
      ) {
     emit InvalidSongRequested(stream_url_);
   }
  else {
    emit FatalError();
  }

  emit Error(message);
  emit Error(debugstr);

}

void GstEngine::NewMetaData(const int pipeline_id, const EngineMetadata &engine_metadata) {

  if (!current_pipeline_|| current_pipeline_->id() != pipeline_id) return;
  emit MetaData(engine_metadata);

}

void GstEngine::AddBufferToScope(GstBuffer *buf, const int pipeline_id, const QString &format) {

  if (!current_pipeline_ || current_pipeline_->id() != pipeline_id) {
    gst_buffer_unref(buf);
    return;
  }

  if (latest_buffer_) {
    gst_buffer_unref(latest_buffer_);
  }

  buffer_format_ = format;
  latest_buffer_ = buf;
  have_new_buffer_ = true;

}

void GstEngine::FadeoutFinished(const int pipeline_id) {

  if (!fadeout_pipelines_.contains(pipeline_id)) {
    return;
  }

  GstEnginePipelinePtr pipeline = fadeout_pipelines_.value(pipeline_id);
  fadeout_pipelines_.remove(pipeline_id);
  FinishPipeline(pipeline);

  emit FadeoutFinishedSignal();

}

void GstEngine::FadeoutPauseFinished() {

  fadeout_pause_pipeline_->SetStateAsync(GST_STATE_PAUSED);
  emit StateChanged(State::Paused);
  StopTimers();
  has_faded_out_to_pause_ = true;
  fadeout_pause_pipeline_ = GstEnginePipelinePtr();
  emit FadeoutFinishedSignal();

}

void GstEngine::SeekNow() {

  if (!waiting_to_seek_) return;
  waiting_to_seek_ = false;

  if (!current_pipeline_) return;

  if (!current_pipeline_->Seek(static_cast<qint64>(seek_pos_))) {
    qLog(Warning) << "Seek failed";
  }

}

void GstEngine::PlayDone(const GstStateChangeReturn ret, const bool pause, const quint64 offset_nanosec, const int pipeline_id) {

  if (!current_pipeline_ || pipeline_id != current_pipeline_->id()) {
    return;
  }

  if (ret == GST_STATE_CHANGE_FAILURE) {
    // Failure, but we got a redirection URL - try loading that instead
    const QByteArray redirect_url = current_pipeline_->redirect_url();
    if (!redirect_url.isEmpty() && redirect_url != current_pipeline_->gst_url()) {
      qLog(Info) << "Redirecting to" << redirect_url;
      GstEnginePipelinePtr old_pipeline = current_pipeline_;
      current_pipeline_ = GstEnginePipelinePtr();
      if (old_pipeline) {
        FinishPipeline(old_pipeline);
      }
      current_pipeline_ = CreatePipeline(current_pipeline_->media_url(), current_pipeline_->stream_url(), redirect_url, end_nanosec_, current_pipeline_->ebur128_loudness_normalizing_gain_db());
      Play(pause, offset_nanosec);
      return;
    }

    // Failure - give up
    qLog(Warning) << "Could not set thread to PLAYING.";
    GstEnginePipelinePtr old_pipeline = current_pipeline_;
    current_pipeline_ = GstEnginePipelinePtr();
    if (old_pipeline) {
      FinishPipeline(old_pipeline);
    }
    BufferingFinished();
    return;
  }

  if (!pause) {
    StartTimers();
  }

  emit StateChanged(pause ? State::Paused : State::Playing);

  // We've successfully started playing a media stream with this url
  emit ValidSongRequested(stream_url_);

}

void GstEngine::BufferingStarted() {

  if (buffering_task_id_ != -1) {
    task_manager_->SetTaskFinished(buffering_task_id_);
  }

  buffering_task_id_ = task_manager_->StartTask(tr("Buffering"));
  task_manager_->SetTaskProgress(buffering_task_id_, 0, 100);

}

void GstEngine::BufferingProgress(const int percent) {
  task_manager_->SetTaskProgress(buffering_task_id_, percent, 100);
}

void GstEngine::BufferingFinished() {

  if (buffering_task_id_ != -1) {
    task_manager_->SetTaskFinished(buffering_task_id_);
    buffering_task_id_ = -1;
  }

}

QByteArray GstEngine::FixupUrl(const QUrl &url) {

  EnsureInitialized();

  QByteArray uri;

  // It's a file:// url with a hostname set.
  // QUrl::fromLocalFile does this when given a \\host\share\file path on Windows.
  // Munge it back into a path that gstreamer will recognise.
  if (url.isLocalFile() && !url.host().isEmpty()) {
    QString str = QLatin1String("file:////") + url.host() + url.path();
    uri = str.toUtf8();
  }
  else if (url.scheme() == QLatin1String("cdda")) {
    QString str;
    if (url.path().isEmpty()) {
      str = url.toString();
      str.remove(str.lastIndexOf(QLatin1Char('a')), 1);
    }
    else {
      // Currently, Gstreamer can't handle input CD devices inside cdda URL.
      // So we handle them ourselves: we extract the track number and re-create a URL with only cdda:// + the track number (which can be handled by Gstreamer).
      // We keep the device in mind, and we will set it later using SourceSetupCallback
      QStringList path = url.path().split(QLatin1Char('/'));
      str = QStringLiteral("cdda://%1").arg(path.takeLast());
      QString device = path.join(QLatin1Char('/'));
      if (current_pipeline_) current_pipeline_->SetSourceDevice(device);
    }
    uri = str.toUtf8();
  }
  else {
    uri = url.toEncoded();
  }

  return uri;

}

void GstEngine::StartFadeout() {

  GstEnginePipelinePtr pipeline = current_pipeline_;

  if (fadeout_pipelines_.contains(pipeline->id())) {
    return;
  }

  fadeout_pipelines_.insert(pipeline->id(), pipeline);
  pipeline->RemoveAllBufferConsumers();

  pipeline->StartFader(fadeout_duration_nanosec_, QTimeLine::Backward);
  QObject::connect(&*pipeline, &GstEnginePipeline::FaderFinished, this, &GstEngine::FadeoutFinished);

}

void GstEngine::StartFadeoutPause() {

  if (!current_pipeline_ || fadeout_pipelines_.contains(current_pipeline_->id())) return;

  fadeout_pause_pipeline_ = current_pipeline_;
  QObject::connect(&*fadeout_pause_pipeline_, &GstEnginePipeline::FaderFinished, this, &GstEngine::FadeoutPauseFinished);
  fadeout_pause_pipeline_->StartFader(fadeout_pause_duration_nanosec_, QTimeLine::Backward, QEasingCurve::Linear, false);

}

void GstEngine::StopFadeoutPause() {

  if (!fadeout_pause_pipeline_) return;

  QObject::disconnect(&*fadeout_pause_pipeline_, &GstEnginePipeline::FaderFinished, this, &GstEngine::FadeoutPauseFinished);
  has_faded_out_to_pause_ = true;
  fadeout_pause_pipeline_ = GstEnginePipelinePtr();

}

void GstEngine::StartTimers() {

  StopTimers();
  timer_id_ = startTimer(kTimerIntervalNanosec / kNsecPerMsec);

}

void GstEngine::StopTimers() {

  if (timer_id_ != -1) {
    killTimer(timer_id_);
    timer_id_ = -1;
  }

}

GstEnginePipelinePtr GstEngine::CreatePipeline() {

  EnsureInitialized();

  GstEnginePipelinePtr ret = make_shared<GstEnginePipeline>();
  ret->set_output_device(output_, device_);
  ret->set_exclusive_mode(exclusive_mode_);
  ret->set_volume_enabled(volume_control_);
  ret->set_stereo_balancer_enabled(stereo_balancer_enabled_);
  ret->set_equalizer_enabled(equalizer_enabled_);
  ret->set_replaygain(rg_enabled_, rg_mode_, rg_preamp_, rg_fallbackgain_, rg_compression_);
  ret->set_ebur128_loudness_normalization(ebur128_loudness_normalization_);
  ret->set_buffer_duration_nanosec(buffer_duration_nanosec_);
  ret->set_buffer_low_watermark(buffer_low_watermark_);
  ret->set_buffer_high_watermark(buffer_high_watermark_);
  ret->set_proxy_settings(proxy_address_, proxy_authentication_, proxy_user_, proxy_pass_);
  ret->set_channels(channels_enabled_, channels_);
  ret->set_bs2b_enabled(bs2b_enabled_);
  ret->set_strict_ssl_enabled(strict_ssl_enabled_);
  ret->set_fading_enabled(fadeout_enabled_ || autocrossfade_enabled_ || fadeout_pause_enabled_);

#ifdef HAVE_SPOTIFY
  ret->set_spotify_login(spotify_username_, spotify_password_);
#endif

  ret->AddBufferConsumer(this);
  for (GstBufferConsumer *consumer : std::as_const(buffer_consumers_)) {
    ret->AddBufferConsumer(consumer);
  }

  QObject::connect(&*ret, &GstEnginePipeline::EndOfStreamReached, this, &GstEngine::EndOfStreamReached);
  QObject::connect(&*ret, &GstEnginePipeline::Error, this, &GstEngine::HandlePipelineError);
  QObject::connect(&*ret, &GstEnginePipeline::MetadataFound, this, &GstEngine::NewMetaData);
  QObject::connect(&*ret, &GstEnginePipeline::BufferingStarted, this, &GstEngine::BufferingStarted);
  QObject::connect(&*ret, &GstEnginePipeline::BufferingProgress, this, &GstEngine::BufferingProgress);
  QObject::connect(&*ret, &GstEnginePipeline::BufferingFinished, this, &GstEngine::BufferingFinished);
  QObject::connect(&*ret, &GstEnginePipeline::VolumeChanged, this, &EngineBase::UpdateVolume);
  QObject::connect(&*ret, &GstEnginePipeline::AboutToFinish, this, &EngineBase::EmitAboutToFinish);

  return ret;

}

GstEnginePipelinePtr GstEngine::CreatePipeline(const QUrl &media_url, const QUrl &stream_url, const QByteArray &gst_url, const qint64 end_nanosec, const double ebur128_loudness_normalizing_gain_db) {

  GstEnginePipelinePtr ret = CreatePipeline();
  QString error;
  if (!ret->InitFromUrl(media_url, stream_url, gst_url, end_nanosec, ebur128_loudness_normalizing_gain_db, error)) {
    ret.reset();
    emit Error(error);
    emit StateChanged(State::Error);
    emit FatalError();
  }

  return ret;

}

void GstEngine::FinishPipeline(GstEnginePipelinePtr pipeline) {

  const int pipeline_id = pipeline->id();

  if (!pipeline->Finish() && !old_pipelines_.contains(pipeline->id())) {
    old_pipelines_.insert(pipeline_id, pipeline);
    QObject::connect(&*pipeline, &GstEnginePipeline::Finished, this, [this, pipeline_id]() {
      PipelineFinished(pipeline_id);
    });
  }

}

void GstEngine::PipelineFinished(const int pipeline_id) {

  qLog(Debug) << "Pipeline" << pipeline_id << "finished";

  GstEnginePipelinePtr pipeline = old_pipelines_[pipeline_id];
  old_pipelines_.remove(pipeline_id);
  if (pipeline == fadeout_pause_pipeline_) {
    StopFadeoutPause();
  }
  pipeline = GstEnginePipelinePtr();

  if (current_pipeline_ && old_pipelines_.isEmpty() && delayed_state_ != State::Empty) {
    switch (delayed_state_) {
      case State::Playing:
        Play(delayed_state_pause_, delayed_state_offset_nanosec_);
        break;
      case State::Paused:
        Pause();
        break;
      default:
        break;
    }
    delayed_state_ = State::Empty;
    delayed_state_pause_ = false;
    delayed_state_offset_nanosec_ = 0;
  }

  qLog(Debug) << (current_pipeline_ ? 1 : 0) + old_pipelines_.count() << "pipelines are active";

}

void GstEngine::UpdateScope(const int chunk_length) {

  using sample_type = EngineBase::Scope::value_type;

  // Prevent dbz or invalid chunk size
  if (!GST_CLOCK_TIME_IS_VALID(GST_BUFFER_DURATION(latest_buffer_))) return;
  if (GST_BUFFER_DURATION(latest_buffer_) == 0) return;

  GstMapInfo map;
  gst_buffer_map(latest_buffer_, &map, GST_MAP_READ);

  // Determine where to split the buffer
  int chunk_density = static_cast<int>((map.size * kNsecPerMsec) / GST_BUFFER_DURATION(latest_buffer_));

  int chunk_size = chunk_length * chunk_density;

  // In case a buffer doesn't arrive in time
  if (scope_chunk_ >= scope_chunks_) {
    scope_chunk_ = 0;
    gst_buffer_unmap(latest_buffer_, &map);
    return;
  }

  const sample_type *source = reinterpret_cast<sample_type*>(map.data);
  sample_type *dest = scope_.data();
  source += (chunk_size / sizeof(sample_type)) * scope_chunk_;

  size_t bytes = 0;

  // Make sure we don't go beyond the end of the buffer
  if (scope_chunk_ == scope_chunks_ - 1) {
    bytes = qMin(static_cast<EngineBase::Scope::size_type>(map.size - (chunk_size * scope_chunk_)), scope_.size() * sizeof(sample_type));
  }
  else {
    bytes = qMin(static_cast<EngineBase::Scope::size_type>(chunk_size), scope_.size() * sizeof(sample_type));
  }

  scope_chunk_++;

  if (buffer_format_.startsWith(QLatin1String("S16LE")) ||
      buffer_format_.startsWith(QLatin1String("U16LE")) ||
      buffer_format_.startsWith(QLatin1String("S24LE")) ||
      buffer_format_.startsWith(QLatin1String("S24_32LE")) ||
      buffer_format_.startsWith(QLatin1String("S32LE")) ||
      buffer_format_.startsWith(QLatin1String("F32LE"))
  ) {
    memcpy(dest, source, bytes);
  }
  else {
    memset(dest, 0, bytes);
  }

  gst_buffer_unmap(latest_buffer_, &map);

  if (scope_chunk_ == scope_chunks_) {
    gst_buffer_unref(latest_buffer_);
    latest_buffer_ = nullptr;
    buffer_format_.clear();
  }

}

void GstEngine::StreamDiscovered(GstDiscoverer*, GstDiscovererInfo *info, GError*, gpointer self) {

  GstEngine *instance = reinterpret_cast<GstEngine*>(self);
  if (!instance->current_pipeline_) return;

  const QByteArray discovered_url = gst_discoverer_info_get_uri(info);

  GstDiscovererResult result = gst_discoverer_info_get_result(info);
  if (result != GST_DISCOVERER_OK) {
    const QString error_message = GSTdiscovererErrorMessage(result);
    qLog(Error) << QStringLiteral("Stream discovery for %1 failed: %2").arg(QString::fromUtf8(discovered_url), error_message);
    return;
  }

  GList *audio_streams = gst_discoverer_info_get_audio_streams(info);
  if (audio_streams) {

    GstDiscovererStreamInfo *stream_info = reinterpret_cast<GstDiscovererStreamInfo*>(g_list_first(audio_streams)->data);

    EngineMetadata engine_metadata;
    if (discovered_url == instance->current_pipeline_->gst_url()) {
      engine_metadata.type = EngineMetadata::Type::Current;
      engine_metadata.media_url = instance->current_pipeline_->media_url();
      engine_metadata.stream_url = instance->current_pipeline_->stream_url();
    }
    else if (discovered_url == instance->current_pipeline_->next_gst_url()) {
      engine_metadata.type = EngineMetadata::Type::Next;
      engine_metadata.media_url = instance->current_pipeline_->next_media_url();
      engine_metadata.stream_url = instance->current_pipeline_->next_stream_url();
    }
    engine_metadata.samplerate = static_cast<int>(gst_discoverer_audio_info_get_sample_rate(GST_DISCOVERER_AUDIO_INFO(stream_info)));
    engine_metadata.bitdepth = static_cast<int>(gst_discoverer_audio_info_get_depth(GST_DISCOVERER_AUDIO_INFO(stream_info)));
    engine_metadata.bitrate = static_cast<int>(gst_discoverer_audio_info_get_bitrate(GST_DISCOVERER_AUDIO_INFO(stream_info)) / 1000);

    GstCaps *caps = gst_discoverer_stream_info_get_caps(stream_info);

    const guint caps_size = gst_caps_get_size(caps);
    for (guint i = 0; i < caps_size; ++i) {
      GstStructure *gst_structure = gst_caps_get_structure(caps, i);
      if (!gst_structure) continue;
      QString mimetype = QString::fromUtf8(gst_structure_get_name(gst_structure));
      if (!mimetype.isEmpty() && mimetype != QLatin1String("audio/mpeg")) {
        engine_metadata.filetype = Song::FiletypeByMimetype(mimetype);
        if (engine_metadata.filetype == Song::FileType::Unknown) {
          qLog(Error) << "Unknown mimetype" << mimetype;
        }
      }
    }

    if (engine_metadata.filetype == Song::FileType::Unknown) {
      gchar *codec_description = gst_pb_utils_get_codec_description(caps);
      QString filetype_description = (codec_description ? QString::fromUtf8(codec_description) : QString());
      g_free(codec_description);
      if (!filetype_description.isEmpty()) {
        engine_metadata.filetype = Song::FiletypeByDescription(filetype_description);
        if (engine_metadata.filetype == Song::FileType::Unknown) {
          qLog(Error) << "Unknown filetype" << filetype_description;
        }
      }
    }

    gst_caps_unref(caps);
    gst_discoverer_stream_info_list_free(audio_streams);

    qLog(Debug) << "Got stream info for" << discovered_url + ":" << Song::TextForFiletype(engine_metadata.filetype);

    emit instance->MetaData(engine_metadata);

  }
  else {
    qLog(Error) << "Could not detect an audio stream in" << discovered_url;
  }

}

void GstEngine::StreamDiscoveryFinished(GstDiscoverer*, gpointer) {}

QString GstEngine::GSTdiscovererErrorMessage(GstDiscovererResult result) {

  switch (result) {
    case GST_DISCOVERER_URI_INVALID:     return QStringLiteral("The URI is invalid");
    case GST_DISCOVERER_TIMEOUT:         return QStringLiteral("The discovery timed-out");
    case GST_DISCOVERER_BUSY:            return QStringLiteral("The discoverer was already discovering a file");
    case GST_DISCOVERER_MISSING_PLUGINS: return QStringLiteral("Some plugins are missing for full discovery");
    case GST_DISCOVERER_ERROR:
    default:                             return QStringLiteral("An error happened and the GError is set");
  }

}

bool GstEngine::ExclusivePipelineActive() const {

  if (old_pipelines_.isEmpty()) {
    return false;
  }

  if (current_pipeline_ && current_pipeline_->exclusive_mode()) {
    return true;
  }

  for (const GstEnginePipelinePtr &pipeline : std::as_const(old_pipelines_)) {
    if (pipeline->exclusive_mode()) {
      return true;
    }
  }

  return false;

}
