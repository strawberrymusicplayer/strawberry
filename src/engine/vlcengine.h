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

#include <optional>

#include <vlc/vlc.h>

#include <QtGlobal>
#include <QObject>
#include <QString>
#include <QUrl>

#include "core/shared_ptr.h"

#include "enginebase.h"

using namespace Qt::Literals::StringLiterals;

struct libvlc_event_t;

class TaskManager;

class VLCEngine : public EngineBase {
  Q_OBJECT

 public:
  explicit VLCEngine(SharedPtr<TaskManager> task_manager, QObject *parent = nullptr);
  ~VLCEngine() override;

  Type type() const override { return Type::VLC; }
  bool Init() override;
  EngineBase::State state() const override { return state_; }
  bool Load(const QUrl &media_url, const QUrl &stream_url, const EngineBase::TrackChangeFlags change, const bool force_stop_at_end, const quint64 beginning_nanosec, const qint64 end_nanosec, const std::optional<double> ebur128_integrated_loudness_lufs) override;
  bool Play(const bool pause, const quint64 offset_nanosec) override;
  void Stop(const bool stop_after = false) override;
  void Pause() override;
  void Unpause() override;
  void Seek(const quint64 offset_nanosec) override;

 protected:
  void SetVolumeSW(const uint percent) override;

 public:
  qint64 position_nanosec() const override;
  qint64 length_nanosec() const override;

  OutputDetailsList GetOutputsList() const override;
  bool ValidOutput(const QString &output) override;
  QString DefaultOutput() const override { return ""_L1; }
  bool CustomDeviceSupport(const QString &output) const override;
  bool ALSADeviceSupport(const QString &output) const override;
  bool ExclusiveModeSupport(const QString &output) const override;

 private:
  libvlc_instance_t *instance_;
  libvlc_media_player_t *player_;
  State state_;

  bool Initialized() const { return (instance_ && player_); }
  uint position() const;
  uint length() const;
  static bool CanDecode(const QUrl &url);
  void AttachCallback(libvlc_event_manager_t *em, libvlc_event_type_t type, libvlc_callback_t callback);
  static void StateChangedCallback(const libvlc_event_t *e, void *data);

  void GetDevicesList(const QString &output) const;
};

#endif  // VLCENGINE_H
