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

#include "config.h"

#include <algorithm>
#include <utility>

#include <QSet>
#include <QList>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/networktimeouts.h"
#include "core/jsonbaserequest.h"
#include "musicbrainzclient.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kTrackUrl[] = "https://musicbrainz.org/ws/2/recording/";
constexpr char kDiscUrl[] = "https://musicbrainz.org/ws/2/discid/";
constexpr char kDateRegex[] = "^[12]\\d{3}";
constexpr int kRequestsDelay = 1200;
constexpr int kDefaultTimeout = 8000;
constexpr int kMaxRequestPerTrack = 3;
}  // namespace

MusicBrainzClient::MusicBrainzClient(SharedPtr<NetworkAccessManager> network, QObject *parent)
    : JsonBaseRequest(network, parent),
      network_(network),
      timeouts_(new NetworkTimeouts(kDefaultTimeout, this)),
      timer_flush_requests_(new QTimer(this)) {

  timer_flush_requests_->setInterval(kRequestsDelay);
  timer_flush_requests_->setSingleShot(true);
  QObject::connect(timer_flush_requests_, &QTimer::timeout, this, &MusicBrainzClient::FlushRequests);

}

MusicBrainzClient::~MusicBrainzClient() {

  CancelAll();

}

void MusicBrainzClient::FlushRequests() {

  FlushMbIdRequests();
  FlushDiscIdRequests();

  if (pending_mbid_requests_.isEmpty() && pending_discid_requests_.isEmpty() && timer_flush_requests_->isActive()) {
    timer_flush_requests_->stop();
  }

}

void MusicBrainzClient::CancelAll() {

  replies_.clear();

  qDeleteAll(mbid_requests_);
  mbid_requests_.clear();

  qDeleteAll(discid_requests_);
  discid_requests_.clear();

}

JsonBaseRequest::JsonObjectResult MusicBrainzClient::ParseJsonObject(QNetworkReply *reply) {

  if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
    return ReplyDataResult(ErrorCode::NetworkError, QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
  }

  JsonObjectResult result(ErrorCode::Success);
  result.network_error = reply->error();
  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
    result.http_status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  }

  const QByteArray data = reply->readAll();
  if (!data.isEmpty()) {
    QJsonParseError json_parse_error;
    const QJsonDocument json_document = QJsonDocument::fromJson(data, &json_parse_error);
    if (json_parse_error.error == QJsonParseError::NoError) {
      const QJsonObject json_object = json_document.object();
      if (json_object.contains("error"_L1)) {
        result.error_code = ErrorCode::APIError;
        result.error_message = json_object["error"_L1].toString();
      }
      else {
        result.json_object = json_document.object();
      }
    }
    else {
      result.error_code = ErrorCode::ParseError;
      result.error_message = json_parse_error.errorString();
    }
  }

  if (result.error_code != ErrorCode::APIError) {
    if (reply->error() != QNetworkReply::NoError) {
      result.error_code = ErrorCode::NetworkError;
      result.error_message = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
    }
    else if (result.http_status_code != 200) {
      result.error_code = ErrorCode::HttpError;
      result.error_message = QStringLiteral("Received HTTP code %1").arg(result.http_status_code);
    }
  }

  return result;

}

void MusicBrainzClient::StartMbIdRequest(const int id, const QStringList &mbid_list) {

  qLog(Debug) << "Starting MusicBrainz MBID request for" << mbid_list;

  int request_number = 0;
  for (const QString &mbid : mbid_list) {
    ++request_number;
    if (request_number > kMaxRequestPerTrack) break;
    pending_mbid_requests_.insert(id, MbIdRequest(id, mbid, request_number));
  }

  if (!timer_flush_requests_->isActive()) {
    timer_flush_requests_->start();
  }

}

