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
#include <cmath>
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
constexpr int kSearchResultsLimit = 12;

QString NormalizeSearchTerm(const QString &text) {
  QString normalized = text.simplified().trimmed().toLower();
  normalized.replace(u'_', u' ');
  return normalized;
}

QString EscapeSearchTerm(const QString &text) {
  QString escaped = text.simplified().trimmed();
  escaped.replace(u'"', u' ');
  escaped.replace(u'\\', u' ');
  return escaped.simplified();
}

QString NormalizeTitleForSearch(const QString &title) {
  QString normalized = title.simplified().trimmed();
  static const QList<QRegularExpression> suffix_patterns = {
      QRegularExpression(QStringLiteral("\\s*\\((official|lyrics?|lyric\\s+video|music\\s+video|audio|hd|4k)[^\\)]*\\)\\s*$"), QRegularExpression::CaseInsensitiveOption),
      QRegularExpression(QStringLiteral("\\s*\\[(official|lyrics?|lyric\\s+video|music\\s+video|audio|hd|4k)[^\\]]*\\]\\s*$"), QRegularExpression::CaseInsensitiveOption),
      QRegularExpression(QStringLiteral("\\s*[-–—:]\\s*(official|lyrics?|lyric\\s+video|music\\s+video|audio|hd|4k)\\b.*$"), QRegularExpression::CaseInsensitiveOption)};

  for (const QRegularExpression &pattern : suffix_patterns) {
    normalized.replace(pattern, QString());
  }
  return normalized.simplified();
}

QStringList TitleSearchTerms(const QString &title) {

  static const QSet<QString> kStopWords = {
      u"the"_s, u"and"_s, u"of"_s, u"a"_s, u"an"_s, u"to"_s, u"in"_s, u"on"_s,
      u"le"_s, u"la"_s, u"les"_s, u"de"_s, u"du"_s, u"des"_s, u"et"_s, u"un"_s, u"une"_s,
      u"el"_s, u"los"_s, u"las"_s, u"y"_s, u"en"_s, u"del"_s, u"da"_s, u"do"_s, u"das"_s, u"dos"_s};
  static const QRegularExpression kLeadingTrackNumberRegex(QStringLiteral("^\\d+\\s*[-–—.:]\\s*"));
  static const QRegularExpression kBracketRegex(QStringLiteral("[\\[\\]\\(\\){}]"));
  static const QRegularExpression kWhitespaceRegex(QStringLiteral("\\s+"));
  static const QRegularExpression kTrimNonWordPrefixRegex(QStringLiteral("^[^\\p{L}\\p{N}]+"));
  static const QRegularExpression kTrimNonWordSuffixRegex(QStringLiteral("[^\\p{L}\\p{N}]+$"));

  QString normalized = NormalizeTitleForSearch(title);
  if (normalized.isEmpty()) {
    normalized = title.simplified().trimmed();
  }

  normalized.replace(kLeadingTrackNumberRegex, QString());
  normalized.replace(u'_', u' ');
  normalized.replace(kBracketRegex, QStringLiteral(" "));
  normalized = EscapeSearchTerm(normalized);

  const QStringList tokens = normalized.split(kWhitespaceRegex, Qt::SkipEmptyParts);
  QStringList terms;
  terms.reserve(tokens.count());
  for (const QString &token : tokens) {
    QString word = token.trimmed().toLower();
    word.remove(kTrimNonWordPrefixRegex);
    word.remove(kTrimNonWordSuffixRegex);
    if (word.isEmpty()) continue;
    if (word.size() <= 1) continue;
    if (word.toInt() > 0 && word.size() <= 2) continue;
    if (kStopWords.contains(word)) continue;
    if (!terms.contains(word)) {
      terms << word;
    }
  }

  // Keep search usable even if aggressive filtering removed everything.
  if (terms.isEmpty()) {
    for (const QString &token : tokens) {
      const QString word = token.trimmed().toLower();
      if (word.size() < 2) continue;
      if (!terms.contains(word)) {
        terms << word;
      }
      if (terms.size() >= 6) break;
    }
  }

  return terms;
}

