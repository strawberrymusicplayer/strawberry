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
#include <utility>

#include <QApplication>
#include <QWidget>
#include <QString>
#include <QAction>
#include <QShortcut>
#include <QKeySequence>

#include "core/logging.h"

#include "globalshortcutsmanager.h"
#include "globalshortcutsbackend.h"

#ifdef HAVE_KGLOBALACCEL_GLOBALSHORTCUTS
#include "globalshortcutsbackend-kglobalaccel.h"
#endif

#ifdef HAVE_X11_GLOBALSHORTCUTS
#  include "globalshortcutsbackend-x11.h"
#endif

#ifdef Q_OS_WIN32
#  include "globalshortcutsbackend-win.h"
#endif

#ifdef Q_OS_MACOS
#  include "globalshortcutsbackend-macos.h"
#endif

#include "constants/globalshortcutssettings.h"

using namespace Qt::Literals::StringLiterals;

GlobalShortcutsManager::GlobalShortcutsManager(QWidget *parent) : QWidget(parent) {

  settings_.beginGroup(GlobalShortcutsSettings::kSettingsGroup);

  // Create actions
  AddShortcut(u"play"_s, tr("Play"), std::bind(&GlobalShortcutsManager::Play, this));
  AddShortcut(u"pause"_s, tr("Pause"), std::bind(&GlobalShortcutsManager::Pause, this));
  AddShortcut(u"play_pause"_s, tr("Play/Pause"), std::bind(&GlobalShortcutsManager::PlayPause, this), QKeySequence(Qt::Key_MediaPlay));
  AddShortcut(u"stop"_s, tr("Stop"), std::bind(&GlobalShortcutsManager::Stop, this), QKeySequence(Qt::Key_MediaStop));
  AddShortcut(u"stop_after"_s, tr("Stop playing after current track"), std::bind(&GlobalShortcutsManager::StopAfter, this));
  AddShortcut(u"next_track"_s, tr("Next track"), std::bind(&GlobalShortcutsManager::Next, this), QKeySequence(Qt::Key_MediaNext));
  AddShortcut(u"prev_track"_s, tr("Previous track"), std::bind(&GlobalShortcutsManager::Previous, this), QKeySequence(Qt::Key_MediaPrevious));
  AddShortcut(u"restart_or_prev_track"_s, tr("Restart or previous track"), std::bind(&GlobalShortcutsManager::RestartOrPrevious, this));
  AddShortcut(u"inc_volume"_s, tr("Increase volume"), std::bind(&GlobalShortcutsManager::IncVolume, this));
  AddShortcut(u"dec_volume"_s, tr("Decrease volume"), std::bind(&GlobalShortcutsManager::DecVolume, this));
  AddShortcut(u"mute"_s, tr("Mute"), std::bind(&GlobalShortcutsManager::Mute, this));
  AddShortcut(u"seek_forward"_s, tr("Seek forward"), std::bind(&GlobalShortcutsManager::SeekForward, this));
  AddShortcut(u"seek_backward"_s, tr("Seek backward"), std::bind(&GlobalShortcutsManager::SeekBackward, this));
  AddShortcut(u"show_hide"_s, tr("Show/Hide"), std::bind(&GlobalShortcutsManager::ShowHide, this));
  AddShortcut(u"show_osd"_s, tr("Show OSD"), std::bind(&GlobalShortcutsManager::ShowOSD, this));
  AddShortcut(u"toggle_pretty_osd"_s, tr("Toggle Pretty OSD"), std::bind(&GlobalShortcutsManager::TogglePrettyOSD, this));  // Toggling possible only for pretty OSD
  AddShortcut(u"shuffle_mode"_s, tr("Change shuffle mode"), std::bind(&GlobalShortcutsManager::CycleShuffleMode, this));
  AddShortcut(u"repeat_mode"_s, tr("Change repeat mode"), std::bind(&GlobalShortcutsManager::CycleRepeatMode, this));
  AddShortcut(u"toggle_scrobbling"_s, tr("Enable/disable scrobbling"), std::bind(&GlobalShortcutsManager::ToggleScrobbling, this));
  AddShortcut(u"love"_s, tr("Love"), std::bind(&GlobalShortcutsManager::Love, this));

  // Create backends - these do the actual shortcut registration

#ifdef HAVE_KGLOBALACCEL_GLOBALSHORTCUTS
  backends_ << new GlobalShortcutsBackendKGlobalAccel(this, this);
#endif

#ifdef Q_OS_MACOS
  backends_ << new GlobalShortcutsBackendMacOS(this, this);
#endif

#ifdef Q_OS_WIN32
  backends_ << new GlobalShortcutsBackendWin(this, this);
#endif

#ifdef HAVE_X11_GLOBALSHORTCUTS
  backends_ << new GlobalShortcutsBackendX11(this, this);
#endif

  ReloadSettings();

}

void GlobalShortcutsManager::ReloadSettings() {

  backends_enabled_.clear();

#ifdef Q_OS_MACOS
  backends_enabled_ << GlobalShortcutsBackend::Type::macOS;
#endif

#ifdef Q_OS_WIN32
  backends_enabled_ << GlobalShortcutsBackend::Type::Win;
#endif

#ifdef HAVE_KGLOBALACCEL_GLOBALSHORTCUTS
  if (settings_.value(GlobalShortcutsSettings::kUseKGlobalAccel, true).toBool()) {
    backends_enabled_ << GlobalShortcutsBackend::Type::KGlobalAccel;
  }
#endif

#ifdef HAVE_X11_GLOBALSHORTCUTS
  if (settings_.value(GlobalShortcutsSettings::kUseX11, false).toBool()) {
    backends_enabled_ << GlobalShortcutsBackend::Type::X11;
  }
#endif

  Unregister();
  Register();

}

void GlobalShortcutsManager::AddShortcut(const QString &id, const QString &name, std::function<void()> signal, const QKeySequence &default_key) {  // clazy:exclude=function-args-by-ref

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

#ifdef HAVE_KGLOBALACCEL_GLOBALSHORTCUTS

bool GlobalShortcutsManager::IsKGlobalAccelAvailable() {

  return GlobalShortcutsBackendKGlobalAccel::IsKGlobalAccelAvailable();

}

#endif

#ifdef HAVE_X11_GLOBALSHORTCUTS

bool GlobalShortcutsManager::IsX11Available() {

  return GlobalShortcutsBackendX11::IsX11Available();

}

#endif

bool GlobalShortcutsManager::Register() {

  for (GlobalShortcutsBackend *backend : std::as_const(backends_)) {
    if (backend->IsAvailable() && backends_enabled_.contains(backend->type())) {
      return backend->Register();
    }
  }

  qLog(Warning) << "No global shortcuts enabled.";

  return false;

}

void GlobalShortcutsManager::Unregister() {

  for (GlobalShortcutsBackend *backend : std::as_const(backends_)) {
    if (backend->is_active()) {
      backend->Unregister();
    }
  }

}

#ifdef Q_OS_MACOS

bool GlobalShortcutsManager::IsMacAccessibilityEnabled() {

  return GlobalShortcutsBackendMacOS::IsAccessibilityEnabled();

}

#endif

void GlobalShortcutsManager::ShowMacAccessibilityDialog() {

#ifdef Q_OS_MACOS
  GlobalShortcutsBackendMacOS::ShowAccessibilityDialog();
#endif

}
