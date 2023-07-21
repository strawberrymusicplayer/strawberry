/*
 * Strawberry Music Player
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

#ifndef SCROBBLERSETTINGSPAGE_H
#define SCROBBLERSETTINGSPAGE_H

#include <memory>

#include "settingspage.h"

#include <QObject>
#include <QString>

#include "core/shared_ptr.h"

class SettingsDialog;
class Ui_ScrobblerSettingsPage;
class AudioScrobbler;
class LastFMScrobbler;
class LibreFMScrobbler;
class ListenBrainzScrobbler;

class ScrobblerSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit ScrobblerSettingsPage(SettingsDialog *dialog, QWidget *parent = nullptr);
  ~ScrobblerSettingsPage() override;

  static const char *kSettingsGroup;

  void Load() override;
  void Save() override;

 private slots:
  void LastFM_Login();
  void LastFM_Logout();
  void LastFM_AuthenticationComplete(const bool success, const QString &error = QString());
  void LibreFM_Login();
  void LibreFM_Logout();
  void LibreFM_AuthenticationComplete(const bool success, const QString &error = QString());
  void ListenBrainz_Login();
  void ListenBrainz_Logout();
  void ListenBrainz_AuthenticationComplete(const bool success, const QString &error = QString());

 private:
  SharedPtr<AudioScrobbler> scrobbler_;
  SharedPtr<LastFMScrobbler> lastfmscrobbler_;
  SharedPtr<LibreFMScrobbler> librefmscrobbler_;
  SharedPtr<ListenBrainzScrobbler> listenbrainzscrobbler_;
  Ui_ScrobblerSettingsPage *ui_;

  bool lastfm_waiting_for_auth_;
  bool librefm_waiting_for_auth_;
  bool listenbrainz_waiting_for_auth_;

  void LastFM_RefreshControls(const bool authenticated);
  void LibreFM_RefreshControls(const bool authenticated);
  void ListenBrainz_RefreshControls(const bool authenticated);
};

#endif  // SCROBBLERSETTINGSPAGE_H
