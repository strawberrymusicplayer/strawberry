/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef BEHAVIOURSETTINGSPAGE_H
#define BEHAVIOURSETTINGSPAGE_H

#include "config.h"

#include <QObject>
#include <QMap>
#include <QString>

#include "settingspage.h"

class SettingsDialog;
class Ui_BehaviourSettingsPage;

class BehaviourSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit BehaviourSettingsPage(SettingsDialog *dialog, QWidget *parent = nullptr);
  ~BehaviourSettingsPage() override;

  static const char *kSettingsGroup;

  // Don't change the values
  enum class StartupBehaviour {
    Remember = 1,
    Show = 2,
    Hide = 3,
    ShowMaximized = 4,
    ShowMinimized = 5
  };

  enum class PlayBehaviour {
    Never = 1,
    IfStopped = 2,
    Always = 3
  };

  enum class PreviousBehaviour {
    DontRestart = 1,
    Restart = 2
  };

  enum class AddBehaviour {
    Append = 1,
    Enqueue = 2,
    Load = 3,
    OpenInNew = 4
  };

  enum class PlaylistAddBehaviour {
    Play = 1,
    Enqueue = 2
  };

  void Load() override;
  void Save() override;

 private slots:
  void ShowTrayIconToggled(bool on);

 private:
  Ui_BehaviourSettingsPage *ui_;
  QMap<QString, QString> language_map_;
};

#endif  // BEHAVIOURSETTINGSPAGE_H
