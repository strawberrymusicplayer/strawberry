/*
 * Strawberry Music Player
 * Copyright 2020, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QVariant>
#include <QByteArray>
#include <QString>
#include <QQueue>
#include <QDateTime>

class QTimer;
class QNetworkReply;

class NetworkAccessManager;

class LastFMImport : public QObject {
  Q_OBJECT

 public:
  explicit LastFMImport(QObject *parent = nullptr);
  ~LastFMImport() override;

  void ReloadSettings();
  void ImportData(const bool lastplayed = true, const bool playcount = true);
  void AbortAll();

 private:
  typedef QPair<QString, QString> Param;
  typedef QList<Param> ParamList;

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
  QByteArray GetReplyData(QNetworkReply *reply);
  QJsonObject ExtractJsonObj(const QByteArray &data);

  void AddGetRecentTracksRequest(const int page = 0);
  void AddGetTopTracksRequest(const int page = 0);

  void SendGetRecentTracksRequest(GetRecentTracksRequest request);
  void SendGetTopTracksRequest(GetTopTracksRequest request);

  void Error(const QString &error, const QVariant &debug = QVariant());

  void UpdateTotal();
  void UpdateProgress();

  void FinishCheck();

 signals:
  void UpdatePlayCount(QString, QString, int);
  void UpdateLastPlayed(QString, QString, QString, qint64);
  void UpdateTotal(int, int);
  void UpdateProgress(int, int);
  void Finished();
  void FinishedWithError(QString);

 private slots:
  void FlushRequests();
  void GetRecentTracksRequestFinished(QNetworkReply *reply, const int page);
  void GetTopTracksRequestFinished(QNetworkReply *reply, const int page);

 private:
  static const int kRequestsDelay;

  NetworkAccessManager *network_;
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
  QList<QNetworkReply*> replies_;

};

#endif  // LASTFMIMPORT_H
