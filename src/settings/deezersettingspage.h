/*
 * Strawberry Music Player
 * Copyright 2018, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef DEEZERSETTINGSPAGE_H
#define DEEZERSETTINGSPAGE_H

#include <QObject>
#include <QString>
#include <QEvent>

#include "settings/settingspage.h"

class DeezerService;
class Ui_DeezerSettingsPage;

class DeezerSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit DeezerSettingsPage(SettingsDialog* parent = nullptr);
  ~DeezerSettingsPage();

  enum SearchBy {
    SearchBy_Songs = 1,
    SearchBy_Albums = 2,
  };

  static const char *kSettingsGroup;

  void Load();
  void Save();

  bool eventFilter(QObject *object, QEvent *event);

signals:
  void Login();
  void Login(const QString &username, const QString &password);

 private slots:
  void LoginClicked();
  void LogoutClicked();
  void LoginSuccess();
  void LoginFailure(QString failure_reason);

 private:
  Ui_DeezerSettingsPage* ui_;
  DeezerService *service_;
};

#endif
