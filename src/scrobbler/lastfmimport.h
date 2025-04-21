/*
 * Strawberry Music Player
 * Copyright 2020-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#ifndef LASTFMIMPORT_H
#define LASTFMIMPORT_H

#include "config.h"

#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QQueue>
#include <QDateTime>

#include "core/jsonbaserequest.h"
#include "includes/shared_ptr.h"
#include "core/jsonbaserequest.h"

class QTimer;
class QNetworkReply;

class NetworkAccessManager;

class LastFMImport : public JsonBaseRequest {
  Q_OBJECT

 public:
  explicit LastFMImport(const SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);
  ~LastFMImport() override;

  QString service_name() const override { return QStringLiteral("LastFMImport"); }
  bool authentication_required() const override { return false; }
  bool authenticated() const override { return false; }
  bool use_authorization_header() const override { return false; }
  QByteArray authorization_header() const override { return QByteArray(); }

  void ReloadSettings();
  void ImportData(const bool lastplayed = true, const bool playcount = true);
  void AbortAll();

 private:
  using Param = QPair<QString, QString>;
  using ParamList = QList<Param>;

  struct GetRecentTracksRequest {
    explicit GetRecentTracksRequest(const int _page) : page(_page) {}
    int page;
  };
  struct GetTopTracksRequest {
    explicit GetTopTracksRequest(const int _page) : page(_page) {}
    int page;
  };

 private:
  QNetworkReply *CreateRequest(const ParamList &request_params);
  JsonObjectResult ParseJsonObject(QNetworkReply *reply);

  void AddGetRecentTracksRequest(const int page = 0);
  void AddGetTopTracksRequest(const int page = 0);

  void SendGetRecentTracksRequest(GetRecentTracksRequest request);
  void SendGetTopTracksRequest(GetTopTracksRequest request);

  void Error(const QString &error, const QVariant &debug = QVariant()) override;

  void UpdateTotalCheck();
  void UpdateProgressCheck();

  void FinishCheck();

 Q_SIGNALS:
  void UpdatePlayCount(const QString&, const QString&, const int, const bool = false);
  void UpdateLastPlayed(const QString&, const QString&, const QString&, const qint64);
  void UpdateTotal(const int, const int);
  void UpdateProgress(const int, const int);
  void Finished();
  void FinishedWithError(const QString &error);

 private Q_SLOTS:
  void FlushRequests();
  void GetRecentTracksRequestFinished(QNetworkReply *reply, const int page);
  void GetTopTracksRequestFinished(QNetworkReply *reply, const int page);

 private:
  SharedPtr<NetworkAccessManager> network_;
  QTimer *timer_flush_requests_;

  QString username_;
  bool lastplayed_;
  bool playcount_;
  int playcount_total_;
  int lastplayed_total_;
  int playcount_received_;
  int lastplayed_received_;
  QQueue<GetRecentTracksRequest> recent_tracks_requests_;
  QQueue<GetTopTracksRequest> top_tracks_requests_;
};

#endif  // LASTFMIMPORT_H
