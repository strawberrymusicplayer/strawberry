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

#ifndef CREDENTIALSBACKENDLIBSECRETGUARD_H
#define CREDENTIALSBACKENDLIBSECRETGUARD_H

#include <QMutex>

class CredentialsBackendLibSecret;

// Shared between the backend and pending libsecret calls, which can finish in another thread after the backend is deleted.
// The backend destructor clears the backend pointer with the mutex locked, so the backend is valid while the mutex is locked and the pointer is set.
struct CredentialsBackendLibSecretGuard {
  QMutex mutex;
  CredentialsBackendLibSecret *backend = nullptr;
};

#endif  // CREDENTIALSBACKENDLIBSECRETGUARD_H
