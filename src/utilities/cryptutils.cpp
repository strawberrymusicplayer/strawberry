/*
 * Strawberry Music Player
 * Copyright 2018-2026, Jonas Kvinge <jonas@jkvinge.net>
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

#include "config.h"
#include "apicredentials.h"

#include <openssl/evp.h>

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QCryptographicHash>

#include "core/logging.h"
#include "cryptutils.h"

#ifndef API_CREDENTIALS_ENCRYPTION_KEY
#  define API_CREDENTIALS_ENCRYPTION_KEY ""
#endif

namespace Utilities {

QByteArray Hmac(const QByteArray &key, const QByteArray &data, const QCryptographicHash::Algorithm method) {

  constexpr int block_size = 64;
  Q_ASSERT(key.length() <= block_size);

  QByteArray inner_padding(block_size, static_cast<char>(0x36));
  QByteArray outer_padding(block_size, static_cast<char>(0x5c));

  for (int i = 0; i < key.length(); ++i) {
    inner_padding[i] = static_cast<char>(inner_padding[i] ^ key[i]);
    outer_padding[i] = static_cast<char>(outer_padding[i] ^ key[i]);
  }

  QByteArray part;
  part.append(inner_padding);
  part.append(data);

  QByteArray total;
  total.append(outer_padding);
  total.append(QCryptographicHash::hash(part, method));

  return QCryptographicHash::hash(total, method);

}

QByteArray HmacSha256(const QByteArray &key, const QByteArray &data) {
  return Hmac(key, data, QCryptographicHash::Sha256);
}

QByteArray HmacMd5(const QByteArray &key, const QByteArray &data) {
  return Hmac(key, data, QCryptographicHash::Md5);
}

QByteArray HmacSha1(const QByteArray &key, const QByteArray &data) {
  return Hmac(key, data, QCryptographicHash::Sha1);
}

namespace {

constexpr char kEncryptedApiCredentialPrefix[] = "ENC:";

QByteArray DecryptAes256Cbc(const QByteArray &key, const QByteArray &iv, const QByteArray &ciphertext) {

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) return QByteArray();

  QByteArray plaintext(ciphertext.size() + EVP_MAX_BLOCK_LENGTH, '\0');
  int len = 0;
  int plaintext_len = 0;
  bool success = EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, reinterpret_cast<const unsigned char*>(key.constData()), reinterpret_cast<const unsigned char*>(iv.constData())) == 1;
  if (success) {
    success = EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(plaintext.data()), &len, reinterpret_cast<const unsigned char*>(ciphertext.constData()), ciphertext.size()) == 1;
    plaintext_len = len;
  }

  if (success) {
    success = EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(plaintext.data()) + plaintext_len, &len) == 1;
    plaintext_len += len;
  }

  EVP_CIPHER_CTX_free(ctx);

  if (!success) return QByteArray();

  plaintext.resize(plaintext_len);

  return plaintext;

}

}  // namespace

QString MaybeDecryptApiCredential(const QString &value) {

  if (!value.startsWith(QLatin1String(kEncryptedApiCredentialPrefix))) {
    return value;
  }

  static const QByteArray ApiCredentialsEncryptionKey = QByteArrayLiteral(API_CREDENTIALS_ENCRYPTION_KEY);
  if (ApiCredentialsEncryptionKey.isEmpty()) {
    return QString();
  }

  const QStringList parts = value.mid(static_cast<int>(qstrlen(kEncryptedApiCredentialPrefix))).split(u':');
  if (parts.size() != 2) {
    qLog(Error) << "Cannot decrypt API credential: malformed encrypted value.";
    return QString();
  }

  const QByteArray iv = QByteArray::fromHex(parts.at(0).toLatin1());
  const QByteArray ciphertext = QByteArray::fromBase64(parts.at(1).toLatin1());
  if (iv.size() != 16 || ciphertext.isEmpty()) {
    qLog(Error) << "Cannot decrypt API credential: malformed encrypted value.";
    return QString();
  }

  const QByteArray key = QCryptographicHash::hash(ApiCredentialsEncryptionKey, QCryptographicHash::Sha256);
  const QByteArray plaintext = DecryptAes256Cbc(key, iv, ciphertext);
  if (plaintext.isEmpty()) {
    qLog(Error) << "Failed to decrypt API credential - check that API_CREDENTIALS_ENCRYPTION_KEY matches the key it was encrypted with.";
    return QString();
  }

  return QString::fromUtf8(plaintext);

}

}  // namespace Utilities
