/*
 * Strawberry Music Player
 * This file was part of Clementine.
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

#ifndef VLCENGINE_H
#define VLCENGINE_H

#include "config.h"

#include <stdbool.h>
#include <vlc/vlc.h>

#include <QtGlobal>
#include <QObject>
#include <QString>
#include <QUrl>

#include "engine_fwd.h"
#include "enginebase.h"

struct libvlc_event_t;

class TaskManager;

class VLCEngine : public Engine::Base {
  Q_OBJECT

 public:
  VLCEngine(TaskManager *task_manager);
  ~VLCEngine();

  bool Init();
  Engine::State state() const { return state_; }
  bool Load(const QUrl &stream_url, const QUrl &original_url, const Engine::TrackChangeFlags change, const bool force_stop_at_end, const quint64 beginning_nanosec, const qint64 end_nanosec);
  bool Play(const quint64 offset_nanosec);
  void Stop(const bool stop_after = false);
  void Pause();
  void Unpause();
  void Seek(const quint64 offset_nanosec);
 protected:
  void SetVolumeSW(const uint percent);
 public:
  virtual qint64 position_nanosec() const;
  virtual qint64 length_nanosec() const;

  OutputDetailsList GetOutputsList() const;
  bool ValidOutput(const QString &output);
  QString DefaultOutput() { return ""; }
  bool CustomDeviceSupport(const QString &output);
  bool ALSADeviceSupport(const QString &output);

 private:
  libvlc_instance_t *instance_;
  libvlc_media_player_t *player_;
  Engine::State state_;

  bool Initialised() const { return (instance_ && player_); }
  uint position() const;
  uint length() const;
  bool CanDecode(const QUrl &url);
  void AttachCallback(libvlc_event_manager_t* em, libvlc_event_type_t type, libvlc_callback_t callback);
  static void StateChangedCallback(const libvlc_event_t* e, void* data);

  PluginDetailsList GetPluginList() const;
  void GetDevicesList(const QString &output) const;

};

#endif // VLCENGINE_H
