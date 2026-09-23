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

#include <QString>

#include "credentialsresult.h"

CredentialsResult::CredentialsResult(const Status _status, const QString &_password, const QString &_error)
    : status(_status),
      password(_password),
      error(_error) {}

CredentialsResult CredentialsResult::Success(const QString &_password) {
  return CredentialsResult(Status::Success, _password);
}

CredentialsResult CredentialsResult::NotFound() {
  return CredentialsResult(Status::NotFound);
}

CredentialsResult CredentialsResult::Error(const QString &_error) {
  return CredentialsResult(Status::Error, QString(), _error);
}
