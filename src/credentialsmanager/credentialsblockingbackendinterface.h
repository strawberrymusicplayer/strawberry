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

#ifndef CREDENTIALSBLOCKINGBACKENDINTERFACE_H
#define CREDENTIALSBLOCKINGBACKENDINTERFACE_H

#include <QString>

#include "credentialsresult.h"

// Abstract interface for a password storage backend without an asynchronous API.
// Passwords are stored per service, each service name (for example "subsonic") identifies exactly one password.
// All functions are blocking and may show a password prompt from the system keyring, CredentialsBlockingBackend runs them in a thread pool.
class CredentialsBlockingBackendInterface {
 public:
  explicit CredentialsBlockingBackendInterface() = default;
  virtual ~CredentialsBlockingBackendInterface() = default;

  // Descriptive name of the backend, used for logging.
  virtual QString name() const = 0;

  // Returns true if the backend is secure, the settings fallback backend is not.
  virtual bool secure() const { return true; }

  // Returns true if the backend can be used on this system right now.
  virtual bool IsAvailable() = 0;

  // Returns Success with the password, NotFound if no password is stored for the service, or Error.
  virtual CredentialsResult ReadPassword(const QString &service) = 0;

  // Stores the password for the service, replacing any existing password.
  virtual CredentialsResult SavePassword(const QString &service, const QString &password) = 0;

  // Deletes the password for the service, returns Success if the password is deleted or did not exist.
  virtual CredentialsResult DeletePassword(const QString &service) = 0;

 private:
  Q_DISABLE_COPY(CredentialsBlockingBackendInterface)
};

#endif  // CREDENTIALSBLOCKINGBACKENDINTERFACE_H
