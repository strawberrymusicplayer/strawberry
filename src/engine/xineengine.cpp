/***************************************************************************
 *   Copyright (C) 2017-2018 Jonas Kvinge <jonas@jkvinge.net>              *
 *   Copyright (C) 2005 Christophe Thommeret <hftom@free.fr>               *
 *             (C) 2005 Ian Monroe <ian@monroe.nu>                         *
 *             (C) 2005-2006 Mark Kretschmann <markey@web.de>              *
 *             (C) 2004-2005 Max Howell <max.howell@methylblue.com>        *
 *             (C) 2003-2004 J. Kofler <kaffeine@gmx.net>                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "config.h"

#ifndef XINE_ENGINE_INTERNAL
#  define XINE_ENGINE_INTERNAL
#endif

#ifndef METRONOM_INTERNAL
#  define METRONOM_INTERNAL
#endif

#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <xine.h>
#ifdef XINE_ANALYZER
#  include <xine/metronom.h>
#endif

#include <memory>
#include <cstdlib>
#include <climits>

#include <QtGlobal>
#include <QMutex>
#include <QCoreApplication>
#include <QByteArray>
#include <QChar>
#include <QFileInfo>
#include <QLocale>
#include <QVariant>
#include <QString>
#include <QStringBuilder>
#include <QStringList>
#include <QUrl>
#include <QTimer>
#include <QtDebug>

#include "core/logging.h"
#include <core/timeconstants.h>
#include "engine_fwd.h"
#include "enginebase.h"
#include "enginetype.h"
#include "xineengine.h"
#ifdef XINE_ANALYZER
#  include "xinescope.h"
#endif

using std::shared_ptr;

#ifndef LLONG_MAX
#define LLONG_MAX 9223372036854775807LL
#endif

// Define this to use xine in a more standard way
//#define XINE_SAFE_MODE

const char *XineEngine::kAutoOutput = "auto";

XineEngine::XineEngine(TaskManager *task_manager)
    : EngineBase(),
    xine_(nullptr),
    audioport_(nullptr),
    stream_(nullptr),
    eventqueue_(nullptr),
#ifdef XINE_ANALYZER
    post_(nullptr),
    prune_(nullptr),
#endif
    preamp_(1.0),
    have_metadata_(false) {

  Q_UNUSED(task_manager);

  type_ = Engine::Xine;
  ReloadSettings();

}

XineEngine::~XineEngine() {

  Cleanup();

}

bool XineEngine::Init() {

  Cleanup();
  SetEnvironment();

  QMutexLocker locker(&init_mutex_);
  xine_ = xine_new();
  if (!xine_) {
    emit Error("Could not initialize xine.");
    return false;
  }

#ifdef XINE_SAFE_MODE
  xine_engine_set_param(xine_, XINE_ENGINE_PARAM_VERBOSITY, 99);
#endif

  xine_init(xine_);

#if !defined(XINE_SAFE_MODE) && defined(XINE_ANALYZER)
  prune_.reset(new PruneScopeThread(this));
  prune_->start();
#endif

  return true;

}

void XineEngine::SetDevice() {

  if (device_.isValid()) {
    bool valid(false);
    xine_cfg_entry_t entry;
    switch (device_.type()) {
      case QVariant::String:
	if (device_.toString().isEmpty()) break;
	valid = true;
        xine_config_register_string(xine_, "audio.device.alsa_front_device", device_.toString().toUtf8().data(), "", "", 10, nullptr, nullptr);
        break;
      case QVariant::ByteArray:
	valid = true;
	xine_config_register_string(xine_, "audio.device.alsa_front_device", device_.toByteArray().data(), "", "", 10, nullptr, nullptr);
        break;
      default:
        qLog(Error) << "Unknown device type" << device_;
        break;
    }
    if (valid) {
      xine_config_lookup_entry(xine_, "audio.device.alsa_front_device", &entry);
      xine_config_update_entry(xine_, &entry);
    }
  }
  current_device_ = device_;

}

bool XineEngine::OpenAudioDriver() {

  SetDevice();

  if (!ValidOutput(output_)) {
    qLog(Error) << "Invalid output detected:" << output_ << " - Resetting to default.";
    output_ = DefaultOutput();
  }

  audioport_ = xine_open_audio_driver(xine_, (output_.isEmpty() || output_ == kAutoOutput ? nullptr : output_.toUtf8().constData()), nullptr);
  if (!audioport_) {
    emit StateChanged(Engine::Error);
    emit FatalError();
    emit Error("Xine was unable to initialize any audio drivers.");
    return false;
  }

#if !defined(XINE_SAFE_MODE) && defined(XINE_ANALYZER)
  post_ = scope_plugin_new(xine_, audioport_);
  if (!post_) {
    xine_close_audio_driver(xine_, audioport_);
    audioport_ = nullptr;
    emit StateChanged(Engine::Error);
    emit FatalError();
    emit Error("Xine was unable to initialize any audio drivers.");
    return false;
  }
#endif

  return true;

}

void XineEngine::CloseAudioDriver() {

#ifdef XINE_ANALYZER
  if (post_) {
    xine_post_dispose(xine_, post_);
    post_ = nullptr;
  }
#endif

  if (audioport_) {
    xine_close_audio_driver(xine_, audioport_);
    audioport_ = nullptr;
  }

}

bool XineEngine::CreateStream() {

  stream_ = xine_stream_new(xine_, audioport_, nullptr);
  if (!stream_) {
    CloseAudioDriver();
    emit Error("Could not create a new Xine stream.");
    return false;
  }

  if (eventqueue_) xine_event_dispose_queue(eventqueue_);
  eventqueue_ = xine_event_new_queue(stream_);
  xine_event_create_listener_thread(eventqueue_, &XineEngine::XineEventListener, (void*)this);

#ifndef XINE_SAFE_MODE
  xine_set_param(stream_, XINE_PARAM_METRONOM_PREBUFFER, 6000);
  xine_set_param(stream_, XINE_PARAM_IGNORE_VIDEO, 1);
#endif

#ifdef XINE_PARAM_EARLY_FINISHED_EVENT
  // Enable gapless playback
  xine_set_param(stream_, XINE_PARAM_EARLY_FINISHED_EVENT, 1);
  qLog(Debug) << "Gapless playback enabled.";
#endif

  return true;

}

void XineEngine::CloseStream() {

  if (stream_)
    xine_close(stream_);

  if (eventqueue_) {
    xine_event_dispose_queue(eventqueue_);
    eventqueue_ = nullptr;
  }

  if (stream_) {
    xine_dispose(stream_);
    stream_ = nullptr;
  }

}

bool XineEngine::EnsureStream() {

  if (!audioport_) {
    bool result = OpenAudioDriver();
    if (!result) return false;
  }
  if (!stream_) return CreateStream();
  return true;

}

void XineEngine::Cleanup() {

#ifdef XINE_ANALYZER
  // Wait until the prune scope thread is done
  if (prune_) {
    prune_->exit();
    prune_->wait();
  }
  prune_.reset();
#endif

  CloseStream();
  CloseAudioDriver();

  if (xine_) xine_exit(xine_);
  xine_ = nullptr;

  //qLog(Debug) << "xine closed";
  //qLog(Debug) << "Scope statistics:";
  //qLog(Debug) << "Average list size: " << log_buffer_count_ / log_scope_call_count_;
  //qLog(Debug) << "Buffer failure: " << double(log_no_suitable_buffer_*100) / log_scope_call_count_ << "%";

}

Engine::State XineEngine::state() const {

  if (!stream_) return Engine::Empty;

  switch(xine_get_status(stream_)) {
    case XINE_STATUS_PLAY:
      return xine_get_param(stream_, XINE_PARAM_SPEED)  != XINE_SPEED_PAUSE ? Engine::Playing : Engine::Paused;
    case XINE_STATUS_IDLE:
      return Engine::Empty;
    case XINE_STATUS_STOP:
    default:
      return stream_url_.isEmpty() ? Engine::Empty : Engine::Idle;
  }

}

bool XineEngine::Load(const QUrl &stream_url, const QUrl &original_url, const Engine::TrackChangeFlags change, const bool force_stop_at_end, const quint64 beginning_nanosec, const qint64 end_nanosec) {

  if (!EnsureStream()) return false;

  have_metadata_ = false;

  Engine::Base::Load(stream_url, original_url, change, force_stop_at_end, beginning_nanosec, end_nanosec);

  xine_close(stream_);

  int result = xine_open(stream_, stream_url.toString().toUtf8());
  if (result) {

#if !defined(XINE_SAFE_MODE) && defined(XINE_ANALYZER)
    xine_post_out_t *source = xine_get_audio_source(stream_);
    xine_post_in_t *target = (xine_post_in_t*)xine_post_input(post_, const_cast<char*>("audio in"));
    xine_post_wire(source, target);
#endif

    return true;
  }

  DetermineAndShowErrorMessage();
  return false;

}

bool XineEngine::Play(const quint64 offset_nanosec) {

  if (!EnsureStream()) return false;

  int offset = (offset_nanosec / kNsecPerMsec);
  const bool has_audio = xine_get_stream_info(stream_, XINE_STREAM_INFO_HAS_AUDIO);
  const bool audio_handled = xine_get_stream_info(stream_, XINE_STREAM_INFO_AUDIO_HANDLED);
  if (!has_audio || !audio_handled) return false;

  int result = xine_play(stream_, 0, offset);
  if (result) {
    emit StateChanged(Engine::Playing);
    return true;
  }
  xine_close(stream_);

  DetermineAndShowErrorMessage();
  return false;

}

void XineEngine::Stop(const bool stop_after) {

  Q_UNUSED(stop_after);

  if (!stream_) return;

  xine_stop(stream_);
  xine_close(stream_);
  xine_set_param(stream_, XINE_PARAM_AUDIO_CLOSE_DEVICE, 1);

  CloseStream();
  CloseAudioDriver();

  emit StateChanged(Engine::Empty);

}

void XineEngine::Pause() {

  if (!stream_) return;

  int result = xine_get_param(stream_, XINE_PARAM_SPEED);
  if (result != XINE_SPEED_PAUSE) {
    xine_set_param(stream_, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);
    xine_set_param(stream_, XINE_PARAM_AUDIO_CLOSE_DEVICE, 1);
    emit StateChanged(Engine::Paused);
  }

}

void XineEngine::Unpause() {

  if (!stream_) return;

  int result = xine_get_param(stream_, XINE_PARAM_SPEED);
  if (result == XINE_SPEED_PAUSE) {
    xine_set_param(stream_, XINE_PARAM_SPEED, XINE_SPEED_NORMAL);
    emit StateChanged(Engine::Playing);
  }

}

void XineEngine::Seek(const quint64 offset_nanosec) {

  if (!EnsureStream()) return;

  int offset = (offset_nanosec / kNsecPerMsec);

  int result = xine_get_param(stream_, XINE_PARAM_SPEED);
  if (result == XINE_SPEED_PAUSE) {
    xine_play(stream_, 0, offset);
    xine_set_param(stream_, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);
  }
  else xine_play(stream_, 0, offset);

}

void XineEngine::SetVolumeSW(const uint vol) {

  if (!stream_) return;
  if (!volume_control_ && vol != 100) return;
  xine_set_param(stream_, XINE_PARAM_AUDIO_AMP_LEVEL, static_cast<uint>(vol * preamp_));

}

qint64 XineEngine::position_nanosec() const {
  if (state() == Engine::Empty) return 0;
  const qint64 result = (position() * kNsecPerMsec);
  return qint64(qMax(0ll, result));
}

qint64 XineEngine::length_nanosec() const {
  if (state() == Engine::Empty) return 0;
  const qint64 result = end_nanosec_ - beginning_nanosec_;

  if (result > 0) {
    return result;
  }
  else {
    // Get the length from the pipeline if we don't know.
    return (length() * kNsecPerMsec);
  }
}

EngineBase::OutputDetailsList XineEngine::GetOutputsList() const {

  OutputDetailsList ret;

  PluginDetailsList plugins = GetPluginList();
  for (const PluginDetails &plugin : plugins) {
    OutputDetails output;
    output.name = plugin.name;
    output.description = plugin.description;
    if (plugin.name == "auto") output.iconname = "soundcard";
    else if ((plugin.name == "alsa")||(plugin.name == "oss")) output.iconname = "alsa";
    else if (plugin.name== "jack") output.iconname = "jack";
    else if (plugin.name == "pulseaudio") output.iconname = "pulseaudio";
    else if (plugin.name == "bluetooth") output.iconname = "bluetooth";
    else if (plugin.name == "file") output.iconname = "document-new";
    else output.iconname = "soundcard";
    ret.append(output);
  }

  return ret;
}

bool XineEngine::ValidOutput(const QString &output) {

  PluginDetailsList plugins = GetPluginList();
  for (const PluginDetails &plugin : plugins) {
    if (plugin.name == output) return(true);
  }
  return(false);

}

bool XineEngine::CustomDeviceSupport(const QString &output) {
  return (output == "alsa" || output == "oss" || output == "jack" || output == "pulseaudio");
}

bool XineEngine::ALSADeviceSupport(const QString &output) {
  return (output == "alsa");
}

void XineEngine::ReloadSettings() {

  Engine::Base::ReloadSettings();

  if (output_.isEmpty()) output_ = DefaultOutput();

}

void XineEngine::SetEnvironment() {

#ifdef Q_OS_WIN
  putenv(QString("XINE_PLUGIN_PATH=" + QCoreApplication::applicationDirPath() + "/xine-plugins").toLatin1().constData());
#endif

#ifdef Q_OS_MACOS
  setenv("XINE_PLUGIN_PATH", QString(QCoreApplication::applicationDirPath() + "/../PlugIns/xine").toLatin1().constData(), 1);
#endif

}

uint XineEngine::length() const {

  if (!stream_) return 0;

  // Xine often delivers nonsense values for VBR files and such, so we only use the length for remote files

  if (stream_url_.isLocalFile()) return 0;
  else {
    int pos = 0, time = 0, length = 0;

    xine_get_pos_length(stream_, &pos, &time, &length);
    if (length < 0) length=0;

    return length;
  }

}

uint XineEngine::position() const {

  if (state() == Engine::Empty) return 0;

  int pos = 0, time = 0, length = 0;

  // Workaround for problems when you seek too quickly, see BUG 99808
  //int tmp = 0, i = 0;
  //while (++i < 4) {
    xine_get_pos_length(stream_, &pos, &time, &length);
    //if (time > tmp) break;
    //usleep(100000);
  //}

  if (state() != Engine::Idle && state() != Engine::Empty && !have_metadata_ && time > 0) {
    FetchMetaData();
  }

  return time;

}

bool XineEngine::CanDecode(const QUrl &url) {

  static QStringList list;

  if (list.isEmpty()) {

    QMutexLocker locker(&const_cast<XineEngine*>(this)->init_mutex_);

    if (list.isEmpty()) {
      char *exts = xine_get_file_extensions(xine_);
      list = QString(exts).split(' ');
      free(exts);
      exts = nullptr;
      // Images
      list.removeAll("png");
      list.removeAll("jpg");
      list.removeAll("jpeg");
      list.removeAll("gif");
      list.removeAll("ilbm");
      list.removeAll("iff");
      // Subtitles
      list.removeAll("asc");
      list.removeAll("txt");
      list.removeAll("sub");
      list.removeAll("srt");
      list.removeAll("smi");
      list.removeAll("ssa");
      // HACK: we also check for m4a because xine plays them but for some reason doesn't return the extension
      if (!list.contains("m4a"))
        list << "m4a";
    }
  }

  if (url.scheme() == "cdda") return true;

  QString path = url.path();

  // Partial downloads from Konqi and other browsers tend to have a .part extension
  if (path.endsWith(".part")) path = path.left(path.length() - 5);

  const QString ext = path.mid(path.lastIndexOf('.') + 1).toLower();

  return list.contains(ext);

}

void XineEngine::SetEqualizerEnabled(const bool enabled) {

  if (!stream_) return;

  equalizer_enabled_ = enabled;

  if (!enabled) {
    QList<int> gains;
    for (uint x = 0; x < 10; x++)
      gains << -101; // sets eq gains to zero.

    SetEqualizerParameters(0, gains);
  }

}

/*
  Sets the eq params for xine engine - have to rescale eq params to fitting range (adapted from kaffeine and xfmedia)

  preamp:
    pre: (-100..100)
    post: (0.1..1.9) - this is not really a preamp but we use the xine preamp parameter for our normal volume. so we make a postamp.

  gains:
    pre: (-100..100)
    post: (1..200) - (1 = down, 100 = middle, 200 = up, 0 = off)
*/
void XineEngine::SetEqualizerParameters(const int preamp, const QList<int> &gains) {

  if (!stream_) return;

  equalizer_gains_ = gains;
  int_preamp_ = preamp;
  QList<int>::ConstIterator it = gains.begin();

  xine_set_param(stream_, XINE_PARAM_EQ_30HZ,    int((*it)*0.995 + 100));
  xine_set_param(stream_, XINE_PARAM_EQ_60HZ,    int((*++it)*0.995 + 100));
  xine_set_param(stream_, XINE_PARAM_EQ_125HZ,   int((*++it)*0.995 + 100));
  xine_set_param(stream_, XINE_PARAM_EQ_250HZ,   int((*++it)*0.995 + 100));
  xine_set_param(stream_, XINE_PARAM_EQ_500HZ,   int((*++it)*0.995 + 100));
  xine_set_param(stream_, XINE_PARAM_EQ_1000HZ,  int((*++it)*0.995 + 100));
  xine_set_param(stream_, XINE_PARAM_EQ_2000HZ,  int((*++it)*0.995 + 100));
  xine_set_param(stream_, XINE_PARAM_EQ_4000HZ,  int((*++it)*0.995 + 100));
  xine_set_param(stream_, XINE_PARAM_EQ_8000HZ,  int((*++it)*0.995 + 100));
  xine_set_param(stream_, XINE_PARAM_EQ_16000HZ, int((*++it)*0.995 + 100));

  preamp_ = (preamp - 0.1 * preamp + 100) / 100.0;
  SetVolume(volume_);

}

