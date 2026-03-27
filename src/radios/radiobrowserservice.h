/*
 * Strawberry Music Player
 * Copyright 2026, Malte Zilinski <malte@zilinski.eu>
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

#ifndef RADIOBROWSERSERVICE_H
#define RADIOBROWSERSERVICE_H

#include <QObject>
#include <QPair>
#include <QList>
#include <QString>
#include <QUrl>

#include "radioservice.h"
#include "radiochannel.h"

class QNetworkReply;
class QDnsLookup;

class TaskManager;
class NetworkAccessManager;

class RadioBrowserService : public RadioService {
  Q_OBJECT

 public:
  explicit RadioBrowserService(const SharedPtr<TaskManager> task_manager, const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);
  ~RadioBrowserService();

  QUrl Homepage() override;
  QUrl Donate() override;

  void Abort();

  void Search(const QString &query,
              const QString &country = QString(),
              const QString &tag = QString(),
              const QString &language = QString(),
              const QString &order = QString(),
              const int limit = 100,
              const int offset = 0);

  void FetchCountries();

 Q_SIGNALS:
  void SearchFinished(const RadioChannelList &channels, const bool has_more);
  void SearchError(const QString &error);
  void CountriesLoaded(const QList<QPair<QString, QString>> &countries);

 public Q_SLOTS:
  void GetChannels() override;

 private Q_SLOTS:
  void DnsLookupFinished();
  void ServerTestReply(QNetworkReply *reply);
  void SearchReply(QNetworkReply *reply, const int task_id, const int limit);
  void CountriesReply(QNetworkReply *reply);

 private:
  void DiscoverServer();
  void TestServer(const QString &hostname);

  QList<QNetworkReply*> replies_;
  QDnsLookup *dns_lookup_;
  QUrl server_url_;
  bool server_discovered_;

  // Pending search to execute after server discovery
  struct PendingSearch {
    QString query;
    QString country;
    QString tag;
    QString language;
    QString order;
    int limit;
    int offset;
  };
  PendingSearch pending_search_;
  bool has_pending_search_;
  bool has_pending_countries_;

  static const QStringList kFallbackServers;
  int fallback_index_;
};

#endif  // RADIOBROWSERSERVICE_H
