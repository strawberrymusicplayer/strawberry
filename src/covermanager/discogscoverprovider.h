/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, Martin Björklund <mbj4668@gmail.com>
 * Copyright 2016-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef DISCOGSCOVERPROVIDER_H
#define DISCOGSCOVERPROVIDER_H

#include "config.h"

#include <QQueue>
#include <QMap>
#include <QVariant>
#include <QByteArray>
#include <QString>

#include "includes/shared_ptr.h"
#include "jsoncoverprovider.h"
#include "albumcoverfetcher.h"

class NetworkAccessManager;
class QNetworkReply;
class QTimer;

class DiscogsCoverProvider : public JsonCoverProvider {
  Q_OBJECT

 public:
  explicit DiscogsCoverProvider(const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);
  ~DiscogsCoverProvider() override;

  bool StartSearch(const QString &artist, const QString &album, const QString &title, const int id) override;
  void CancelSearch(const int id) override;

  enum class DiscogsCoverType {
    Master,
    Release
  };

  struct DiscogsCoverReleaseContext {
    explicit DiscogsCoverReleaseContext(const int _search_id = 0, const quint64 _id = 0, const QUrl &_url = QUrl()) : search_id(_search_id), id(_id), url(_url) {}
    int search_id;
    quint64 id;
    QUrl url;
  };
  struct DiscogsCoverSearchContext {
    explicit DiscogsCoverSearchContext(const int _id = 0, const QString &_artist = QString(), const QString &_album = QString(), const DiscogsCoverType _type = DiscogsCoverType::Master) : id(_id), artist(_artist), album(_album), type(_type) {}
    int id;
    QString artist;
    QString album;
    DiscogsCoverType type;
    QMap<quint64, DiscogsCoverReleaseContext> requests_release_;
    CoverProviderSearchResults results;
  };

 private:
  void SendSearchRequest(SharedPtr<DiscogsCoverSearchContext> search);
  void SendReleaseRequest(const DiscogsCoverReleaseContext &release);
  QNetworkReply *CreateRequest(const QUrl &url, const ParamList &params = ParamList());
  void StartReleaseRequest(SharedPtr<DiscogsCoverSearchContext> search, const quint64 release_id, const QUrl &url);
  JsonObjectResult ParseJsonObject(QNetworkReply *reply);
  void EndSearch(SharedPtr<DiscogsCoverSearchContext> search, const quint64 release_id = 0);
  void Error(const QString &error, const QVariant &debug = QVariant()) override;

 private Q_SLOTS:
  void FlushRequests();
  void HandleSearchReply(QNetworkReply *reply, const int id);
  void HandleReleaseReply(QNetworkReply *reply, const int search_id, const quint64 release_id);

 private:
  QTimer *timer_flush_requests_;
  QQueue<SharedPtr<DiscogsCoverSearchContext>> queue_search_requests_;
  QQueue<DiscogsCoverReleaseContext> queue_release_requests_;
  QMap<int, SharedPtr<DiscogsCoverSearchContext>> requests_search_;
};

Q_DECLARE_METATYPE(DiscogsCoverProvider::DiscogsCoverSearchContext)
Q_DECLARE_METATYPE(DiscogsCoverProvider::DiscogsCoverReleaseContext)

#endif  // DISCOGSCOVERPROVIDER_H
