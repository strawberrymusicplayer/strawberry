/*
 * Strawberry Music Player
 * Copyright 2018-2023, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QList>
#include <QString>
#include <QSettings>

#include "core/application.h"
#include "core/song.h"
#include "core/settings.h"
#include "settings/settingsdialog.h"
#include "settings/scrobblersettingspage.h"
#include "scrobblersettings.h"

ScrobblerSettings::ScrobblerSettings(Application *app, QObject *parent)
    : QObject(parent),
      app_(app),
      enabled_(false),
      offline_(false),
      scrobble_button_(false),
      love_button_(false),
      submit_delay_(0),
      prefer_albumartist_(false),
      show_error_dialog_(false),
      strip_remastered_(false) {

  ReloadSettings();

}

void ScrobblerSettings::ReloadSettings() {

  Settings s;
  s.beginGroup(ScrobblerSettingsPage::kSettingsGroup);
  enabled_ = s.value("enabled", false).toBool();
  offline_ = s.value("offline", false).toBool();
  scrobble_button_ = s.value("scrobble_button", false).toBool();
  love_button_ = s.value("love_button", false).toBool();
  submit_delay_ = s.value("submit", 0).toInt();
  prefer_albumartist_ = s.value("albumartist", false).toBool();
  show_error_dialog_ = s.value("show_error_dialog", true).toBool();
  strip_remastered_ = s.value("strip_remastered", true).toBool();
  const QStringList sources = s.value("sources").toStringList();
  s.endGroup();

  sources_.clear();

  if (sources.isEmpty()) {
    sources_ << Song::Source::Unknown
             << Song::Source::LocalFile
             << Song::Source::Collection
             << Song::Source::CDDA
             << Song::Source::Device
             << Song::Source::Stream
             << Song::Source::Tidal
             << Song::Source::Subsonic
             << Song::Source::Qobuz
             << Song::Source::SomaFM
             << Song::Source::RadioParadise;
  }
  else {
    for (const QString &source : sources) {
      sources_ << Song::SourceFromText(source);
    }
  }

  Q_EMIT ScrobblingEnabledChanged(enabled_);
  Q_EMIT ScrobbleButtonVisibilityChanged(scrobble_button_);
  Q_EMIT LoveButtonVisibilityChanged(love_button_);

}

void ScrobblerSettings::ToggleScrobbling() {

  bool enabled_old_ = enabled_;
  enabled_ = !enabled_;

  Settings s;
  s.beginGroup(ScrobblerSettingsPage::kSettingsGroup);
  s.setValue("enabled", enabled_);
  s.endGroup();

  if (enabled_ != enabled_old_) Q_EMIT ScrobblingEnabledChanged(enabled_);

}

void ScrobblerSettings::ToggleOffline() {

  bool offline_old_ = offline_;
  offline_ = !offline_;

  Settings s;
  s.beginGroup(ScrobblerSettingsPage::kSettingsGroup);
  s.setValue("offline", offline_);
  s.endGroup();

  if (offline_ != offline_old_) { Q_EMIT ScrobblingOfflineChanged(offline_); }

}

void ScrobblerSettings::ShowConfig() {
  app_->OpenSettingsDialogAtPage(SettingsDialog::Page::Scrobbler);
}

void ScrobblerSettings::ErrorReceived(const QString &error) {
  Q_EMIT ErrorMessage(error);
}
