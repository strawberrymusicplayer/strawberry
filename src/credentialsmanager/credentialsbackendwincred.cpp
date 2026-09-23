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

#include <windows.h>
#include <wincred.h>

#include <QByteArray>
#include <QString>

#include "credentialsbackendwincred.h"

using namespace Qt::Literals::StringLiterals;

namespace {

constexpr char kTargetNamePrefix[] = "Strawberry/";
constexpr wchar_t kUserName[] = L"strawberry";

QString TargetName(const QString &service) {
  return QLatin1String(kTargetNamePrefix) + service;
}

QString ErrorString(const DWORD error_code) {

  wchar_t *buffer = nullptr;
  const DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error_code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
  QString error_string;
  if (length > 0 && buffer) {
    error_string = QString::fromWCharArray(buffer, static_cast<qsizetype>(length)).trimmed();
  }
  if (buffer) LocalFree(buffer);

  if (error_string.isEmpty()) {
    error_string = QStringLiteral("Error code %1.").arg(error_code);
  }

  return error_string;

}

}  // namespace

QString CredentialsBackendWinCred::name() const {
  return u"Windows Credential Manager"_s;
}

bool CredentialsBackendWinCred::IsAvailable() {
  return true;
}

CredentialsResult CredentialsBackendWinCred::ReadPassword(const QString &service) {

  const QString target_name = TargetName(service);

  PCREDENTIALW credential = nullptr;
  if (!CredReadW(reinterpret_cast<LPCWSTR>(target_name.utf16()), CRED_TYPE_GENERIC, 0, &credential)) {
    const DWORD error_code = GetLastError();
    if (error_code == ERROR_NOT_FOUND) return CredentialsResult::NotFound();
    return CredentialsResult::Error(ErrorString(error_code));
  }

  // The password is stored as UTF-16, the same as credentials added through the Windows user interface.
  const QString password = QString::fromUtf16(reinterpret_cast<const char16_t*>(credential->CredentialBlob), static_cast<qsizetype>(credential->CredentialBlobSize / sizeof(char16_t)));
  CredFree(credential);

  return CredentialsResult::Success(password);

}

CredentialsResult CredentialsBackendWinCred::SavePassword(const QString &service, const QString &password) {

  // The password is stored as UTF-16, and the credential blob is limited to CRED_MAX_CREDENTIAL_BLOB_SIZE bytes.
  const qsizetype password_size = password.size() * static_cast<qsizetype>(sizeof(char16_t));
  if (password_size > CRED_MAX_CREDENTIAL_BLOB_SIZE) {
    return CredentialsResult::Error(QStringLiteral("Password is too long, Windows Credential Manager allows at most %1 characters.").arg(CRED_MAX_CREDENTIAL_BLOB_SIZE / sizeof(char16_t)));
  }

  const QString target_name = TargetName(service);

  CREDENTIALW credential{};
  credential.Type = CRED_TYPE_GENERIC;
  credential.TargetName = const_cast<LPWSTR>(reinterpret_cast<LPCWSTR>(target_name.utf16()));
  credential.CredentialBlobSize = static_cast<DWORD>(password_size);
  credential.CredentialBlob = const_cast<LPBYTE>(reinterpret_cast<const BYTE*>(password.utf16()));
  // Roam the password with the user profile like the settings in the registry, without a roaming profile it is stored locally.
  credential.Persist = CRED_PERSIST_ENTERPRISE;
  credential.UserName = const_cast<LPWSTR>(kUserName);

  if (!CredWriteW(&credential, 0)) {
    return CredentialsResult::Error(ErrorString(GetLastError()));
  }

  return CredentialsResult::Success();

}

CredentialsResult CredentialsBackendWinCred::DeletePassword(const QString &service) {

  const QString target_name = TargetName(service);

  if (!CredDeleteW(reinterpret_cast<LPCWSTR>(target_name.utf16()), CRED_TYPE_GENERIC, 0)) {
    const DWORD error_code = GetLastError();
    if (error_code == ERROR_NOT_FOUND) return CredentialsResult::Success();
    return CredentialsResult::Error(ErrorString(error_code));
  }

  return CredentialsResult::Success();

}
