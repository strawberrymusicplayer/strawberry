/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QObject>
#include <QSet>
#include <QList>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QRegExp>
#include <QtAlgorithms>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QXmlStreamReader>
#include <QTimer>
#include <QtDebug>

#include "core/closure.h"
#include "core/logging.h"
#include "core/network.h"
#include "core/networktimeouts.h"
#include "core/utilities.h"
#include "musicbrainzclient.h"

const char *MusicBrainzClient::kTrackUrl = "https://musicbrainz.org/ws/2/recording/";
const char *MusicBrainzClient::kDiscUrl = "https://musicbrainz.org/ws/2/discid/";
const char *MusicBrainzClient::kDateRegex = "^[12]\\d{3}";
const int MusicBrainzClient::kRequestsDelay = 1200;
const int MusicBrainzClient::kDefaultTimeout = 8000;
const int MusicBrainzClient::kMaxRequestPerTrack = 3;

MusicBrainzClient::MusicBrainzClient(QObject *parent, QNetworkAccessManager *network)
    : QObject(parent),
      network_(network ? network : new NetworkAccessManager(this)),
      timeouts_(new NetworkTimeouts(kDefaultTimeout, this)),
      timer_flush_requests_(new QTimer(this)) {

  timer_flush_requests_->setInterval(kRequestsDelay);
  timer_flush_requests_->setSingleShot(true);
  connect(timer_flush_requests_, SIGNAL(timeout()), this, SLOT(FlushRequests()));

}

