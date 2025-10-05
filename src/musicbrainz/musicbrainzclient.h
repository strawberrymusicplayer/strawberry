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

#ifndef MUSICBRAINZCLIENT_H
#define MUSICBRAINZCLIENT_H

#include "config.h"

#include <tuple>

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QMap>
#include <QMultiMap>
#include <QVariant>
#include <QString>
#include <QStringList>

#include "includes/shared_ptr.h"
#include "core/jsonbaserequest.h"

class QNetworkReply;
class QTimer;
class QJsonValue;
class QJsonObject;
class QJsonArray;
class NetworkAccessManager;
class NetworkTimeouts;

class MusicBrainzClient : public JsonBaseRequest {
  Q_OBJECT

 public:
  explicit MusicBrainzClient(SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);
  ~MusicBrainzClient() override;

  virtual QString service_name() const override { return QLatin1String("MusicBrainz"); }
  virtual bool authentication_required() const override { return false; }
  virtual bool authenticated() const override { return true; }
  virtual bool use_authorization_header() const override { return false; }
  virtual QByteArray authorization_header() const override { return QByteArray(); }

  struct Result {
   public:
    Result() : duration_msec_(0), track_(0), year_(-1) {}

    bool operator<(const Result &other) const {
        return std::tie(title_, artist_, sort_artist_, album_artist_, sort_album_artist_, album_, duration_msec_, track_, year_)
             < std::tie(other.title_, other.artist_, other.sort_artist_, other.album_artist_, other.sort_album_artist_, other.album_, other.duration_msec_, other.track_, other.year_);
    }

    bool operator==(const Result &other) const {
      return title_ == other.title_ &&
             artist_ == other.artist_ &&
             sort_artist_ == other.sort_artist_ &&
             album_artist_ == other.album_artist_ &&
             sort_album_artist_ == other.sort_album_artist_ &&
             album_ == other.album_ &&
             duration_msec_ == other.duration_msec_ &&
             track_ == other.track_ &&
             year_ == other.year_;
    }

    QString title_;
    QString artist_;
    QString sort_artist_;
    QString album_artist_;
    QString sort_album_artist_;
    QString album_;
    int duration_msec_;
    int track_;
    int year_;
  };
  using ResultList = QList<Result>;

  void StartMbIdRequest(const int id, const QStringList &mbid);
  void CancelMbIdRequest(const int id);

  void StartDiscIdRequest(const QString &discid);
  void CancelDiscIdRequest(const QString &disc_id);

  void CancelAll();

 Q_SIGNALS:
  void MbIdFinished(const int id, const MusicBrainzClient::ResultList &result, const QString &error = QString());
  void DiscIdFinished(const QString &disc_id, const MusicBrainzClient::ResultList &result, const QString &error = QString());

 private Q_SLOTS:
  void FlushRequests();
  // ID identifies the track, and request_number means it's the 'request_number'th request for this track
  void MbIdRequestFinished(QNetworkReply *reply, const int id, const int request_number);
  void DiscIdRequestFinished(const QString &discid, QNetworkReply *reply);

 private:
  class MbIdRequest {
   public:
    MbIdRequest() : id(0), number(0) {}
    MbIdRequest(const int _id, const QString &_mbid, const int _number) : id(_id), mbid(_mbid), number(_number) {}
    int id;
    QString mbid;
    int number;
  };

  enum class UniqueResultsSortOption {
    SortResults = 0,
    KeepOriginalOrder
  };

  class Artist {
   public:
    QString name_;
    QString sort_name_;
  };

  class Track {
   public:
    Track() : number_(0), duration_msec_(0) {}
    int number_;
    QString title_;
    Artist artist_;
    int duration_msec_;
  };
  using TrackList = QList<Track>;

  class Media {
   public:
    QList<Track> tracks_;
    QStringList disc_ids_;
  };
  using MediaList = QList<Media>;

  class Release {
   public:
    Release() : year_(0) {}

    Artist artist_;
    QString album_;
    int year_;
    QList<Media> media_;
  };
  using ReleaseList = QList<Release>;

  class PendingResults {
   public:
    PendingResults(const int sort_id, const ResultList &results) : sort_id_(sort_id), results_(results) {}

    bool operator<(const PendingResults &other) const {
      return sort_id_ < other.sort_id_;
    }

    int sort_id_;
    ResultList results_;
  };

  JsonObjectResult ParseJsonObject(QNetworkReply *reply);
  void FlushMbIdRequests();
  void FlushDiscIdRequests();
  void SendMbIdRequest(const MbIdRequest &request);
  void SendDiscIdRequest(const QString &disc_id);

  static ReleaseList ParseReleases(const QJsonArray &array_releases);
  static Release ParseRelease(const QJsonObject &object_release);
  static MediaList ParseMediaList(const QJsonArray &array_media_list);
  static Media ParseMedia(const QJsonObject &object_media);
  static Artist ParseArtistCredit(const QJsonArray &array_artist_credits);
  static QStringList ParseDiscIds(const QJsonArray &array_discs);
  static TrackList ParseTracks(const QJsonArray &array_tracks);
  static Track ParseTrack(const QJsonObject &object_track);
  static QString ParseDate(const QString &date_str);
  static ResultList ResultListFromReleases(const ReleaseList &releases, const QString &disc_id = QString());
  static ResultList UniqueResults(const ResultList &results, const UniqueResultsSortOption opt = UniqueResultsSortOption::SortResults);

 private:
  SharedPtr<NetworkAccessManager> network_;
  NetworkTimeouts *timeouts_;

  QMultiMap<int, MbIdRequest> pending_mbid_requests_;
  QList<QString> pending_discid_requests_;

  QMultiMap<int, QNetworkReply*> mbid_requests_;
  QMultiMap<QString, QNetworkReply*> discid_requests_;

  QMap<int, QList<PendingResults>> pending_results_;
  QTimer *timer_flush_requests_;
};

inline size_t qHash(const MusicBrainzClient::Result &result) {
  return qHash(result.album_) ^ qHash(result.artist_) ^ qHash(result.sort_artist_) ^ qHash(result.album_artist_) ^ qHash(result.sort_album_artist_) ^ result.duration_msec_ ^ qHash(result.title_) ^ result.track_ ^ result.year_;
}

#endif  // MUSICBRAINZCLIENT_H
