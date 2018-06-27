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

#include <memory>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <xine.h>

#include <QtGlobal>
#include <QObject>
#include <QMutex>
#include <QThread>
#include <QEvent>
#include <QList>
#include <QString>
#include <QUrl>

#include "engine_fwd.h"
#include "enginebase.h"

using std::shared_ptr;

class TaskManager;
class PruneScopeThread;
class XineFader;
class XineOutFader;

class XineEvent : public QEvent {
public:
  enum EventType {
    PlaybackFinished,
    InfoMessage,
    StatusMessage,
    MetaInfoChanged,
    Redirecting,
    LastFMTrackChanged,
  };

  XineEvent(EventType type, void* data = nullptr) : QEvent(QEvent::Type(type)), data_(data) {}

  void setData(void *data) { data_ = data; }
  void *data() const { return data_; }

private:
  void *data_;
};

class XineEngine : public Engine::Base {
    Q_OBJECT
    
public:

  XineEngine(TaskManager *task_manager);
  ~XineEngine();

  bool Init();
  Engine::State state() const;
  bool Load(const QUrl &url, Engine::TrackChangeFlags change, bool force_stop_at_end, quint64 beginning_nanosec, qint64 end_nanosec);
  bool Play(quint64 offset_nanosec);
  void Stop(bool stop_after = false);
  void Pause();
  void Unpause();
  void Seek(quint64 offset_nanosec);
  void SetVolumeSW(uint );

  qint64 position_nanosec() const;
  qint64 length_nanosec() const;

  const Engine::Scope& scope(int chunk_length);

  QString DefaultOutput() { return "auto"; }
  OutputDetailsList GetOutputsList() const;
  bool CustomDeviceSupport(const QString &name);

  void ReloadSettings();

  void SetEnvironment();

  uint length() const;
  uint position() const;

  bool CanDecode(const QUrl &);

  bool MetaDataForUrl(const QUrl &url, Engine::SimpleMetaBundle &b);
  bool GetAudioCDContents(const QString &device, QList<QUrl> &urls);
  bool FlushBuffer();

  void SetEqualizerEnabled(bool enabled);
  void SetEqualizerParameters(int preamp, const QList<int>&);

  void FadeOut(uint fadeLength, bool* terminate, bool exiting = false);

  static void XineEventListener(void*, const xine_event_t*);
  bool event(QEvent*);

  Engine::SimpleMetaBundle fetchMetaData() const;

  bool MakeNewStream();
  bool EnsureStream();

  void DetermineAndShowErrorMessage(); //call after failure to load/play

  // Simple accessors

  xine_stream_t *stream() { return stream_; }
  float preamp() { return preamp_; }
  bool stop_fader() { return stop_fader_; }
  void set_stop_fader(bool stop_fader) { stop_fader_ = stop_fader; }

private:

  static const char *kAutoOutput;

  xine_t *xine_;
  xine_stream_t *stream_;
  xine_audio_port_t *audioport_;
  xine_event_queue_t *eventqueue_;
  xine_post_t *post_;
  float preamp_;
  bool stop_fader_;
  bool fadeout_running_;
  std::unique_ptr<PruneScopeThread> prune_;

  QUrl url_;

  static int last_error_;
  static time_t last_error_time_;

  uint log_buffer_count_ = 0;
  uint log_scope_call_count_ = 1; // Prevent divideByZero
  uint log_no_suitable_buffer_ = 0;

  std::unique_ptr<XineFader> s_fader_;
  std::unique_ptr<XineOutFader> s_outfader_;

  int int_preamp_;
  QMutex init_mutex_;
  int64_t current_vpts_;
  QList<int> equalizer_gains_;
  int fade_length_;
  bool fade_next_track_;

  mutable Engine::SimpleMetaBundle current_bundle_;

  PluginDetailsList GetPluginList() const;

private slots:
  void PruneScope();

signals:
  void InfoMessage(const QString&);
};

class PruneScopeThread : public QThread {
public:
  PruneScopeThread(XineEngine *parent);

protected:
  void run();

private:
  XineEngine *engine_;

};

#endif