QString BuildSearchQuery(const QString &title, const QString &artist, const QString &album) {
  // Build a tolerant query for noisy file names while still preferring exact phrases.
  QStringList query_parts;
  const QStringList title_terms = TitleSearchTerms(title);
  QString normalized_title = NormalizeTitleForSearch(title);
  if (normalized_title.isEmpty()) {
    normalized_title = title;
  }
  normalized_title = EscapeSearchTerm(normalized_title);
  const QString normalized_artist = EscapeSearchTerm(artist);
  const QString normalized_album = EscapeSearchTerm(album);

  if (!title_terms.isEmpty()) {
    QStringList recording_terms;
    for (const QString &term : title_terms.mid(0, 8)) {
      recording_terms << QStringLiteral("recording:\"%1\"").arg(term);
    }
    const QString recording_or_terms = recording_terms.join(u" OR "_s);
    if (!normalized_title.isEmpty() && normalized_title.contains(u' ')) {
      query_parts << QStringLiteral("(recording:\"%1\" OR (%2))").arg(normalized_title, recording_or_terms);
    }
    else {
      query_parts << QStringLiteral("(%1)").arg(recording_or_terms);
    }
  }
  if (!normalized_artist.isEmpty()) {
    query_parts << QStringLiteral("artist:\"%1\"").arg(normalized_artist);
  }
  // Keep album as a fallback discriminator only when title/artist is incomplete.
  if (!normalized_album.isEmpty() && (title_terms.isEmpty() || normalized_artist.isEmpty())) {
    query_parts << QStringLiteral("release:\"%1\"").arg(normalized_album);
  }

  return query_parts.join(u" AND "_s);
}

int ComputeSearchScore(const MusicBrainzClient::Result &result, const QString &title, const QString &artist, const QString &album, const int duration_msec, const int musicbrainz_score) {

  QString normalized_title = NormalizeSearchTerm(NormalizeTitleForSearch(title));
  if (normalized_title.isEmpty()) {
    normalized_title = NormalizeSearchTerm(title);
  }
  const QString normalized_artist = NormalizeSearchTerm(artist);
  const QString normalized_album = NormalizeSearchTerm(album);

  const QString result_title = NormalizeSearchTerm(result.title_);
  const QString result_artist = NormalizeSearchTerm(result.artist_);
  const QString result_album = NormalizeSearchTerm(result.album_);

  int score = qBound(0, musicbrainz_score, 100) * 100;

  if (!normalized_title.isEmpty() && !result_title.isEmpty()) {
    if (normalized_title == result_title) score += 150;
    else if (result_title.contains(normalized_title) || normalized_title.contains(result_title)) score += 75;
  }

  if (!normalized_artist.isEmpty() && !result_artist.isEmpty()) {
    if (normalized_artist == result_artist) score += 120;
    else if (result_artist.contains(normalized_artist) || normalized_artist.contains(result_artist)) score += 60;
  }

  if (!normalized_album.isEmpty() && !result_album.isEmpty()) {
    if (normalized_album == result_album) score += 80;
    else if (result_album.contains(normalized_album) || normalized_album.contains(result_album)) score += 40;
  }

  if (duration_msec > 0 && result.duration_msec_ > 0) {
    const int diff = std::abs(result.duration_msec_ - duration_msec);
    if (diff <= 1000) score += 80;
    else if (diff <= 2500) score += 50;
    else if (diff <= 5000) score += 25;
    else if (diff <= 10000) score += 10;
    else score -= 20;
  }

  return score;

}
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
  FlushSearchRequests();
  FlushDiscIdRequests();

  if (pending_mbid_requests_.isEmpty() &&
      pending_search_requests_.isEmpty() &&
      pending_discid_requests_.isEmpty() &&
      timer_flush_requests_->isActive()) {
    timer_flush_requests_->stop();
  }

}

