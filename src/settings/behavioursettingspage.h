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

  void Load() override;
  void Save() override;

 private Q_SLOTS:
  void ShowTrayIconToggled(const bool on);

 private:
  Ui_BehaviourSettingsPage *ui_;
  QMap<QString, QString> language_map_;
};

#endif  // BEHAVIOURSETTINGSPAGE_H
