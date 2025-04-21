/*
 * Strawberry Music Player
 * Copyright 2022-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SPOTIFYSETTINGSPAGE_H
#define SPOTIFYSETTINGSPAGE_H

#include "config.h"

#include <QObject>
#include <QString>

#include "includes/shared_ptr.h"
#include "settings/settingspage.h"

class QEvent;
class QShowEvent;
class SpotifyService;
class SettingsDialog;
class Ui_SpotifySettingsPage;

class SpotifySettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit SpotifySettingsPage(SettingsDialog *dialog, const SharedPtr<SpotifyService> service, QWidget *parent = nullptr);
  ~SpotifySettingsPage() override;

  void Load() override;
  void Save() override;

  bool eventFilter(QObject *object, QEvent *event) override;

 protected:
  void showEvent(QShowEvent *e) override;

 Q_SIGNALS:
  void Authorize();

 private Q_SLOTS:
  void LoginClicked();
  void LogoutClicked();
  void LoginSuccess();
  void LoginFailure(const QString &failure_reason);

 private:
  Ui_SpotifySettingsPage *ui_;
  const SharedPtr<SpotifyService> service_;
};

#endif  // SPOTIFYSETTINGSPAGE_H
