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

#ifndef CREDENTIALSBACKENDINTERFACE_H
#define CREDENTIALSBACKENDINTERFACE_H

#include <functional>

#include <QObject>
#include <QString>

#include "credentialsresult.h"

// Abstract interface for an asynchronous password storage backend, used by the credentials manager.
// Passwords are stored per service, each service name (for example "subsonic") identifies exactly one password.
// The credentials manager only runs one request at a time, and calls the functions from the thread the backend lives in.
// Callbacks are called in the thread the backend lives in, and might be called before the function returns.
// Backends without an asynchronous API implement CredentialsBlockingBackendInterface instead, and are wrapped in CredentialsBlockingBackend.
class CredentialsBackendInterface : public QObject {
  Q_OBJECT

 public:
  explicit CredentialsBackendInterface(QObject *parent = nullptr) : QObject(parent) {}

  using AvailableCallback = std::function<void(const bool available)>;
  using ResultCallback = std::function<void(const CredentialsResult &credentials_result)>;

  // Descriptive name of the backend, used for logging.
  virtual QString name() const = 0;

  // Returns true if the backend is secure, the settings fallback backend is not.
  virtual bool secure() const { return true; }

  // Checks if the backend can be used on this system right now.
  virtual void CheckAvailable(const AvailableCallback &available_callback) = 0;

  // Finishes with Success and the password, NotFound if no password is stored for the service, or Error.
  virtual void ReadPassword(const QString &service, const ResultCallback &callback) = 0;

  // Stores the password for the service, replacing any existing password.
  virtual void SavePassword(const QString &service, const QString &password, const ResultCallback &callback) = 0;

  // Deletes the password for the service, finishes with Success if the password is deleted or did not exist.
  virtual void DeletePassword(const QString &service, const ResultCallback &callback) = 0;
};

#endif  // CREDENTIALSBACKENDINTERFACE_H
