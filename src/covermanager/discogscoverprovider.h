/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2012, Martin Björklund <mbj4668@gmail.com>
 * Copyright 2016, Jonas Kvinge <jonas@jkvinge.net>
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

#include <memory>

#include <QObject>
#include <QMetaType>
#include <QPair>
#include <QList>
#include <QQueue>
#include <QMap>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QJsonObject>

#include "jsoncoverprovider.h"
#include "albumcoverfetcher.h"

class NetworkAccessManager;
class QNetworkReply;
class QTimer;
class Application;

class DiscogsCoverProvider : public JsonCoverProvider {
  Q_OBJECT

 public:
  explicit DiscogsCoverProvider(Application *app, QObject *parent = nullptr);
  ~DiscogsCoverProvider() override;

  bool StartSearch(const QString &artist, const QString &album, const QString &title, const int id) override;
  void CancelSearch(const int id) override;

  enum DiscogsCoverType {
    DiscogsCoverType_Master,
    DiscogsCoverType_Release,
  };

  struct DiscogsCoverReleaseContext {
    explicit DiscogsCoverReleaseContext(const quint64 _search_id = 0, const quint64 _id = 0, const QUrl &_url = QUrl()) : search_id(_search_id), id(_id), url(_url) {}
    quint64 search_id;
    quint64 id;
    QUrl url;
  };
  struct DiscogsCoverSearchContext {
    explicit DiscogsCoverSearchContext(const int _id = 0, const QString &_artist = QString(), const QString &_album = QString(), const DiscogsCoverType _type = DiscogsCoverType_Master) : id(_id), artist(_artist), album(_album), type(_type) {}
    int id;
    QString artist;
    QString album;
    DiscogsCoverType type;
    QMap<quint64, DiscogsCoverReleaseContext> requests_release_;
    CoverSearchResults results;
  };

 private:
  typedef QPair<QString, QString> Param;
  typedef QList<Param> ParamList;

  void SendSearchRequest(std::shared_ptr<DiscogsCoverSearchContext> search);
  void SendReleaseRequest(const DiscogsCoverReleaseContext release);
  QNetworkReply *CreateRequest(QUrl url, const ParamList &params_provided = ParamList());
  QByteArray GetReplyData(QNetworkReply *reply);
  void StartReleaseRequest(std::shared_ptr<DiscogsCoverSearchContext> search, const quint64 release_id, const QUrl &url);
  void EndSearch(std::shared_ptr<DiscogsCoverSearchContext> search, const quint64 release_id = 0);
  void Error(const QString &error, const QVariant &debug = QVariant()) override;

 private slots:
  void FlushRequests();
  void HandleSearchReply(QNetworkReply *reply, const int id);
  void HandleReleaseReply(QNetworkReply *reply, const int id, const quint64 release_id);

 private:
  static const char *kUrlSearch;
  static const char *kAccessKeyB64;
  static const char *kSecretKeyB64;
  static const int kRequestsDelay;

  NetworkAccessManager *network_;
  QTimer *timer_flush_requests_;
  QQueue<std::shared_ptr<DiscogsCoverSearchContext>> queue_search_requests_;
  QQueue<DiscogsCoverReleaseContext> queue_release_requests_;
  QMap<int, std::shared_ptr<DiscogsCoverSearchContext>> requests_search_;
  QList<QNetworkReply*> replies_;

};

Q_DECLARE_METATYPE(DiscogsCoverProvider::DiscogsCoverSearchContext)
Q_DECLARE_METATYPE(DiscogsCoverProvider::DiscogsCoverReleaseContext)

#endif  // DISCOGSCOVERPROVIDER_H