void XineEngine::XineEventListener(void *p, const xine_event_t *event) {

  if (!p) return;
  XineEngine *engine = reinterpret_cast<XineEngine*>(p);

  switch(event->type) {
    case XINE_EVENT_UI_SET_TITLE:
      qLog(Debug) << "XINE_EVENT_UI_SET_TITLE";
      engine->FetchMetaData();
      break;

    case XINE_EVENT_UI_PLAYBACK_FINISHED:
      qLog(Debug) << "XINE_EVENT_UI_PLAYBACK_FINISHED";
      emit engine->TrackEnded();
      break;

    case XINE_EVENT_PROGRESS:
      {
        xine_progress_data_t *pd = (xine_progress_data_t*)event->data;
        QString msg = QString("%1 %2%").arg(QString::fromUtf8(pd->description)).arg(QString::number(pd->percent) + QLocale::system().percent());
        //qLog(Debug) << "Xine:" << msg;
      }
      break;

    case XINE_EVENT_MRL_REFERENCE_EXT:
      {
        // Xine has read the stream and found it actually links to something else so we need to play that instead
        QString message = QString::fromUtf8(static_cast<xine_mrl_reference_data_ext_t*>(event->data)->mrl);
        //emit StatusText(QString("Redirecting to: ").arg(*message));
        engine->Load(QUrl(message), engine->original_url_, Engine::Auto, false, 0, 0);
        engine->Play(0);
      }
      break;

    case XINE_EVENT_UI_MESSAGE:
    {
      qLog(Debug) << "XINE_EVENT_UI_MESSAGE";

      xine_ui_message_data_t *data = (xine_ui_message_data_t *)event->data;
      QString message;

      switch (data->type) {

        case XINE_MSG_NO_ERROR:
          {
            // Series of \0 separated strings, terminated with a \0\0
            char str[2000];
            char *p = str;
            for (char *msg = data->messages; !(*msg == '\0' && *(msg+1) == '\0'); ++msg, ++p)
              *p = *msg == '\0' ? '\n' : *msg;
            *p = '\0';
            qLog(Debug) << "Xine:" << str;
            break;
          }

        case XINE_MSG_ENCRYPTED_SOURCE:
          message = "Source is encrypted.";
          if (data->explanation) {
            message += " : ";
            message += QString::fromUtf8((char*)data + data->parameters);
          }
          emit engine->StateChanged(Engine::Error);
          emit engine->InvalidSongRequested(engine->stream_url_);
          break;
        case XINE_MSG_UNKNOWN_HOST:
          message = "The host is unknown.";
          if (data->explanation) {
            message += " : ";
            message += QString::fromUtf8((char*)data + data->parameters);
          }
          emit engine->StateChanged(Engine::Error);
          emit engine->InvalidSongRequested(engine->stream_url_);
          break;
        case XINE_MSG_UNKNOWN_DEVICE:
          message = "The device name you specified seems invalid.";
          if (data->explanation) {
            message += " : ";
            message += QString::fromUtf8((char*)data + data->parameters);
          }
          emit engine->StateChanged(Engine::Error);
          emit engine->InvalidSongRequested(engine->stream_url_);
          break;
        case XINE_MSG_NETWORK_UNREACHABLE:
          message = "The network appears unreachable.";
          if (data->explanation) {
            message += " : ";
            message += QString::fromUtf8((char*)data + data->parameters);
          }
          emit engine->StateChanged(Engine::Error);
          emit engine->InvalidSongRequested(engine->stream_url_);
          break;
        case XINE_MSG_AUDIO_OUT_UNAVAILABLE:
          message = "Audio output unavailable; the device is busy.";
          if (data->explanation) {
            message += " : ";
            message += QString::fromUtf8((char*)data + data->parameters);
          }
          emit engine->StateChanged(Engine::Error);
          emit engine->FatalError();
          break;
        case XINE_MSG_CONNECTION_REFUSED:
          message = "Connection refused.";
          if (data->explanation) {
            message += " : ";
            message += QString::fromUtf8((char*)data + data->parameters);
          }
          emit engine->StateChanged(Engine::Error);
          emit engine->InvalidSongRequested(engine->stream_url_);
          break;
        case XINE_MSG_FILE_NOT_FOUND:
          message = "File not found.";
          if (data->explanation) {
            message += " : ";
            message += QString::fromUtf8((char*)data + data->parameters);
          }
          emit engine->StateChanged(Engine::Error);
          emit engine->InvalidSongRequested(engine->stream_url_);
          break;
        case XINE_MSG_PERMISSION_ERROR:
          message = "Access denied.";
          if (data->explanation) {
            message += " : ";
            message += QString::fromUtf8((char*)data + data->parameters);
          }
          emit engine->StateChanged(Engine::Error);
          emit engine->InvalidSongRequested(engine->stream_url_);
          break;
        case XINE_MSG_READ_ERROR:
          message = "Read error.";
          if (data->explanation) {
            message += " : ";
            message += QString::fromUtf8((char*)data + data->parameters);
          }
          emit engine->StateChanged(Engine::Error);
          emit engine->InvalidSongRequested(engine->stream_url_);
          break;
        case XINE_MSG_LIBRARY_LOAD_ERROR:
          message = "A problem occurred while loading a library or decoder.";
          if (data->explanation) {
            message += " : ";
            message += QString::fromUtf8((char*)data + data->parameters);
          }
          emit engine->StateChanged(Engine::Error);
          emit engine->FatalError();
          break;
        case XINE_MSG_GENERAL_WARNING:
          message = "General Warning";
          if (data->explanation) {
            message += ": ";
            message += QString::fromUtf8((char*)data + data->explanation);
          }
          else message += ".";
          break;
        case XINE_MSG_SECURITY:
          message = "Security Warning";
          if (data->explanation) {
            message += ": ";
            message += QString::fromUtf8((char*)data + data->explanation);
          }
          else message += ".";
          break;
        default:
          message = "Unknown Error";
          if (data->explanation) {
            message += ": ";
            message += QString::fromUtf8((char*)data + data->explanation);
          }
          else message += ".";
          break;
      }
      emit engine->Error(message);
    }
  }

}

