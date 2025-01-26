/*
 * Strawberry Music Player
 * Copyright 2024-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef OPENTIDALCOVERPROVIDER_H
#define OPENTIDALCOVERPROVIDER_H

#include "config.h"

#include <QQueue>
#include <QVariant>
#include <QString>
#include <QDateTime>

#include "includes/shared_ptr.h"
#include "jsoncoverprovider.h"

class QNetworkReply;
class NetworkAccessManager;
class QTimer;
class OAuthenticator;

class OpenTidalCoverProvider : public JsonCoverProvider {
  Q_OBJECT

 public:
  explicit OpenTidalCoverProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);

  bool StartSearch(const QString &artist, const QString &album, const QString &title, const int id) override;
  void CancelSearch(const int id) override;

 private:
  struct SearchRequest {
    explicit SearchRequest(const int _id, const QString &_artist, const QString &_album, const QString &_title) : id(_id), artist(_artist), album(_album), title(_title) {}
    int id;
    QString artist;
    QString album;
    QString title;
  };
  using SearchRequestPtr = SharedPtr<SearchRequest>;

 private:
  void LoginCheck();
  void Login();
  void SendSearchRequest(SearchRequestPtr request);
  JsonObjectResult ParseJsonObject(QNetworkReply *reply);
  void FinishAllSearches();
  void Error(const QString &error, const QVariant &debug = QVariant()) override;

 private Q_SLOTS:
  void OAuthFinished(const bool success, const QString &error = QString());
  void FlushRequests();
  void HandleSearchReply(QNetworkReply *reply, OpenTidalCoverProvider::SearchRequestPtr search_request);

 private:
  OAuthenticator *oauth_;
  QTimer *timer_flush_requests_;
  bool login_in_progress_;
  QDateTime last_login_attempt_;
  QQueue<SearchRequestPtr> search_requests_queue_;
};

#endif  // OPENTIDALCOVERPROVIDER_H
