/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2020, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef OSDBASE_H
#define OSDBASE_H

#include "config.h"

#include <memory>

#include <QtGlobal>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QDateTime>
#include <QImage>

#include "core/song.h"
#include "playlist/playlistsequence.h"

class Application;
class OSDPretty;
class SystemTrayIcon;

class OSDBase : public QObject {
  Q_OBJECT

 public:
  explicit OSDBase(SystemTrayIcon *tray_icon, Application *app, QObject *parent = nullptr);
  ~OSDBase() override;

  static const char *kSettingsGroup;

  enum Behaviour {
    Disabled = 0,
    Native,
    TrayPopup,
    Pretty,
  };

  int timeout_msec() const { return timeout_msec_; }
  void ReloadPrettyOSDSettings();
  void SetPrettyOSDToggleMode(bool toggle);

  virtual bool SupportsNativeNotifications();
  virtual bool SupportsTrayPopups();

  QString app_name() { return app_name_; }

 public slots:
  void ReloadSettings();

  void SongChanged(const Song &song);
  void Paused();
  void Resumed();
  void Stopped();
  void StopAfterToggle(bool stop);
  void PlaylistFinished();
  void VolumeChanged(int value);
  void RepeatModeChanged(PlaylistSequence::RepeatMode mode);
  void ShuffleModeChanged(PlaylistSequence::ShuffleMode mode);

  void ReshowCurrentSong();

  void ShowPreview(const Behaviour type, const QString &line1, const QString &line2, const Song &song);

 private:
  enum MessageType {
    Type_Summary,
    Type_Message,
  };
  void ShowPlaying(const Song &song, const QUrl &cover_url, const QImage &image, const bool preview = false);
  void ShowMessage(const QString &summary, const QString &message = QString(), const QString icon = QString("strawberry"), const QImage &image = QImage());
  QString ReplaceMessage(const MessageType type, const QString &message, const Song &song);
  virtual void ShowMessageNative(const QString &summary, const QString &message, const QString &icon = QString(), const QImage &image = QImage());

 private slots:
  void AlbumCoverLoaded(const Song &song, const QUrl &cover_url, const QImage &image);

 private:
  Application *app_;
  SystemTrayIcon *tray_icon_;
  OSDPretty *pretty_popup_;

  QString app_name_;
  int timeout_msec_;
  Behaviour behaviour_;
  bool show_on_volume_change_;
  bool show_art_;
  bool show_on_play_mode_change_;
  bool show_on_pause_;
  bool show_on_resume_;
  bool use_custom_text_;
  QString custom_text1_;
  QString custom_text2_;

  bool force_show_next_;
  bool ignore_next_stopped_;
  bool playing_;

  Song song_playing_;
  Song last_song_;
  QUrl last_image_uri_;
  QImage last_image_;

};

#endif  // OSDBASE_H
