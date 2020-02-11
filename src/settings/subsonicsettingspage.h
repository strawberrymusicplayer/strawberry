/*
 * Strawberry Music Player
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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

#include "settings/settingspage.h"

class QEvent;
class SettingsDialog;
class SubsonicService;
class Ui_SubsonicSettingsPage;

class SubsonicSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit SubsonicSettingsPage(SettingsDialog* parent = nullptr);
  ~SubsonicSettingsPage();

  static const char *kSettingsGroup;

  void Load();
  void Save();

  bool eventFilter(QObject *object, QEvent *event);

 signals:
  void Test();
  void Test(QUrl url, const QString &username, const QString &password);

 private slots:
  void TestClicked();
  void TestSuccess();
  void TestFailure(QString failure_reason);

 private:
  Ui_SubsonicSettingsPage* ui_;
  SubsonicService *service_;
};

#endif  // SUBSONICSETTINGSPAGE_H
