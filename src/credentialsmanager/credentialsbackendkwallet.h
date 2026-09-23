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

#ifndef CREDENTIALSBACKENDKWALLET_H
#define CREDENTIALSBACKENDKWALLET_H

#include <functional>

#include <QObject>
#include <QVariantList>
#include <QString>

#include "credentialsbackendinterface.h"
#include "credentialsdbusutils.h"

// Stores passwords in KWallet using the org.kde.KWallet D-Bus interface of kwalletd6 (or kwalletd5), all calls are asynchronous.
class CredentialsBackendKWallet : public CredentialsBackendInterface {
  Q_OBJECT

 public:
  explicit CredentialsBackendKWallet(QObject *parent = nullptr);
  ~CredentialsBackendKWallet() override;

  QString name() const override;
  void CheckAvailable(const AvailableCallback &available_callback) override;
  void ReadPassword(const QString &service, const ResultCallback &callback) override;
  void SavePassword(const QString &service, const QString &password, const ResultCallback &callback) override;
  void DeletePassword(const QString &service, const ResultCallback &callback) override;

 private:
  using SuccessCallback = std::function<void(const bool success, const QString &error)>;
  using BoolCallback = std::function<void(const bool success, const bool value, const QString &error)>;

  void Call(const QString &method, const QVariantList &arguments, const CredentialsDBusUtils::ReplyCallback &callback, const int timeout = -1);
  void CheckService(const int index, const AvailableCallback &available_callback);
  void Open(const SuccessCallback &callback);
  void OpenWallet(const SuccessCallback &callback);
  void HasFolder(const BoolCallback &callback);
  void HasEntry(const QString &service, const BoolCallback &callback);
  void CreateFolder(const SuccessCallback &callback);
  void WritePassword(const QString &service, const QString &password, const ResultCallback &callback);

 private:
  QString service_name_;
  QString path_;
  int handle_;
};

#endif  // CREDENTIALSBACKENDKWALLET_H
