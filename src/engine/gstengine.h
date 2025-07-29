/***************************************************************************
 *   Copyright (C) 2003-2005 by Mark Kretschmann <markey@web.de>           *
 *   Copyright (C) 2005 by Jakub Stachowski <qbast@go2.pl>                 *
 *   Copyright (C) 2006 Paul Cifarelli <paul@cifarelli.net>                *
 *   Copyright (C) 2017-2024 Jonas Kvinge <jonas@jkvinge.net>              *
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
#include <QMap>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "enginebase.h"
#include "gstenginepipeline.h"
#include "gstbufferconsumer.h"

class QTimer;
class QTimerEvent;
class TaskManager;

class GstEngine : public EngineBase, public GstBufferConsumer {
  Q_OBJECT

 public:
  explicit GstEngine(SharedPtr<TaskManager> task_manager, QObject *parent = nullptr);
  ~GstEngine() override;

  static const char *kAutoSink;
  static const char *kALSASink;

  bool Init() override;
  State state() const override;
  void StartPreloading(const QUrl &media_url, const QUrl &stream_url, const bool force_stop_at_end, const qint64 beginning_offset_nanosec, const qint64 end_offset_nanosec) override;
  bool Load(const QUrl &media_url, const QUrl &stream_url, const EngineBase::TrackChangeFlags change, const bool force_stop_at_end, const quint64 beginning_offset_nanosec, const qint64 end_offset_nanosec, const std::optional<double> ebur128_integrated_loudness_lufs) override;
  bool Play(const bool pause, const quint64 offset_nanosec) override;
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
  QString DefaultOutput() const override { return QLatin1String(kAutoSink); }
  bool CustomDeviceSupport(const QString &output) const override;
  bool ALSADeviceSupport(const QString &output) const override;
  bool ExclusiveModeSupport(const QString &output) const override;

  void ConsumeBuffer(GstBuffer *buffer, const int pipeline_id, const QString &format) override;

 public Q_SLOTS:
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

 private Q_SLOTS:
  void EndOfStreamReached(const int pipeline_id, const bool has_next_track);
  void HandlePipelineError(const int pipeline_id, const int domain, const int error_code, const QString &message, const QString &debugstr);
  void NewMetaData(const int pipeline_id, const EngineMetadata &engine_metadata);
  void AddBufferToScope(GstBuffer *buf, const int pipeline_id, const QString &format);
  void FadeoutFinished(const int pipeline_id);
  void FadeoutPauseFinished();
  void SeekNow();
  void PlayDone(const GstStateChangeReturn ret, const bool pause, const quint64 offset_nanosec, const int pipeline_id);

  void BufferingStarted();
  void BufferingProgress(int percent);
  void BufferingFinished();

  void PipelineFinished(const int pipeline_id);

 private:
  QByteArray FixupUrl(const QUrl &url);

  void StartFadeout(GstEnginePipelinePtr pipeline);
  void StartFadeoutPause();
  void StopFadeoutPause();

  void StartTimers();
  void StopTimers();

  GstEnginePipelinePtr CreatePipeline();
  GstEnginePipelinePtr CreatePipeline(const QUrl &media_url, const QUrl &stream_url, const QByteArray &gst_url, const qint64 beginning_offset_nanosec, const qint64 end_offset_nanosec, const double ebur128_loudness_normalizing_gain_db);

  void FinishPipeline(GstEnginePipelinePtr pipeline);

  void UpdateScope(int chunk_length);

  static void StreamDiscovered(GstDiscoverer *discoverer, GstDiscovererInfo *info, GError *error, gpointer self);
  static void StreamDiscoveryFinished(GstDiscoverer *discoverer, gpointer self);
  static QString GSTdiscovererErrorMessage(GstDiscovererResult result);

  bool OldExclusivePipelineActive() const;
  bool AnyExclusivePipelineActive() const;

#ifdef HAVE_SPOTIFY
  void SetSpotifyAccessToken() override;
#endif

 private:
  SharedPtr<TaskManager> task_manager_;
  GstDiscoverer *discoverer_;

  int buffering_task_id_;

  GstEnginePipelinePtr current_pipeline_;
  QMap<int, GstEnginePipelinePtr> fadeout_pipelines_;
  GstEnginePipelinePtr fadeout_pause_pipeline_;
  QMap<int, GstEnginePipelinePtr> old_pipelines_;

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

  bool has_faded_out_to_pause_;

  int scope_chunk_;
  bool have_new_buffer_;
  int scope_chunks_;
  QString buffer_format_;

  int discovery_finished_cb_id_;
  int discovery_discovered_cb_id_;

  State delayed_state_;
  bool delayed_state_pause_;
  quint64 delayed_state_offset_nanosec_;
};

#endif  // GSTENGINE_H
