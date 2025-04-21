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
#include <QtAlgorithms>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QXmlStreamReader>
#include <QTimer>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/networktimeouts.h"
#include "utilities/xmlutils.h"
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
    : QObject(parent),
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

QByteArray MusicBrainzClient::GetReplyData(QNetworkReply *reply, QString &error) {

  QByteArray data;

  if (reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
    data = reply->readAll();
  }
  else {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      Error(QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    }
    else {
      // See if there is Json data containing "error" - then use that instead.
      data = reply->readAll();
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      if (json_error.error == QJsonParseError::NoError && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("error"_L1)) {
          error = json_obj["error"_L1].toString();
        }
      }
      if (error.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
          error = QStringLiteral("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
        else {
          error = QStringLiteral("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
        }
        Error(error, data);
      }
      else Error(error);
    }
    return QByteArray();
  }

  return data;

}

void MusicBrainzClient::Cancel(int id) {

  while (!requests_.isEmpty() && requests_.contains(id)) {
    QNetworkReply *reply = requests_.take(id);
    QObject::disconnect(reply, nullptr, this, nullptr);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

}

void MusicBrainzClient::CancelAll() {

  qDeleteAll(requests_);
  requests_.clear();

}

void MusicBrainzClient::Start(const int id, const QStringList &mbid_list) {

  int request_number = 0;
  for (const QString &mbid : mbid_list) {
    ++request_number;
    if (request_number > kMaxRequestPerTrack) break;
    Request request(id, mbid, request_number);
    requests_pending_.insert(id, request);
  }

  if (!timer_flush_requests_->isActive()) {
    timer_flush_requests_->start();
  }

}

void MusicBrainzClient::StartDiscIdRequest(const QString &discid) {

  const ParamList params = ParamList() << Param(u"inc"_s, u"artists+recordings"_s);

  QUrlQuery url_query;
  url_query.setQueryItems(params);
  QUrl url(QString::fromLatin1(kDiscUrl) + discid);
  url.setQuery(url_query);

  QNetworkRequest network_request(url);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = network_->get(network_request);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, discid, reply]() { DiscIdRequestFinished(discid, reply); });

  timeouts_->AddReply(reply);

}

void MusicBrainzClient::FlushRequests() {

  if (!requests_.isEmpty() || requests_pending_.isEmpty()) return;

  const Request request = requests_pending_.take(requests_pending_.firstKey());

  const ParamList params = ParamList() << Param(u"inc"_s, u"artists+releases+media"_s);

  QUrlQuery url_query;
  url_query.setQueryItems(params);
  QUrl url(QString::fromLatin1(kTrackUrl) + request.mbid);
  url.setQuery(url_query);

  QNetworkRequest network_request(url);
  network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  QNetworkReply *reply = network_->get(network_request);
  QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { RequestFinished(reply, request.id, request.number); });
  requests_.insert(request.id, reply);

  timeouts_->AddReply(reply);

}

void MusicBrainzClient::RequestFinished(QNetworkReply *reply, const int id, const int request_number) {

  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  const qint64 nb_removed = requests_.remove(id, reply);
  if (nb_removed != 1) {
    qLog(Debug) << "MusicBrainz: Unknown reply received:" << nb_removed << "requests removed, while only one was supposed to be removed";
  }

  if (!timer_flush_requests_->isActive() && requests_.isEmpty() && !requests_pending_.isEmpty()) {
    timer_flush_requests_->start();
  }

  QString error;
  QByteArray data = GetReplyData(reply, error);
  if (!data.isEmpty()) {
    QXmlStreamReader reader(data);
    ResultList res;
    while (!reader.atEnd()) {
      if (reader.readNext() == QXmlStreamReader::StartElement && reader.name().toString() == "recording"_L1) {
        const ResultList tracks = ParseTrack(&reader);
        for (const Result &track : tracks) {
          if (!track.title_.isEmpty()) {
            res << track;
          }
        }
      }
    }
    pending_results_[id] << PendingResults(request_number, res);
  }

  // No more pending requests for this id: emit the results we have.
  if (!requests_.contains(id) && !requests_pending_.contains(id)) {
    // Merge the results we have
    ResultList ret;
    QList<PendingResults> result_list_list = pending_results_.take(id);
    std::sort(result_list_list.begin(), result_list_list.end());
    for (const PendingResults &result_list : std::as_const(result_list_list)) {
      ret << result_list.results_;
    }
    Q_EMIT Finished(id, UniqueResults(ret, UniqueResultsSortOption::KeepOriginalOrder), error);
  }

}

