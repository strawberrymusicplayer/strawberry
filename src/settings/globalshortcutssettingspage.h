/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef GLOBALSHORTCUTSSETTINGSPAGE_H
#define GLOBALSHORTCUTSSETTINGSPAGE_H

#include "config.h"

#include <QObject>
#include <QMap>
#include <QString>
#include <QKeySequence>

#include "includes/scoped_ptr.h"
#include "globalshortcuts/globalshortcutsmanager.h"
#include "settingspage.h"

class QTreeWidgetItem;
class GlobalShortcutGrabber;
class SettingsDialog;
class Ui_GlobalShortcutsSettingsPage;

class GlobalShortcutsSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit GlobalShortcutsSettingsPage(SettingsDialog *dialog, GlobalShortcutsManager *global_shortcuts_manager, QWidget *parent = nullptr);
  ~GlobalShortcutsSettingsPage() override;

  void Load() override;
  void Save() override;

 private Q_SLOTS:
  void ShortcutOptionsChanged();

  void ItemClicked(QTreeWidgetItem*);
  void NoneClicked();
  void DefaultClicked();
  void ChangeClicked();

 private:
  struct Shortcut {
    GlobalShortcutsManager::Shortcut s;
    QKeySequence key;
    QTreeWidgetItem *item;
  };

  void SetShortcut(const QString &id, const QKeySequence &key);

  void X11Warning();

 private:
  Ui_GlobalShortcutsSettingsPage *ui_;

  GlobalShortcutsManager *global_shortcuts_manager_;

  bool initialized_;
  ScopedPtr<GlobalShortcutGrabber> grabber_;

  QMap<QString, Shortcut> shortcuts_;

  QString current_id_;
};

#endif  // GLOBALSHORTCUTSSETTINGSPAGE_H
