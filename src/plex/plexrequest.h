/*
 * Strawberry Music Player
 * Copyright 2025-2026, Strawberry Music Player contributors
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

#ifndef PLEXREQUEST_H
#define PLEXREQUEST_H

#include "config.h"

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QSet>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QJsonObject>

#include "core/song.h"
#include "plexbaserequest.h"

class QNetworkReply;
class PlexService;
class PlexUrlHandler;

class PlexRequest : public PlexBaseRequest {
  Q_OBJECT

 public:
  explicit PlexRequest(PlexService *service, const SharedPtr<NetworkAccessManager> network, PlexUrlHandler *url_handler, const SongMap &existing_songs = SongMap(), const QStringList &rejected_song_ids = QStringList(), const qint64 updated_since = 0, QObject *parent = nullptr);
  ~PlexRequest() override;

  void GetLibrarySections();

  qint64 newest_updated_at() const { return newest_updated_at_; }
  bool incremental() const { return incremental_; }
  QStringList rejected_song_ids() const { return QStringList(rejected_song_ids_.cbegin(), rejected_song_ids_.cend()); }

 Q_SIGNALS:
  void Results(const SongMap &songs, const QString &error);
  void UpdateStatus(const QString &text);
  void ProgressSetMaximum(const int max);
  void UpdateProgress(const int progress);

 private:
  void HandleLibrarySectionsReply(QNetworkReply *reply);
  void GetSectionSongs(const QString &section_key, const int offset = 0);
  void HandleSectionSongsReply(QNetworkReply *reply, const QString &section_key, const int offset_requested);
  void GetSectionCount(const QString &section_key);
  void HandleSectionCountReply(QNetworkReply *reply);
  void ParseSong(const QJsonObject &object_metadata);
  void RejectSong(const QString &song_id, const QString &reason, const QJsonObject &object_metadata);

  void SongsFinishCheck();
  void FinishCheck();
  void Error(const QString &error, const QVariant &debug = QVariant()) override;

  PlexUrlHandler *url_handler_;

  bool finished_;
  bool incremental_;
  qint64 updated_since_;
  qint64 newest_updated_at_;
  int server_total_songs_;

  int songs_requests_active_;
  int songs_total_;
  int songs_received_;

  SongMap songs_;
  QSet<QString> rejected_song_ids_;  // Songs the server counts, but which could not be parsed.
  QStringList errors_;
  QList<QNetworkReply*> replies_;
};

#endif  // PLEXREQUEST_H