void MusicBrainzClient::DiscIdRequestFinished(const QString &discid, QNetworkReply *reply) {

  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  ResultList ret;
  QString artist;
  QString album;
  int year = 0;

  QString error;
  QByteArray data = GetReplyData(reply, error);
  if (data.isEmpty()) {
    Q_EMIT DiscIdFinished(artist, album, ret, error);
    return;
  }

  // Parse xml result:
  // -get title
  // -get artist
  // -get year
  // -get all the tracks' tags
  // Note: If there are multiple releases for the discid, the first
  // release is chosen.
  QXmlStreamReader reader(data);
  while (!reader.atEnd()) {
    QXmlStreamReader::TokenType type = reader.readNext();
    if (type == QXmlStreamReader::StartElement) {
      QString name = reader.name().toString();
      if (name == "title"_L1) {
        album = reader.readElementText();
      }
      else if (name == "date"_L1) {
        QRegularExpression regex(QString::fromLatin1(kDateRegex));
        QRegularExpressionMatch re_match = regex.match(reader.readElementText());
        if (re_match.capturedStart() == 0) {
          year = re_match.captured(0).toInt();
        }
      }
      else if (name == "artist-credit"_L1) {
        ParseArtist(&reader, &artist);
      }
      else if (name == "medium-list"_L1) {
        break;
      }
    }
  }

  while (!reader.atEnd()) {
    QXmlStreamReader::TokenType token = reader.readNext();
    QString name = reader.name().toString();
    if (token == QXmlStreamReader::StartElement && name == "medium"_L1) {
      // Get the medium with a matching discid.
      if (MediumHasDiscid(discid, &reader)) {
        const ResultList tracks = ParseMedium(&reader);
        for (const Result &track : tracks) {
          if (!track.title_.isEmpty()) {
            ret << track;
          }
        }
      }
      else {
        Utilities::ConsumeCurrentElement(&reader);
      }
    }
    else if (token == QXmlStreamReader::EndElement && name == "medium-list"_L1) {
      break;
    }
  }

  // If we parsed a year, copy it to the tracks.
  if (year > 0) {
    for (ResultList::iterator it = ret.begin(); it != ret.end(); ++it) {
      it->year_ = year;
    }
  }

  Q_EMIT DiscIdFinished(artist, album, UniqueResults(ret, UniqueResultsSortOption::SortResults));

}

bool MusicBrainzClient::MediumHasDiscid(const QString &discid, QXmlStreamReader *reader) {

  while (!reader->atEnd()) {
    QXmlStreamReader::TokenType type = reader->readNext();
    QString name = reader->name().toString();

    if (type == QXmlStreamReader::StartElement && name == "disc"_L1 && reader->attributes().value("id"_L1).toString() == discid) {
      return true;
    }
    if (type == QXmlStreamReader::EndElement && name == "disc-list"_L1) {
      return false;
    }
  }
  qLog(Debug) << "Reached end of xml stream without encountering </disc-list>";
  return false;

}

MusicBrainzClient::ResultList MusicBrainzClient::ParseMedium(QXmlStreamReader *reader) {

  ResultList ret;
  while (!reader->atEnd()) {
    QXmlStreamReader::TokenType type = reader->readNext();
    QString name = reader->name().toString();

    if (type == QXmlStreamReader::StartElement) {
      if (name == "track"_L1) {
        Result result;
        result = ParseTrackFromDisc(reader);
        ret << result;
      }
    }

    if (type == QXmlStreamReader::EndElement && name == "track-list"_L1) {
      break;
    }
  }

  return ret;

}

MusicBrainzClient::Result MusicBrainzClient::ParseTrackFromDisc(QXmlStreamReader *reader) {

  Result result;

  while (!reader->atEnd()) {
    QXmlStreamReader::TokenType type = reader->readNext();
    QString name = reader->name().toString();

    if (type == QXmlStreamReader::StartElement) {
      if (name == "position"_L1) {
        result.track_ = reader->readElementText().toInt();
      }
      else if (name == "length"_L1) {
        result.duration_msec_ = reader->readElementText().toInt();
      }
      else if (name == "title"_L1) {
        result.title_ = reader->readElementText();
      }
    }

    if (type == QXmlStreamReader::EndElement && name == "track"_L1) {
      break;
    }
  }

  return result;

}