void MusicBrainzClient::CancelMbIdRequest(const int id) {

  while (!mbid_requests_.isEmpty() && mbid_requests_.contains(id)) {
    QNetworkReply *reply = mbid_requests_.take(id);
    replies_.removeAll(reply);
    QObject::disconnect(reply, nullptr, this, nullptr);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

}

void MusicBrainzClient::FlushMbIdRequests() {

  if (!mbid_requests_.isEmpty() || pending_mbid_requests_.isEmpty()) return;

  SendMbIdRequest(pending_mbid_requests_.take(pending_mbid_requests_.firstKey()));

}

void MusicBrainzClient::SendMbIdRequest(const MbIdRequest &request) {

  QUrlQuery url_query;
  url_query.addQueryItem(u"inc"_s, u"releases+media+artists"_s);
  url_query.addQueryItem(u"status"_s, u"official"_s);
  url_query.addQueryItem(u"fmt"_s, u"json"_s);

  QNetworkReply *reply = CreateGetRequest(QUrl(QString::fromLatin1(kTrackUrl) + request.mbid), url_query);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { MbIdRequestFinished(reply, request.id, request.number); });
  mbid_requests_.insert(request.id, reply);

  timeouts_->AddReply(reply);

}

void MusicBrainzClient::MbIdRequestFinished(QNetworkReply *reply, const int id, const int request_number) {

  if (replies_.contains(reply)) {
    replies_.removeAll(reply);
  }
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (mbid_requests_.contains(id, reply)) {
    mbid_requests_.remove(id, reply);
  }

  if (!timer_flush_requests_->isActive() && mbid_requests_.isEmpty() && !pending_mbid_requests_.isEmpty()) {
    timer_flush_requests_->start();
  }

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Q_EMIT MbIdFinished(id, ResultList(), json_object_result.error_message);
    return;
  }
  const QJsonObject object_root = json_object_result.json_object;

  if (object_root.contains("releases"_L1) && object_root.value("releases"_L1).isArray()) {
    const ReleaseList releases = ParseReleases(object_root.value("releases"_L1).toArray());
    if (!releases.isEmpty()) {
      pending_results_[id] << PendingResults(request_number, ResultListFromReleases(releases));
    }
  }

  if (!mbid_requests_.contains(id) && !pending_mbid_requests_.contains(id)) {
    QList<PendingResults> pending_results_list = pending_results_.take(id);
    std::sort(pending_results_list.begin(), pending_results_list.end());
    ResultList results;
    for (const PendingResults &pending_results : std::as_const(pending_results_list)) {
      results << pending_results.results_;
    }
    Q_EMIT MbIdFinished(id, UniqueResults(results, UniqueResultsSortOption::KeepOriginalOrder));
  }

}

void MusicBrainzClient::StartDiscIdRequest(const QString &disc_id) {

  qLog(Debug) << "Starting MusicBrainz Disc ID request for" << disc_id;

  pending_discid_requests_ << disc_id;

  if (!timer_flush_requests_->isActive()) {
    timer_flush_requests_->start();
  }

}

void MusicBrainzClient::CancelDiscIdRequest(const QString &disc_id) {

  while (!discid_requests_.isEmpty() && discid_requests_.contains(disc_id)) {
    QNetworkReply *reply = discid_requests_.take(disc_id);
    replies_.removeAll(reply);
    QObject::disconnect(reply, nullptr, this, nullptr);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

}

void MusicBrainzClient::FlushDiscIdRequests() {

  if (!discid_requests_.isEmpty() || pending_discid_requests_.isEmpty()) return;

  SendDiscIdRequest(pending_discid_requests_.takeFirst());

}

void MusicBrainzClient::SendDiscIdRequest(const QString &disc_id) {

  QUrlQuery url_query;
  url_query.addQueryItem(u"inc"_s, u"recordings+artists"_s);
  url_query.addQueryItem(u"fmt"_s, u"json"_s);

  QNetworkReply *reply = CreateGetRequest(QUrl(QString::fromLatin1(kDiscUrl) + disc_id), url_query);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, disc_id, reply]() { DiscIdRequestFinished(disc_id, reply); });

  timeouts_->AddReply(reply);

}

