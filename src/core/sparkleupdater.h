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

#ifndef SPARKLEUPDATER_H
#define SPARKLEUPDATER_H

#include <QObject>

class QAction;

#ifdef __OBJC__
@class AppUpdaterDelegate;
#endif

class SparkleUpdater : public QObject {
  Q_OBJECT

 public:
  explicit SparkleUpdater(QAction *action_check_updates, QObject *parent = nullptr);

 public Q_SLOTS:
  void CheckForUpdates();

 private:
#ifdef __OBJC__
  AppUpdaterDelegate *updater_delegate_;
#else
  void *updater_delegate_;
#endif
};

#endif  // SPARKLEUPDATER_H
