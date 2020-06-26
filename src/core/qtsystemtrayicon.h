/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#ifndef QTSYSTEMTRAYICON_H
#define QTSYSTEMTRAYICON_H

#include "config.h"

#include <QObject>
#include <QSystemTrayIcon>
#include <QString>
#include <QUrl>
#include <QIcon>
#include <QPixmap>
#include <QAction>
#include <QtEvents>

#include "systemtrayicon.h"

class QMenu;
class QEvent;

class Song;

class QtSystemTrayIcon : public SystemTrayIcon {
  Q_OBJECT

 public:
  explicit QtSystemTrayIcon(QObject *parent = nullptr);
  ~QtSystemTrayIcon() override;

  void SetupMenu(QAction *previous, QAction *play, QAction *stop, QAction *stop_after, QAction *next, QAction *mute, QAction *love, QAction *quit) override;
  bool IsVisible() const override;
  void SetVisible(bool visible) override;

  void ShowPopup(const QString &summary, const QString &message, int timeout) override;

  void SetNowPlaying(const Song &song, const QUrl &cover_url) override;
  void ClearNowPlaying() override;

  bool MuteEnabled() const override { return action_mute_->isVisible(); }
  void SetMuteEnabled(bool enabled) override { action_mute_->setVisible(enabled); }

 protected:
  // SystemTrayIcon
  void UpdateIcon() override;
  void SetPaused() override;
  void SetPlaying(bool enable_play_pause = false) override;
  void SetStopped() override;
  void MuteButtonStateChanged(bool value) override;
  void LoveVisibilityChanged(bool value) override;
  void LoveStateChanged(bool value) override;

  // QObject
  bool eventFilter(QObject*, QEvent*) override;

 private slots:
  void Clicked(QSystemTrayIcon::ActivationReason);

 private:
  QSystemTrayIcon *tray_;
  QMenu *menu_;
  QString app_name_;
  QIcon icon_;
  QPixmap normal_icon_;
  QPixmap grey_icon_;

  QAction *action_play_pause_;
  QAction *action_stop_;
  QAction *action_stop_after_this_track_;
  QAction *action_mute_;
  QAction *action_love_;

#ifndef Q_OS_WIN
  QString de_;
  QString pattern_;
#endif

};

#endif  // QTSYSTEMTRAYICON_H
