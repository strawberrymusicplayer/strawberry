/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#include <memory>

#include <QObject>
#include <QSettings>
#include <QMap>
#include <QString>
#include <QKeySequence>

#include "globalshortcuts/globalshortcuts.h"
#include "settingspage.h"

class QTreeWidgetItem;
class GlobalShortcutGrabber;
class SettingsDialog;
class Ui_GlobalShortcutsSettingsPage;

class GlobalShortcutsSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit GlobalShortcutsSettingsPage(SettingsDialog *dialog);
  ~GlobalShortcutsSettingsPage();
  static const char *kSettingsGroup;

  bool IsEnabled() const;

  void Load();
  void Save();

 private slots:

#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
#ifdef HAVE_X11
  void X11Changed(bool);
#endif
#ifdef HAVE_DBUS
  void GSDChanged(bool);
  void OpenGnomeKeybindingProperties();
#endif
#endif

  void ItemClicked(QTreeWidgetItem*);
  void NoneClicked();
  void DefaultClicked();
  void ChangeClicked();

 private:
  struct Shortcut {
    GlobalShortcuts::Shortcut s;
    QKeySequence key;
    QTreeWidgetItem *item;
  };

  void SetShortcut(const QString &id, const QKeySequence &key);

  void X11Warning();

 private:
  Ui_GlobalShortcutsSettingsPage *ui_;

  bool initialised_;
  std::unique_ptr<GlobalShortcutGrabber> grabber_;

  QSettings settings_;
  QMap<QString, Shortcut> shortcuts_;

  QString current_id_;
  QString de_;

};

#endif  // GLOBALSHORTCUTSSETTINGSPAGE_H
