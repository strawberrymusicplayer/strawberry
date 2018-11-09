/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef DEEZERENGINE_H
#define DEEZERENGINE_H

#include "config.h"

#include <stdbool.h>
#include <deezer/deezer-connect.h>
#include <deezer/deezer-player.h>

#include <QtGlobal>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QDateTime>

#include "engine_fwd.h"
#include "enginebase.h"

class TaskManager;

class DeezerEngine : public Engine::Base {
  Q_OBJECT

 public:
  DeezerEngine(TaskManager *task_manager);
  ~DeezerEngine();

  bool Init();
  void ReloadSettings();
  Engine::State state() const { return state_; }
  bool Load(const QUrl &media_url, const QUrl &original_url, Engine::TrackChangeFlags change, bool force_stop_at_end, quint64 beginning_nanosec, qint64 end_nanosec);
  bool Play(quint64 offset_nanosec);
  void Stop(bool stop_after = false);
  void Pause();
  void Unpause();
  void Seek(quint64 offset_nanosec);
 protected:
  void SetVolumeSW(uint percent);
 public:
  virtual qint64 position_nanosec() const;
  virtual qint64 length_nanosec() const;

  OutputDetailsList GetOutputsList() const;
  bool ValidOutput(const QString &output);
  QString DefaultOutput() { return ""; }
  bool CustomDeviceSupport(const QString &output);
  bool ALSADeviceSupport(const QString &output);

 private:
  static const char *kAppID;
  static const char *kProductVersion;
  static const char *kProductID;
  static const char *kPath;
  Engine::State state_;
  dz_connect_handle connect_;
  dz_player_handle player_;
  QString access_token_;
  QDateTime expiry_time_;
  qint64 position_;
  bool stopping_;

  bool Initialised() const;
  bool CanDecode(const QUrl &url);

  static void ConnectEventCallback(dz_connect_handle handle, dz_connect_event_handle event, void *delegate);
  static void PlayerEventCallback(dz_player_handle handle, dz_player_event_handle event, void *supervisor);
  static void PlayerMetaDataCallback(dz_player_handle handle, dz_track_metadata_handle metadata, void *userdata);
  static void PlayerProgressCallback(dz_player_handle handle, dz_useconds_t progress, void *userdata);

 public slots:
  void LoadAccessToken();

};

#endif
