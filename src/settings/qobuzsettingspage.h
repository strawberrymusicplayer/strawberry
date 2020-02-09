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

#ifndef QOBUZSETTINGSPAGE_H
#define QOBUZSETTINGSPAGE_H

#include <QObject>
#include <QString>

#include "settings/settingspage.h"

class QEvent;
class SettingsDialog;
class QobuzService;
class Ui_QobuzSettingsPage;

class QobuzSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit QobuzSettingsPage(SettingsDialog* parent = nullptr);
  ~QobuzSettingsPage();

  static const char *kSettingsGroup;

  void Load();
  void Save();

  bool eventFilter(QObject *object, QEvent *event);

 signals:
  void Login();
  void Login(const QString &username, const QString &password, const QString &token);

 private slots:
  void LoginClicked();
  void LogoutClicked();
  void LoginSuccess();
  void LoginFailure(QString failure_reason);

 private:
  Ui_QobuzSettingsPage* ui_;
  QobuzService *service_;
};

#endif
