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

#ifndef XINEENGINE_H
#define XINEENGINE_H

#include "config.h"

#ifndef XINE_ENGINE_INTERNAL
#  define XINE_ENGINE_INTERNAL
#endif

#include <memory>
#include <cstdint>
#include <sys/types.h>
#include <xine.h>

#include <QtGlobal>
#include <QObject>
#include <QMutex>
#include <QThread>
#include <QList>
#include <QVariant>
#include <QString>
#include <QUrl>

#include "engine_fwd.h"
#include "enginebase.h"

class TaskManager;
class PruneScopeThread;

class XineEngine : public Engine::Base {
    Q_OBJECT

 public:
  explicit XineEngine(TaskManager *task_manager);
  ~XineEngine() override;

  bool Init() override;
  Engine::State state() const override;
  bool Load(const QUrl &stream_url, const QUrl &original_url, Engine::TrackChangeFlags change, bool force_stop_at_end, quint64 beginning_nanosec, qint64 end_nanosec) override;
  bool Play(quint64 offset_nanosec) override;
  void Stop(bool stop_after = false) override;
  void Pause() override;
  void Unpause() override;
  void Seek(quint64 offset_nanosec) override;
  void SetVolumeSW(uint) override;

  qint64 position_nanosec() const override;
  qint64 length_nanosec() const override;

#ifdef XINE_ANALYZER
  const Engine::Scope& scope(int chunk_length);
#endif

  OutputDetailsList GetOutputsList() const override;
  bool ValidOutput(const QString &output) override;
  QString DefaultOutput() override { return "auto"; }
  bool CustomDeviceSupport(const QString &output) override;
  bool ALSADeviceSupport(const QString &output) override;

  void ReloadSettings() override;

  bool CanDecode(const QUrl &);

  void SetEqualizerEnabled(bool enabled) override;
  void SetEqualizerParameters(int preamp, const QList<int>&) override;

  // Simple accessors

  xine_stream_t *stream() { return stream_; }
  float preamp() { return preamp_; }

 private:
  static const char *kAutoOutput;

  QString current_output_;
  QVariant current_device_;

  xine_t *xine_;
  xine_audio_port_t *audioport_;
  xine_stream_t *stream_;
  xine_event_queue_t *eventqueue_;
#ifdef XINE_ANALYZER
  xine_post_t *post_;
  std::unique_ptr<PruneScopeThread> prune_;
#endif
  float preamp_;

  QUrl stream_url_;
  QUrl original_url_;
  bool have_metadata_;

  uint log_buffer_count_ = 0;
  uint log_scope_call_count_ = 1; // Prevent divideByZero
  uint log_no_suitable_buffer_ = 0;

  int int_preamp_;
  QMutex init_mutex_;
  int64_t current_vpts_;
  QList<int> equalizer_gains_;

  mutable Engine::SimpleMetaBundle current_bundle_;

  void SetEnvironment();

  void Cleanup();
  void SetDevice();
  bool OpenAudioDriver();
  void CloseAudioDriver();
  bool CreateStream();
  void CloseStream();
  bool EnsureStream();

  uint length() const;
  uint position() const;

  static void XineEventListener(void*, const xine_event_t*);

  void DetermineAndShowErrorMessage();
  Engine::SimpleMetaBundle FetchMetaData() const;

  PluginDetailsList GetPluginList() const;

#ifdef XINE_ANALYZER
 private slots:
  void PruneScope();
#endif

signals:
  void InfoMessage(const QString&);
};

#ifdef XINE_ANALYZER
class PruneScopeThread : public QThread {
public:
  PruneScopeThread(XineEngine *parent);

protected:
  void run();

private:
  XineEngine *engine_;

};
#endif

#endif
