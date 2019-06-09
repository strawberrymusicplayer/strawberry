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

#ifndef TIDALSETTINGSPAGE_H
#define TIDALSETTINGSPAGE_H

#include <QObject>
#include <QString>
#include <QEvent>

#include "settings/settingspage.h"

class TidalService;
class Ui_TidalSettingsPage;

class TidalSettingsPage : public SettingsPage {
  Q_OBJECT

 public:
  explicit TidalSettingsPage(SettingsDialog* parent = nullptr);
  ~TidalSettingsPage();

  static const char *kSettingsGroup;

  enum StreamUrlMethod {
    StreamUrlMethod_StreamUrl,
    StreamUrlMethod_UrlPostPaywall,
    StreamUrlMethod_PlaybackInfoPostPaywall,
  };

  void Load();
  void Save();

  bool eventFilter(QObject *object, QEvent *event);

 signals:
  void Login();
  void Login(const QString &username, const QString &password, const QString &token);

 private slots:
  void OAuthClicked(bool enabled);
  void LoginClicked();
  void LogoutClicked();
  void LoginSuccess();
  void LoginFailure(QString failure_reason);

 private:
  Ui_TidalSettingsPage* ui_;
  TidalService *service_;
};

#endif
