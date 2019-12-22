/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2019, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef CONTEXTSETTINGSPAGE_H
#define CONTEXTSETTINGSPAGE_H

#include "config.h"

#include <stdbool.h>
#include <QObject>
#include <QString>
#include <QAction>
#include <QtEvents>
#include <QCheckBox>
#include <QLabel>

#include "settingspage.h"

class SettingsDialog;
class Ui_ContextSettingsPage;

class ContextSettingsPage : public SettingsPage
{
  Q_OBJECT

public:
  ContextSettingsPage(SettingsDialog *dialog);
  ~ContextSettingsPage();

  enum ContextSettingsOrder {
    TECHNICAL_DATA,
    ENGINE_AND_DEVICE,
    ALBUMS_BY_ARTIST,
    SONG_LYRICS,
    NELEMS,
  };

  static const char *kSettingsGroup;
  static const char *kSettingsTitleFmt;
  static const char *kSettingsSummaryFmt;
  static const char *kSettingsGroupLabels[ContextSettingsOrder::NELEMS];
  static const char *kSettingsGroupEnable[ContextSettingsOrder::NELEMS];

  void Load();
  void Save();

private slots:
  void InsertVariableFirstLine(QAction *action);
  void InsertVariableSecondLine(QAction *action);
  void ShowMenuTooltip(QAction *action);

private:
  Ui_ContextSettingsPage *ui_;
  QCheckBox *checkboxes[ContextSettingsOrder::NELEMS];
};

#endif // CONTEXTSETTINGSPAGE_H
