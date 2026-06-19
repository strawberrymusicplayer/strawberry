/*
 * Strawberry Music Player
 * Copyright 2018-2025, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QList>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QTimer>
#include <QScopeGuard>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/networkaccessmanager.h"
#include "core/song.h"
#include "constants/timeconstants.h"
#include "opentidalservice.h"
#include "opentidalurlhandler.h"
#include "opentidalbaserequest.h"
#include "opentidalrequest.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr char kApiHost[] = "https://openapi.tidal.com";
constexpr int kMaxConcurrentRequests = 1;
constexpr int kFlushRequestsDelay = 400;
}  // namespace

OpenTidalRequest::OpenTidalRequest(OpenTidalService *service, OpenTidalUrlHandler *url_handler, const SharedPtr<NetworkAccessManager> network, const Type query_type, QObject *parent)
    : OpenTidalBaseRequest(service, network, parent),
      service_(service),
      url_handler_(url_handler),
      timer_flush_requests_(new QTimer(this)),
      query_type_(query_type),
      coversize_(service->coversize()),
      query_id_(-1),
      finished_(false),
      requests_active_(0),
      requests_total_(0),
      requests_received_(0) {

  timer_flush_requests_->setInterval(kFlushRequestsDelay);
  timer_flush_requests_->setSingleShot(false);
  QObject::connect(timer_flush_requests_, &QTimer::timeout, this, &OpenTidalRequest::FlushRequests);

}

void OpenTidalRequest::Search(const int query_id, const QString &search_text) {
  query_id_ = query_id;
  search_text_ = search_text;
}

QUrl OpenTidalRequest::ApiUrl(const QString &path, const QStringList &includes) const {

  QUrlQuery url_query;
  url_query.addQueryItem(u"countryCode"_s, service_->country_code());
  if (!includes.isEmpty()) {
    url_query.addQueryItem(u"include"_s, includes.join(u','));
  }

  QUrl url(QLatin1String(OpenTidalService::kApiUrl) + QLatin1Char('/') + path);
  url.setQuery(url_query);

  return url;

}

QUrl OpenTidalRequest::NextUrl(const QJsonObject &json_object) const {

  if (!json_object.contains("links"_L1) || !json_object["links"_L1].isObject()) {
    return QUrl();
  }
  const QJsonObject object_links = json_object["links"_L1].toObject();
  if (!object_links.contains("next"_L1) || !object_links["next"_L1].isString()) {
    return QUrl();
  }
  const QString next = object_links["next"_L1].toString();
  if (next.isEmpty()) return QUrl();

  if (next.startsWith("http"_L1, Qt::CaseInsensitive)) {
    return QUrl(next);
  }
  if (next.startsWith(u'/')) {
    return QUrl(QLatin1String(kApiHost) + next);
  }

  return QUrl();

}

void OpenTidalRequest::Process() {

  if (IsSearch()) {
    Q_EMIT UpdateStatus(query_id_, tr("Searching..."));
  }

  const QString query = QString::fromUtf8(QUrl::toPercentEncoding(search_text_));
  const quint64 user_id = service_->user_id();

  switch (query_type_) {
    case Type::SearchSongs:
      AddRequest(RequestType::DiscoverTracks, ApiUrl(QStringLiteral("searchResults/%1").arg(query), {u"tracks"_s, u"tracks.albums"_s, u"tracks.artists"_s, u"tracks.albums.coverArt"_s}));
      break;
    case Type::SearchAlbums:
      AddRequest(RequestType::DiscoverAlbums, ApiUrl(QStringLiteral("searchResults/%1").arg(query), {u"albums"_s}));
      break;
    case Type::SearchArtists:
      AddRequest(RequestType::DiscoverArtists, ApiUrl(QStringLiteral("searchResults/%1").arg(query), {u"artists"_s}));
      break;
    case Type::FavouriteSongs:
      AddRequest(RequestType::DiscoverTracks, ApiUrl(QStringLiteral("userCollections/%1/relationships/tracks").arg(user_id), {u"tracks"_s, u"tracks.albums"_s, u"tracks.artists"_s, u"tracks.albums.coverArt"_s}));
      break;
    case Type::FavouriteAlbums:
      AddRequest(RequestType::DiscoverAlbums, ApiUrl(QStringLiteral("userCollections/%1/relationships/albums").arg(user_id), {u"albums"_s}));
      break;
    case Type::FavouriteArtists:
      AddRequest(RequestType::DiscoverArtists, ApiUrl(QStringLiteral("userCollections/%1/relationships/artists").arg(user_id), {u"artists"_s}));
      break;
    default:
      Error(u"Invalid query type."_s);
      FinishCheck();
      break;
  }

}

void OpenTidalRequest::AddRequest(const RequestType type, const QUrl &url) {

  requests_queue_.enqueue(Request(type, url));
  ++requests_total_;
  StartRequests();

}

void OpenTidalRequest::AddAlbumExpandRequest(const QString &album_id) {

  if (album_id.isEmpty() || albums_seen_.contains(album_id)) return;
  albums_seen_.insert(album_id);
  AddRequest(RequestType::AlbumExpand, ApiUrl(QStringLiteral("albums/%1").arg(album_id), {u"items"_s, u"items.artists"_s, u"artists"_s, u"coverArt"_s}));

}

void OpenTidalRequest::AddArtistAlbumsRequest(const QString &artist_id) {

  if (artist_id.isEmpty() || artists_seen_.contains(artist_id)) return;
  artists_seen_.insert(artist_id);
  AddRequest(RequestType::ArtistAlbums, ApiUrl(QStringLiteral("artists/%1/relationships/albums").arg(artist_id), {u"albums"_s}));

}

void OpenTidalRequest::StartRequests() {

  if (!timer_flush_requests_->isActive()) {
    timer_flush_requests_->start();
  }

}

void OpenTidalRequest::FlushRequests() {

  if (finished_) {
    timer_flush_requests_->stop();
    return;
  }

  while (!requests_queue_.isEmpty() && requests_active_ < kMaxConcurrentRequests) {
    const Request request = requests_queue_.dequeue();
    QNetworkReply *reply = CreateGetRequest(request.url);
    replies_ << reply;
    const RequestType type = request.type;
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, type]() { ReplyReceived(reply, type); });
    ++requests_active_;
  }

  if (requests_queue_.isEmpty() && requests_active_ <= 0) {
    timer_flush_requests_->stop();
  }

}

QString OpenTidalRequest::IncludeKey(const QString &type, const QString &id) {
  return type + u':' + id;
}

QHash<QString, QJsonObject> OpenTidalRequest::ParseIncluded(const QJsonObject &json_object) {

  QHash<QString, QJsonObject> included;

  if (!json_object.contains("included"_L1) || !json_object["included"_L1].isArray()) {
    return included;
  }

  const QJsonArray array_included = json_object["included"_L1].toArray();
  for (const QJsonValue &value : array_included) {
    if (!value.isObject()) continue;
    const QJsonObject object = value.toObject();
    const QString type = object["type"_L1].toString();
    const QString id = object["id"_L1].toString();
    if (type.isEmpty() || id.isEmpty()) continue;
    included.insert(IncludeKey(type, id), object);
  }

  return included;

}

QString OpenTidalRequest::RelationshipId(const QJsonObject &resource, const QString &relationship) {

  const QStringList ids = RelationshipIds(resource, relationship);
  return ids.isEmpty() ? QString() : ids.first();

}

QStringList OpenTidalRequest::RelationshipIds(const QJsonObject &resource, const QString &relationship) {

  QStringList ids;

  if (!resource.contains("relationships"_L1) || !resource["relationships"_L1].isObject()) {
    return ids;
  }
  const QJsonObject object_relationships = resource["relationships"_L1].toObject();
  if (!object_relationships.contains(relationship) || !object_relationships[relationship].isObject()) {
    return ids;
  }
  const QJsonObject object_relationship = object_relationships[relationship].toObject();
  if (!object_relationship.contains("data"_L1)) {
    return ids;
  }
  const QJsonValue value_data = object_relationship["data"_L1];
  if (value_data.isArray()) {
    const QJsonArray array_data = value_data.toArray();
    for (const QJsonValue &value : array_data) {
      if (!value.isObject()) continue;
      const QString id = value.toObject()["id"_L1].toString();
      if (!id.isEmpty()) ids << id;
    }
  }
  else if (value_data.isObject()) {
    const QString id = value_data.toObject()["id"_L1].toString();
    if (!id.isEmpty()) ids << id;
  }

  return ids;

}

qint64 OpenTidalRequest::ParseDuration(const QString &str) {

  // ISO 8601 duration, e.g. "PT3M4S" or "PT1H2M3.5S".
  static const QRegularExpression regex(u"P(?:([0-9]+)D)?T(?:([0-9]+)H)?(?:([0-9]+)M)?(?:([0-9]+(?:\\.[0-9]+)?)S)?"_s);
  const QRegularExpressionMatch match = regex.match(str);
  if (!match.hasMatch()) return 0;

  const qint64 days = match.captured(1).toLongLong();
  const qint64 hours = match.captured(2).toLongLong();
  const qint64 minutes = match.captured(3).toLongLong();
  const double seconds = match.captured(4).toDouble();

  const double total_seconds = static_cast<double>(((days * 24 + hours) * 60 + minutes) * 60) + seconds;
  return static_cast<qint64>(total_seconds * static_cast<double>(kNsecPerSec));

}

void OpenTidalRequest::ReplyReceived(QNetworkReply *reply, const RequestType type) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  --requests_active_;
  ++requests_received_;

  if (finished_) return;

  const QScopeGuard finish_check = qScopeGuard([this]() {
    if (requests_total_ > 0) {
      Q_EMIT UpdateProgress(query_id_, static_cast<int>((static_cast<float>(requests_received_) / static_cast<float>(requests_total_)) * 100.0F));
    }
    FinishCheck();
  });

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject &json_object = json_object_result.json_object;
  const QHash<QString, QJsonObject> included = ParseIncluded(json_object);

  switch (type) {
    case RequestType::DiscoverTracks:
      HandleDiscoverTracks(json_object, included);
      break;
    case RequestType::DiscoverAlbums:
      HandleDiscoverAlbums(json_object, included);
      break;
    case RequestType::DiscoverArtists:
      HandleDiscoverArtists(json_object, included);
      break;
    case RequestType::ArtistAlbums:
      HandleArtistAlbums(json_object, included);
      break;
    case RequestType::AlbumExpand:
      HandleAlbumExpand(json_object, included);
      break;
  }

  const QUrl next_url = NextUrl(json_object);
  if (next_url.isValid()) {
    AddRequest(type, next_url);
  }

}

QUrl OpenTidalRequest::ParseCoverUrl(const QString &album_id, const QHash<QString, QJsonObject> &included) {

  if (album_id.isEmpty()) return QUrl();

  const QJsonObject object_album = included.value(IncludeKey(u"albums"_s, album_id));
  if (object_album.isEmpty()) return QUrl();

  const QString artwork_id = RelationshipId(object_album, u"coverArt"_s);
  if (artwork_id.isEmpty()) return QUrl();

  const QJsonObject object_artwork = included.value(IncludeKey(u"artworks"_s, artwork_id));
  if (object_artwork.isEmpty() || !object_artwork.contains("attributes"_L1)) return QUrl();

  const QJsonObject object_attributes = object_artwork["attributes"_L1].toObject();
  if (!object_attributes.contains("files"_L1) || !object_attributes["files"_L1].isArray()) return QUrl();

  const int desired_width = coversize_.split(u'x').value(0).toInt();

  QUrl best_url;
  int best_width = 0;
  const QJsonArray array_files = object_attributes["files"_L1].toArray();
  for (const QJsonValue &value_file : array_files) {
    if (!value_file.isObject()) continue;
    const QJsonObject object_file = value_file.toObject();
    const QString href = object_file["href"_L1].toString();
    if (href.isEmpty() || !object_file["meta"_L1].isObject()) continue;
    const QJsonObject object_meta = object_file["meta"_L1].toObject();
    const int width = object_meta["width"_L1].toInt();
    if (width == desired_width) {
      return QUrl(href);
    }
    if (width > best_width) {
      best_width = width;
      best_url = QUrl(href);
    }
  }

  return best_url;

}

void OpenTidalRequest::HandleDiscoverTracks(const QJsonObject &json_object, const QHash<QString, QJsonObject> &included) {

  Q_UNUSED(json_object)

  const QList<QJsonObject> resources = included.values();
  for (const QJsonObject &object : resources) {
    if (object["type"_L1].toString() != "tracks"_L1) continue;

    const QString album_id = RelationshipId(object, u"albums"_s);
    QString album_title;
    QString album_artist;
    if (!album_id.isEmpty()) {
      const QJsonObject object_album = included.value(IncludeKey(u"albums"_s, album_id));
      album_title = object_album["attributes"_L1].toObject()["title"_L1].toString();
      const QString album_artist_id = RelationshipId(object_album, u"artists"_s);
      if (!album_artist_id.isEmpty()) {
        album_artist = included.value(IncludeKey(u"artists"_s, album_artist_id))["attributes"_L1].toObject()["name"_L1].toString();
      }
    }

    const QUrl cover_url = ParseCoverUrl(album_id, included);

    const Song song = ParseTrack(object, album_id, album_title, album_artist, cover_url, 0, 0);
    if (song.is_valid()) {
      songs_.insert(song.song_id(), song);
    }
  }

}

void OpenTidalRequest::HandleDiscoverAlbums(const QJsonObject &json_object, const QHash<QString, QJsonObject> &included) {

  Q_UNUSED(json_object)

  const QList<QJsonObject> resources = included.values();
  for (const QJsonObject &object : resources) {
    if (object["type"_L1].toString() != "albums"_L1) continue;
    const QString album_id = object["id"_L1].toString();
    AddAlbumExpandRequest(album_id);
  }

}

void OpenTidalRequest::HandleDiscoverArtists(const QJsonObject &json_object, const QHash<QString, QJsonObject> &included) {

  Q_UNUSED(json_object)

  const QList<QJsonObject> resources = included.values();
  for (const QJsonObject &object : resources) {
    if (object["type"_L1].toString() != "artists"_L1) continue;
    const QString artist_id = object["id"_L1].toString();
    AddArtistAlbumsRequest(artist_id);
  }

}

void OpenTidalRequest::HandleArtistAlbums(const QJsonObject &json_object, const QHash<QString, QJsonObject> &included) {

  Q_UNUSED(json_object)

  const QList<QJsonObject> resources = included.values();
  for (const QJsonObject &object : resources) {
    if (object["type"_L1].toString() != "albums"_L1) continue;
    AddAlbumExpandRequest(object["id"_L1].toString());
  }

}

void OpenTidalRequest::HandleAlbumExpand(const QJsonObject &json_object, const QHash<QString, QJsonObject> &included) {

  if (!json_object.contains("data"_L1) || !json_object["data"_L1].isObject()) {
    return;
  }
  const QJsonObject object_album = json_object["data"_L1].toObject();
  const QString album_id = object_album["id"_L1].toString();
  const QString album_title = object_album["attributes"_L1].toObject()["title"_L1].toString();

  QString album_artist;
  const QString album_artist_id = RelationshipId(object_album, u"artists"_s);
  if (!album_artist_id.isEmpty()) {
    album_artist = included.value(IncludeKey(u"artists"_s, album_artist_id))["attributes"_L1].toObject()["name"_L1].toString();
  }

  // The album cover art is included directly with the album resource.
  QHash<QString, QJsonObject> cover_lookup = included;
  cover_lookup.insert(IncludeKey(u"albums"_s, album_id), object_album);
  const QUrl cover_url = ParseCoverUrl(album_id, cover_lookup);

  // The items relationship carries the resource identifiers with the volume and track numbers in their meta.
  if (!object_album.contains("relationships"_L1) || !object_album["relationships"_L1].isObject()) {
    return;
  }
  const QJsonObject object_relationships = object_album["relationships"_L1].toObject();
  if (!object_relationships.contains("items"_L1) || !object_relationships["items"_L1].isObject()) {
    return;
  }
  const QJsonObject object_items = object_relationships["items"_L1].toObject();
  if (!object_items.contains("data"_L1) || !object_items["data"_L1].isArray()) {
    return;
  }

  const QJsonArray array_items = object_items["data"_L1].toArray();
  for (const QJsonValue &value_item : array_items) {
    if (!value_item.isObject()) continue;
    const QJsonObject object_item = value_item.toObject();
    if (object_item["type"_L1].toString() != "tracks"_L1) continue;
    const QString track_id = object_item["id"_L1].toString();

    int volume_number = 1;
    int track_number = 0;
    if (object_item.contains("meta"_L1) && object_item["meta"_L1].isObject()) {
      const QJsonObject object_meta = object_item["meta"_L1].toObject();
      volume_number = object_meta["volumeNumber"_L1].toInt(1);
      track_number = object_meta["trackNumber"_L1].toInt(0);
    }

    const QJsonObject object_track = included.value(IncludeKey(u"tracks"_s, track_id));
    if (object_track.isEmpty()) continue;

    const Song song = ParseTrack(object_track, album_id, album_title, album_artist, cover_url, volume_number, track_number);
    if (song.is_valid()) {
      songs_.insert(song.song_id(), song);
    }
  }

}

Song OpenTidalRequest::ParseTrack(const QJsonObject &track, const QString &album_id, const QString &album_title, const QString &album_artist, const QUrl &cover_url, const int volume_number, const int track_number) {

  Song song(Song::Source::OpenTidal);

  const QString song_id = track["id"_L1].toString();
  if (song_id.isEmpty()) {
    Error(u"Invalid Json reply, track is missing id."_s, track);
    return song;
  }

  if (!track.contains("attributes"_L1) || !track["attributes"_L1].isObject()) {
    Error(u"Invalid Json reply, track is missing attributes."_s, track);
    return song;
  }
  const QJsonObject object_attributes = track["attributes"_L1].toObject();

  QString title = object_attributes["title"_L1].toString();
  if (service_->remove_remastered()) {
    title = Song::TitleRemoveMisc(title);
  }

  // Resolve the track artist from the track's artists relationship if available, otherwise fall back to the album artist.
  QString artist = album_artist;
  QString artist_id;
  const QString track_artist_id = RelationshipId(track, u"artists"_s);
  if (!track_artist_id.isEmpty()) {
    artist_id = track_artist_id;
  }

  QString album = album_title;
  if (service_->album_explicit() && object_attributes["explicit"_L1].toBool() && !album.isEmpty()) {
    album.append(" (Explicit)"_L1);
  }

  QUrl url;
  url.setScheme(url_handler_->scheme());
  url.setPath(song_id);

  song.set_source(Song::Source::OpenTidal);
  song.set_song_id(song_id);
  song.set_album_id(album_id);
  song.set_artist_id(artist_id);
  if (!album_artist.isEmpty() && album_artist != artist) song.set_albumartist(album_artist);
  song.set_album(album);
  song.set_artist(artist);
  song.set_title(title);
  song.set_track(track_number);
  song.set_disc(volume_number);
  song.set_url(url);
  song.set_length_nanosec(ParseDuration(object_attributes["duration"_L1].toString()));
  if (cover_url.isValid()) {
    song.set_art_automatic(cover_url);
  }
  song.set_comment(object_attributes["copyright"_L1].toObject()["text"_L1].toString());
  song.set_directory_id(0);
  song.set_filetype(Song::FileType::Stream);
  song.set_filesize(0);
  song.set_mtime(0);
  song.set_ctime(0);
  song.set_valid(true);

  return song;

}

void OpenTidalRequest::FinishCheck() {

  if (finished_) return;

  if (requests_queue_.isEmpty() && requests_active_ <= 0) {
    if (timer_flush_requests_->isActive()) {
      timer_flush_requests_->stop();
    }
    finished_ = true;
    if (songs_.isEmpty()) {
      if (error_.isEmpty()) {
        if (IsSearch()) {
          Q_EMIT Results(query_id_, SongMap(), tr("No match."));
        }
        else {
          Q_EMIT Results(query_id_);
        }
      }
      else {
        Q_EMIT Results(query_id_, SongMap(), error_);
      }
    }
    else {
      Q_EMIT Results(query_id_, songs_);
    }
  }

}

void OpenTidalRequest::Error(const QString &error_message, const QVariant &debug_output) {

  qLog(Error) << "OpenTidal:" << error_message;
  if (debug_output.isValid()) {
    qLog(Debug) << debug_output;
  }

  error_ = error_message;

}
