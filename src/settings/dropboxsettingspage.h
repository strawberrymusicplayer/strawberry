/*
 * Strawberry Music Player
 * Copyright 2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef DROPBOXSETTINGSPAGE_H
#define DROPBOXSETTINGSPAGE_H

#include <QObject>

#include "includes/shared_ptr.h"
#include "settingspage.h"

class DropboxService;
class Ui_DropboxSettingsPage;

class DropboxSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit DropboxSettingsPage(SettingsDialog *dialog, const SharedPtr<DropboxService> service, QWidget *parent);
  ~DropboxSettingsPage();

  void Load() override;
  void Save() override;

  bool eventFilter(QObject *object, QEvent *event) override;

 Q_SIGNALS:
  void Authorize();

 private Q_SLOTS:
  void LoginClicked();
  void LogoutClicked();
  void LoginSuccess();
  void LoginFailure(const QString &failure_reason);
  void ResetClicked();

 private:
  Ui_DropboxSettingsPage *ui_;
  const SharedPtr<DropboxService> service_;
};

#endif  // DROPBOXSETTINGSPAGE_H