Engine::SimpleMetaBundle XineEngine::FetchMetaData() const {

  Engine::SimpleMetaBundle bundle;
  bundle.url        = original_url_;
  bundle.title      = QString::fromUtf8(xine_get_meta_info(stream_, XINE_META_INFO_TITLE));
  bundle.artist     = QString::fromUtf8(xine_get_meta_info(stream_, XINE_META_INFO_ARTIST));
  bundle.album      = QString::fromUtf8(xine_get_meta_info(stream_, XINE_META_INFO_ALBUM));
  bundle.comment    = QString::fromUtf8(xine_get_meta_info(stream_, XINE_META_INFO_COMMENT));
  bundle.genre      = QString::fromUtf8(xine_get_meta_info(stream_, XINE_META_INFO_GENRE));
  bundle.length     = 0;

  bundle.year       = QString::fromUtf8(xine_get_meta_info(stream_, XINE_META_INFO_YEAR)).toInt();
  bundle.track      = QString::fromUtf8(xine_get_meta_info(stream_, XINE_META_INFO_TRACK_NUMBER)).toInt();

  bundle.samplerate = xine_get_stream_info(stream_, XINE_STREAM_INFO_AUDIO_SAMPLERATE);
  bundle.bitdepth   = xine_get_stream_info(stream_, XINE_STREAM_INFO_AUDIO_BITS);
  bundle.bitrate    = xine_get_stream_info(stream_, XINE_STREAM_INFO_AUDIO_BITRATE) / 1000;

  qLog(Debug) << "Metadata received" << bundle.title
                                     << bundle.artist
                                     << bundle.album
                                     << bundle.comment
                                     << bundle.genre
                                     << bundle.length
                                     << bundle.year
                                     << bundle.track
                                     << bundle.samplerate
                                     << bundle.bitdepth
                                     << bundle.bitrate;

  current_bundle_ = bundle;
  XineEngine *engine = const_cast<XineEngine*>(this);
  engine->have_metadata_ = true;
  emit engine->MetaData(bundle);

  return bundle;

}

