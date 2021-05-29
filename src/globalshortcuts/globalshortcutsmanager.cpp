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

#include "config.h"

#include <functional>

#include <QApplication>
#include <QWidget>
#include <QVariant>
#include <QString>
#include <QAction>
#include <QShortcut>
#include <QKeySequence>
#ifdef HAVE_DBUS
# include <QDBusConnectionInterface>
#endif

#include "globalshortcutsmanager.h"
#include "globalshortcutsbackend.h"

#ifdef HAVE_DBUS
#  include "globalshortcutsbackend-kde.h"
#  include "globalshortcutsbackend-gnome.h"
#  include "globalshortcutsbackend-mate.h"
#endif
#if (defined(HAVE_X11) && defined(HAVE_QPA_QPLATFORMNATIVEINTERFACE_H)) || defined(Q_OS_WIN)
#  include "globalshortcutsbackend-system.h"
#endif
#ifdef Q_OS_MACOS
#  include "globalshortcutsbackend-macos.h"
#endif

#include "settings/globalshortcutssettingspage.h"

GlobalShortcutsManager::GlobalShortcutsManager(QWidget *parent)
    : QWidget(parent),
      kde_backend_(nullptr),
      gnome_backend_(nullptr),
      mate_backend_(nullptr),
      system_backend_(nullptr),
      use_kde_(true),
      use_gnome_(true),
      use_mate_(true),
      use_x11_(false)
  {

  settings_.beginGroup(GlobalShortcutsSettingsPage::kSettingsGroup);

  // Create actions
  AddShortcut("play", "Play", std::bind(&GlobalShortcutsManager::Play, this));
  AddShortcut("pause", "Pause", std::bind(&GlobalShortcutsManager::Pause, this));
  AddShortcut("play_pause", "Play/Pause", std::bind(&GlobalShortcutsManager::PlayPause, this), QKeySequence(Qt::Key_MediaPlay));
  AddShortcut("stop", "Stop", std::bind(&GlobalShortcutsManager::Stop, this), QKeySequence(Qt::Key_MediaStop));
  AddShortcut("stop_after", "Stop playing after current track", std::bind(&GlobalShortcutsManager::StopAfter, this));
  AddShortcut("next_track", "Next track", std::bind(&GlobalShortcutsManager::Next, this), QKeySequence(Qt::Key_MediaNext));
  AddShortcut("prev_track", "Previous track", std::bind(&GlobalShortcutsManager::Previous, this), QKeySequence(Qt::Key_MediaPrevious));
  AddShortcut("inc_volume", "Increase volume", std::bind(&GlobalShortcutsManager::IncVolume, this));
  AddShortcut("dec_volume", "Decrease volume", std::bind(&GlobalShortcutsManager::DecVolume, this));
  AddShortcut("mute", tr("Mute"), std::bind(&GlobalShortcutsManager::Mute, this));
  AddShortcut("seek_forward", "Seek forward", std::bind(&GlobalShortcutsManager::SeekForward, this));
  AddShortcut("seek_backward", "Seek backward", std::bind(&GlobalShortcutsManager::SeekBackward, this));
  AddShortcut("show_hide", "Show/Hide", std::bind(&GlobalShortcutsManager::ShowHide, this));
  AddShortcut("show_osd", "Show OSD", std::bind(&GlobalShortcutsManager::ShowOSD, this));
  AddShortcut("toggle_pretty_osd", "Toggle Pretty OSD", std::bind(&GlobalShortcutsManager::TogglePrettyOSD, this)); // Toggling possible only for pretty OSD
  AddShortcut("shuffle_mode", "Change shuffle mode", std::bind(&GlobalShortcutsManager::CycleShuffleMode, this));
  AddShortcut("repeat_mode", "Change repeat mode", std::bind(&GlobalShortcutsManager::CycleRepeatMode, this));
  AddShortcut("toggle_scrobbling", "Enable/disable scrobbling", std::bind(&GlobalShortcutsManager::ToggleScrobbling, this));
  AddShortcut("love", "Love", std::bind(&GlobalShortcutsManager::Love, this));

  // Create backends - these do the actual shortcut registration
#ifdef HAVE_DBUS
  kde_backend_ = new GlobalShortcutsBackendKDE(this);
  gnome_backend_ = new GlobalShortcutsBackendGnome(this);
  mate_backend_ = new GlobalShortcutsBackendMate(this);
#endif

#ifdef Q_OS_MACOS
  if (!system_backend_)
    system_backend_ = new GlobalShortcutsBackendMacOS(this);
#endif
#if defined(Q_OS_WIN)
  if (!system_backend_)
    system_backend_ = new GlobalShortcutsBackendSystem(this);
#endif
#if defined(HAVE_X11) && defined(HAVE_QPA_QPLATFORMNATIVEINTERFACE_H)
  if (!system_backend_ && IsX11Available())
    system_backend_ = new GlobalShortcutsBackendSystem(this);
#endif

  ReloadSettings();

}

