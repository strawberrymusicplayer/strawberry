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

#ifndef NOTIFICATIONSSETTINGSPAGE_H
#define NOTIFICATIONSSETTINGSPAGE_H

#include "config.h"

#include <QObject>
#include <QString>

#include "settingspage.h"

class QAction;
class QHideEvent;
class QShowEvent;

class OSDBase;
class OSDPretty;
class SettingsDialog;
class Ui_NotificationsSettingsPage;

class NotificationsSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit NotificationsSettingsPage(SettingsDialog *dialog, OSDBase *osd, QWidget *parent = nullptr);
  ~NotificationsSettingsPage() override;

  void Load() override;
  void Save() override;

 protected:
  void hideEvent(QHideEvent *e) override;
  void showEvent(QShowEvent *e) override;

 private Q_SLOTS:
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

  void PrettyOSDChanged();

  void DiscordRPCChanged();

 private:
  Ui_NotificationsSettingsPage *ui_;
  OSDBase *osd_;
  OSDPretty *pretty_popup_;
};

#endif  // NOTIFICATIONSSETTINGSPAGE_H
