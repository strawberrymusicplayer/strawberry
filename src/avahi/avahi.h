/*
 * Strawberry Music Player
 * Copyright 2024, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef AVAHI_H
#define AVAHI_H

#include <QObject>
#include <QByteArray>
#include <QString>

#include "core/zeroconf.h"

class QDBusPendingCallWatcher;
class OrgFreedesktopAvahiEntryGroupInterface;

class Avahi : public Zeroconf {
  Q_OBJECT

public:
  explicit Avahi(QObject *parent = nullptr);

 private:
  void AddService(const QString &path);
  void Commit();

 private Q_SLOTS:
  void PublishInternalFinished(QDBusPendingCallWatcher *watcher);
  void AddServiceFinished(QDBusPendingCallWatcher *watcher);
  void CommitFinished(QDBusPendingCallWatcher *watcher);

 protected:
  virtual void PublishInternal(const QString &domain, const QString &type, const QByteArray &name, quint16 port) override;

 private:
  QString domain_;
  QString type_;
  QByteArray name_;
  quint16 port_;
  OrgFreedesktopAvahiEntryGroupInterface *entry_group_interface_;
};

#endif  // AVAHI_H
