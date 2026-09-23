/*
 * Strawberry Music Player
 * Copyright 2026, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef CREDENTIALSBACKENDGNOMEKEYRING_H
#define CREDENTIALSBACKENDGNOMEKEYRING_H

#include <functional>

#include <QObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QDBusObjectPath>

#include "credentialsbackendinterface.h"
#include "credentialsdbusutils.h"

using SecretServiceAttributes = QMap<QString, QString>;

// Stores passwords in GNOME Keyring using the org.freedesktop.secrets (Secret Service) D-Bus API, all calls are asynchronous.
// This also works with other keyrings implementing the Secret Service API, for example KeePassXC.
// Items are stored with the same attributes as the libsecret backend, so both backends can read each others passwords.
class CredentialsBackendGnomeKeyring : public CredentialsBackendInterface {
  Q_OBJECT

 public:
  explicit CredentialsBackendGnomeKeyring(QObject *parent = nullptr);
  ~CredentialsBackendGnomeKeyring() override;

  QString name() const override;
  void CheckAvailable(const AvailableCallback &available_callback) override;
  void ReadPassword(const QString &service, const ResultCallback &callback) override;
  void SavePassword(const QString &service, const QString &password, const ResultCallback &callback) override;
  void DeletePassword(const QString &service, const ResultCallback &callback) override;

 private:
  using SuccessCallback = std::function<void(const bool success, const QString &error)>;
  using PromptCallback = std::function<void(const bool success, const QVariant &result, const QString &error)>;
  using PathCallback = std::function<void(const bool success, const QDBusObjectPath &path, const QString &error)>;
  using PathsCallback = std::function<void(const bool success, const QList<QDBusObjectPath> &paths, const QString &error)>;

  void Call(const QString &path, const QString &interface, const QString &method, const QVariantList &arguments, const CredentialsDBusUtils::ReplyCallback &callback);
  static SecretServiceAttributes Attributes(const QString &service);

  void OpenSession(const SuccessCallback &callback);
  void RunPrompt(const QDBusObjectPath &prompt, const PromptCallback &callback);
  void Unlock(const QList<QDBusObjectPath> &objects, const PathsCallback &callback);
  void SearchItems(const QString &service, const PathsCallback &callback);
  void DefaultCollection(const PathCallback &callback);
  void CreateItem(const QDBusObjectPath &collection, const QString &service, const QString &password, const ResultCallback &callback);
  void DeleteItems(const QList<QDBusObjectPath> &items, const ResultCallback &callback);

 private:
  QDBusObjectPath session_;
};

#endif  // CREDENTIALSBACKENDGNOMEKEYRING_H
