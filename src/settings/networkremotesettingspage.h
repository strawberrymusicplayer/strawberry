/*
 * Strawberry Music Player
 * Copyright 2025, Leopold List <leo@zudiewiener.com>
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

#ifndef NETWORKREMOTESETTINGSPAGE_H
#define NETWORKREMOTESETTINGSPAGE_H

#include "settingspage.h"
#include "networkremote/remotesettings.h"

class SettingsDialog;
class Ui_NetworkRemoteSettingsPage;
class NetworkRemote;

class NetworkRemoteSettingsPage : public SettingsPage{
  Q_OBJECT
 public:
  explicit NetworkRemoteSettingsPage(SettingsDialog *dialog, QWidget *parent = nullptr);
  ~NetworkRemoteSettingsPage() override;
  void Load() override;
  void Save() override;
  void Refresh();

 Q_SIGNALS:
  void remoteSettingsChanged();

 private:
  Ui_NetworkRemoteSettingsPage *ui_;
  NetworkRemoteSettings *settings_;

 private Q_SLOTS:
  void RemoteButtonClicked();
  void LocalConnectButtonClicked();
  void PortChanged();
  void DisplayIP();
};

#endif // NETWORKREMOTESETTINGSPAGE_H
