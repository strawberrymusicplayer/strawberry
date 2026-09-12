/*
 * Strawberry Music Player
 * Copyright 2025, Strawberry Music Player contributors
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

#ifndef PLEXSETTINGSPAGE_H
#define PLEXSETTINGSPAGE_H

#include "config.h"

#include <QObject>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "settings/settingspage.h"
#include "plex/plexservice.h"

class QEvent;
class QShowEvent;
class SettingsDialog;
class Ui_PlexSettingsPage;

class PlexSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit PlexSettingsPage(SettingsDialog *dialog, const SharedPtr<PlexService> service, QWidget *parent = nullptr);
  ~PlexSettingsPage() override;

  void Load() override;
  void Save() override;

  bool eventFilter(QObject *object, QEvent *event) override;

 protected:
  void showEvent(QShowEvent *e) override;

 private Q_SLOTS:
  void LoginClicked();
  void LogoutClicked();
  void LoginSuccess();
  void LoginFailure(const QString &failure_reason);
  void ServersFound(const PlexService::ServerList &servers);
  void RefreshServersClicked();
  void TestClicked();
  void TestSuccess();
  void TestFailure(const QString &failure_reason);

 private:
  QUrl ServerUrlFromUi() const;
  QString ServerTokenForUrl(const QUrl &url) const;

  Ui_PlexSettingsPage *ui_;
  const SharedPtr<PlexService> service_;
  PlexService::ServerList servers_;
};

#endif  // PLEXSETTINGSPAGE_H