void MusicBrainzClient::DiscIdRequestFinished(const QString &disc_id, QNetworkReply *reply) {

  if (replies_.contains(reply)) {
    replies_.removeAll(reply);
  }
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (discid_requests_.contains(disc_id)) {
    discid_requests_.remove(disc_id);
  }

  if (!timer_flush_requests_->isActive() && discid_requests_.isEmpty() && !pending_discid_requests_.isEmpty()) {
    timer_flush_requests_->start();
  }

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Q_EMIT DiscIdFinished(disc_id, ResultList(), json_object_result.error_message);
    return;
  }
  const QJsonObject object_root = json_object_result.json_object;

  ResultList results;
  if (object_root.contains("releases"_L1) && object_root.value("releases"_L1).isArray()) {
    const ReleaseList releases = ParseReleases(object_root.value("releases"_L1).toArray());
    if (!releases.isEmpty()) {
      results = ResultListFromReleases(ReleaseList() << releases.first(), disc_id);
    }
  }

  Q_EMIT DiscIdFinished(disc_id, UniqueResults(results, UniqueResultsSortOption::SortResults));

}

MusicBrainzClient::ReleaseList MusicBrainzClient::ParseReleases(const QJsonArray &array_releases) {

  ReleaseList releases;
  for (const QJsonValue &value_release : array_releases) {
    if (!value_release.isObject()) continue;
    releases << ParseRelease(value_release.toObject());
  }

  return releases;

}

MusicBrainzClient::Release MusicBrainzClient::ParseRelease(const QJsonObject &object_release) {

  Release release;

  if (object_release.contains("artist-credit"_L1) && object_release.value("artist-credit"_L1).isArray()) {
    release.artist_ = ParseArtistCredit(object_release.value("artist-credit"_L1).toArray());
  }
  if (object_release.contains("title"_L1) && object_release.value("title"_L1).isString()) {
    release.album_ = object_release.value("title"_L1).toString();
  }
  if (object_release.contains("date"_L1) && object_release.value("date"_L1).isString()) {
    const QString year_str = ParseDate(object_release.value("date"_L1).toString());
    if (!year_str.isEmpty()) {
      release.year_ = year_str.toInt();
    }
  }
  if (object_release.contains("media"_L1) && object_release.value("media"_L1).isArray()) {
    release.media_ = ParseMediaList(object_release.value("media"_L1).toArray());
  }

  return release;

}

MusicBrainzClient::MediaList MusicBrainzClient::ParseMediaList(const QJsonArray &array_media_list) {

  MediaList media_list;
  for (const QJsonValue &value_media : array_media_list) {
    if (!value_media.isObject()) continue;
    media_list << ParseMedia(value_media.toObject());
  }

  return media_list;

}

MusicBrainzClient::Media MusicBrainzClient::ParseMedia(const QJsonObject &object_media) {

  Media media;

  if (object_media.contains("discs"_L1) && object_media.value("discs"_L1).isArray()) {
    media.disc_ids_ = ParseDiscIds(object_media.value("discs"_L1).toArray());
  }

  if (object_media.contains("tracks"_L1) && object_media.value("tracks"_L1).isArray()) {
    media.tracks_ = ParseTracks(object_media.value("tracks"_L1).toArray());
  }

  return media;

}

MusicBrainzClient::Artist MusicBrainzClient::ParseArtistCredit(const QJsonArray &array_artist_credits) {

  Artist artist;
  for (const QJsonValue &value_artist_credit : array_artist_credits) {
    if (!value_artist_credit.isObject()) continue;
    const QJsonObject object_artist_credit = value_artist_credit.toObject();
    QString name;
    QString sort_name;
    QString join_phrase;
    if (object_artist_credit.contains("name"_L1) && object_artist_credit.value("name"_L1).isString()) {
      name = object_artist_credit.value("name"_L1).toString();
    }
    if (object_artist_credit.contains("artist"_L1) && object_artist_credit.value("artist"_L1).isObject()) {
      const QJsonObject object_artist = object_artist_credit.value("artist"_L1).toObject();
      if (object_artist.contains("name"_L1) && object_artist.value("name"_L1).isString()) {
        name = object_artist.value("name"_L1).toString();
      }
      if (object_artist.contains("sort-name"_L1) && object_artist.value("sort-name"_L1).isString()) {
        sort_name = object_artist.value("sort-name"_L1).toString();
      }
    }
    if (object_artist_credit.contains("joinphrase"_L1) && object_artist_credit.value("joinphrase"_L1).isString()) {
      join_phrase = object_artist_credit.value("joinphrase"_L1).toString();
    }
    if (!name.isEmpty()) {
      artist.name_ += name + join_phrase;
    }
    if (!sort_name.isEmpty()) {
      artist.sort_name_ += sort_name + join_phrase;
    }
  }

  return artist;

}

