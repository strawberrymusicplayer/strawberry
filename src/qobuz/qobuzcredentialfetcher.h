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

#ifndef QOBUZCREDENTIALFETCHER_H
#define QOBUZCREDENTIALFETCHER_H

#include "config.h"

#include <QObject>
#include <QString>

#include "includes/shared_ptr.h"

class QNetworkReply;
class NetworkAccessManager;

class QobuzCredentialFetcher : public QObject {
  Q_OBJECT

 public:
  explicit QobuzCredentialFetcher(const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

  void FetchCredentials();

 Q_SIGNALS:
  void CredentialsFetched(const QString &app_id, const QString &app_secret);
  void CredentialsFetchError(const QString &error);

 private Q_SLOTS:
  void LoginPageReceived();
  void BundleReceived();

 private:
  QString ExtractAppId(const QString &bundle);
  QString ExtractAppSecret(const QString &bundle);

  const SharedPtr<NetworkAccessManager> network_;
  QNetworkReply *login_page_reply_;
  QNetworkReply *bundle_reply_;
  QString bundle_url_;
};

#endif  // QOBUZCREDENTIALFETCHER_H
