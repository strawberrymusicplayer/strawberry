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

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include <QByteArray>
#include <QString>

#include "credentialsbackendkeychain.h"

using namespace Qt::Literals::StringLiterals;

namespace {

constexpr char kServiceNamePrefix[] = "Strawberry/";

QString ErrorString(const OSStatus status) {

  QString error_string;
  CFStringRef message = SecCopyErrorMessageString(status, nullptr);
  if (message) {
    error_string = QString::fromCFString(message);
    CFRelease(message);
  }

  if (error_string.isEmpty()) {
    error_string = QStringLiteral("Error code %1.").arg(status);
  }

  return error_string;

}

void DictionarySetString(CFMutableDictionaryRef dictionary, const void *key, const QString &value) {

  CFStringRef string = value.toCFString();
  CFDictionarySetValue(dictionary, key, string);
  CFRelease(string);

}

// Returns a query matching the generic password item for the service, the caller must release it.
CFMutableDictionaryRef CreateQuery(const QString &service) {

  CFMutableDictionaryRef query = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFDictionarySetValue(query, kSecClass, kSecClassGenericPassword);
  DictionarySetString(query, kSecAttrService, QLatin1String(kServiceNamePrefix) + service);
  DictionarySetString(query, kSecAttrAccount, service);

  return query;

}

}  // namespace

QString CredentialsBackendKeychain::name() const {
  return u"Keychain"_s;
}

bool CredentialsBackendKeychain::IsAvailable() {
  return true;
}

CredentialsResult CredentialsBackendKeychain::ReadPassword(const QString &service) {

  CFMutableDictionaryRef query = CreateQuery(service);
  CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
  CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);

  CFTypeRef result = nullptr;
  const OSStatus status = SecItemCopyMatching(query, &result);
  CFRelease(query);

  if (status == errSecItemNotFound) return CredentialsResult::NotFound();
  if (status != errSecSuccess) return CredentialsResult::Error(ErrorString(status));
  if (!result) return CredentialsResult::NotFound();

  QString password;
  if (CFGetTypeID(result) == CFDataGetTypeID()) {
    CFDataRef data = static_cast<CFDataRef>(result);
    password = QString::fromUtf8(reinterpret_cast<const char*>(CFDataGetBytePtr(data)), static_cast<qsizetype>(CFDataGetLength(data)));
  }
  CFRelease(result);

  return CredentialsResult::Success(password);

}

CredentialsResult CredentialsBackendKeychain::SavePassword(const QString &service, const QString &password) {

  const QByteArray password_utf8 = password.toUtf8();
  CFDataRef data = CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(password_utf8.constData()), static_cast<CFIndex>(password_utf8.size()));

  CFMutableDictionaryRef query = CreateQuery(service);

  CFMutableDictionaryRef attributes = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
  CFDictionarySetValue(attributes, kSecValueData, data);

  OSStatus status = SecItemUpdate(query, attributes);
  if (status == errSecItemNotFound) {
    CFDictionarySetValue(query, kSecValueData, data);
    DictionarySetString(query, kSecAttrLabel, QStringLiteral("Strawberry %1 password").arg(service));
    status = SecItemAdd(query, nullptr);
  }

  CFRelease(attributes);
  CFRelease(query);
  CFRelease(data);

  if (status != errSecSuccess) {
    return CredentialsResult::Error(ErrorString(status));
  }

  return CredentialsResult::Success();

}

CredentialsResult CredentialsBackendKeychain::DeletePassword(const QString &service) {

  CFMutableDictionaryRef query = CreateQuery(service);
  const OSStatus status = SecItemDelete(query);
  CFRelease(query);

  if (status != errSecSuccess && status != errSecItemNotFound) {
    return CredentialsResult::Error(ErrorString(status));
  }

  return CredentialsResult::Success();

}
