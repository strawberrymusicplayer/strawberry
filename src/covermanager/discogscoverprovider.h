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
#include <QMap>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QJsonObject>

#include "jsoncoverprovider.h"
#include "albumcoverfetcher.h"

class QNetworkAccessManager;
class QNetworkReply;
class Application;

class DiscogsCoverProvider : public JsonCoverProvider {
  Q_OBJECT

 public:
  explicit DiscogsCoverProvider(Application *app, QObject *parent = nullptr);
  ~DiscogsCoverProvider();

  bool StartSearch(const QString &artist, const QString &album, const QString &title, const int id);

  void CancelSearch(const int id);

 private slots:
  void HandleSearchReply(QNetworkReply *reply, const int id);
  void HandleReleaseReply(QNetworkReply *reply, const int id, const quint64 release_id);

 public:
  struct DiscogsCoverReleaseContext {
    explicit DiscogsCoverReleaseContext(const quint64 _id = 0, const QUrl &_url = QUrl()) : id(_id), url(_url) {}
    quint64 id;
    QUrl url;
  };
  struct DiscogsCoverSearchContext {
    explicit DiscogsCoverSearchContext() : id(-1) {}
    int id;
    QString artist;
    QString album;
    QMap<quint64, DiscogsCoverReleaseContext> requests_release_;
    CoverSearchResults results;
  };

 private:
  typedef QPair<QString, QString> Param;
  typedef QList<Param> ParamList;

  QNetworkReply *CreateRequest(QUrl url, const ParamList &params_provided = ParamList());
  QByteArray GetReplyData(QNetworkReply *reply);
  void StartRelease(std::shared_ptr<DiscogsCoverSearchContext> search, const quint64 release_id, const QUrl &url);
  void EndSearch(std::shared_ptr<DiscogsCoverSearchContext> search, const DiscogsCoverReleaseContext &release = DiscogsCoverReleaseContext());
  void Error(const QString &error, const QVariant &debug = QVariant());

 private:
  static const char *kUrlSearch;
  static const char *kAccessKeyB64;
  static const char *kSecretKeyB64;

  QNetworkAccessManager *network_;
  QMap<int, std::shared_ptr<DiscogsCoverSearchContext>> requests_search_;

};

Q_DECLARE_METATYPE(DiscogsCoverProvider::DiscogsCoverSearchContext)
Q_DECLARE_METATYPE(DiscogsCoverProvider::DiscogsCoverReleaseContext)

#endif  // DISCOGSCOVERPROVIDER_H