QByteArray MusicBrainzClient::GetReplyData(QNetworkReply *reply, QString &error) {

  QByteArray data;

  if (reply->error() == QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
      data = reply->readAll();
  }
  else {
    if (reply->error() != QNetworkReply::NoError && reply->error() < 200) {
      // This is a network error, there is nothing more to do.
      Error(QString("%1 (%2)").arg(reply->errorString()).arg(reply->error()));
    }
    else {
      // See if there is Json data containing "error" - then use that instead.
      data = reply->readAll();
      QJsonParseError json_error;
      QJsonDocument json_doc = QJsonDocument::fromJson(data, &json_error);
      if (json_error.error == QJsonParseError::NoError && json_doc.isObject()) {
        QJsonObject json_obj = json_doc.object();
        if (!json_obj.isEmpty() && json_obj.contains("error")) {
          error = json_obj["error"].toString();
        }
      }
      if (error.isEmpty()) {
        if (reply->error() != QNetworkReply::NoError) {
          error = QString("%1 (%2)").arg(reply->errorString()).arg(reply->error());
        }
        else {
          error = QString("Received HTTP code %1").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt());
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
    disconnect(reply, 0, this, 0);
    if (reply->isRunning()) reply->abort();
    reply->deleteLater();
  }

}

void MusicBrainzClient::CancelAll() {

  qDeleteAll(requests_.values());
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

  const ParamList params = ParamList() << Param("inc", "artists+recordings");

  QUrlQuery url_query;
  url_query.setQueryItems(params);
  QUrl url(kDiscUrl + discid);
  url.setQuery(url_query);

  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
  QNetworkReply *reply = network_->get(req);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(DiscIdRequestFinished(QString, QNetworkReply*)), discid, reply);

  timeouts_->AddReply(reply);

}

void MusicBrainzClient::FlushRequests() {

  if (!requests_.isEmpty() || requests_pending_.isEmpty()) return;

  Request request = requests_pending_.take(requests_pending_.firstKey());

  const ParamList params = ParamList() << Param("inc", "artists+releases+media");

  QUrlQuery url_query;
  url_query.setQueryItems(params);
  QUrl url(kTrackUrl + request.mbid);
  url.setQuery(url_query);

  QNetworkRequest req(url);
  req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
  QNetworkReply *reply = network_->get(req);
  NewClosure(reply, SIGNAL(finished()), this, SLOT(RequestFinished(QNetworkReply*, int, int)), reply, request.id, request.number);
  requests_.insert(request.id, reply);

  timeouts_->AddReply(reply);

}

void MusicBrainzClient::RequestFinished(QNetworkReply *reply, const int id, const int request_number) {

  reply->deleteLater();

  const int nb_removed = requests_.remove(id, reply);
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
    if (reader.readNext() == QXmlStreamReader::StartElement && reader.name() == "recording") {
        ResultList tracks = ParseTrack(&reader);
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
    for (const PendingResults &result_list : result_list_list) {
      ret << result_list.results_;
    }
    emit Finished(id, UniqueResults(ret, KeepOriginalOrder), error);
  }

}

void MusicBrainzClient::DiscIdRequestFinished(const QString &discid, QNetworkReply *reply) {

  reply->deleteLater();

  ResultList ret;
  QString artist;
  QString album;
  int year = 0;

  QString error;
  QByteArray data = GetReplyData(reply, error);
  if (data.isEmpty()) {
    emit Finished(artist, album, ret, error);
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
      QStringRef name = reader.name();
      if (name == "title") {
        album = reader.readElementText();
      }
      else if (name == "date") {
        QRegExp regex(kDateRegex);
        if (regex.indexIn(reader.readElementText()) == 0) {
          year = regex.cap(0).toInt();
        }
      }
      else if (name == "artist-credit") {
        ParseArtist(&reader, &artist);
      }
      else if (name == "medium-list") {
        break;
      }
    }
  }

  while (!reader.atEnd()) {
    QXmlStreamReader::TokenType token = reader.readNext();
    if (token == QXmlStreamReader::StartElement && reader.name() == "medium") {
      // Get the medium with a matching discid.
      if (MediumHasDiscid(discid, &reader)) {
        ResultList tracks = ParseMedium(&reader);
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
    else if (token == QXmlStreamReader::EndElement && reader.name() == "medium-list") {
      break;
    }
  }

  // If we parsed a year, copy it to the tracks.
  if (year > 0) {
    for (ResultList::iterator it = ret.begin(); it != ret.end(); ++it) {
      it->year_ = year;
    }
  }

  emit Finished(artist, album, UniqueResults(ret, SortResults));

}

bool MusicBrainzClient::MediumHasDiscid(const QString &discid, QXmlStreamReader *reader) {

  while (!reader->atEnd()) {
    QXmlStreamReader::TokenType type = reader->readNext();

    if (type == QXmlStreamReader::StartElement && reader->name() == "disc" && reader->attributes().value("id").toString() == discid) {
      return true;
    }
    else if (type == QXmlStreamReader::EndElement && reader->name() == "disc-list") {
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

    if (type == QXmlStreamReader::StartElement) {
      if (reader->name() == "track") {
        Result result;
        result = ParseTrackFromDisc(reader);
        ret << result;
      }
    }

    if (type == QXmlStreamReader::EndElement && reader->name() == "track-list") {
      break;
    }
  }

  return ret;

}

MusicBrainzClient::Result MusicBrainzClient::ParseTrackFromDisc(QXmlStreamReader *reader) {

  Result result;

  while (!reader->atEnd()) {
    QXmlStreamReader::TokenType type = reader->readNext();

    if (type == QXmlStreamReader::StartElement) {
      QStringRef name = reader->name();
      if (name == "position") {
        result.track_ = reader->readElementText().toInt();
      }
      else if (name == "length") {
        result.duration_msec_ = reader->readElementText().toInt();
      }
      else if (name == "title") {
        result.title_ = reader->readElementText();
      }
    }

    if (type == QXmlStreamReader::EndElement && reader->name() == "track") {
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

    if (type == QXmlStreamReader::StartElement) {
      QStringRef name = reader->name();

      if (name == "title") {
        result.title_ = reader->readElementText();
      }
      else if (name == "length") {
        result.duration_msec_ = reader->readElementText().toInt();
      }
      else if (name == "artist-credit") {
        ParseArtist(reader, &result.artist_);
      }
      else if (name == "release") {
        releases << ParseRelease(reader);
      }
    }

    if (type == QXmlStreamReader::EndElement && reader->name() == "recording") {
      break;
    }
  }

  ResultList ret;
  if (releases.isEmpty()) {
    ret << result;
  }
  else {
    std::stable_sort(releases.begin(), releases.end());
    for (const Release &release : releases) {
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

    if (type == QXmlStreamReader::StartElement && reader->name() == "name-credit") {
      join_phrase = reader->attributes().value("joinphrase").toString();
    }

    if (type == QXmlStreamReader::StartElement && reader->name() == "name") {
      *artist += reader->readElementText() + join_phrase;
    }

    if (type == QXmlStreamReader::EndElement && reader->name() == "artist-credit") {
      return;
    }
  }
}

MusicBrainzClient::Release MusicBrainzClient::ParseRelease(QXmlStreamReader *reader) {

  Release ret;

  while (!reader->atEnd()) {
    QXmlStreamReader::TokenType type = reader->readNext();

    if (type == QXmlStreamReader::StartElement) {
      QStringRef name = reader->name();
      if (name == "title") {
        ret.album_ = reader->readElementText();
      }
      else if (name == "status") {
        ret.SetStatusFromString(reader->readElementText());
      }
      else if (name == "date") {
        QRegExp regex(kDateRegex);
        if (regex.indexIn(reader->readElementText()) == 0) {
          ret.year_ = regex.cap(0).toInt();
        }
      }
      else if (name == "track-list") {
        ret.track_ = reader->attributes().value("offset").toString().toInt() + 1;
        Utilities::ConsumeCurrentElement(reader);
      }
    }

    if (type == QXmlStreamReader::EndElement && reader->name() == "release") {
      break;
    }
  }

  return ret;

}

MusicBrainzClient::ResultList MusicBrainzClient::UniqueResults(const ResultList& results, UniqueResultsSortOption opt) {

  ResultList ret;
  if (opt == SortResults) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    ret = QSet<Result>(results.begin(), results.end()).values();
#else
    ret = QSet<Result>::fromList(results).values();
#endif
    std::sort(ret.begin(), ret.end());
  }
  else {  // KeepOriginalOrder
    // Qt doesn't provide a ordered set (QSet "stores values in an unspecified order" according to Qt documentation).
    // We might use std::set instead, but it's probably faster to use ResultList directly to avoid converting from one structure to another.
    for (const Result& res : results) {
      if (!ret.contains(res)) {
        ret << res;
      }
    }
  }
  return ret;

}

void MusicBrainzClient::Error(const QString &error, QVariant debug) {

  qLog(Error) << "MusicBrainz:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}