void MusicBrainzClient::CancelAll() {

  replies_.clear();

  qDeleteAll(mbid_requests_);
  mbid_requests_.clear();

  qDeleteAll(search_requests_);
  search_requests_.clear();

  qDeleteAll(discid_requests_);
  discid_requests_.clear();

  pending_mbid_requests_.clear();
  pending_search_requests_.clear();
  pending_discid_requests_.clear();
  pending_results_.clear();

  if (timer_flush_requests_->isActive()) {
    timer_flush_requests_->stop();
  }

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

void MusicBrainzClient::StartSearchRequest(const int id, const QString &title, const QString &artist, const QString &album, const int duration_msec) {

  // Fallback entry point used when AcoustID could not resolve a recording id.
  const QString query = BuildSearchQuery(title, artist, album);
  if (query.isEmpty()) {
    Q_EMIT MbIdFinished(id, ResultList(), QStringLiteral("MusicBrainz search requires at least a title, artist or album tag."));
    return;
  }

  qLog(Debug) << "Starting MusicBrainz search request for track id" << id << "query" << query;
  pending_search_requests_ << SearchRequest(id, query, title.trimmed(), artist.trimmed(), album.trimmed(), duration_msec);

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

  while (!search_requests_.isEmpty() && search_requests_.contains(id)) {
    QNetworkReply *reply = search_requests_.take(id);
    replies_.removeAll(reply);
    QObject::disconnect(reply, nullptr, this, nullptr);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

  pending_mbid_requests_.remove(id);
  pending_results_.remove(id);
  for (auto it = pending_search_requests_.begin(); it != pending_search_requests_.end();) {
    if (it->id == id) {
      it = pending_search_requests_.erase(it);
    }
    else {
      ++it;
    }
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

void MusicBrainzClient::FlushSearchRequests() {

  if (!search_requests_.isEmpty() || pending_search_requests_.isEmpty()) return;

  SendSearchRequest(pending_search_requests_.takeFirst());

}

void MusicBrainzClient::SendSearchRequest(const SearchRequest &request) {

  QUrlQuery url_query;
  url_query.addQueryItem(u"query"_s, request.query);
  url_query.addQueryItem(u"limit"_s, QString::number(kSearchResultsLimit));
  url_query.addQueryItem(u"fmt"_s, u"json"_s);
  url_query.addQueryItem(u"inc"_s, u"releases+artists"_s);

  QNetworkReply *reply = CreateGetRequest(QUrl(QString::fromLatin1(kTrackUrl)), url_query);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() {
    SearchRequestFinished(reply, request.id, request.query, request.title, request.artist, request.album, request.duration_msec);
  });
  search_requests_.insert(request.id, reply);

  timeouts_->AddReply(reply);

}

void MusicBrainzClient::SearchRequestFinished(QNetworkReply *reply, const int id, const QString &query, const QString &title, const QString &artist, const QString &album, const int duration_msec) {

  if (replies_.contains(reply)) {
    replies_.removeAll(reply);
  }
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  if (search_requests_.contains(id, reply)) {
    search_requests_.remove(id, reply);
  }

  if (!timer_flush_requests_->isActive() &&
      search_requests_.isEmpty() &&
      !pending_search_requests_.isEmpty()) {
    timer_flush_requests_->start();
  }

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Q_EMIT MbIdFinished(id, ResultList(), json_object_result.error_message);
    return;
  }

  // Keep a score so we can surface the best metadata first in the Tag Fetcher.
  struct ScoredResult {
    Result result_;
    int score_;
  };

  QList<ScoredResult> scored_results;
  const QJsonObject object_root = json_object_result.json_object;
  if (object_root.contains("recordings"_L1) && object_root.value("recordings"_L1).isArray()) {
    const QJsonArray array_recordings = object_root.value("recordings"_L1).toArray();
    for (const QJsonValue &value_recording : array_recordings) {
      if (!value_recording.isObject()) continue;
      const QJsonObject object_recording = value_recording.toObject();

      QString recording_title;
      if (object_recording.contains("title"_L1) && object_recording.value("title"_L1).isString()) {
        recording_title = object_recording.value("title"_L1).toString();
      }

      int recording_duration_msec = 0;
      if (object_recording.contains("length"_L1) && object_recording.value("length"_L1).isDouble()) {
        recording_duration_msec = object_recording.value("length"_L1).toInt();
      }

      Artist recording_artist;
      if (object_recording.contains("artist-credit"_L1) && object_recording.value("artist-credit"_L1).isArray()) {
        recording_artist = ParseArtistCredit(object_recording.value("artist-credit"_L1).toArray());
      }

      int musicbrainz_score = 0;
      if (object_recording.contains("score"_L1)) {
        const QJsonValue value_score = object_recording.value("score"_L1);
        if (value_score.isDouble()) {
          musicbrainz_score = value_score.toInt();
        }
        else if (value_score.isString()) {
          musicbrainz_score = value_score.toString().toInt();
        }
      }

      auto append_result = [&](const QString &result_album, const Artist &result_album_artist, const int result_year) {
        Result result;
        result.title_ = recording_title;
        result.artist_ = recording_artist.name_;
        result.sort_artist_ = recording_artist.sort_name_;
        result.album_ = result_album;
        result.album_artist_ = result_album_artist.name_;
        result.sort_album_artist_ = result_album_artist.sort_name_;
        result.duration_msec_ = recording_duration_msec;
        result.year_ = result_year;
        result.track_ = 0;
        const int score = ComputeSearchScore(result, title, artist, album, duration_msec, musicbrainz_score);
        scored_results << ScoredResult{result, score};
      };

      bool have_release_results = false;
      if (object_recording.contains("releases"_L1) && object_recording.value("releases"_L1).isArray()) {
        const QJsonArray array_releases = object_recording.value("releases"_L1).toArray();
        for (const QJsonValue &value_release : array_releases) {
          if (!value_release.isObject()) continue;
          const QJsonObject object_release = value_release.toObject();

          QString release_title;
          if (object_release.contains("title"_L1) && object_release.value("title"_L1).isString()) {
            release_title = object_release.value("title"_L1).toString();
          }

          Artist release_artist = recording_artist;
          if (object_release.contains("artist-credit"_L1) && object_release.value("artist-credit"_L1).isArray()) {
            release_artist = ParseArtistCredit(object_release.value("artist-credit"_L1).toArray());
          }

          int release_year = -1;
          if (object_release.contains("date"_L1) && object_release.value("date"_L1).isString()) {
            const QString release_year_str = ParseDate(object_release.value("date"_L1).toString());
            if (!release_year_str.isEmpty()) {
              release_year = release_year_str.toInt();
            }
          }

          append_result(release_title, release_artist, release_year);
          have_release_results = true;
        }
      }

      if (!have_release_results) {
        append_result(QString(), recording_artist, -1);
      }
    }
  }

  if (scored_results.isEmpty()) {
    Q_EMIT MbIdFinished(id, ResultList(), QStringLiteral("No MusicBrainz results matched this track metadata."));
    return;
  }

  std::stable_sort(scored_results.begin(), scored_results.end(), [](const ScoredResult &a, const ScoredResult &b) {
    return a.score_ > b.score_;
  });

  ResultList results;
  results.reserve(scored_results.count());
  for (const ScoredResult &scored_result : std::as_const(scored_results)) {
    results << scored_result.result_;
  }

  qLog(Debug) << "MusicBrainz search for track id" << id << "query" << query << "returned" << results.count() << "candidate(s)";
  Q_EMIT MbIdFinished(id, UniqueResults(results, UniqueResultsSortOption::KeepOriginalOrder));

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
