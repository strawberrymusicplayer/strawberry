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

#ifndef PLAYBACKSETTINGSPAGE_H
#define PLAYBACKSETTINGSPAGE_H

#include "config.h"

#include "settingspage.h"

class Ui_PlaybackSettingsPage;

class PlaybackSettingsPage : public SettingsPage {
  Q_OBJECT

public:
  PlaybackSettingsPage(SettingsDialog* dialog);
  ~PlaybackSettingsPage();
  static const char *kSettingsGroup;

  void Load();
  void Save();

 private slots:
  void FadingOptionsChanged();

private:
  Ui_PlaybackSettingsPage* ui_;
};

#endif  // PLAYBACKSETTINGSPAGE_H
