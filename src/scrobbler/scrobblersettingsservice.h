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

#ifndef SCROBBLERSETTINGSSERVICE_H
#define SCROBBLERSETTINGSSERVICE_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QString>

#include "core/song.h"

class Song;

class ScrobblerSettingsService : public QObject {
  Q_OBJECT

 public:
  explicit ScrobblerSettingsService(QObject *parent = nullptr);

  void ReloadSettings();

  bool enabled() const { return enabled_; }
  bool offline() const { return offline_; }
  bool scrobble_button() const { return scrobble_button_; }
  bool love_button() const { return love_button_; }
  int submit_delay() const { return submit_delay_; }
  bool prefer_albumartist() const { return prefer_albumartist_; }
  bool show_error_dialog() const { return show_error_dialog_; }
  bool strip_remastered() const { return strip_remastered_; }
  QList<Song::Source> sources() const { return sources_; }

 public Q_SLOTS:
  void ToggleScrobbling();
  void ToggleOffline();
  void ErrorReceived(const QString &error);

 Q_SIGNALS:
  void ErrorMessage(const QString &error);
  void ScrobblingEnabledChanged(const bool value);
  void ScrobblingOfflineChanged(const bool value);
  void ScrobbleButtonVisibilityChanged(const bool value);
  void LoveButtonVisibilityChanged(const bool value);

 private:
  bool enabled_;
  bool offline_;
  bool scrobble_button_;
  bool love_button_;
  int submit_delay_;
  bool prefer_albumartist_;
  bool show_error_dialog_;
  bool strip_remastered_;
  QList<Song::Source> sources_;

  Q_DISABLE_COPY(ScrobblerSettingsService)
};

#endif  // SCROBBLERSETTINGSSERVICE_H