void GlobalShortcutsManager::ReloadSettings() {

  // The actual shortcuts have been set in our actions for us by the config dialog already - we just need to reread the gnome settings.
  use_kde_ = settings_.value("use_kde", true).toBool();
  use_gnome_ = settings_.value("use_gnome", true).toBool();
  use_mate_ = settings_.value("use_mate", true).toBool();
  use_x11_ = settings_.value("use_x11", false).toBool();

  Unregister();
  Register();

}

void GlobalShortcutsManager::AddShortcut(const QString &id, const QString &name, std::function<void()> signal, const QKeySequence &default_key) {

  Shortcut shortcut = AddShortcut(id, name, default_key);
  QObject::connect(shortcut.action, &QAction::triggered, this, signal);

}

GlobalShortcutsManager::Shortcut GlobalShortcutsManager::AddShortcut(const QString &id, const QString &name, const QKeySequence &default_key) {

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

bool GlobalShortcutsManager::IsKdeAvailable() const {

#ifdef HAVE_DBUS
  return kde_backend_->IsAvailable();
#else
  return false;
#endif

}

bool GlobalShortcutsManager::IsGnomeAvailable() const {

#ifdef HAVE_DBUS
  return gnome_backend_->IsAvailable();
#else
  return false;
#endif

}

bool GlobalShortcutsManager::IsMateAvailable() const {

#ifdef HAVE_DBUS
  return mate_backend_->IsAvailable();
#else
  return false;
#endif

}

bool GlobalShortcutsManager::IsX11Available() const {

  return QApplication::platformName() == "xcb";

}

void GlobalShortcutsManager::Register() {

  if (use_kde_ && kde_backend_ && kde_backend_->Register()) return;
  if (use_gnome_ && gnome_backend_ && gnome_backend_->Register()) return;
  if (use_mate_ && mate_backend_ && mate_backend_->Register()) return;
#if defined(HAVE_X11) && defined(HAVE_QPA_QPLATFORMNATIVEINTERFACE_H) // If this system has X11, only use the system backend if X11 is enabled in the global shortcut settings
  if (use_x11_) {
#endif
    if (system_backend_)
      system_backend_->Register();
#if defined(HAVE_X11) && defined(HAVE_QPA_QPLATFORMNATIVEINTERFACE_H)
   }
#endif

}

void GlobalShortcutsManager::Unregister() {

  if (kde_backend_ && kde_backend_->is_active()) kde_backend_->Unregister();
  if (gnome_backend_ && gnome_backend_->is_active()) gnome_backend_->Unregister();
  if (mate_backend_ && mate_backend_->is_active()) mate_backend_->Unregister();
  if (system_backend_ && system_backend_->is_active()) system_backend_->Unregister();

}

bool GlobalShortcutsManager::IsMacAccessibilityEnabled() const {
#ifdef Q_OS_MACOS
  if (system_backend_) return qobject_cast<GlobalShortcutsBackendMacOS*>(system_backend_)->IsAccessibilityEnabled();
  else return false;
#else
  return true;
#endif
}

void GlobalShortcutsManager::ShowMacAccessibilityDialog() {
#ifdef Q_OS_MACOS
  if (system_backend_) qobject_cast<GlobalShortcutsBackendMacOS*>(system_backend_)->ShowAccessibilityDialog();
#endif
}
