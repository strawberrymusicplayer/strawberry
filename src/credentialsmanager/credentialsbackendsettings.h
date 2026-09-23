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

#ifndef CREDENTIALSBACKENDSETTINGS_H
#define CREDENTIALSBACKENDSETTINGS_H

#include <QString>

#include "credentialsblockingbackendinterface.h"

// Fallback backend storing passwords base64 encoded in the settings file.
// This is not secure, and is only used when no system keyring is available.
class CredentialsBackendSettings : public CredentialsBlockingBackendInterface {
 public:
  explicit CredentialsBackendSettings() = default;

  QString name() const override;
  bool secure() const override { return false; }
  bool IsAvailable() override { return true; }
  CredentialsResult ReadPassword(const QString &service) override;
  CredentialsResult SavePassword(const QString &service, const QString &password) override;
  CredentialsResult DeletePassword(const QString &service) override;
};

#endif  // CREDENTIALSBACKENDSETTINGS_H
