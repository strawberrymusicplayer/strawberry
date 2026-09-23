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

#include <QByteArray>
#include <QString>
#include <QSettings>

#include "core/settings.h"
#include "credentialsbackendsettings.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kSettingsGroup[] = "Credentials";
}

QString CredentialsBackendSettings::name() const {
  return u"Settings"_s;
}

CredentialsResult CredentialsBackendSettings::ReadPassword(const QString &service) {

  Settings s;
  s.beginGroup(kSettingsGroup);
  const QByteArray password = s.value(service).toByteArray();
  s.endGroup();

  if (password.isEmpty()) return CredentialsResult::NotFound();

  return CredentialsResult::Success(QString::fromUtf8(QByteArray::fromBase64(password)));

}

CredentialsResult CredentialsBackendSettings::SavePassword(const QString &service, const QString &password) {

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.setValue(service, QString::fromLatin1(password.toUtf8().toBase64()));
  s.endGroup();
  s.sync();

  if (s.status() != QSettings::NoError) {
    return CredentialsResult::Error(u"Failed to write settings file."_s);
  }

  return CredentialsResult::Success();

}

CredentialsResult CredentialsBackendSettings::DeletePassword(const QString &service) {

  Settings s;
  s.beginGroup(kSettingsGroup);
  s.remove(service);
  s.endGroup();
  s.sync();

  if (s.status() != QSettings::NoError) {
    return CredentialsResult::Error(u"Failed to write settings file."_s);
  }

  return CredentialsResult::Success();

}