void XineEngine::DetermineAndShowErrorMessage() {

  int errno;
  QString message;

  errno = xine_get_error(stream_);
  switch (errno) {

    case XINE_ERROR_NO_INPUT_PLUGIN:
      message = "No suitable input plugin. This often means that the url's protocol is not supported. Network failures are other possible causes.";
      break;

    case XINE_ERROR_NO_DEMUX_PLUGIN:
      message = "No suitable demux plugin. This often means that the file format is not supported.";
      break;

    case XINE_ERROR_DEMUX_FAILED:
      message = "Demuxing failed.";
      break;

    case XINE_ERROR_INPUT_FAILED:
      message = "Could not open file.";
      break;

    case XINE_ERROR_MALFORMED_MRL:
      message = "The location is malformed.";
      break;

    case XINE_ERROR_NONE:
      // Xine is thick. Xine doesn't think there is an error but there may be! We check for other errors below.
    default:
      emit FatalError();
      int result = xine_get_stream_info(stream_, XINE_STREAM_INFO_AUDIO_HANDLED);
      if (!result) {
        // xine can read the plugin but it didn't find any codec
        // THUS xine=daft for telling us it could handle the format in canDecode!
        message = "There is no available decoder.";
        QString const ext = QFileInfo(stream_url_.path()).completeSuffix();
        break;
      }
      result = xine_get_stream_info(stream_, XINE_STREAM_INFO_HAS_AUDIO);
      if (!result) {
        message = "There is no audio channel!";
        break;
      }
      break;
  }

  emit Error(message);

}

