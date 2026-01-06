/*
 * Strawberry Music Player
 * Copyright 2019-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "qobuzcredentialfetcher.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kLoginPageUrl[] = "https://play.qobuz.com/login";
constexpr char kPlayQobuzUrl[] = "https://play.qobuz.com";
}  // namespace

QobuzCredentialFetcher::QobuzCredentialFetcher(const SharedPtr<NetworkAccessManager> network, QObject *parent)
    : QObject(parent),
      network_(network),
      login_page_reply_(nullptr),
      bundle_reply_(nullptr) {}

void QobuzCredentialFetcher::FetchCredentials() {

  qLog(Debug) << "Qobuz: Fetching credentials from web player";

  QNetworkRequest request(QUrl(QString::fromLatin1(kLoginPageUrl)));
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  request.setHeader(QNetworkRequest::UserAgentHeader, u"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"_s);

  login_page_reply_ = network_->get(request);
  QObject::connect(login_page_reply_, &QNetworkReply::finished, this, &QobuzCredentialFetcher::LoginPageReceived);

}

void QobuzCredentialFetcher::LoginPageReceived() {

  if (!login_page_reply_) return;

  QNetworkReply *reply = login_page_reply_;
  login_page_reply_ = nullptr;

  if (reply->error() != QNetworkReply::NoError) {
    QString error = QStringLiteral("Failed to fetch login page: %1").arg(reply->errorString());
    qLog(Error) << "Qobuz:" << error;
    reply->deleteLater();
    Q_EMIT CredentialsFetchError(error);
    return;
  }

  const QString login_page = QString::fromUtf8(reply->readAll());
  reply->deleteLater();

  // Extract bundle.js URL from the login page
  // Pattern: <script src="(/resources/\d+\.\d+\.\d+-[a-z]\d{3}/bundle\.js)"></script>
  static const QRegularExpression bundle_url_regex(u"<script src=\"(/resources/[\\d.]+-[a-z]\\d+/bundle\\.js)\"></script>"_s);
  const QRegularExpressionMatch bundle_match = bundle_url_regex.match(login_page);

  if (!bundle_match.hasMatch()) {
    QString error = u"Failed to find bundle.js URL in login page"_s;
    qLog(Error) << "Qobuz:" << error;
    Q_EMIT CredentialsFetchError(error);
    return;
  }

  bundle_url_ = bundle_match.captured(1);
  qLog(Debug) << "Qobuz: Found bundle URL:" << bundle_url_;

  // Fetch the bundle.js
  QNetworkRequest request(QUrl(QString::fromLatin1(kPlayQobuzUrl) + bundle_url_));
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  request.setHeader(QNetworkRequest::UserAgentHeader, u"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"_s);

  bundle_reply_ = network_->get(request);
  QObject::connect(bundle_reply_, &QNetworkReply::finished, this, &QobuzCredentialFetcher::BundleReceived);

}

void QobuzCredentialFetcher::BundleReceived() {

  if (!bundle_reply_) return;

  QNetworkReply *reply = bundle_reply_;
  bundle_reply_ = nullptr;

  if (reply->error() != QNetworkReply::NoError) {
    QString error = QStringLiteral("Failed to fetch bundle.js: %1").arg(reply->errorString());
    qLog(Error) << "Qobuz:" << error;
    reply->deleteLater();
    Q_EMIT CredentialsFetchError(error);
    return;
  }

  const QString bundle = QString::fromUtf8(reply->readAll());
  reply->deleteLater();

  qLog(Debug) << "Qobuz: Bundle size:" << bundle.length();

  const QString app_id = ExtractAppId(bundle);
  if (app_id.isEmpty()) {
    QString error = u"Failed to extract app_id from bundle"_s;
    qLog(Error) << "Qobuz:" << error;
    Q_EMIT CredentialsFetchError(error);
    return;
  }

  const QString app_secret = ExtractAppSecret(bundle);
  if (app_secret.isEmpty()) {
    QString error = u"Failed to extract app_secret from bundle"_s;
    qLog(Error) << "Qobuz:" << error;
    Q_EMIT CredentialsFetchError(error);
    return;
  }

  qLog(Debug) << "Qobuz: Successfully extracted credentials - app_id:" << app_id;
  Q_EMIT CredentialsFetched(app_id, app_secret);

}

QString QobuzCredentialFetcher::ExtractAppId(const QString &bundle) {

  // Pattern: production:{api:{appId:"(\d+)"
  static const QRegularExpression app_id_regex(u"production:\\{api:\\{appId:\"(\\d+)\""_s);
  const QRegularExpressionMatch app_id_match = app_id_regex.match(bundle);

  if (app_id_match.hasMatch()) {
    return app_id_match.captured(1);
  }

  return QString();

}

QString QobuzCredentialFetcher::ExtractAppSecret(const QString &bundle) {

  // The plain-text appSecret in the bundle doesn't work for API requests.
  // We need to use the Spoofbuz method to extract the real secrets:
  // 1. Find seed/timezone pairs
  // 2. Find info/extras for each timezone
  // 3. Combine seed + info + extras, remove last 44 chars, base64 decode

  // Pattern to find seed and timezone: [a-z].initialSeed("seed",window.utimezone.timezone)
  static const QRegularExpression seed_regex(u"[a-z]\\.initialSeed\\(\"([\\w=]+)\",window\\.utimezone\\.([a-z]+)\\)"_s);

  QMap<QString, QString> seeds;  // timezone -> seed
  QRegularExpressionMatchIterator seed_iter = seed_regex.globalMatch(bundle);
  while (seed_iter.hasNext()) {
    const QRegularExpressionMatch seed_match = seed_iter.next();
    const QString seed = seed_match.captured(1);
    const QString tz = seed_match.captured(2);
    seeds[tz] = seed;
    qLog(Debug) << "Qobuz: Found seed for timezone" << tz;
  }

  if (seeds.isEmpty()) {
    qLog(Error) << "Qobuz: No seed/timezone pairs found in bundle";
    return QString();
  }

  // Try each timezone - Berlin was confirmed working
  const QStringList preferred_order = {u"berlin"_s, u"london"_s, u"abidjan"_s};

  for (const QString &tz : preferred_order) {
    if (!seeds.contains(tz)) {
      continue;
    }

    // Pattern to find info and extras for this timezone
    // name:"xxx/Berlin",info:"...",extras:"..."
    const QString capitalized_tz = tz.at(0).toUpper() + tz.mid(1);
    const QString info_pattern = QStringLiteral("name:\"\\w+/%1\",info:\"([\\w=]+)\",extras:\"([\\w=]+)\"").arg(capitalized_tz);
    const QRegularExpression info_regex(info_pattern);
    const QRegularExpressionMatch info_match = info_regex.match(bundle);

    if (!info_match.hasMatch()) {
      qLog(Debug) << "Qobuz: No info/extras found for timezone" << tz;
      continue;
    }

    const QString seed = seeds[tz];
    const QString info = info_match.captured(1);
    const QString extras = info_match.captured(2);

    qLog(Debug) << "Qobuz: Decoding secret for timezone" << tz;

    // Combine seed + info + extras
    const QString combined = seed + info + extras;

    // Remove last 44 characters
    if (combined.length() <= 44) {
      qLog(Debug) << "Qobuz: Combined string too short for timezone" << tz;
      continue;
    }
    const QString trimmed = combined.left(combined.length() - 44);

    // Base64 decode
    const QByteArray decoded = QByteArray::fromBase64(trimmed.toLatin1());
    const QString secret = QString::fromLatin1(decoded);

    // Validate: should be 32 hex characters
    static const QRegularExpression hex_regex(u"^[a-f0-9]{32}$"_s);
    if (hex_regex.match(secret).hasMatch()) {
      qLog(Debug) << "Qobuz: Successfully decoded secret from timezone" << tz;
      return secret;
    }

    qLog(Debug) << "Qobuz: Decoded secret invalid for timezone" << tz;
  }

  // Try any remaining timezones not in preferred order
  for (auto it = seeds.constBegin(); it != seeds.constEnd(); ++it) {
    const QString &tz = it.key();
    if (preferred_order.contains(tz)) {
      continue;  // Already tried
    }

    const QString capitalized_tz = tz.at(0).toUpper() + tz.mid(1);
    const QString info_pattern = QStringLiteral("name:\"\\w+/%1\",info:\"([\\w=]+)\",extras:\"([\\w=]+)\"").arg(capitalized_tz);
    const QRegularExpression info_regex(info_pattern);
    const QRegularExpressionMatch info_match = info_regex.match(bundle);

    if (!info_match.hasMatch()) {
      continue;
    }

    const QString seed = it.value();
    const QString info = info_match.captured(1);
    const QString extras = info_match.captured(2);

    const QString combined = seed + info + extras;
    if (combined.length() <= 44) {
      continue;
    }
    const QString trimmed = combined.left(combined.length() - 44);

    const QByteArray decoded = QByteArray::fromBase64(trimmed.toLatin1());
    const QString secret = QString::fromLatin1(decoded);

    static const QRegularExpression hex_regex(u"^[a-f0-9]{32}$"_s);
    if (hex_regex.match(secret).hasMatch()) {
      qLog(Debug) << "Qobuz: Successfully decoded secret from timezone" << tz;
      return secret;
    }
  }

  qLog(Error) << "Qobuz: Failed to decode any valid app_secret from bundle";
  return QString();

}
