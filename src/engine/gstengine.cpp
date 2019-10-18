/***************************************************************************
 *   Copyright (C) 2003-2005 by Mark Kretschmann <markey@web.de>           *
 *   Copyright (C) 2005 by Jakub Stachowski <qbast@go2.pl>                 *
 *   Copyright (C) 2006 Paul Cifarelli <paul@cifarelli.net>                *
 *   Copyright (C) 2017-2018 Jonas Kvinge <jonas@jkvinge.net>              *
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
#include <gio/gio.h>
#include <memory>
#include <vector>
#include <math.h>
#include <string>

#include <gst/gst.h>

#include <QtGlobal>
#include <QFuture>
#include <QTimer>
#include <QList>
#include <QByteArray>
#include <QChar>
#include <QString>
#include <QStringBuilder>
#include <QStringList>
#include <QUrl>
#include <QTimeLine>
#include <QTimerEvent>
#include <QMetaObject>
#include <QFlags>
#include <QSettings>
#include <QtDebug>

#include "core/closure.h"
#include "core/utilities.h"
#include "core/logging.h"
#include "core/taskmanager.h"
#include "core/timeconstants.h"
#include "enginebase.h"
#include "enginetype.h"
#include "gstengine.h"
#include "gstenginepipeline.h"
#include "gstbufferconsumer.h"

#include "settings/backendsettingspage.h"

using std::shared_ptr;
using std::vector;

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

GstEngine::GstEngine(TaskManager *task_manager)
    : Engine::Base(),
      task_manager_(task_manager),
      buffering_task_id_(-1),
      latest_buffer_(nullptr),
      stereo_balance_(0.0f),
      seek_timer_(new QTimer(this)),
      timer_id_(-1),
      next_element_id_(0),
      is_fading_out_to_pause_(false),
      has_faded_out_(false),
      scope_chunk_(0),
      have_new_buffer_(false) {

  type_ = Engine::GStreamer;
  seek_timer_->setSingleShot(true);
  seek_timer_->setInterval(kSeekDelayNanosec / kNsecPerMsec);
  connect(seek_timer_, SIGNAL(timeout()), SLOT(SeekNow()));

  ReloadSettings();

}

GstEngine::~GstEngine() {
  EnsureInitialised();
  current_pipeline_.reset();
}

bool GstEngine::Init() {

  return true;

}

Engine::State GstEngine::state() const {

  if (!current_pipeline_) return stream_url_.isEmpty() ? Engine::Empty : Engine::Idle;

  switch (current_pipeline_->state()) {
    case GST_STATE_NULL:
      return Engine::Empty;
    case GST_STATE_READY:
      return Engine::Idle;
    case GST_STATE_PLAYING:
      return Engine::Playing;
    case GST_STATE_PAUSED:
      return Engine::Paused;
    default:
      return Engine::Empty;
  }

}

void GstEngine::StartPreloading(const QUrl &stream_url, const QUrl &original_url, const bool force_stop_at_end, const qint64 beginning_nanosec, const qint64 end_nanosec) {

  EnsureInitialised();

  QByteArray gst_url = FixupUrl(stream_url);

  // No crossfading, so we can just queue the new URL in the existing pipeline and get gapless playback (hopefully)
  if (current_pipeline_)
    current_pipeline_->SetNextUrl(gst_url, original_url, beginning_nanosec, force_stop_at_end ? end_nanosec : 0);

}

bool GstEngine::Load(const QUrl &stream_url, const QUrl &original_url, Engine::TrackChangeFlags change, const bool force_stop_at_end, const quint64 beginning_nanosec, const qint64 end_nanosec) {

  EnsureInitialised();

  Engine::Base::Load(stream_url, original_url, change, force_stop_at_end, beginning_nanosec, end_nanosec);

  QByteArray gst_url = FixupUrl(stream_url);

  bool crossfade = current_pipeline_ && ((crossfade_enabled_ && change & Engine::Manual) || (autocrossfade_enabled_ && change & Engine::Auto) || ((crossfade_enabled_ || autocrossfade_enabled_) && change & Engine::Intro));

  if (change & Engine::Auto && change & Engine::SameAlbum && !crossfade_same_album_)
    crossfade = false;

  if (!crossfade && current_pipeline_ && current_pipeline_->stream_url() == gst_url && change & Engine::Auto) {
    // We're not crossfading, and the pipeline is already playing the URI we want, so just do nothing.
    return true;
  }

  shared_ptr<GstEnginePipeline> pipeline = CreatePipeline(gst_url, original_url, force_stop_at_end ? end_nanosec : 0);
  if (!pipeline) return false;

  if (crossfade) StartFadeout();

  BufferingFinished();
  current_pipeline_ = pipeline;

  SetVolume(volume_);
  SetStereoBalance(stereo_balance_);
  SetEqualizerParameters(equalizer_preamp_, equalizer_gains_);

  // Maybe fade in this track
  if (crossfade)
    current_pipeline_->StartFader(fadeout_duration_nanosec_, QTimeLine::Forward);

  return true;

}

bool GstEngine::Play(const quint64 offset_nanosec) {

  EnsureInitialised();

  if (!current_pipeline_ || current_pipeline_->is_buffering()) return false;

  QFuture<GstStateChangeReturn> future = current_pipeline_->SetState(GST_STATE_PLAYING);
  NewClosure(future, this, SLOT(PlayDone(QFuture<GstStateChangeReturn>, quint64, int)), future, offset_nanosec, current_pipeline_->id());

  if (is_fading_out_to_pause_) {
    current_pipeline_->SetState(GST_STATE_PAUSED);
  }

  return true;

}

void GstEngine::Stop(const bool stop_after) {

  StopTimers();

  stream_url_ = QUrl();  // To ensure we return Empty from state()
  original_url_ = QUrl();
  beginning_nanosec_ = end_nanosec_ = 0;

  // Check if we started a fade out. If it isn't finished yet and the user pressed stop, we cancel the fader and just stop the playback.
  if (is_fading_out_to_pause_) {
    disconnect(current_pipeline_.get(), SIGNAL(FaderFinished()), 0, 0);
    is_fading_out_to_pause_ = false;
    has_faded_out_ = true;

    fadeout_pause_pipeline_.reset();
    fadeout_pipeline_.reset();
  }

  if (fadeout_enabled_ && current_pipeline_ && !stop_after) StartFadeout();

  current_pipeline_.reset();
  BufferingFinished();
  emit StateChanged(Engine::Empty);

}

void GstEngine::Pause() {

  if (!current_pipeline_ || current_pipeline_->is_buffering()) return;

  // Check if we started a fade out. If it isn't finished yet and the user pressed play, we inverse the fader and resume the playback.
  if (is_fading_out_to_pause_) {
    disconnect(current_pipeline_.get(), SIGNAL(FaderFinished()), 0, 0);
    current_pipeline_->StartFader(fadeout_pause_duration_nanosec_, QTimeLine::Forward, QTimeLine::EaseInOutCurve, false);
    is_fading_out_to_pause_ = false;
    has_faded_out_ = false;
    emit StateChanged(Engine::Playing);
    return;
  }

  if (current_pipeline_->state() == GST_STATE_PLAYING) {
    if (fadeout_pause_enabled_) {
      StartFadeoutPause();
    }
    else {
      current_pipeline_->SetState(GST_STATE_PAUSED);
      emit StateChanged(Engine::Paused);
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
      disconnect(current_pipeline_.get(), SIGNAL(FaderFinished()), 0, 0);
      current_pipeline_->StartFader(fadeout_pause_duration_nanosec_, QTimeLine::Forward, QTimeLine::EaseInOutCurve, false);
      has_faded_out_ = false;
    }

    emit StateChanged(Engine::Playing);

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

void GstEngine::SetVolumeSW(const uint percent) {
  if (current_pipeline_) current_pipeline_->SetVolume(percent);
}

qint64 GstEngine::position_nanosec() const {

  if (!current_pipeline_) return 0;

  const qint64 result = current_pipeline_->position() - beginning_nanosec_;
  return qint64(qMax(0ll, result));

}

qint64 GstEngine::length_nanosec() const {

  if (!current_pipeline_) return 0;

  const qint64 result = end_nanosec_ - beginning_nanosec_;

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
      scope_chunks_ = ceil(((double)GST_BUFFER_DURATION(latest_buffer_) / (double)(chunk_length * kNsecPerMsec)));
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

  const_cast<GstEngine*>(this)->EnsureInitialised();

  EngineBase::OutputDetailsList ret;

  PluginDetailsList plugins = GetPluginList("Sink/Audio");

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

  EnsureInitialised();

  PluginDetailsList plugins = GetPluginList("Sink/Audio");
  for (const PluginDetails &plugin : plugins) {
    if (plugin.name == output) return(true);
  }
  return(false);

}

bool GstEngine::CustomDeviceSupport(const QString &output) {
  return (output == kALSASink || output == kOpenALSASink || output == kOSSSink || output == kOSS4Sink || output == kPulseSink || output == kA2DPSink || output == kAVDTPSink);
}

bool GstEngine::ALSADeviceSupport(const QString &output) {
  return (output == kALSASink);
}

void GstEngine::ReloadSettings() {

  Engine::Base::ReloadSettings();

  if (output_.isEmpty()) output_ = kAutoSink;

}

GstElement *GstEngine::CreateElement(const QString &factoryName, GstElement *bin, const bool showerror) {

  // Make a unique name
  QString name = factoryName + "-" + QString::number(next_element_id_++);

  GstElement *element = gst_element_factory_make(factoryName.toUtf8().constData(), name.toUtf8().constData());
  if (!element) {
    if (showerror) emit Error(QString("GStreamer could not create the element: %1.").arg(factoryName));
    else qLog(Error) << "GStreamer could not create the element:" << factoryName;
    emit StateChanged(Engine::Error);
    emit FatalError();
    return nullptr;
  }

  if (bin) gst_bin_add(GST_BIN(bin), element);

  return element;
}

void GstEngine::ConsumeBuffer(GstBuffer *buffer, const int pipeline_id, const QString &format) {

  // Schedule this to run in the GUI thread.  The buffer gets added to the queue and unreffed by UpdateScope.
  if (!QMetaObject::invokeMethod(this, "AddBufferToScope", Q_ARG(GstBuffer*, buffer), Q_ARG(int, pipeline_id), Q_ARG(QString, format))) {
    qLog(Warning) << "Failed to invoke AddBufferToScope on GstEngine";
  }

}

void GstEngine::SetEqualizerEnabled(const bool enabled) {

  equalizer_enabled_ = enabled;

  if (current_pipeline_) current_pipeline_->SetEqualizerEnabled(enabled);
}

void GstEngine::SetEqualizerParameters(const int preamp, const QList<int> &band_gains) {

  equalizer_preamp_ = preamp;
  equalizer_gains_ = band_gains;

  if (current_pipeline_)
    current_pipeline_->SetEqualizerParams(preamp, band_gains);

}

void GstEngine::SetStereoBalance(const float value) {

  stereo_balance_ = value;

  if (current_pipeline_) current_pipeline_->SetStereoBalance(value);

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
    const qint64 gap = buffer_duration_nanosec_ + (autocrossfade_enabled_ ? fadeout_duration_nanosec_ : kPreloadGapNanosec);

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

void GstEngine::HandlePipelineError(int pipeline_id, const QString &message, int domain, int error_code) {

  if (!current_pipeline_.get() || current_pipeline_->id() != pipeline_id) return;

  qLog(Error) << "Gstreamer error:" << domain << error_code << message;

  current_pipeline_.reset();
  BufferingFinished();
  emit StateChanged(Engine::Error);

  if (domain == GST_RESOURCE_ERROR && (error_code == GST_RESOURCE_ERROR_NOT_FOUND || error_code == GST_RESOURCE_ERROR_NOT_AUTHORIZED)) {
     emit InvalidSongRequested(stream_url_);
   }
  else {
    emit FatalError();
  }

  emit Error(message);

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
  emit StateChanged(Engine::Paused);
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

  if (!current_pipeline_->Seek(seek_pos_)) {
    qLog(Warning) << "Seek failed";
  }
}

void GstEngine::PlayDone(QFuture<GstStateChangeReturn> future, const quint64 offset_nanosec, const int pipeline_id) {

  GstStateChangeReturn ret = future.result();

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

  emit StateChanged(Engine::Playing);
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

  const_cast<GstEngine*>(this)->EnsureInitialised();

  PluginDetailsList ret;

  GstRegistry *registry = gst_registry_get();
  GList *const features = gst_registry_get_feature_list(registry, GST_TYPE_ELEMENT_FACTORY);

  GList *p = features;
  while (p) {
    GstElementFactory *factory = GST_ELEMENT_FACTORY(p->data);
    if (QString(gst_element_factory_get_klass(factory)).contains(classname)) {;
      PluginDetails details;
      details.name = QString::fromUtf8(gst_plugin_feature_get_name(p->data));
      details.description = QString::fromUtf8(gst_element_factory_get_metadata(factory, GST_ELEMENT_METADATA_DESCRIPTION));
      ret << details;
      //qLog(Debug) << details.name << details.description;
    }
    p = g_list_next(p);
  }

  gst_plugin_feature_list_free(features);
  return ret;

}

QByteArray GstEngine::FixupUrl(const QUrl &url) {

  EnsureInitialised();

  QByteArray uri;

  // It's a file:// url with a hostname set.
  // QUrl::fromLocalFile does this when given a \\host\share\file path on Windows.
  // Munge it back into a path that gstreamer will recognise.
  if (url.scheme() == "file" && !url.host().isEmpty()) {
    QString str = "file:////" + url.host() + url.path();
    uri = str.toLocal8Bit();
  }
  else if (url.scheme() == "cdda") {
    QString str;
    if (url.path().isEmpty()) {
      str = url.toString();
      str.remove(str.lastIndexOf(QChar('a')), 1);
    }
    else {
      // Currently, Gstreamer can't handle input CD devices inside cdda URL.
      // So we handle them ourselves: we extract the track number and re-create an URL with only cdda:// + the track number (which can be handled by Gstreamer).
      // We keep the device in mind, and we will set it later using SourceSetupCallback
      QStringList path = url.path().split('/');
      str = QString("cdda://%1").arg(path.takeLast());
      QString device = path.join("/");
      if (current_pipeline_) current_pipeline_->SetSourceDevice(device);
    }
    uri = str.toLocal8Bit();
  }
  else {
    uri = url.toEncoded();
  }

  return uri;

}

void GstEngine::StartFadeout() {

  if (is_fading_out_to_pause_) return;

  fadeout_pipeline_ = current_pipeline_;
  disconnect(fadeout_pipeline_.get(), 0, 0, 0);
  fadeout_pipeline_->RemoveAllBufferConsumers();

  fadeout_pipeline_->StartFader(fadeout_duration_nanosec_, QTimeLine::Backward);
  connect(fadeout_pipeline_.get(), SIGNAL(FaderFinished()), SLOT(FadeoutFinished()));

}

void GstEngine::StartFadeoutPause() {

  fadeout_pause_pipeline_ = current_pipeline_;
  disconnect(fadeout_pause_pipeline_.get(), SIGNAL(FaderFinished()), 0, 0);

  fadeout_pause_pipeline_->StartFader(fadeout_pause_duration_nanosec_, QTimeLine::Backward, QTimeLine::EaseInOutCurve, false);
  if (fadeout_pipeline_ && fadeout_pipeline_->state() == GST_STATE_PLAYING) {
    fadeout_pipeline_->StartFader(fadeout_pause_duration_nanosec_, QTimeLine::Backward, QTimeLine::LinearCurve, false);
  }
  connect(fadeout_pause_pipeline_.get(), SIGNAL(FaderFinished()), SLOT(FadeoutPauseFinished()));
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

shared_ptr<GstEnginePipeline> GstEngine::CreatePipeline() {

  EnsureInitialised();

  shared_ptr<GstEnginePipeline> ret(new GstEnginePipeline(this));
  ret->set_output_device(output_, device_);
  ret->set_volume_control(volume_control_);
  ret->set_replaygain(rg_enabled_, rg_mode_, rg_preamp_, rg_compression_);
  ret->set_buffer_duration_nanosec(buffer_duration_nanosec_);
  ret->set_buffer_min_fill(buffer_min_fill_);
  ret->SetEqualizerEnabled(equalizer_enabled_);

  ret->AddBufferConsumer(this);
  for (GstBufferConsumer *consumer : buffer_consumers_) {
    ret->AddBufferConsumer(consumer);
  }

  connect(ret.get(), SIGNAL(EndOfStreamReached(int, bool)), SLOT(EndOfStreamReached(int, bool)));
  connect(ret.get(), SIGNAL(Error(int, QString, int, int)), SLOT(HandlePipelineError(int, QString, int, int)));
  connect(ret.get(), SIGNAL(MetadataFound(int, Engine::SimpleMetaBundle)), SLOT(NewMetaData(int, Engine::SimpleMetaBundle)));
  connect(ret.get(), SIGNAL(BufferingStarted()), SLOT(BufferingStarted()));
  connect(ret.get(), SIGNAL(BufferingProgress(int)), SLOT(BufferingProgress(int)));
  connect(ret.get(), SIGNAL(BufferingFinished()), SLOT(BufferingFinished()));

  return ret;

}

shared_ptr<GstEnginePipeline> GstEngine::CreatePipeline(const QByteArray &gst_url, const QUrl &original_url, const qint64 end_nanosec) {

  shared_ptr<GstEnginePipeline> ret = CreatePipeline();
  if (!ret->InitFromUrl(gst_url, original_url, end_nanosec)) ret.reset();
  return ret;

}

void GstEngine::UpdateScope(const int chunk_length) {

  typedef Engine::Scope::value_type sample_type;

  // Prevent dbz or invalid chunk size
  if (!GST_CLOCK_TIME_IS_VALID(GST_BUFFER_DURATION(latest_buffer_))) return;
  if (GST_BUFFER_DURATION(latest_buffer_) == 0) return;

  GstMapInfo map;
  gst_buffer_map(latest_buffer_, &map, GST_MAP_READ);

  // Determine where to split the buffer
  int chunk_density = (map.size * kNsecPerMsec) / GST_BUFFER_DURATION(latest_buffer_);

  int chunk_size = chunk_length * chunk_density;

  // In case a buffer doesn't arrive in time
  if (scope_chunk_ >= scope_chunks_) {
    scope_chunk_ = 0;
    return;
  }

  const sample_type *source = reinterpret_cast<sample_type*>(map.data);
  sample_type *dest = scope_.data();
  source += (chunk_size / sizeof(sample_type)) * scope_chunk_;

  int bytes = 0;

  // Make sure we don't go beyond the end of the buffer
  if (scope_chunk_ == scope_chunks_ - 1) {
    bytes = qMin(static_cast<Engine::Scope::size_type>(map.size - (chunk_size  * scope_chunk_)), scope_.size() * sizeof(sample_type));
  }
  else {
    bytes = qMin(static_cast<Engine::Scope::size_type>(chunk_size), scope_.size() * sizeof(sample_type));
  }

  scope_chunk_++;

  if (buffer_format_ == "S16LE" ||
      buffer_format_ == "S16BE" ||
      buffer_format_ == "U16LE" ||
      buffer_format_ == "U16BE" ||
      buffer_format_ == "S16" ||
      buffer_format_ == "U16")
    memcpy(dest, source, bytes);
  else
    memset(dest, 0, bytes);

  gst_buffer_unmap(latest_buffer_, &map);

  if (scope_chunk_ == scope_chunks_) {
    gst_buffer_unref(latest_buffer_);
    latest_buffer_ = nullptr;
    buffer_format_.clear();
  }

}
