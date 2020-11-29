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

#include "config.h"

#include <QWidget>
#include <QVariant>
#include <QString>
#include <QAction>
#include <QShortcut>
#include <QKeySequence>
#ifdef HAVE_DBUS
# include <QDBusConnectionInterface>
#endif
#ifdef HAVE_X11EXTRAS
#  include <QX11Info>
#endif

#include "globalshortcuts.h"
#include "globalshortcutbackend.h"

#ifdef HAVE_DBUS
#  include "globalshortcutbackend-gsd.h"
#  include "globalshortcutbackend-kde.h"
#endif
#if defined(HAVE_X11EXTRAS) || defined(Q_OS_WIN)
#  include "globalshortcutbackend-system.h"
#endif
#ifdef Q_OS_MACOS
#  include "globalshortcutbackend-macos.h"
#endif

#include "settings/shortcutssettingspage.h"

GlobalShortcuts::GlobalShortcuts(QWidget *parent)
    : QWidget(parent),
      gsd_backend_(nullptr),
      kde_backend_(nullptr),
      system_backend_(nullptr),
      use_gsd_(true),
      use_kde_(true),
      use_x11_(false)
  {

  settings_.beginGroup(GlobalShortcutsSettingsPage::kSettingsGroup);

  // Create actions
  AddShortcut("play", "Play", SIGNAL(Play()));
  AddShortcut("pause", "Pause", SIGNAL(Pause()));
  AddShortcut("play_pause", "Play/Pause", SIGNAL(PlayPause()), QKeySequence(Qt::Key_MediaPlay));
  AddShortcut("stop", "Stop", SIGNAL(Stop()), QKeySequence(Qt::Key_MediaStop));
  AddShortcut("stop_after", "Stop playing after current track", SIGNAL(StopAfter()));
  AddShortcut("next_track", "Next track", SIGNAL(Next()), QKeySequence(Qt::Key_MediaNext));
  AddShortcut("prev_track", "Previous track", SIGNAL(Previous()), QKeySequence(Qt::Key_MediaPrevious));
  AddShortcut("inc_volume", "Increase volume", SIGNAL(IncVolume()));
  AddShortcut("dec_volume", "Decrease volume", SIGNAL(DecVolume()));
  AddShortcut("mute", tr("Mute"), SIGNAL(Mute()));
  AddShortcut("seek_forward", "Seek forward", SIGNAL(SeekForward()));
  AddShortcut("seek_backward", "Seek backward", SIGNAL(SeekBackward()));
  AddShortcut("show_hide", "Show/Hide", SIGNAL(ShowHide()));
  AddShortcut("show_osd", "Show OSD", SIGNAL(ShowOSD()));
  AddShortcut("toggle_pretty_osd", "Toggle Pretty OSD", SIGNAL(TogglePrettyOSD())); // Toggling possible only for pretty OSD
  AddShortcut("shuffle_mode", "Change shuffle mode", SIGNAL(CycleShuffleMode()));
  AddShortcut("repeat_mode", "Change repeat mode", SIGNAL(CycleRepeatMode()));
  AddShortcut("toggle_scrobbling", "Enable/disable scrobbling", SIGNAL(ToggleScrobbling()));
  AddShortcut("love", "Love", SIGNAL(Love()));

  // Create backends - these do the actual shortcut registration
#ifdef HAVE_DBUS
  gsd_backend_ = new GlobalShortcutBackendGSD(this);
  kde_backend_ = new GlobalShortcutBackendKDE(this);
#endif

#ifdef Q_OS_MACOS
  if (!system_backend_)
    system_backend_ = new GlobalShortcutBackendMacOS(this);
#endif
#if defined(Q_OS_WIN)
  if (!system_backend_)
    system_backend_ = new GlobalShortcutBackendSystem(this);
#endif
#if defined(HAVE_X11EXTRAS)
  if (!system_backend_ && IsX11Available())
    system_backend_ = new GlobalShortcutBackendSystem(this);
#endif

  ReloadSettings();

}

void GlobalShortcuts::ReloadSettings() {

  // The actual shortcuts have been set in our actions for us by the config dialog already - we just need to reread the gnome settings.
  use_gsd_ = settings_.value("use_gsd", true).toBool();
  use_kde_ = settings_.value("use_kde", true).toBool();
  use_x11_ = settings_.value("use_x11", false).toBool();

  Unregister();
  Register();

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

  // Create application wide QShortcut to hide keyevents mapped to global shortcuts from widgets.
  shortcut.shortcut = new QShortcut(key_sequence, this);
  shortcut.shortcut->setContext(Qt::ApplicationShortcut);

  shortcuts_[id] = shortcut;

  return shortcut;

}

bool GlobalShortcuts::IsGsdAvailable() const {

#ifdef HAVE_DBUS
  return QDBusConnection::sessionBus().interface()->isServiceRegistered(GlobalShortcutBackendGSD::kGsdService) || QDBusConnection::sessionBus().interface()->isServiceRegistered(GlobalShortcutBackendGSD::kGsdService2);
#else
  return false;
#endif

}

bool GlobalShortcuts::IsKdeAvailable() const {

#ifdef HAVE_DBUS
  return QDBusConnection::sessionBus().interface()->isServiceRegistered(GlobalShortcutBackendKDE::kKdeService);
#else
  return false;
#endif

}

bool GlobalShortcuts::IsX11Available() const {

#ifdef HAVE_X11EXTRAS
  return QX11Info::isPlatformX11();
#else
  return false;
#endif

}

void GlobalShortcuts::Register() {

  if (use_gsd_ && gsd_backend_ && gsd_backend_->Register()) return;
  if (use_kde_ && kde_backend_ && kde_backend_->Register()) return;
#ifdef HAVE_X11EXTRAS // If this system has X11, only use the system backend if X11 is enabled in the global shortcut settings
  if (use_x11_) {
#endif
    if (system_backend_)
      system_backend_->Register();
#ifdef HAVE_X11EXTRAS
   }
#endif

}

void GlobalShortcuts::Unregister() {

  if (gsd_backend_ && gsd_backend_->is_active()) gsd_backend_->Unregister();
  if (kde_backend_ && kde_backend_->is_active()) kde_backend_->Unregister();
  if (system_backend_ && system_backend_->is_active()) system_backend_->Unregister();

}

bool GlobalShortcuts::IsMacAccessibilityEnabled() const {
#ifdef Q_OS_MACOS
  if (system_backend_) return qobject_cast<GlobalShortcutBackendMacOS*>(system_backend_)->IsAccessibilityEnabled();
  else return false;
#else
  return true;
#endif
}

void GlobalShortcuts::ShowMacAccessibilityDialog() {
#ifdef Q_OS_MACOS
  if (system_backend_) qobject_cast<GlobalShortcutBackendMacOS*>(system_backend_)->ShowAccessibilityDialog();
#endif
}
