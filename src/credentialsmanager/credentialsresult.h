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

#ifndef CREDENTIALSRESULT_H
#define CREDENTIALSRESULT_H

#include <QString>

class CredentialsResult {
 public:
  enum class Status {
    Success,
    NotFound,
    Error
  };

  CredentialsResult(const Status _status = Status::Error, const QString &_password = QString(), const QString &_error = QString());

  static CredentialsResult Success(const QString &_password = QString());
  static CredentialsResult NotFound();
  static CredentialsResult Error(const QString &_error);

  bool success() const { return status == Status::Success; }

  Status status;
  QString password;
  QString error;
};

#endif  // CREDENTIALSRESULT_H
