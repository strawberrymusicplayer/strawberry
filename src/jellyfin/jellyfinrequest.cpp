/*
 * Strawberry Music Player
 * Copyright 2026, Strawberry Music Player contributors
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

#include <QtGlobal>
#include <QObject>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QNetworkReply>
#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QScopeGuard>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "core/song.h"
#include "core/networktimeouts.h"
#include "utilities/coverutils.h"
#include "utilities/imageutils.h"
#include "utilities/strutils.h"
#include "jellyfinservice.h"
#include "jellyfinbaserequest.h"
#include "jellyfinrequest.h"

using namespace Qt::Literals::StringLiterals;

namespace {
constexpr int kLimit = 200;
constexpr int kMaxConcurrentRequests = 3;
constexpr int kMaxConcurrentAlbumCoverRequests = 3;
constexpr int kMaxPageRetries = 3;
constexpr int kMaxPages = 1000;
constexpr int kCoverSize = 600;
constexpr int kRequestTimeoutMs = 30000;
// Number of artist and album search results expanded into their tracks.
constexpr int kSearchArtistsLimit = 4;
constexpr int kSearchAlbumsLimit = 10;
constexpr int kTracksLimit = 1000;
}  // namespace

JellyfinRequest::JellyfinRequest(JellyfinService *service, const SharedPtr<NetworkAccessManager> network, const Type query_type, QObject *parent)
    : JellyfinBaseRequest(service, network, parent),
      timeouts_(new NetworkTimeouts(kRequestTimeoutMs, this)),
      query_type_(query_type),
      query_id_(0),
      finished_(false),
      requests_active_(0),
      items_total_(-1),
      items_received_(0),
      paging_complete_(false),
      unauthorized_(false),
      tracks_requests_active_(0),
      album_covers_requests_active_(0),
      album_covers_requested_(0),
      album_covers_received_(0) {}

void JellyfinRequest::Process() {

  query_id_ = 0;
  StartRequests();

}

void JellyfinRequest::Search(const int query_id, const QString &search_text) {

  query_id_ = query_id;
  search_text_ = search_text;
  StartRequests();

}

void JellyfinRequest::Abort() {

  // Don't process replies or send more requests, the request was replaced or is not needed anymore.
  finished_ = true;
  AbortNetworkReplies();

}

void JellyfinRequest::StartRequests() {

  if (query_type_ == Type::None) {
    Q_EMIT Results(query_id_, SongMap(), tr("Invalid query type."));
    return;
  }

  if (IsSearch()) {
    Q_EMIT UpdateStatus(query_id_, tr("Searching for %1...").arg(search_text_));
  }
  else {
    switch (query_type_) {
      case Type::FavouriteArtists:
        Q_EMIT UpdateStatus(query_id_, tr("Retrieving favorite artists..."));
        break;
      case Type::FavouriteAlbums:
        Q_EMIT UpdateStatus(query_id_, tr("Retrieving favorite albums..."));
        break;
      default:
        Q_EMIT UpdateStatus(query_id_, tr("Retrieving favorite songs..."));
        break;
    }
  }

  AddRequest(0);

}

void JellyfinRequest::AddRequest(const int offset) {

  // Don't send more requests with the rejected access token.
  if (finished_ || unauthorized_) return;
  if (pages_scheduled_.contains(offset) || pages_queued_.contains(offset)) return;

  pages_queued_.insert(offset);
  requests_queue_.enqueue(offset);
  FlushRequests();

}

void JellyfinRequest::FlushRequests() {

  if (finished_) return;

  while (!requests_queue_.isEmpty() && requests_active_ < kMaxConcurrentRequests) {

    const int offset = requests_queue_.dequeue();
    ++requests_active_;

    ParamList params;
    params << Param(u"Recursive"_s, u"true"_s)
           << Param(u"SortBy"_s, u"SortName,DateCreated"_s)
           << Param(u"SortOrder"_s, u"Ascending"_s)
           << Param(u"EnableTotalRecordCount"_s, u"true"_s)
           << Param(u"Fields"_s, u"MediaStreams,Artists"_s)
           << Param(u"StartIndex"_s, QString::number(offset));

    switch (query_type_) {
      case Type::SearchArtists:
        params << Param(u"Limit"_s, QString::number(kSearchArtistsLimit));
        break;
      case Type::SearchAlbums:
        params << Param(u"Limit"_s, QString::number(kSearchAlbumsLimit));
        break;
      default:
        params << Param(u"Limit"_s, QString::number(kLimit));
        break;
    }

    params << Param(u"IncludeItemTypes"_s, IncludeItemTypes());
    if (IsSearch()) {
      params << Param(u"SearchTerm"_s, search_text_);
    }
    else {
      params << Param(u"Filters"_s, u"IsFavorite"_s);
    }

    QNetworkReply *reply = CreateGetRequest(RessourcePath(), params);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, offset]() { ReplyReceived(reply, offset); });
    timeouts_->AddReply(reply);

  }

}

QString JellyfinRequest::RessourcePath() const {
  return u"Users/%1/Items"_s.arg(service()->user_id());
}

QString JellyfinRequest::IncludeItemTypes() const {

  switch (query_type_) {
    case Type::FavouriteArtists:
    case Type::SearchArtists:
      return u"MusicArtist"_s;
    case Type::FavouriteAlbums:
    case Type::SearchAlbums:
      return u"MusicAlbum"_s;
    case Type::FavouriteSongs:
    case Type::SearchSongs:
      return u"Audio"_s;
    default:
      return QString();
  }

}

int JellyfinRequest::Catalog() const {

  switch (query_type_) {
    case Type::FavouriteArtists:
      return JellyfinService::CatalogArtists;
    case Type::FavouriteAlbums:
      return JellyfinService::CatalogAlbums;
    case Type::FavouriteSongs:
      return JellyfinService::CatalogSongs;
    default:
      return JellyfinService::CatalogNone;
  }

}

void JellyfinRequest::ReplyReceived(QNetworkReply *reply, const int offset_requested) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  --requests_active_;

  const QScopeGuard finish_check = qScopeGuard([this]() { FinishCheck(); });

  if (finished_) return;

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {

    pages_queued_.remove(offset_requested);

    if (json_object_result.http_status_code == 401) {
      qLog(Debug) << "Jellyfin:" << "Received HTTP code 401 for offset" << offset_requested << "- re-authenticating.";
      Unauthorized(reply);
      return;
    }

    if (RetryPage(offset_requested)) return;
    Error(json_object_result.error_message);
    return;
  }

  // The server accepted the access token.
  service()->TokenValidated();

  const QJsonObject json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    pages_queued_.remove(offset_requested);
    if (RetryPage(offset_requested)) return;
    Error(tr("Received an empty reply for offset %1.").arg(offset_requested));
    return;
  }

  const JsonArrayResult array_items_result = GetJsonArray(json_object, u"Items"_s);
  if (!array_items_result.success()) {
    pages_queued_.remove(offset_requested);
    if (RetryPage(offset_requested)) return;
    Error(array_items_result.error_message);
    return;
  }

  const QJsonArray array_items = array_items_result.json_array;
  const int page_items = static_cast<int>(array_items.size());

  int items_received = 0;
  SongMap available_songs;
  for (const QJsonValue &value_item : array_items) {

    if (!value_item.isObject()) {
      Warn(u"Invalid Json reply, item is not an object."_s);
      continue;
    }

    if (IsParentQuery()) {
      const QString parent_id = value_item.toObject().value(u"Id"_s).toString();
      if (!parent_id.isEmpty()) {
        parent_names_.insert(parent_id, value_item.toObject().value(u"Name"_s).toString());
        AddTracksRequest(parent_id);
      }
      continue;
    }

    Song song(Song::Source::Jellyfin);
    if (!ParseItem(song, value_item.toObject())) {
      continue;
    }

    if (!songs_.contains(song.song_id())) ++items_received;
    songs_.insert(song.song_id(), song);
    available_songs.insert(song.song_id(), song);

  }

  if (!available_songs.isEmpty()) {
    Q_EMIT SongsAvailable(query_id_, available_songs);
  }

  items_received_ += page_items;
  pages_queued_.remove(offset_requested);
  pages_scheduled_.insert(offset_requested);
  page_retries_.remove(offset_requested);

  // The TotalRecordCount is used for progress and sanity checks only, since some Jellyfin versions misreport or omit it.
  // Paging is terminated by the server returning a page with fewer items than requested.
  if (json_object.contains(u"TotalRecordCount"_s) && json_object.value(u"TotalRecordCount"_s).isDouble()) {
    const int server_total = json_object.value(u"TotalRecordCount"_s).toInt();
    if (server_total > items_total_) items_total_ = server_total;
  }
  if (items_total_ < items_received_) items_total_ = items_received_;

  Q_EMIT UpdateProgress(query_id_, GetProgress(items_received_, items_total_));

  qLog(Debug) << "Jellyfin:" << "Page for offset" << offset_requested << "->" << page_items << "items (" << items_received << "new), total" << items_total_ << ", unique songs" << songs_.size() << ", received" << items_received_;

  const int next_offset = offset_requested + kLimit;
  if (IsSearch()) {
    // Only the first page of search results is used.
    paging_complete_ = true;
  }
  else if (page_items >= kLimit && !paging_complete_) {
    if (pages_scheduled_.size() >= kMaxPages) {
      // Use the songs received so far.
      paging_complete_ = true;
      Warn(QStringLiteral("Stopping pagination after reaching the maximum number of pages (%1), the catalog is incomplete.").arg(kMaxPages));
    }
    else {
      AddRequest(next_offset);
    }
  }
  else {
    paging_complete_ = true;
    qLog(Debug) << "Jellyfin:" << "Paging complete at offset" << offset_requested << "-" << songs_.size() << "unique songs of" << items_total_ << "total items.";
  }

}

bool JellyfinRequest::RetryPage(const int offset) {

  if (unauthorized_) return false;

  int &retry_count = page_retries_[offset];
  if (retry_count >= kMaxPageRetries) {
    page_retries_.remove(offset);
    return false;
  }
  ++retry_count;
  AddRequest(offset);
  return true;

}

void JellyfinRequest::Unauthorized(QNetworkReply *reply) {

  // Report the error once, and don't send more requests with the rejected access token, FinishCheck() emits the results when the active requests finished.
  if (unauthorized_) return;
  unauthorized_ = true;

  requests_queue_.clear();
  tracks_requests_queue_.clear();
  if (service()->Unauthorized(QString::fromUtf8(reply->request().rawHeader("X-Emby-Token")), Catalog())) {
    Error(tr("Not authorized, logging in again."));
  }
  else {
    Error(tr("Not authorized with Jellyfin, try again later."));
  }

}

void JellyfinRequest::AddTracksRequest(const QString &parent_id, const int offset, const int retries) {

  if (finished_ || unauthorized_) return;
  if (offset == 0 && retries == 0) {
    if (tracks_requests_sent_.contains(parent_id)) return;
    tracks_requests_sent_.insert(parent_id);
  }

  tracks_requests_queue_.enqueue({parent_id, offset, retries});
  FlushTracksRequests();

}

void JellyfinRequest::FlushTracksRequests() {

  if (finished_) return;

  while (!tracks_requests_queue_.isEmpty() && tracks_requests_active_ < kMaxConcurrentRequests) {

    const TracksRequest request = tracks_requests_queue_.dequeue();
    ++tracks_requests_active_;

    ParamList params;
    params << Param(u"Recursive"_s, u"true"_s)
           << Param(u"IncludeItemTypes"_s, u"Audio"_s)
           << Param(u"SortBy"_s, u"ParentIndexNumber,IndexNumber,SortName"_s)
           << Param(u"SortOrder"_s, u"Ascending"_s)
           << Param(u"Fields"_s, u"MediaStreams,Artists"_s)
           << Param(u"Limit"_s, QString::number(kTracksLimit))
           << Param(u"StartIndex"_s, QString::number(request.offset));
    if (IsArtistQuery()) {
      params << Param(u"ArtistIds"_s, request.parent_id);
    }
    else {
      params << Param(u"ParentId"_s, request.parent_id);
    }

    QNetworkReply *reply = CreateGetRequest(RessourcePath(), params);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { TracksReplyReceived(reply, request); });
    timeouts_->AddReply(reply);

  }

}

void JellyfinRequest::TracksReplyReceived(QNetworkReply *reply, const TracksRequest &request) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  --tracks_requests_active_;

  const QScopeGuard finish_check = qScopeGuard([this]() { FinishCheck(); });

  if (finished_) return;

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {
    if (json_object_result.http_status_code == 401) {
      Unauthorized(reply);
      return;
    }
    if (request.retries < kMaxPageRetries) {
      AddTracksRequest(request.parent_id, request.offset, request.retries + 1);
      return;
    }
    Error(json_object_result.error_message);
    return;
  }

  service()->TokenValidated();

  const JsonArrayResult array_items_result = GetJsonArray(json_object_result.json_object, u"Items"_s);
  if (!array_items_result.success()) {
    if (request.retries < kMaxPageRetries) {
      AddTracksRequest(request.parent_id, request.offset, request.retries + 1);
      return;
    }
    Error(array_items_result.error_message);
    return;
  }

  const QJsonArray array_items = array_items_result.json_array;
  SongMap available_songs;
  for (const QJsonValue &value_item : array_items) {
    if (!value_item.isObject()) continue;
    Song song(Song::Source::Jellyfin);
    if (ParseItem(song, value_item.toObject())) {
      // The artist the songs were loaded for, the first artist of a song can be another artist, this is used to change the favorite artist.
      // A song can belong to several of the artists, prefer the artist it's shown for in the artists tab, which groups by album artist, then its artist.
      // Otherwise keep the artist it was loaded for first.
      if (IsArtistQuery()) {
        const auto artist_match = [this, &song](const QString &artist_id) {
          const QString artist_name = parent_names_.value(artist_id);
          if (artist_name.isEmpty()) return 0;
          if (song.albumartist().compare(artist_name, Qt::CaseInsensitive) == 0) return 2;
          if (song.artist().compare(artist_name, Qt::CaseInsensitive) == 0) return 1;
          return 0;
        };
        const QString previous_artist_id = songs_.contains(song.song_id()) ? songs_.value(song.song_id()).artist_id() : QString();
        song.set_artist_id(!previous_artist_id.isEmpty() && artist_match(previous_artist_id) >= artist_match(request.parent_id) ? previous_artist_id : request.parent_id);
      }
      songs_.insert(song.song_id(), song);
      available_songs.insert(song.song_id(), song);
    }
  }

  if (!available_songs.isEmpty()) {
    Q_EMIT SongsAvailable(query_id_, available_songs);
  }

  qLog(Debug) << "Jellyfin:" << "Received" << array_items.size() << "tracks for" << request.parent_id << "at offset" << request.offset;

  // Request the next page of tracks.
  if (array_items.size() >= kTracksLimit) {
    AddTracksRequest(request.parent_id, request.offset + kTracksLimit);
  }

}

bool JellyfinRequest::ParseItem(Song &song, const QJsonObject &json_object) {

  if (!json_object.contains(u"Id"_s) || !json_object.contains(u"Name"_s) || !json_object.contains(u"Type"_s)) {
    // Skip the item, one invalid item should not discard the whole catalog.
    Warn(u"Invalid Json reply, item is missing Id, Name or Type."_s, json_object);
    return false;
  }

  const QString item_id = json_object.value(u"Id"_s).toString();
  const QString name = json_object.value(u"Name"_s).toString();
  const QString type = json_object.value(u"Type"_s).toString();

  if (item_id.isEmpty() || name.isEmpty()) {
    Warn(u"Invalid Json reply, item has empty Id or Name."_s, json_object);
    return false;
  }

  song.set_source(Song::Source::Jellyfin);
  song.set_song_id(item_id);
  song.set_url(QUrl(u"jellyfin://"_s + item_id));

  if (type == u"Audio"_s) {
    return ParseAudio(song, json_object);
  }

  return false;

}

bool JellyfinRequest::ParseAudio(Song &song, const QJsonObject &json_object) {

  const QString name = json_object.value(u"Name"_s).toString();
  const QString item_id = json_object.value(u"Id"_s).toString();
  QString album = json_object.value(u"Album"_s).toString();
  const QString album_id = json_object.value(u"AlbumId"_s).toString();
  QString album_artist = json_object.value(u"AlbumArtist"_s).isNull() ? QString() : json_object.value(u"AlbumArtist"_s).toString();
  const QString album_artist_id = json_object.value(u"AlbumArtistId"_s).isNull() ? QString() : json_object.value(u"AlbumArtistId"_s).toString();

  QStringList artists;
  if (json_object.contains(u"Artists"_s) && json_object.value(u"Artists"_s).isArray()) {
    const QJsonArray array_artists = json_object.value(u"Artists"_s).toArray();
    for (const QJsonValue &value_artist : array_artists) {
      if (value_artist.isString() && !value_artist.toString().isEmpty()) {
        artists << value_artist.toString();
      }
    }
  }

  QString artist_id = album_artist_id;
  if (json_object.contains(u"ArtistItems"_s) && json_object.value(u"ArtistItems"_s).isArray()) {
    const QJsonArray array_artist_items = json_object.value(u"ArtistItems"_s).toArray();
    for (const QJsonValue &value_artist_item : array_artist_items) {
      if (!value_artist_item.isObject()) continue;
      const QJsonObject object_artist_item = value_artist_item.toObject();
      if (object_artist_item.contains(u"Id"_s) && object_artist_item.contains(u"Name"_s)) {
        artist_id = object_artist_item.value(u"Id"_s).toString();
        break;
      }
    }
  }

  // A featured artist doesn't make the album a compilation, only an album artist that is not part of any of the track artists does.
  // Jellyfin doesn't split artists like "Artist feat. Other Artist", so the album artist only has to be contained in a track artist.
  bool compilation = false;
  if (!album_artist.isEmpty()) {
    compilation = album_artist.compare("Various Artists"_L1, Qt::CaseInsensitive) == 0;
    if (!compilation && !artists.isEmpty()) {
      compilation = !std::any_of(artists.cbegin(), artists.cend(), [&album_artist](const QString &track_artist) { return track_artist.contains(album_artist, Qt::CaseInsensitive); });
    }
  }
  QString artist = artists.isEmpty() ? album_artist : artists[0];
  if (artist.isEmpty()) artist = u"Unknown Artist"_s;
  if (album.isEmpty()) album = u"Unknown Album"_s;
  if (album_artist.isEmpty()) album_artist = artist;

  const int track = json_object.value(u"IndexNumber"_s).toInt();
  const int disc = json_object.value(u"ParentIndexNumber"_s).toInt();
  const int year = json_object.value(u"ProductionYear"_s).toInt();
  const qint64 length_nanosec = static_cast<qint64>(json_object.value(u"RunTimeTicks"_s).toDouble() * 100.0);

  QString genre;
  if (json_object.contains(u"Genres"_s) && json_object.value(u"Genres"_s).isArray()) {
    const QJsonArray array_genres = json_object.value(u"Genres"_s).toArray();
    for (const QJsonValue &value_genre : array_genres) {
      if (value_genre.isString() && !value_genre.toString().isEmpty()) {
        genre = value_genre.toString();
        break;
      }
    }
  }

  // The bitrate of the audio stream is in bits per second.
  int bitrate = 0;
  if (json_object.contains(u"MediaStreams"_s) && json_object.value(u"MediaStreams"_s).isArray()) {
    const QJsonArray array_streams = json_object.value(u"MediaStreams"_s).toArray();
    for (const QJsonValue &value_stream : array_streams) {
      if (!value_stream.isObject()) continue;
      const QJsonObject object_stream = value_stream.toObject();
      if (object_stream.value(u"Type"_s).toString() != "Audio"_L1) continue;
      bitrate = static_cast<int>(object_stream.value(u"BitRate"_s).toDouble() / 1000.0);
      break;
    }
  }

  Song::FileType filetype(Song::FileType::Stream);
  if (json_object.contains(u"Container"_s)) {
    const QString container = json_object.value(u"Container"_s).toString();
    if (!container.isEmpty()) filetype = Song::FiletypeByExtension(container);
  }

  QString cover_item_id;
  QString cover_image_tag;
  const QString album_primary_image_tag = json_object.value(u"AlbumPrimaryImageTag"_s).toString();
  if (!album_id.isEmpty() && !album_primary_image_tag.isEmpty()) {
    cover_item_id = album_id;
    cover_image_tag = album_primary_image_tag;
  }
  else if (json_object.contains(u"ImageTags"_s) && json_object.value(u"ImageTags"_s).isObject()) {
    const QJsonObject object_image_tags = json_object.value(u"ImageTags"_s).toObject();
    if (object_image_tags.contains(u"Primary"_s)) {
      cover_item_id = item_id;
      cover_image_tag = object_image_tags.value(u"Primary"_s).toString();
    }
  }

  song.set_title(name);
  song.set_artist(artist);
  song.set_albumartist(compilation ? u"Various Artists"_s : album_artist);
  song.set_artist_id(artist_id);
  song.set_album(album);
  song.set_album_id(album_id);
  song.set_track(track);
  song.set_disc(disc);
  song.set_year(year);
  song.set_genre(genre);
  song.set_length_nanosec(length_nanosec);
  song.set_bitrate(bitrate);
  song.set_filetype(filetype);
  if (compilation) song.set_compilation_detected(true);
  song.set_directory_id(0);
  // Only set a cover when an image exists, the album would otherwise request an image that doesn't exist on every catalog load.
  if (!cover_item_id.isEmpty()) song.set_art_automatic(QUrl(CreateImageUrl(cover_item_id, cover_image_tag)));
  song.set_valid(true);

  return true;

}

QString JellyfinRequest::CreateImageUrl(const QString &item_id, const QString &image_tag) const {

  QUrl url = CreateUrl(u"Items/%1/Images/Primary"_s.arg(item_id));
  QUrlQuery url_query;
  url_query.addQueryItem(u"maxWidth"_s, QString::number(kCoverSize));
  url_query.addQueryItem(u"maxHeight"_s, QString::number(kCoverSize));
  url_query.addQueryItem(u"quality"_s, QString::number(90));
  if (!image_tag.isEmpty()) url_query.addQueryItem(u"tag"_s, image_tag);
  // Don't add the access token, the URL is saved in the database and the token changes with each login.
  // Jellyfin serves item images without authentication, and cover downloads send the authorization headers.
  url.setQuery(url_query);

  return url.toString();

}

void JellyfinRequest::GetAlbumCovers() {

  const SongList songs = songs_.values();
  for (const Song &song : songs) {
    if (!song.art_automatic().isEmpty()) AddAlbumCoverRequest(song);
  }
  FlushAlbumCoverRequests();

  if (album_covers_requested_ > 0) {
    Q_EMIT UpdateStatus(query_id_, tr("Retrieving album covers for %n album(s)...", "", album_covers_requested_));
  }
  Q_EMIT UpdateProgress(query_id_, GetProgress(0, album_covers_requested_));

}

void JellyfinRequest::AddAlbumCoverRequest(const Song &song) {

  const QUrl cover_url = song.art_automatic();
  if (!cover_url.isValid() || cover_url.scheme() == QLatin1String("file")) return;

  // Songs share a cover download only when they have the same image, which is identified by the item in the cover URL and the image tag.
  // The cover can be the image of the album or of the song itself, and the tag changes when the image changes.
  const QStringList path_parts = cover_url.path().split(u'/', Qt::SkipEmptyParts);
  const qsizetype items_index = path_parts.lastIndexOf(u"Items"_s);
  if (items_index < 0 || items_index + 1 >= path_parts.size()) return;
  const QString image_item_id = path_parts.at(items_index + 1);
  const QString image_tag = QUrlQuery(cover_url).queryItemValue(u"tag"_s);
  // The image ID and tag from the server are used in the cache filename, so only allow letters and digits, which Jellyfin uses.
  static const QRegularExpression regex_id(u"^[A-Za-z0-9]+$"_s);
  if (!regex_id.match(image_item_id).hasMatch() || (!image_tag.isEmpty() && !regex_id.match(image_tag).hasMatch())) return;
  const QString image_id = image_tag.isEmpty() ? image_item_id : image_item_id + u'-' + image_tag;

  const QString cover_path = Song::ImageCacheDir(Song::Source::Jellyfin);
  QDir dir(cover_path);
  if (!dir.exists()) dir.mkpath(cover_path);

  AlbumCoverRequest request;
  request.image_id = image_id;
  request.url = cover_url;
  // The filename already includes the image tag from the cover URL.
  request.filename = cover_path + QLatin1Char('/') + CoverUtils::CoverFilenameFromSource(Song::Source::Jellyfin, cover_url, song.effective_albumartist(), song.album(), image_item_id, u"jpg"_s);
  if (request.filename.isEmpty()) return;

  // The cover is already in the cache, so reuse it instead of re-downloading it on every catalog load.
  if (QFile::exists(request.filename)) {
    if (songs_.contains(song.song_id())) {
      songs_[song.song_id()].set_art_automatic(QUrl::fromLocalFile(request.filename));
    }
    return;
  }

  // Record the song ID under the shared image ID so that every song belonging to the same album gets the downloaded cover applied, while only a single network request is queued for each image.
  const bool already_requested = album_covers_requests_sent_.contains(image_id);
  album_covers_requests_sent_[image_id] << song.song_id();
  if (already_requested) return;

  ++album_covers_requested_;

  album_cover_requests_queue_.enqueue(request);

}

void JellyfinRequest::FlushAlbumCoverRequests() {

  while (!album_cover_requests_queue_.isEmpty() && album_covers_requests_active_ < kMaxConcurrentAlbumCoverRequests) {

    const AlbumCoverRequest request = album_cover_requests_queue_.dequeue();
    ++album_covers_requests_active_;

    QNetworkReply *reply = CreateGetRequestFromUrl(request.url);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, request]() { AlbumCoverReceived(reply, request); });
    timeouts_->AddReply(reply);

  }

}

void JellyfinRequest::AlbumCoverReceived(QNetworkReply *reply, const AlbumCoverRequest &request) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  --album_covers_requests_active_;
  ++album_covers_received_;

  const QScopeGuard album_cover_finish_check = qScopeGuard([this]() { AlbumCoverFinishCheck(); });

  if (finished_) return;

  Q_EMIT UpdateProgress(query_id_, GetProgress(album_covers_received_, album_covers_requested_));

  if (!album_covers_requests_sent_.contains(request.image_id)) return;

  // Covers are optional, so failed cover downloads are only logged, they don't make the results an error.

  if (reply->error() != QNetworkReply::NoError) {
    Warn(QStringLiteral("%1 (%2) for %3").arg(reply->errorString(), QString::number(reply->error()), Utilities::UrlForLog(request.url)));
    album_covers_requests_sent_.remove(request.image_id);
    return;
  }

  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    Warn(QStringLiteral("Received HTTP code %1 for %2.").arg(QString::number(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()), Utilities::UrlForLog(request.url)));
    album_covers_requests_sent_.remove(request.image_id);
    return;
  }

  QString mimetype = reply->header(QNetworkRequest::ContentTypeHeader).toString();
  if (mimetype.contains(u';')) {
    mimetype = mimetype.left(mimetype.indexOf(u';'));
  }
  if (!ImageUtils::SupportedImageMimeTypes().contains(mimetype, Qt::CaseInsensitive) && !ImageUtils::SupportedImageFormats().contains(mimetype, Qt::CaseInsensitive)) {
    Warn(QStringLiteral("Unsupported mimetype for image reader %1 for %2").arg(mimetype, Utilities::UrlForLog(request.url)));
    album_covers_requests_sent_.remove(request.image_id);
    return;
  }

  const QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    Warn(QStringLiteral("Received empty image data for %1").arg(Utilities::UrlForLog(request.url)));
    album_covers_requests_sent_.remove(request.image_id);
    return;
  }

  QByteArrayList format_list = QImageReader::imageFormatsForMimeType(mimetype.toUtf8());
  char *format = nullptr;
  if (!format_list.isEmpty()) {
    format = format_list[0].data();
  }

  QImage image;
  if (image.loadFromData(data, format)) {
    if (image.save(request.filename, format)) {
      const QStringList song_ids = album_covers_requests_sent_.take(request.image_id);
      for (const QString &song_id : song_ids) {
        if (songs_.contains(song_id)) {
          songs_[song_id].set_art_automatic(QUrl::fromLocalFile(request.filename));
        }
      }
    }
    else {
      Warn(QStringLiteral("Error saving image data to %1.").arg(request.filename));
      album_covers_requests_sent_.remove(request.image_id);
    }
  }
  else {
    Warn(QStringLiteral("Error decoding image data from %1.").arg(Utilities::UrlForLog(request.url)));
    album_covers_requests_sent_.remove(request.image_id);
  }

}

void JellyfinRequest::AlbumCoverFinishCheck() {

  if (finished_) return;

  if (!album_cover_requests_queue_.isEmpty() && album_covers_requests_active_ < kMaxConcurrentAlbumCoverRequests) {
    FlushAlbumCoverRequests();
  }

  FinishCheck();

}

int JellyfinRequest::GetProgress(const int count, const int total) {

  return total <= 0 ? 0 : (count * 100) / total;

}

void JellyfinRequest::FinishCheck() {

  if (finished_) return;

  if (!requests_queue_.isEmpty() && requests_active_ < kMaxConcurrentRequests) {
    FlushRequests();
  }

  if (!tracks_requests_queue_.isEmpty() && tracks_requests_active_ < kMaxConcurrentRequests) {
    FlushTracksRequests();
  }

  // Results with an error are discarded, so don't download covers for them.
  if (download_album_covers() &&
      paging_complete_ &&
      !unauthorized_ &&
      errors_.isEmpty() &&
      requests_queue_.isEmpty() &&
      requests_active_ <= 0 &&
      tracks_requests_queue_.isEmpty() &&
      tracks_requests_active_ <= 0 &&
      album_covers_requested_ == 0) {
    GetAlbumCovers();
  }

  if (requests_queue_.isEmpty() &&
      requests_active_ <= 0 &&
      tracks_requests_queue_.isEmpty() &&
      tracks_requests_active_ <= 0 &&
      album_cover_requests_queue_.isEmpty() &&
      album_covers_requests_active_ <= 0 &&
      album_covers_received_ >= album_covers_requested_) {

    // A page that failed already reported its error.
    if (!paging_complete_ && !unauthorized_ && errors_.isEmpty()) {
      Error(tr("Catalog may be incomplete: stopped receiving results before the end of the list was reached."));
    }
    else if (items_total_ > 0 && items_received_ > items_total_) {
      qLog(Debug) << "Jellyfin:" << "Received" << items_received_ << "items but the server reported" << items_total_ << "total.";
    }

    finished_ = true;
    if (songs_.isEmpty() && errors_.isEmpty()) {
      // The search view shows the message, a catalog without favorites is just empty.
      Q_EMIT Results(query_id_, SongMap(), IsSearch() ? tr("No match.") : QString());
    }
    else if (errors_.isEmpty()) {
      Q_EMIT Results(query_id_, songs_);
    }
    else {
      Q_EMIT Results(query_id_, songs_, Utilities::StringListToHTML(errors_));
    }
  }

}

void JellyfinRequest::Warn(const QString &error, const QVariant &debug) {

  qLog(Warning) << "Jellyfin:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}

void JellyfinRequest::Error(const QString &error, const QVariant &debug) {

  if (!error.isEmpty()) {
    qLog(Error) << "Jellyfin:" << error;
    errors_ << error;
  }
  if (debug.isValid()) qLog(Debug) << debug;

}