#ifdef XINE_ANALYZER

const Engine::Scope &XineEngine::scope(const int chunk_length) {

  Q_UNUSED(chunk_length);

  if (!post_ || !stream_ || xine_get_status(stream_) != XINE_STATUS_PLAY)
    return scope_;

  MyNode *const myList = scope_plugin_list(post_);
  metronom_t *const myMetronom = scope_plugin_metronom(post_);
  const int myChannels = scope_plugin_channels(post_);
  int scopeidx = 0;

  if (myChannels > 2) return scope_;

  for (int n, frame = 0; frame < 512;) {

    MyNode *best_node = 0;

    for (MyNode *node = myList->next; node != myList; node = node->next, log_buffer_count_++)
      if (node->vpts <= current_vpts_ && (!best_node || node->vpts > best_node->vpts))
        best_node = node;

    if (!best_node || best_node->vpts_end < current_vpts_) {
      log_no_suitable_buffer_++;
      break;
    }

    int64_t diff = current_vpts_;
    diff -= best_node->vpts;
    diff *= 1<<16;
    diff /= myMetronom->pts_per_smpls;

    const int16_t *data16  = best_node->mem;
    data16 += diff;

    diff += diff % myChannels;  // Important correction to ensure we don't overflow the buffer
    diff /= myChannels;         // Use units of frames, not samples

    // Calculate the number of available samples in this buffer
    n  = best_node->num_frames;
    n -= diff;
    n += frame; //clipping for # of frames we need

    if (n > 512)
      n = 512; // We don't want more than 512 frames

    for (int c; frame < n; ++frame, data16 += myChannels) {
      for (c = 0; c < myChannels; ++c) {
        // We now give interleaved pcm to the scope
        scope_[scopeidx++] = data16[c];
        if (myChannels == 1) // Duplicate mono samples
          scope_[scopeidx++] = data16[c];
      }
    }

    current_vpts_ = best_node->vpts_end;
    current_vpts_++; // FIXME: Needs to be done for some reason, or you get situations where it uses same buffer again and again
  }

  log_scope_call_count_++;

  return scope_;

}