MusicBrainzClient::ResultList MusicBrainzClient::ParseTrack(QXmlStreamReader *reader) {

  Result result;
  QList<Release> releases;

  while (!reader->atEnd()) {
    QXmlStreamReader::TokenType type = reader->readNext();
    QString name = reader->name().toString();

    if (type == QXmlStreamReader::StartElement) {

      if (name == "title"_L1) {
        result.title_ = reader->readElementText();
      }
      else if (name == "length"_L1) {
        result.duration_msec_ = reader->readElementText().toInt();
      }
      else if (name == "artist-credit"_L1) {
        ParseArtist(reader, &result.artist_);
      }
      else if (name == "release"_L1) {
        releases << ParseRelease(reader);
      }
    }

    if (type == QXmlStreamReader::EndElement && name == "recording"_L1) {
      break;
    }
  }

  ResultList ret;
  if (releases.isEmpty()) {
    ret << result;
  }
  else {
    std::stable_sort(releases.begin(), releases.end());
    ret.reserve(releases.count());
    for (const Release &release : std::as_const(releases)) {
      ret << release.CopyAndMergeInto(result);
    }
  }

  return ret;

}

// Parse the artist. Multiple artists are joined together with the joinphrase from musicbrainz.
void MusicBrainzClient::ParseArtist(QXmlStreamReader *reader, QString *artist) {

  QString join_phrase;

  while (!reader->atEnd()) {
    QXmlStreamReader::TokenType type = reader->readNext();
    QString name = reader->name().toString();
    if (type == QXmlStreamReader::StartElement && name == "name-credit"_L1) {
      join_phrase = reader->attributes().value("joinphrase"_L1).toString();
    }

    if (type == QXmlStreamReader::StartElement && name == "name"_L1) {
      *artist += reader->readElementText() + join_phrase;
    }

    if (type == QXmlStreamReader::EndElement && name == "artist-credit"_L1) {
      return;
    }
  }
}

MusicBrainzClient::Release MusicBrainzClient::ParseRelease(QXmlStreamReader *reader) {

  Release ret;

  while (!reader->atEnd()) {
    QXmlStreamReader::TokenType type = reader->readNext();
    QString name = reader->name().toString();

    if (type == QXmlStreamReader::StartElement) {
      if (name == "title"_L1) {
        ret.album_ = reader->readElementText();
      }
      else if (name == "status"_L1) {
        ret.SetStatusFromString(reader->readElementText());
      }
      else if (name == "date"_L1) {
        QRegularExpression regex(QString::fromLatin1(kDateRegex));
        QRegularExpressionMatch re_match = regex.match(reader->readElementText());
        if (re_match.capturedStart() == 0) {
          ret.year_ = re_match.captured(0).toInt();
        }
      }
      else if (name == "track-list"_L1) {
        ret.track_ = reader->attributes().value("offset"_L1).toString().toInt() + 1;
        Utilities::ConsumeCurrentElement(reader);
      }
    }

    if (type == QXmlStreamReader::EndElement && name == "release"_L1) {
      break;
    }
  }

  return ret;

}

MusicBrainzClient::ResultList MusicBrainzClient::UniqueResults(const ResultList &results, UniqueResultsSortOption opt) {

  ResultList ret;
  if (opt == UniqueResultsSortOption::SortResults) {
    ret = QSet<Result>(results.begin(), results.end()).values();
    std::sort(ret.begin(), ret.end());
  }
  else {  // KeepOriginalOrder
    // Qt doesn't provide an ordered set (QSet "stores values in an unspecified order" according to Qt documentation).
    // We might use std::set instead, but it's probably faster to use ResultList directly to avoid converting from one structure to another.
    for (const Result &res : results) {
      if (!ret.contains(res)) {
        ret << res;
      }
    }
  }
  return ret;

}

void MusicBrainzClient::Error(const QString &error, const QVariant &debug) {

  qLog(Error) << "MusicBrainz:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
