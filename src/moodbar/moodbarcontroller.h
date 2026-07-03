/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, David Sansome <me@davidsansome.com>
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef MOODBARCONTROLLER_H
#define MOODBARCONTROLLER_H

#include <QObject>
#include <QByteArray>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/song.h"
#include "moodbarpipeline.h"

class MoodbarLoader;
class Player;

class MoodbarController : public QObject {
  Q_OBJECT

 public:
  explicit MoodbarController(const SharedPtr<Player> player, const SharedPtr<MoodbarLoader> moodbar_loader, QObject *parent = nullptr);

  void ReloadSettings();

 Q_SIGNALS:
  void CurrentMoodbarDataChanged(const QByteArray &data = QByteArray());
  void StyleChanged();

 public Q_SLOTS:
  void CurrentSongChanged(const Song &song);
  void PlaybackStopped();

  // Starts or stops generating the current song's moodbar for the seekbar.
  // Driven by MoodbarProxyStyle::MoodbarShow when the seekbar mode changes.
  void SetEnabled(const bool enabled);

 private:
  // Called by both SetEnabled() and ReloadSettings() so the "generate vs clear vs no-op" decision lives in one place.
  void ApplyEnabledTransition(const bool was_enabled);
  void GenerateMoodbar(const Song &song);
  void AsyncLoadComplete(MoodbarPipelinePtr pipeline, const QUrl &url);

 private:
  const SharedPtr<Player> player_;
  const SharedPtr<MoodbarLoader> moodbar_loader_;
  bool enabled_;
  Song current_song_;
};

#endif  // MOODBARCONTROLLER_H
