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
  explicit BehaviourSettingsPage(SettingsDialog *dialog);
  ~BehaviourSettingsPage();

  static const char *kSettingsGroup;

  enum PlayBehaviour {
    PlayBehaviour_Never = 1,
    PlayBehaviour_IfStopped = 2,
    PlayBehaviour_Always = 3,
  };

  enum PreviousBehaviour {
    PreviousBehaviour_DontRestart = 1,
    PreviousBehaviour_Restart = 2
  };

  enum AddBehaviour {
    AddBehaviour_Append = 1,
    AddBehaviour_Enqueue = 2,
    AddBehaviour_Load = 3,
    AddBehaviour_OpenInNew = 4
  };

  enum PlaylistAddBehaviour {
    PlaylistAddBehaviour_Play = 1,
    PlaylistAddBehaviour_Enqueue = 2,
  };

  void Load();
  void Save();

private slots:
  void ShowTrayIconToggled(bool on);

private:
  Ui_BehaviourSettingsPage *ui_;
  QMap<QString, QString> language_map_;

};

#endif // BEHAVIOURSETTINGSPAGE_H
