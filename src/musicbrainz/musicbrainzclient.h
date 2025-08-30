/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <QtGlobal>
#include <QObject>
#include <QList>
#include <QMap>
#include <QMultiMap>
#include <QVariant>
#include <QString>
#include <QStringList>

#include "includes/shared_ptr.h"

class QNetworkReply;
class QTimer;
class QXmlStreamReader;
class NetworkAccessManager;
class NetworkTimeouts;

class MusicBrainzClient : public QObject {
  Q_OBJECT

  // Gets metadata for a particular MBID.
  // An MBID is created from a fingerprint using MusicDnsClient.
  // You can create one MusicBrainzClient and make multiple requests using it.
  // IDs are provided by the caller when a request is started and included in the Finished signal - they have no meaning to MusicBrainzClient.

 public:
  // The second argument allows for specifying a custom network access manager.
  // It is used in tests. The ownership of network is not transferred.
  explicit MusicBrainzClient(SharedPtr<NetworkAccessManager> network, QObject *parent = nullptr);
  ~MusicBrainzClient() override;

  struct Result {
    Result() : duration_msec_(0), track_(0), year_(-1) {}

    bool operator<(const Result &other) const {
#define cmp(field)                      \
  if ((field) < other.field) return true; \
  if ((field) > other.field) return false;

      cmp(track_);
      cmp(year_);
      cmp(title_);
      cmp(artist_);
      return false;

#undef cmp
    }

    bool operator==(const Result &other) const {
      return
             title_ == other.title_ &&
             artist_ == other.artist_ &&
             album_ == other.album_ &&
             duration_msec_ == other.duration_msec_ &&
             track_ == other.track_ &&
             year_ == other.year_;
    }

    QString title_;
    QString artist_;
    QString album_;
    int duration_msec_;
    int track_;
    int year_;
  };
  using ResultList = QList<Result>;

  // Starts a request and returns immediately.  Finished() will be emitted later with the same ID.
  void Start(const int id, const QStringList &mbid);
  void StartDiscIdRequest(const QString &discid);

  // Cancels the request with the given ID.  Finished() will never be emitted for that ID.  Does nothing if there is no request with the given ID.
  void Cancel(int id);

  // Cancels all requests.  Finished() will never be emitted for any pending requests.
  void CancelAll();

 Q_SIGNALS:
  // Finished signal emitted when feching songs tags
  void Finished(const int id, const MusicBrainzClient::ResultList &result, const QString &error = QString());
  // Finished signal emitted when fechting album's songs tags using DiscId
  void DiscIdFinished(const QString &artist, const QString &album, const MusicBrainzClient::ResultList &result, const QString &error = QString());

 private Q_SLOTS:
  void FlushRequests();
  // id identifies the track, and request_number means it's the 'request_number'th request for this track
  void RequestFinished(QNetworkReply *reply, const int id, const int request_number);
  void DiscIdRequestFinished(const QString &discid, QNetworkReply *reply);

 private:
  using Param = QPair<QString, QString>;
  using ParamList = QList<Param>;

  struct Request {
    Request() : id(0), number(0) {}
    Request(const int _id, const QString &_mbid, const int _number) : id(_id), mbid(_mbid), number(_number) {}
    int id;
    QString mbid;
    int number;
  };

  // Used as parameter for UniqueResults
  enum class UniqueResultsSortOption {
    SortResults = 0,
    KeepOriginalOrder
  };

  struct Release {

    enum class Status {
      Unknown = 0,
      PseudoRelease,
      Bootleg,
      Promotional,
      Official
    };

    Release() : track_(0), year_(0), status_(Status::Unknown) {}

    Result CopyAndMergeInto(const Result &orig) const {
      Result ret(orig);
      ret.album_ = album_;
      ret.track_ = track_;
      ret.year_ = year_;
      return ret;
    }

    void SetStatusFromString(const QString &s) {
      if (s.compare(QLatin1String("Official"), Qt::CaseInsensitive) == 0) {
        status_ = Status::Official;
      }
      else if (s.compare(QLatin1String("Promotion"), Qt::CaseInsensitive) == 0) {
        status_ = Status::Promotional;
      }
      else if (s.compare(QLatin1String("Bootleg"), Qt::CaseInsensitive) == 0) {
        status_ = Status::Bootleg;
      }
      else if (s.compare(QLatin1String("Pseudo-release"), Qt::CaseInsensitive) == 0) {
        status_ = Status::PseudoRelease;
      }
      else {
        status_ = Status::Unknown;
      }
    }

    bool operator<(const Release &other) const {
      // Compare status so that "best" status (e.g. Official) will be first when sorting a list of releases.
      return status_ > other.status_;
    }

    QString album_;
    int track_;
    int year_;
    Status status_;
  };

  struct PendingResults {
    PendingResults(int sort_id, const ResultList &results) : sort_id_(sort_id), results_(results) {}

    bool operator<(const PendingResults &other) const {
      return sort_id_ < other.sort_id_;
    }

    int sort_id_;
    ResultList results_;
  };

  static QByteArray GetReplyData(QNetworkReply *reply, QString &error);
  static bool MediumHasDiscid(const QString &discid, QXmlStreamReader *reader);
  static ResultList ParseMedium(QXmlStreamReader *reader);
  static Result ParseTrackFromDisc(QXmlStreamReader *reader);
  static ResultList ParseTrack(QXmlStreamReader *reader);
  static void ParseArtist(QXmlStreamReader *reader, QString *artist);
  static Release ParseRelease(QXmlStreamReader *reader);
  static ResultList UniqueResults(const ResultList &results, UniqueResultsSortOption opt = UniqueResultsSortOption::SortResults);
  static void Error(const QString &error, const QVariant &debug = QVariant());

 private:
  SharedPtr<NetworkAccessManager> network_;
  NetworkTimeouts *timeouts_;
  QMultiMap<int, Request> requests_pending_;
  QMultiMap<int, QNetworkReply*> requests_;
  // Results we received so far, kept here until all the replies are finished
  QMap<int, QList<PendingResults>> pending_results_;
  QTimer *timer_flush_requests_;

};

inline size_t qHash(const MusicBrainzClient::Result &result) {
  return qHash(result.album_) ^ qHash(result.artist_) ^ result.duration_msec_ ^ qHash(result.title_) ^ result.track_ ^ result.year_;
}

#endif  // MUSICBRAINZCLIENT_H
