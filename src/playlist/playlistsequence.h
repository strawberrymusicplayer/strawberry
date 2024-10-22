/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2020-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef PLAYLISTSEQUENCE_H
#define PLAYLISTSEQUENCE_H

#include "config.h"

#include <QObject>
#include <QWidget>
#include <QString>
#include <QIcon>
#include <QPixmap>

#include "includes/scoped_ptr.h"

class QMenu;
class QAction;

class SettingsProvider;
class Ui_PlaylistSequence;

class PlaylistSequence : public QWidget {
  Q_OBJECT

 public:
  explicit PlaylistSequence(QWidget *parent = nullptr, SettingsProvider *settings = nullptr);
  ~PlaylistSequence() override;

  enum class RepeatMode {
    Off = 0,
    Track = 1,
    Album = 2,
    Playlist = 3,
    OneByOne = 4,
    Intro = 5
  };
  enum class ShuffleMode {
    Off = 0,
    All = 1,
    InsideAlbum = 2,
    Albums = 3
  };

  RepeatMode repeat_mode() const;
  ShuffleMode shuffle_mode() const;

  QMenu *repeat_menu() const { return repeat_menu_; }
  QMenu *shuffle_menu() const { return shuffle_menu_; }

 public Q_SLOTS:
  void SetRepeatMode(const PlaylistSequence::RepeatMode mode);
  void SetShuffleMode(const PlaylistSequence::ShuffleMode mode);
  void CycleShuffleMode();
  void CycleRepeatMode();

 Q_SIGNALS:
  void RepeatModeChanged(const PlaylistSequence::RepeatMode mode);
  void ShuffleModeChanged(const PlaylistSequence::ShuffleMode mode);

 private Q_SLOTS:
  void RepeatActionTriggered(QAction *action);
  void ShuffleActionTriggered(QAction *action);

 private:
  void Load();
  void Save();
  static QIcon AddDesaturatedIcon(const QIcon &icon);
  static QPixmap DesaturatedPixmap(const QPixmap &pixmap);

 private:
  Ui_PlaylistSequence *ui_;
  ScopedPtr<SettingsProvider> settings_;

  QMenu *repeat_menu_;
  QMenu *shuffle_menu_;

  bool loading_;
  RepeatMode repeat_mode_;
  ShuffleMode shuffle_mode_;
};

#endif  // PLAYLISTSEQUENCE_H
