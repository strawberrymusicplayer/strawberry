/*
 * Strawberry Music Player
 * This file was part of Clementine
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2017-2018, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef PHONONENGINE_H
#define PHONONENGINE_H

#include "config.h"

#include <phonon/mediaobject.h>
#include <phonon/audiooutput.h>

#include <QtGlobal>
#include <QObject>
#include <QUrl>

#include "enginebase.h"

class QTimer;
class TaskManager;

class PhononEngine : public Engine::Base {
  Q_OBJECT

 public:
  PhononEngine(TaskManager *task_manager);
  ~PhononEngine();

  bool Init();

  OutputDetailsList GetOutputsList() const;

  bool CanDecode(const QUrl &url);

  bool Load(const QUrl &stream_url, const QUrl &original_url, const Engine::TrackChangeFlags change, const bool force_stop_at_end, const quint64 beginning_nanosec, const qint64 end_nanosec);
  bool Play(const quint64 offset_nanosec);
  void Stop(const bool stop_after = false);
  void Pause();
  void Unpause();

  Engine::State state() const;
  uint position() const;
  uint length() const;

  void Seek(const quint64 offset_nanosec);

  qint64 position_nanosec() const;
  qint64 length_nanosec() const;

  QString DefaultOutput() { return QString(""); }
  bool ValidOutput(const QString &output);
  bool CustomDeviceSupport(const QString &output);
  bool ALSADeviceSupport(const QString &output);

 protected:
  void SetVolumeSW(const uint percent );

 private slots:
  void PhononFinished();
  void PhononStateChanged(const Phonon::State new_state);
  void StateTimeoutExpired();

 private:
  Phonon::MediaObject *media_object_;
  Phonon::AudioOutput *audio_output_;

  QTimer *state_timer_;

  qint64 seek_offset_;
};

#endif // PHONONENGINE_H
