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

#include <QByteArray>
#include <QString>
#include <QDBusConnection>
#include <QDBusPendingReply>
#include <QDBusPendingCallWatcher>

#include "core/logging.h"

#include "avahi.h"
#include "avahi/avahiserver.h"
#include "avahi/avahientrygroup.h"

using namespace Qt::StringLiterals;

Avahi::Avahi(QObject *parent) : Zeroconf(parent), port_(0), entry_group_interface_(nullptr) {}

void Avahi::PublishInternal(const QString &domain, const QString &type, const QByteArray &name, quint16 port) {

  domain_ = domain;
  type_ = type;
  name_ = name;
  port_ = port;

  OrgFreedesktopAvahiServerInterface server_interface(u"org.freedesktop.Avahi"_s, u"/"_s, QDBusConnection::systemBus());
  QDBusPendingReply<QDBusObjectPath> reply = server_interface.EntryGroupNew();
  QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply);
  QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, &Avahi::PublishInternalFinished);

}

void Avahi::PublishInternalFinished(QDBusPendingCallWatcher *watcher) {

  const QDBusPendingReply<QDBusObjectPath> path_reply = watcher->reply();

  watcher->deleteLater();

  if (path_reply.isError()) {
    qLog(Error) << "Failed to create Avahi entry group:" << path_reply.error();
    qLog(Info) << "This might be because 'disable-user-service-publishing'" << "is set to 'yes' in avahi-daemon.conf";
    return;
  }

  AddService(path_reply.reply().path());

}

void Avahi::AddService(const QString &path) {

  entry_group_interface_ = new OrgFreedesktopAvahiEntryGroupInterface(u"org.freedesktop.Avahi"_s, path, QDBusConnection::systemBus());
  QDBusPendingReply<> reply = entry_group_interface_->AddService(-1, -1, 0, QString::fromUtf8(name_.constData(), name_.size()), type_, domain_, QString(), port_);
  QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply);
  QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, &Avahi::AddServiceFinished);

}

void Avahi::AddServiceFinished(QDBusPendingCallWatcher *watcher) {

  const QDBusPendingReply<QDBusObjectPath> path_reply = watcher->reply();

  watcher->deleteLater();

  if (path_reply.isError()) {
    qLog(Error) << "Failed to add Avahi service:" << path_reply.error();
    return;
  }

  Commit();

}

void Avahi::Commit() {

  QDBusPendingReply<> reply = entry_group_interface_->Commit();
  QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply);
  QObject::connect(watcher, &QDBusPendingCallWatcher::finished, this, &Avahi::CommitFinished);

}

void Avahi::CommitFinished(QDBusPendingCallWatcher *watcher) {

  const QDBusPendingReply<QDBusObjectPath> path_reply = watcher->reply();

  watcher->deleteLater();

  entry_group_interface_->deleteLater();
  entry_group_interface_ = nullptr;

  if (path_reply.isError()) {
    qLog(Debug) << "Commit error:" << path_reply.error();
  }
  else {
    qLog(Debug) << "Remote interface published on Avahi";
  }

}
