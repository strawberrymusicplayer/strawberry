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

#ifndef NOTIFICATIONSSETTINGSPAGE_H
#define NOTIFICATIONSSETTINGSPAGE_H

#include "config.h"

#include <QObject>
#include <QString>

#include "settingspage.h"

class QAction;
class QHideEvent;
class QShowEvent;

class OSDPretty;
class SettingsDialog;
class Ui_NotificationsSettingsPage;

class NotificationsSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit NotificationsSettingsPage(SettingsDialog *dialog);
  ~NotificationsSettingsPage();
  static const char *kSettingsGroup;

  void Load();
  void Save();

 protected:
  void hideEvent(QHideEvent*);
  void showEvent(QShowEvent*);

 private slots:
  void NotificationTypeChanged();
  void NotificationCustomTextChanged(bool enabled);
  void PrepareNotificationPreview();
  void InsertVariableFirstLine(QAction *action);
  void InsertVariableSecondLine(QAction *action);
  void ShowMenuTooltip(QAction *action);

  void PrettyOpacityChanged(int value);
  void PrettyColorPresetChanged(int index);
  void ChooseBgColor();
  void ChooseFgColor();
  void ChooseFont();

  void UpdatePopupVisible();

 private:
  Ui_NotificationsSettingsPage *ui_;
  OSDPretty *pretty_popup_;
};

#endif // NOTIFICATIONSSETTINGSPAGE_H
