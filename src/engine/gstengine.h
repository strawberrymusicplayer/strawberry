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

#ifndef GSTENGINE_H
#define GSTENGINE_H

#include "config.h"

#include <optional>

#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

#include <QtGlobal>
#include <QObject>
#include <QFuture>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>

#include "core/shared_ptr.h"
#include "enginebase.h"
#include "gststartup.h"
#include "gstbufferconsumer.h"

class QTimer;
class QTimerEvent;
class TaskManager;
class GstEnginePipeline;

class GstEngine : public EngineBase, public GstBufferConsumer {
  Q_OBJECT

 public:
  explicit GstEngine(SharedPtr<TaskManager> task_manager, QObject *parent = nullptr);
  ~GstEngine() override;

  static const char *kAutoSink;

  Type type() const override { return Type::GStreamer; }
  bool Init() override;
  EngineBase::State state() const override;
  void StartPreloading(const QUrl &media_url, const QUrl &stream_url, const bool force_stop_at_end, const qint64 beginning_nanosec, const qint64 end_nanosec) override;
  bool Load(const QUrl &media_url, const QUrl &stream_url, const EngineBase::TrackChangeFlags change, const bool force_stop_at_end, const quint64 beginning_nanosec, const qint64 end_nanosec, const std::optional<double> ebur128_integrated_loudness_lufs) override;
  bool Play(const quint64 offset_nanosec) override;
  void Stop(const bool stop_after = false) override;
  void Pause() override;
  void Unpause() override;
  void Seek(const quint64 offset_nanosec) override;

 protected:
  void SetVolumeSW(const uint volume) override;

 public:
  qint64 position_nanosec() const override;
  qint64 length_nanosec() const override;
  const EngineBase::Scope &scope(const int chunk_length) override;

  OutputDetailsList GetOutputsList() const override;
  bool ValidOutput(const QString &output) override;
  QString DefaultOutput() override { return kAutoSink; }
  bool CustomDeviceSupport(const QString &output) override;
  bool ALSADeviceSupport(const QString &output) override;

  void SetStartup(GstStartup *gst_startup) { gst_startup_ = gst_startup; }
  void EnsureInitialized() { gst_startup_->EnsureInitialized(); }

  void ConsumeBuffer(GstBuffer *buffer, const int pipeline_id, const QString &format) override;

 public slots:
  void ReloadSettings() override;

  // Set whether stereo balancer is enabled
  void SetStereoBalancerEnabled(const bool enabled) override;

  // Set Stereo balance, range -1.0f..1.0f
  void SetStereoBalance(const float value) override;

  // Set whether equalizer is enabled
  void SetEqualizerEnabled(const bool) override;

  // Set equalizer preamp and gains, range -100..100. Gains are 10 values.
  void SetEqualizerParameters(const int preamp, const QList<int> &band_gains) override;

  void AddBufferConsumer(GstBufferConsumer *consumer);
  void RemoveBufferConsumer(GstBufferConsumer *consumer);

 protected:
  void timerEvent(QTimerEvent *e) override;

 private slots:
  void EndOfStreamReached(const int pipeline_id, const bool has_next_track);
  void HandlePipelineError(const int pipeline_id, const int domain, const int error_code, const QString &message, const QString &debugstr);
  void NewMetaData(const int pipeline_id, const EngineMetadata &engine_metadata);
  void AddBufferToScope(GstBuffer *buf, const int pipeline_id, const QString &format);
  void FadeoutFinished();
  void FadeoutPauseFinished();
  void SeekNow();
  void PlayDone(const GstStateChangeReturn ret, const quint64, const int);

  void BufferingStarted();
  void BufferingProgress(int percent);
  void BufferingFinished();

 private:
  QByteArray FixupUrl(const QUrl &url);

  void StartFadeout();
  void StartFadeoutPause();

  void StartTimers();
  void StopTimers();

  SharedPtr<GstEnginePipeline> CreatePipeline();
  SharedPtr<GstEnginePipeline> CreatePipeline(const QUrl &media_url, const QUrl &stream_url, const QByteArray &gst_url, const qint64 end_nanosec, const double ebur128_loudness_normalizing_gain_db);

  void UpdateScope(int chunk_length);

  static void StreamDiscovered(GstDiscoverer*, GstDiscovererInfo *info, GError*, gpointer self);
  static void StreamDiscoveryFinished(GstDiscoverer*, gpointer);
  static QString GSTdiscovererErrorMessage(GstDiscovererResult result);

 private:
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
  static const int kDiscoveryTimeoutS;
  static const qint64 kTimerIntervalNanosec;
  static const qint64 kPreloadGapNanosec;
  static const qint64 kSeekDelayNanosec;

  SharedPtr<TaskManager> task_manager_;
  GstStartup *gst_startup_;
  GstDiscoverer *discoverer_;

  int buffering_task_id_;

  SharedPtr<GstEnginePipeline> current_pipeline_;
  SharedPtr<GstEnginePipeline> fadeout_pipeline_;
  SharedPtr<GstEnginePipeline> fadeout_pause_pipeline_;

  QList<GstBufferConsumer*> buffer_consumers_;

  GstBuffer *latest_buffer_;

  bool stereo_balancer_enabled_;
  float stereo_balance_;

  bool equalizer_enabled_;
  int equalizer_preamp_;
  QList<int> equalizer_gains_;

  // Hack to stop seeks happening too often
  QTimer *seek_timer_;
  bool waiting_to_seek_;
  quint64 seek_pos_;

  int timer_id_;

  bool is_fading_out_to_pause_;
  bool has_faded_out_;

  int scope_chunk_;
  bool have_new_buffer_;
  int scope_chunks_;
  QString buffer_format_;

  int discovery_finished_cb_id_;
  int discovery_discovered_cb_id_;
};

#endif  // GSTENGINE_H
