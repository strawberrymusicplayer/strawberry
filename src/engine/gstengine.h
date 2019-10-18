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

#ifndef GSTENGINE_H
#define GSTENGINE_H

#include "config.h"

#include <memory>
#include <stdbool.h>

#include <gst/gst.h>

#include <QtGlobal>
#include <QObject>
#include <QFuture>
#include <QByteArray>
#include <QList>
#include <QVariant>
#include <QString>
#include <QUrl>
#include <QTimer>
#include <QTimerEvent>

#include "core/timeconstants.h"
#include "engine_fwd.h"
#include "enginebase.h"
#include "gststartup.h"
#include "gstbufferconsumer.h"

class TaskManager;
class GstEnginePipeline;

#ifdef Q_OS_MACOS
struct _GTlsDatabase;
typedef struct _GTlsDatabase GTlsDatabase;
#endif

/**
 * @class GstEngine
 * @short GStreamer engine plugin
 * @author Mark Kretschmann <markey@web.de>
 */
class GstEngine : public Engine::Base, public GstBufferConsumer {
  Q_OBJECT

 public:
  GstEngine(TaskManager *task_manager);
  ~GstEngine();

  bool Init();
  Engine::State state() const;
  void StartPreloading(const QUrl &stream_url, const QUrl &original_url, const bool force_stop_at_end, const qint64 beginning_nanosec, const qint64 end_nanosec);
  bool Load(const QUrl &stream_url, const QUrl &original_url, const Engine::TrackChangeFlags change, const bool force_stop_at_end, const quint64 beginning_nanosec, const qint64 end_nanosec);
  bool Play(const quint64 offset_nanosec);
  void Stop(const bool stop_after = false);
  void Pause();
  void Unpause();
  void Seek(const quint64 offset_nanosec);

 protected:
  void SetVolumeSW(const uint percent);

 public:
  qint64 position_nanosec() const;
  qint64 length_nanosec() const;
  const Engine::Scope &scope(const int chunk_length);

  OutputDetailsList GetOutputsList() const;
  bool ValidOutput(const QString &output);
  QString DefaultOutput() { return kAutoSink; }
  bool CustomDeviceSupport(const QString &output);
  bool ALSADeviceSupport(const QString &output);

  void SetStartup(GstStartup *gst_startup) { gst_startup_ = gst_startup; }
  void EnsureInitialised() { gst_startup_->EnsureInitialised(); }

  GstElement *CreateElement(const QString &factoryName, GstElement *bin = nullptr, const bool showerror = true);
  void ConsumeBuffer(GstBuffer *buffer, const int pipeline_id, const QString &format);

 public slots:

  void ReloadSettings();

  /** Set whether equalizer is enabled */
  void SetEqualizerEnabled(const bool);

  /** Set equalizer preamp and gains, range -100..100. Gains are 10 values. */
  void SetEqualizerParameters(const int preamp, const QList<int> &bandGains);

  /** Set Stereo balance, range -1.0f..1.0f */
  void SetStereoBalance(const float value);

  void AddBufferConsumer(GstBufferConsumer *consumer);
  void RemoveBufferConsumer(GstBufferConsumer *consumer);

#ifdef Q_OS_MACOS
  GTlsDatabase *tls_database() const { return tls_database_; }
#endif

 protected:
  void timerEvent(QTimerEvent*);

 private slots:
  void EndOfStreamReached(const int pipeline_id, const bool has_next_track);
  void HandlePipelineError(const int pipeline_id, const QString &message, const int domain, const int error_code);
  void NewMetaData(const int pipeline_id, const Engine::SimpleMetaBundle &bundle);
  void AddBufferToScope(GstBuffer *buf, const int pipeline_id, const QString &format);
  void FadeoutFinished();
  void FadeoutPauseFinished();
  void SeekNow();
  void PlayDone(QFuture<GstStateChangeReturn> future, const quint64, const int);

  void BufferingStarted();
  void BufferingProgress(int percent);
  void BufferingFinished();

 private:
  static const char *kAutoSink;
  static const char *kALSASink;
  static const char *kOpenALSASink;
  static const char *kOSSSink;
  static const char *kOSS4Sink;
  static const char *kJackAudioSink;
  static const char *kPulseSink;
  static const char *kA2DPSink;
  static const char *kAVDTPSink;
  static const char *InterAudiosink;
  static const char *kDirectSoundSink;
  static const char *kOSXAudioSink;

  PluginDetailsList GetPluginList(const QString &classname) const;
  QByteArray FixupUrl(const QUrl &url);

  void StartFadeout();
  void StartFadeoutPause();

  void StartTimers();
  void StopTimers();

  std::shared_ptr<GstEnginePipeline> CreatePipeline();
  std::shared_ptr<GstEnginePipeline> CreatePipeline(const QByteArray &gst_url, const QUrl &original_url, const qint64 end_nanosec);

  void UpdateScope(int chunk_length);

 private:
  static const qint64 kTimerIntervalNanosec = 1000 * kNsecPerMsec;  // 1s
  static const qint64 kPreloadGapNanosec = 5000 * kNsecPerMsec;     // 5s
  static const qint64 kSeekDelayNanosec = 100 * kNsecPerMsec;       // 100msec

  TaskManager *task_manager_;
  GstStartup *gst_startup_;
  int buffering_task_id_;

  std::shared_ptr<GstEnginePipeline> current_pipeline_;
  std::shared_ptr<GstEnginePipeline> fadeout_pipeline_;
  std::shared_ptr<GstEnginePipeline> fadeout_pause_pipeline_;
  QUrl preloaded_url_;

  QList<GstBufferConsumer*> buffer_consumers_;

  GstBuffer *latest_buffer_;

  int equalizer_preamp_;
  QList<int> equalizer_gains_;
  float stereo_balance_;

  mutable bool can_decode_success_;
  mutable bool can_decode_last_;

  // Hack to stop seeks happening too often
  QTimer *seek_timer_;
  bool waiting_to_seek_;
  quint64 seek_pos_;

  int timer_id_;
  int next_element_id_;

  bool is_fading_out_to_pause_;
  bool has_faded_out_;

  int scope_chunk_;
  bool have_new_buffer_;
  int scope_chunks_;
  QString buffer_format_;

#ifdef Q_OS_MACOS
  GTlsDatabase* tls_database_;
#endif

};

#endif  /* GSTENGINE_H */
