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

#include <glib.h>
#include <glib-object.h>
#include <memory>
#include <algorithm>
#include <vector>
#include <cmath>

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

#include "core/logging.h"
#include "core/taskmanager.h"
#include "core/signalchecker.h"
#include "utilities/timeconstants.h"
#include "enginebase.h"
#include "enginetype.h"
#include "gstengine.h"
#include "gstenginepipeline.h"
#include "gstbufferconsumer.h"

const char *GstEngine::kAutoSink = "autoaudiosink";
const char *GstEngine::kALSASink = "alsasink";
const char *GstEngine::kOpenALSASink = "openalsink";
const char *GstEngine::kOSSSink = "osssink";
const char *GstEngine::kOSS4Sink = "oss4sink";
const char *GstEngine::kJackAudioSink = "jackaudiosink";
const char *GstEngine::kPulseSink = "pulsesink";
const char *GstEngine::kA2DPSink = "a2dpsink";
const char *GstEngine::kAVDTPSink = "avdtpsink";
const char *GstEngine::InterAudiosink = "interaudiosink";
const char *GstEngine::kDirectSoundSink = "directsoundsink";
const char *GstEngine::kOSXAudioSink = "osxaudiosink";
const int GstEngine::kDiscoveryTimeoutS = 10;

GstEngine::GstEngine(TaskManager *task_manager, QObject *parent)
    : Engine::Base(Engine::EngineType::GStreamer, parent),
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
      is_fading_out_to_pause_(false),
      has_faded_out_(false),
      scope_chunk_(0),
      have_new_buffer_(false),
      scope_chunks_(0),
      discovery_finished_cb_id_(-1),
      discovery_discovered_cb_id_(-1) {

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

Engine::State GstEngine::state() const {

  if (!current_pipeline_) return stream_url_.isEmpty() ? Engine::State::Empty : Engine::State::Idle;

  switch (current_pipeline_->state()) {
    case GST_STATE_NULL:
      return Engine::State::Empty;
    case GST_STATE_READY:
      return Engine::State::Idle;
    case GST_STATE_PLAYING:
      return Engine::State::Playing;
    case GST_STATE_PAUSED:
      return Engine::State::Paused;
    default:
      return Engine::State::Empty;
  }

}

void GstEngine::StartPreloading(const QUrl &stream_url, const QUrl &original_url, const bool force_stop_at_end, const qint64 beginning_nanosec, const qint64 end_nanosec) {

  EnsureInitialized();

  QByteArray gst_url = FixupUrl(stream_url);

  // No crossfading, so we can just queue the new URL in the existing pipeline and get gapless playback (hopefully)
  if (current_pipeline_) {
    current_pipeline_->SetNextUrl(gst_url, original_url, beginning_nanosec, force_stop_at_end ? end_nanosec : 0);
    // Add request to discover the stream
    if (discoverer_) {
      if (!gst_discoverer_discover_uri_async(discoverer_, gst_url.constData())) {
        qLog(Error) << "Failed to start stream discovery for" << gst_url;
      }
    }
  }

}

bool GstEngine::Load(const QUrl &stream_url, const QUrl &original_url, Engine::TrackChangeFlags change, const bool force_stop_at_end, const quint64 beginning_nanosec, const qint64 end_nanosec) {

  EnsureInitialized();

  Engine::Base::Load(stream_url, original_url, change, force_stop_at_end, beginning_nanosec, end_nanosec);

  QByteArray gst_url = FixupUrl(stream_url);

  bool crossfade = current_pipeline_ && ((crossfade_enabled_ && change & Engine::TrackChangeType::Manual) || (autocrossfade_enabled_ && change & Engine::TrackChangeType::Auto) || ((crossfade_enabled_ || autocrossfade_enabled_) && change & Engine::TrackChangeType::Intro));

  if (change & Engine::TrackChangeType::Auto && change & Engine::TrackChangeType::SameAlbum && !crossfade_same_album_)
    crossfade = false;

  if (!crossfade && current_pipeline_ && current_pipeline_->stream_url() == gst_url && change & Engine::TrackChangeType::Auto) {
    // We're not crossfading, and the pipeline is already playing the URI we want, so just do nothing.
    return true;
  }

  std::shared_ptr<GstEnginePipeline> pipeline = CreatePipeline(gst_url, original_url, force_stop_at_end ? end_nanosec : 0);
  if (!pipeline) return false;

  if (crossfade) StartFadeout();

  BufferingFinished();
  current_pipeline_ = pipeline;

  SetVolume(volume_);
  SetStereoBalance(stereo_balance_);
  SetEqualizerParameters(equalizer_preamp_, equalizer_gains_);

  // Maybe fade in this track
  if (crossfade) {
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
  if (discoverer_) {
    if (!gst_discoverer_discover_uri_async(discoverer_, gst_url.constData())) {
      qLog(Error) << "Failed to start stream discovery for" << gst_url;
    }
  }

  return true;

}

bool GstEngine::Play(const quint64 offset_nanosec) {

  EnsureInitialized();

  if (!current_pipeline_ || current_pipeline_->is_buffering()) return false;

  QFutureWatcher<GstStateChangeReturn> *watcher = new QFutureWatcher<GstStateChangeReturn>();
  const int pipeline_id = current_pipeline_->id();
  QObject::connect(watcher, &QFutureWatcher<GstStateChangeReturn>::finished, this, [this, watcher, pipeline_id, offset_nanosec]() {
    const GstStateChangeReturn ret = watcher->result();
    watcher->deleteLater();
    PlayDone(ret, offset_nanosec, pipeline_id);
  });
  QFuture<GstStateChangeReturn> future = current_pipeline_->SetState(GST_STATE_PLAYING);
  watcher->setFuture(future);

  if (is_fading_out_to_pause_) {
    current_pipeline_->SetState(GST_STATE_PAUSED);
  }

  return true;

}

void GstEngine::Stop(const bool stop_after) {

  StopTimers();

  stream_url_.clear();  // To ensure we return Empty from state()
  original_url_.clear();
  beginning_nanosec_ = end_nanosec_ = 0;

  // Check if we started a fade out. If it isn't finished yet and the user pressed stop, we cancel the fader and just stop the playback.
  if (is_fading_out_to_pause_) {
    QObject::disconnect(current_pipeline_.get(), &GstEnginePipeline::FaderFinished, nullptr, nullptr);
    is_fading_out_to_pause_ = false;
    has_faded_out_ = true;

    fadeout_pause_pipeline_.reset();
    fadeout_pipeline_.reset();
  }

  if (fadeout_enabled_ && current_pipeline_ && !stop_after) StartFadeout();

  current_pipeline_.reset();
  BufferingFinished();
  emit StateChanged(Engine::State::Empty);

}

void GstEngine::Pause() {

  if (!current_pipeline_ || current_pipeline_->is_buffering()) return;

  // Check if we started a fade out. If it isn't finished yet and the user pressed play, we inverse the fader and resume the playback.
  if (is_fading_out_to_pause_) {
    QObject::disconnect(current_pipeline_.get(), &GstEnginePipeline::FaderFinished, nullptr, nullptr);
    current_pipeline_->StartFader(fadeout_pause_duration_nanosec_, QTimeLine::Forward, QEasingCurve::InOutQuad, false);
    is_fading_out_to_pause_ = false;
    has_faded_out_ = false;
    emit StateChanged(Engine::State::Playing);
    return;
  }

  if (current_pipeline_->state() == GST_STATE_PLAYING) {
    if (fadeout_pause_enabled_) {
      StartFadeoutPause();
    }
    else {
      current_pipeline_->SetState(GST_STATE_PAUSED);
      emit StateChanged(Engine::State::Paused);
      StopTimers();
    }
  }

}

void GstEngine::Unpause() {

  if (!current_pipeline_ || current_pipeline_->is_buffering()) return;

  if (current_pipeline_->state() == GST_STATE_PAUSED) {
    current_pipeline_->SetState(GST_STATE_PLAYING);

    // Check if we faded out last time. If yes, fade in no matter what the settings say.
    // If we pause with fadeout, deactivate fadeout and resume playback, the player would be muted if not faded in.
    if (has_faded_out_) {
      QObject::disconnect(current_pipeline_.get(), &GstEnginePipeline::FaderFinished, nullptr, nullptr);
      current_pipeline_->StartFader(fadeout_pause_duration_nanosec_, QTimeLine::Forward, QEasingCurve::InOutQuad, false);
      has_faded_out_ = false;
    }

    emit StateChanged(Engine::State::Playing);

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

const Engine::Scope &GstEngine::scope(const int chunk_length) {

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

  PluginDetailsList plugins = GetPluginList("Sink/Audio");
  EngineBase::OutputDetailsList ret;
  ret.reserve(plugins.count());
  for (const PluginDetails &plugin : plugins) {
    OutputDetails output;
    output.name = plugin.name;
    output.description = plugin.description;
    if (plugin.name == kAutoSink) output.iconname = "soundcard";
    else if (plugin.name == kALSASink || plugin.name == kOSS4Sink) output.iconname = "alsa";
    else if (plugin.name == kJackAudioSink) output.iconname = "jack";
    else if (plugin.name == kPulseSink) output.iconname = "pulseaudio";
    else if (plugin.name == kA2DPSink || plugin.name == kAVDTPSink) output.iconname = "bluetooth";
    else output.iconname = "soundcard";
    ret.append(output);
  }

  return ret;

}

bool GstEngine::ValidOutput(const QString &output) {

  EnsureInitialized();

  PluginDetailsList plugins = GetPluginList("Sink/Audio");
  return std::any_of(plugins.begin(), plugins.end(), [output](const PluginDetails &plugin) { return plugin.name == output; });

}

bool GstEngine::CustomDeviceSupport(const QString &output) {
  return (output == kALSASink || output == kOpenALSASink || output == kOSSSink || output == kOSS4Sink || output == kPulseSink || output == kA2DPSink || output == kAVDTPSink || output == kJackAudioSink);
}

bool GstEngine::ALSADeviceSupport(const QString &output) {
  return (output == kALSASink);
}

void GstEngine::ReloadSettings() {

  Engine::Base::ReloadSettings();

  if (output_.isEmpty()) output_ = kAutoSink;

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

  if (current_pipeline_) {
    const qint64 current_position = position_nanosec();
    const qint64 current_length = length_nanosec();

    const qint64 remaining = current_length - current_position;

    const qint64 fudge = kTimerIntervalNanosec + 100 * kNsecPerMsec;  // Mmm fudge
    const qint64 gap = static_cast<qint64>(buffer_duration_nanosec_) + (autocrossfade_enabled_ ? fadeout_duration_nanosec_ : kPreloadGapNanosec);

    // only if we know the length of the current stream...
    if (current_length > 0) {
      // emit TrackAboutToEnd when we're a few seconds away from finishing
      if (remaining < gap + fudge) {
        EmitAboutToEnd();
      }
    }
  }

}

void GstEngine::EndOfStreamReached(const int pipeline_id, const bool has_next_track) {

  if (!current_pipeline_.get() || current_pipeline_->id() != pipeline_id)
    return;

  if (!has_next_track) {
    current_pipeline_.reset();
    BufferingFinished();
  }
  emit TrackEnded();

}

void GstEngine::HandlePipelineError(const int pipeline_id, const int domain, const int error_code, const QString &message, const QString &debugstr) {

  if (!current_pipeline_.get() || current_pipeline_->id() != pipeline_id) return;

  qLog(Error) << "GStreamer error:" << domain << error_code << message;

  current_pipeline_.reset();
  BufferingFinished();
  emit StateChanged(Engine::State::Error);

  if (
      (domain == static_cast<int>(GST_RESOURCE_ERROR) && (
        error_code == static_cast<int>(GST_RESOURCE_ERROR_NOT_FOUND) ||
        error_code == static_cast<int>(GST_RESOURCE_ERROR_OPEN_READ) ||
        error_code == static_cast<int>(GST_RESOURCE_ERROR_NOT_AUTHORIZED)
      ))
      || (domain == static_cast<int>(GST_STREAM_ERROR) && error_code == static_cast<int>(GST_STREAM_ERROR_TYPE_NOT_FOUND))
      ) {
     emit InvalidSongRequested(stream_url_);
   }
  else {
    emit FatalError();
  }

  emit Error(message);
  emit Error(debugstr);

}

void GstEngine::NewMetaData(const int pipeline_id, const Engine::SimpleMetaBundle &bundle) {

  if (!current_pipeline_.get() || current_pipeline_->id() != pipeline_id) return;
  emit MetaData(bundle);

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

void GstEngine::FadeoutFinished() {
  fadeout_pipeline_.reset();
  emit FadeoutFinishedSignal();
}

void GstEngine::FadeoutPauseFinished() {

  fadeout_pause_pipeline_->SetState(GST_STATE_PAUSED);
  current_pipeline_->SetState(GST_STATE_PAUSED);
  emit StateChanged(Engine::State::Paused);
  StopTimers();

  is_fading_out_to_pause_ = false;
  has_faded_out_ = true;
  fadeout_pause_pipeline_.reset();
  fadeout_pipeline_.reset();

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

void GstEngine::PlayDone(const GstStateChangeReturn ret, const quint64 offset_nanosec, const int pipeline_id) {

  if (!current_pipeline_ || pipeline_id != current_pipeline_->id()) {
    return;
  }

  if (ret == GST_STATE_CHANGE_FAILURE) {
    // Failure, but we got a redirection URL - try loading that instead
    QByteArray redirect_url = current_pipeline_->redirect_url();
    if (!redirect_url.isEmpty() && redirect_url != current_pipeline_->stream_url()) {
      qLog(Info) << "Redirecting to" << redirect_url;
      current_pipeline_ = CreatePipeline(redirect_url, current_pipeline_->original_url(), end_nanosec_);
      Play(offset_nanosec);
      return;
    }

    // Failure - give up
    qLog(Warning) << "Could not set thread to PLAYING.";
    current_pipeline_.reset();
    BufferingFinished();
    return;
  }

  StartTimers();

  // Initial offset
  if (offset_nanosec != 0 || beginning_nanosec_ != 0) {
    Seek(offset_nanosec);
  }

  emit StateChanged(Engine::State::Playing);
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

GstEngine::PluginDetailsList GstEngine::GetPluginList(const QString &classname) const {

  const_cast<GstEngine*>(this)->EnsureInitialized();

  PluginDetailsList ret;

  GstRegistry *registry = gst_registry_get();
  GList *const features = gst_registry_get_feature_list(registry, GST_TYPE_ELEMENT_FACTORY);

  GList *p = features;
  while (p) {
    GstElementFactory *factory = GST_ELEMENT_FACTORY(p->data);
    if (QString(gst_element_factory_get_klass(factory)).contains(classname)) {
      PluginDetails details;
      details.name = QString::fromUtf8(gst_plugin_feature_get_name(p->data));
      details.description = QString::fromUtf8(gst_element_factory_get_metadata(factory, GST_ELEMENT_METADATA_DESCRIPTION));
      if (details.name == "wasapi2sink" && details.description == "Stream audio to an audio capture device through WASAPI") {
        details.description += " 2";
      }
      ret << details;
      //qLog(Debug) << details.name << details.description;
    }
    p = g_list_next(p);
  }

  gst_plugin_feature_list_free(features);
  return ret;

}

QByteArray GstEngine::FixupUrl(const QUrl &url) {

  EnsureInitialized();

  QByteArray uri;

  // It's a file:// url with a hostname set.
  // QUrl::fromLocalFile does this when given a \\host\share\file path on Windows.
  // Munge it back into a path that gstreamer will recognise.
  if (url.isLocalFile() && !url.host().isEmpty()) {
    QString str = "file:////" + url.host() + url.path();
    uri = str.toUtf8();
  }
  else if (url.scheme() == "cdda") {
    QString str;
    if (url.path().isEmpty()) {
      str = url.toString();
      str.remove(str.lastIndexOf(QChar('a')), 1);
    }
    else {
      // Currently, Gstreamer can't handle input CD devices inside cdda URL.
      // So we handle them ourselves: we extract the track number and re-create a URL with only cdda:// + the track number (which can be handled by Gstreamer).
      // We keep the device in mind, and we will set it later using SourceSetupCallback
      QStringList path = url.path().split('/');
      str = QString("cdda://%1").arg(path.takeLast());
      QString device = path.join("/");
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

  if (is_fading_out_to_pause_) return;

  fadeout_pipeline_ = current_pipeline_;
  QObject::disconnect(fadeout_pipeline_.get(), nullptr, nullptr, nullptr);
  fadeout_pipeline_->RemoveAllBufferConsumers();

  fadeout_pipeline_->StartFader(fadeout_duration_nanosec_, QTimeLine::Backward);
  QObject::connect(fadeout_pipeline_.get(), &GstEnginePipeline::FaderFinished, this, &GstEngine::FadeoutFinished);

}

void GstEngine::StartFadeoutPause() {

  fadeout_pause_pipeline_ = current_pipeline_;
  QObject::disconnect(fadeout_pause_pipeline_.get(), &GstEnginePipeline::FaderFinished, nullptr, nullptr);

  fadeout_pause_pipeline_->StartFader(fadeout_pause_duration_nanosec_, QTimeLine::Backward, QEasingCurve::InOutQuad, false);
  if (fadeout_pipeline_ && fadeout_pipeline_->state() == GST_STATE_PLAYING) {
    fadeout_pipeline_->StartFader(fadeout_pause_duration_nanosec_, QTimeLine::Backward, QEasingCurve::Linear, false);
  }
  QObject::connect(fadeout_pause_pipeline_.get(), &GstEnginePipeline::FaderFinished, this, &GstEngine::FadeoutPauseFinished);
  is_fading_out_to_pause_ = true;

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

std::shared_ptr<GstEnginePipeline> GstEngine::CreatePipeline() {

  EnsureInitialized();

  std::shared_ptr<GstEnginePipeline> ret = std::make_shared<GstEnginePipeline>();
  ret->set_output_device(output_, device_);
  ret->set_volume_enabled(volume_control_);
  ret->set_stereo_balancer_enabled(stereo_balancer_enabled_);
  ret->set_equalizer_enabled(equalizer_enabled_);
  ret->set_replaygain(rg_enabled_, rg_mode_, rg_preamp_, rg_fallbackgain_, rg_compression_);
  ret->set_buffer_duration_nanosec(buffer_duration_nanosec_);
  ret->set_buffer_low_watermark(buffer_low_watermark_);
  ret->set_buffer_high_watermark(buffer_high_watermark_);
  ret->set_proxy_settings(proxy_address_, proxy_authentication_, proxy_user_, proxy_pass_);
  ret->set_channels(channels_enabled_, channels_);
  ret->set_bs2b_enabled(bs2b_enabled_);
  ret->set_strict_ssl_enabled(strict_ssl_enabled_);
  ret->set_fading_enabled(fadeout_enabled_ || autocrossfade_enabled_ || fadeout_pause_enabled_);

  ret->AddBufferConsumer(this);
  for (GstBufferConsumer *consumer : buffer_consumers_) {
    ret->AddBufferConsumer(consumer);
  }

  QObject::connect(ret.get(), &GstEnginePipeline::EndOfStreamReached, this, &GstEngine::EndOfStreamReached);
  QObject::connect(ret.get(), &GstEnginePipeline::Error, this, &GstEngine::HandlePipelineError);
  QObject::connect(ret.get(), &GstEnginePipeline::MetadataFound, this, &GstEngine::NewMetaData);
  QObject::connect(ret.get(), &GstEnginePipeline::BufferingStarted, this, &GstEngine::BufferingStarted);
  QObject::connect(ret.get(), &GstEnginePipeline::BufferingProgress, this, &GstEngine::BufferingProgress);
  QObject::connect(ret.get(), &GstEnginePipeline::BufferingFinished, this, &GstEngine::BufferingFinished);
  QObject::connect(ret.get(), &GstEnginePipeline::VolumeChanged, this, &EngineBase::UpdateVolume);

  return ret;

}

std::shared_ptr<GstEnginePipeline> GstEngine::CreatePipeline(const QByteArray &gst_url, const QUrl &original_url, const qint64 end_nanosec) {

  std::shared_ptr<GstEnginePipeline> ret = CreatePipeline();
  QString error;
  if (!ret->InitFromUrl(gst_url, original_url, end_nanosec, error)) {
    ret.reset();
    emit Error(error);
    emit StateChanged(Engine::State::Error);
    emit FatalError();
  }

  return ret;

}

void GstEngine::UpdateScope(const int chunk_length) {

  using sample_type = Engine::Scope::value_type;

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
    bytes = qMin(static_cast<Engine::Scope::size_type>(map.size - (chunk_size * scope_chunk_)), scope_.size() * sizeof(sample_type));
  }
  else {
    bytes = qMin(static_cast<Engine::Scope::size_type>(chunk_size), scope_.size() * sizeof(sample_type));
  }

  scope_chunk_++;

  if (buffer_format_.startsWith("S16LE") ||
      buffer_format_.startsWith("U16LE") ||
      buffer_format_.startsWith("S24LE") ||
      buffer_format_.startsWith("S24_32LE") ||
      buffer_format_.startsWith("S32LE") ||
      buffer_format_.startsWith("F32LE")
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

  QString discovered_url(gst_discoverer_info_get_uri(info));

  GstDiscovererResult result = gst_discoverer_info_get_result(info);
  if (result != GST_DISCOVERER_OK) {
    QString error_message = GSTdiscovererErrorMessage(result);
    qLog(Error) << QString("Stream discovery for %1 failed: %2").arg(discovered_url, error_message);
    return;
  }

  GList *audio_streams = gst_discoverer_info_get_audio_streams(info);
  if (audio_streams) {

    GstDiscovererStreamInfo *stream_info = reinterpret_cast<GstDiscovererStreamInfo*>(g_list_first(audio_streams)->data);

    Engine::SimpleMetaBundle bundle;
    if (discovered_url == instance->current_pipeline_->stream_url()) {
      bundle.type = Engine::SimpleMetaBundle::Type::Current;
      bundle.url = instance->current_pipeline_->original_url();
    }
    else if (discovered_url == instance->current_pipeline_->next_stream_url()) {
      bundle.type = Engine::SimpleMetaBundle::Type::Next;
      bundle.url = instance->current_pipeline_->next_original_url();
    }
    bundle.stream_url = QUrl(discovered_url);
    bundle.samplerate = static_cast<int>(gst_discoverer_audio_info_get_sample_rate(GST_DISCOVERER_AUDIO_INFO(stream_info)));
    bundle.bitdepth = static_cast<int>(gst_discoverer_audio_info_get_depth(GST_DISCOVERER_AUDIO_INFO(stream_info)));
    bundle.bitrate = static_cast<int>(gst_discoverer_audio_info_get_bitrate(GST_DISCOVERER_AUDIO_INFO(stream_info)) / 1000);

    GstCaps *caps = gst_discoverer_stream_info_get_caps(stream_info);

    const guint caps_size = gst_caps_get_size(caps);
    for (guint i = 0; i < caps_size; ++i) {
      GstStructure *gst_structure = gst_caps_get_structure(caps, i);
      if (!gst_structure) continue;
      QString mimetype = gst_structure_get_name(gst_structure);
      if (!mimetype.isEmpty() && mimetype != "audio/mpeg") {
        bundle.filetype = Song::FiletypeByMimetype(mimetype);
        if (bundle.filetype == Song::FileType::Unknown) {
          qLog(Error) << "Unknown mimetype" << mimetype;
        }
      }
    }

    if (bundle.filetype == Song::FileType::Unknown) {
      gchar *codec_description = gst_pb_utils_get_codec_description(caps);
      QString filetype_description = (codec_description ? QString(codec_description) : QString());
      g_free(codec_description);
      if (!filetype_description.isEmpty()) {
        bundle.filetype = Song::FiletypeByDescription(filetype_description);
        if (bundle.filetype == Song::FileType::Unknown) {
          qLog(Error) << "Unknown filetype" << filetype_description;
        }
      }
    }

    gst_caps_unref(caps);
    gst_discoverer_stream_info_list_free(audio_streams);

    qLog(Debug) << "Got stream info for" << discovered_url + ":" << Song::TextForFiletype(bundle.filetype);

    emit instance->MetaData(bundle);

  }
  else {
    qLog(Error) << "Could not detect an audio stream in" << discovered_url;
  }

}

void GstEngine::StreamDiscoveryFinished(GstDiscoverer*, gpointer) {}

QString GstEngine::GSTdiscovererErrorMessage(GstDiscovererResult result) {

  switch (result) {
    case GST_DISCOVERER_URI_INVALID:     return "The URI is invalid";
    case GST_DISCOVERER_TIMEOUT:         return "The discovery timed-out";
    case GST_DISCOVERER_BUSY:            return "The discoverer was already discovering a file";
    case GST_DISCOVERER_MISSING_PLUGINS: return "Some plugins are missing for full discovery";
    case GST_DISCOVERER_ERROR:
    default:                             return "An error happened and the GError is set";
  }

}