void XineEngine::PruneScope() {

  if (!stream_) return;

  // Here we prune the buffer list regularly

  MyNode *myList = scope_plugin_list(post_);

  if (!myList) return;

  // We operate on a subset of the list for thread-safety
  MyNode * const first_node = myList->next;
  MyNode const * const list_end = myList;

  current_vpts_ = (xine_get_status(stream_) == XINE_STATUS_PLAY) ? xine_get_current_vpts(stream_)
    : LLONG_MAX; //if state is not playing OR paused, empty the list
  //: std::numeric_limits<int64_t>::max(); //TODO Don't support crappy gcc 2.95

  for (MyNode *prev = first_node, *node = first_node->next; node != list_end; node = node->next) {
    //we never delete first_node this maintains thread-safety
    if (node->vpts_end < current_vpts_) {
      prev->next = node->next;

      free(node->mem);
      free(node);

      node = prev;
    }

    prev = node;
  }
}

PruneScopeThread::PruneScopeThread(XineEngine *parent) : engine_(parent) {}

void PruneScopeThread::run() {

  QTimer timer;
  connect(&timer, SIGNAL(timeout()), engine_, SLOT(PruneScope()), Qt::DirectConnection);
  timer.start(1000);

  exec();

}

#endif

EngineBase::PluginDetailsList XineEngine::GetPluginList() const {

  PluginDetailsList ret;
  const char *const *plugins = xine_list_audio_output_plugins(xine_);

  {
    PluginDetails details;
    details.name = "auto";
    details.description = "Automatically detected";
    ret << details;
  }

  for (int i =0 ; plugins[i] ; ++i) {
    PluginDetails details;
    details.name = QString::fromUtf8(plugins[i]);
    if (details.name == "alsa") details.description = "ALSA audio output";
    else if (details.name == "oss") details.description = "OSS audio output";
    else if (details.name == "pulseaudio") details.description = "PulseAudio audio output";
    else if (details.name == "file") details.description = "File audio output";
    else if (details.name == "none") details.description = "None";
    else details.description = QString::fromUtf8(plugins[i]);
    ret << details;
  }

  return ret;

}


