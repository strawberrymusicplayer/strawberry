/*
 * Strawberry Music Player
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef SUBSONICSETTINGSPAGE_H
#define SUBSONICSETTINGSPAGE_H

#include "config.h"

#include <QObject>
#include <QString>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "constants/subsonicsettings.h"
#include "settings/settingspage.h"

class QEvent;
class SettingsDialog;
class SubsonicService;
class Ui_SubsonicSettingsPage;

class SubsonicSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit SubsonicSettingsPage(SettingsDialog *dialog, const SharedPtr<SubsonicService> service, QWidget *parent = nullptr);
  ~SubsonicSettingsPage() override;

  void Load() override;
  void Save() override;

  bool eventFilter(QObject *object, QEvent *event) override;

 Q_SIGNALS:
  void Test(const QUrl &url, const QString &username, const QString &password, const SubsonicSettings::AuthMethod auth_method, const bool redirect = false);

 private Q_SLOTS:
  void CheckboxDownloadAlbumCoversToggled(bool enabled);
  void TestClicked();
  void TestSuccess();
  void TestFailure(const QString &failure_reason);

 private:
  Ui_SubsonicSettingsPage *ui_;
  const SharedPtr<SubsonicService> service_;
};

#endif  // SUBSONICSETTINGSPAGE_H
