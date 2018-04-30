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

#include "config.h"

#include <QWidget>
#include <QVariant>
#include <QString>
#include <QAction>
#include <QShortcut>
#include <QKeySequence>
#ifdef QT_DBUS_LIB
# include <QDBusConnectionInterface>
#endif

#include "globalshortcuts.h"
#include "globalshortcutbackend.h"
#include "gnomeglobalshortcutbackend.h"
#include "qxtglobalshortcutbackend.h"
#include "settings/shortcutssettingspage.h"

GlobalShortcuts::GlobalShortcuts(QWidget *parent)
    : QWidget(parent),
      gnome_backend_(nullptr),
      system_backend_(nullptr),
      use_gnome_(false) {

  settings_.beginGroup(GlobalShortcutsSettingsPage::kSettingsGroup);

  // Create actions
  AddShortcut("play", tr("Play"), SIGNAL(Play()));
  AddShortcut("pause", tr("Pause"), SIGNAL(Pause()));
  AddShortcut("play_pause", tr("Play/Pause"), SIGNAL(PlayPause()), QKeySequence(Qt::Key_MediaPlay));
  AddShortcut("stop", tr("Stop"), SIGNAL(Stop()), QKeySequence(Qt::Key_MediaStop));
  AddShortcut("stop_after", tr("Stop playing after current track"), SIGNAL(StopAfter()));
  AddShortcut("next_track", tr("Next track"), SIGNAL(Next()), QKeySequence(Qt::Key_MediaNext));
  AddShortcut("prev_track", tr("Previous track"), SIGNAL(Previous()), QKeySequence(Qt::Key_MediaPrevious));
  AddShortcut("inc_volume", tr("Increase volume"), SIGNAL(IncVolume()));
  AddShortcut("dec_volume", tr("Decrease volume"), SIGNAL(DecVolume()));
  AddShortcut("mute", tr("Mute"), SIGNAL(Mute()));
  AddShortcut("seek_forward", tr("Seek forward"), SIGNAL(SeekForward()));
  AddShortcut("seek_backward", tr("Seek backward"), SIGNAL(SeekBackward()));
  AddShortcut("show_hide", tr("Show/Hide"), SIGNAL(ShowHide()));
  AddShortcut("show_osd", tr("Show OSD"), SIGNAL(ShowOSD()));
  AddShortcut("toggle_pretty_osd", tr("Toggle Pretty OSD"), SIGNAL(TogglePrettyOSD())); // Toggling possible only for pretty OSD
  AddShortcut("shuffle_mode", tr("Change shuffle mode"), SIGNAL(CycleShuffleMode()));
  AddShortcut("repeat_mode", tr("Change repeat mode"), SIGNAL(CycleRepeatMode()));

  // Create backends - these do the actual shortcut registration
  gnome_backend_ = new GnomeGlobalShortcutBackend(this);

#ifndef Q_OS_DARWIN
  system_backend_ = new QxtGlobalShortcutBackend(this);
#else
  system_backend_ = new MacGlobalShortcutBackend(this);
#endif

  ReloadSettings();

}

void GlobalShortcuts::AddShortcut(const QString &id, const QString &name, const char *signal, const QKeySequence &default_key) {

  Shortcut shortcut = AddShortcut(id, name, default_key);
  connect(shortcut.action, SIGNAL(triggered()), this, signal);
}

GlobalShortcuts::Shortcut GlobalShortcuts::AddShortcut(const QString &id, const QString &name, const QKeySequence &default_key) {

  Shortcut shortcut;
  shortcut.action = new QAction(name, this);
  QKeySequence key_sequence = QKeySequence::fromString(settings_.value(id, default_key.toString()).toString());
  shortcut.action->setShortcut(key_sequence);
  shortcut.id = id;
  shortcut.default_key = default_key;

  // Create application wide QShortcut to hide keyevents mapped to global
  // shortcuts from widgets.
  shortcut.shortcut = new QShortcut(key_sequence, this);
  shortcut.shortcut->setContext(Qt::ApplicationShortcut);

  shortcuts_[id] = shortcut;

  return shortcut;

}

bool GlobalShortcuts::IsGsdAvailable() const {

#ifdef QT_DBUS_LIB
  return QDBusConnection::sessionBus().interface()->isServiceRegistered(GnomeGlobalShortcutBackend::kGsdService);
#else  // QT_DBUS_LIB
  return false;
#endif

}

void GlobalShortcuts::ReloadSettings() {

  // The actual shortcuts have been set in our actions for us by the config dialog already - we just need to reread the gnome settings.
  use_gnome_ = settings_.value("use_gnome", true).toBool();

  Unregister();
  Register();

}

void GlobalShortcuts::Unregister() {
  if (gnome_backend_->is_active()) gnome_backend_->Unregister();
  if (system_backend_->is_active()) system_backend_->Unregister();
}

void GlobalShortcuts::Register() {
  if (use_gnome_ && gnome_backend_->Register()) return;
  system_backend_->Register();
}

bool GlobalShortcuts::IsMacAccessibilityEnabled() const {
#ifdef Q_OS_MAC
  return static_cast<MacGlobalShortcutBackend*>(system_backend_)->IsAccessibilityEnabled();
#else
  return true;
#endif
}

void GlobalShortcuts::ShowMacAccessibilityDialog() {
#ifdef Q_OS_MAC
  static_cast<MacGlobalShortcutBackend*>(system_backend_)->ShowAccessibilityDialog();
#endif
}

