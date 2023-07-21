/*
 * Strawberry Music Player
 * Copyright 2021, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef GLOBALSHORTCUTSBACKEND_MATE_H
#define GLOBALSHORTCUTSBACKEND_MATE_H

#include "config.h"

#include <QObject>
#include <QString>

#include "globalshortcutsbackend.h"

class QDBusPendingCallWatcher;
class GlobalShortcutsManager;
class OrgMateSettingsDaemonMediaKeysInterface;

class GlobalShortcutsBackendMate : public GlobalShortcutsBackend {
  Q_OBJECT

 public:
  explicit GlobalShortcutsBackendMate(GlobalShortcutsManager *manager, QObject *parent = nullptr);

  bool IsAvailable() const override;
  static bool IsMateAvailable();

 protected:
  bool DoRegister() override;
  void DoUnregister() override;

 private slots:
  void RegisterFinished(QDBusPendingCallWatcher *watcher);

  void MateMediaKeyPressed(const QString &application, const QString &key);

 private:
  static const char *kService1;
  static const char *kService2;
  static const char *kPath;

  OrgMateSettingsDaemonMediaKeysInterface *interface_;
  bool is_connected_;
};

#endif  // GLOBALSHORTCUTSBACKEND_Mate_H