QString MusicBrainzClient::ParseDate(const QString &date_str) {

  static QRegularExpression regex(QString::fromLatin1(kDateRegex));
  const QRegularExpressionMatch match = regex.match(date_str);
  if (match.capturedStart() == 0) {
    return match.captured(0);
  }

  return QString();

}

QStringList MusicBrainzClient::ParseDiscIds(const QJsonArray &array_discs) {

  QStringList disc_ids;
  for (const QJsonValue &value_disc : array_discs) {
    if (!value_disc.isObject()) continue;
    const QJsonObject object_disc = value_disc.toObject();
    if (object_disc.contains("id"_L1)) {
      disc_ids << object_disc.value("id"_L1).toString();
    }
  }

  return disc_ids;

}

MusicBrainzClient::TrackList MusicBrainzClient::ParseTracks(const QJsonArray &array_tracks) {

  TrackList tracks;
  for (const QJsonValue &value_track : array_tracks) {
    if (!value_track.isObject()) continue;
    tracks << ParseTrack(value_track.toObject());
  }

  return tracks;

}

MusicBrainzClient::Track MusicBrainzClient::ParseTrack(const QJsonObject &object_track) {

  Track track;
  if (object_track.contains("position"_L1) && object_track.value("position"_L1).isDouble()) {
    track.number_ = object_track.value("position"_L1).toInt();
  }
  if (object_track.contains("title"_L1) && object_track.value("title"_L1).isString()) {
    track.title_ = object_track.value("title"_L1).toString();
  }
  if (object_track.contains("length"_L1) && object_track.value("length"_L1).isDouble()) {
    track.duration_msec_ = object_track.value("length"_L1).toInt();
  }
  if (object_track.contains("artist-credit"_L1) && object_track.value("artist-credit"_L1).isArray()) {
    track.artist_ = ParseArtistCredit(object_track.value("artist-credit"_L1).toArray());
  }

  return track;

}

MusicBrainzClient::ResultList MusicBrainzClient::ResultListFromReleases(const ReleaseList &releases, const QString &disc_id) {

  ResultList results;
  for (const Release &release : releases) {
    for (const Media &media : release.media_) {
      if (!disc_id.isEmpty() && !media.disc_ids_.contains(disc_id)) {
        continue;
      }
      for (const Track &track : media.tracks_) {
        Result result;
        result.year_ = release.year_;
        result.album_artist_ = release.artist_.name_;
        result.sort_album_artist_ = release.artist_.sort_name_;
        result.album_ = release.album_;
        result.title_ = track.title_;
        result.artist_ = track.artist_.name_;
        result.sort_artist_ = track.artist_.sort_name_;
        result.track_ = track.number_;
        result.duration_msec_ = track.duration_msec_;
        results << result;
      }
    }
  }

  return results;

}

MusicBrainzClient::ResultList MusicBrainzClient::UniqueResults(const ResultList &results, const UniqueResultsSortOption opt) {

  ResultList unique_results;
  if (opt == UniqueResultsSortOption::SortResults) {
    unique_results = QSet<Result>(results.begin(), results.end()).values();
    std::sort(unique_results.begin(), unique_results.end());
  }
  else {
    for (const Result &result : results) {
      if (!unique_results.contains(result)) {
        unique_results << result;
      }
    }
  }

  return unique_results;

}

