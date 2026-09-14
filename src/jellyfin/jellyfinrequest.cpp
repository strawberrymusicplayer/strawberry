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

#include <utility>

#include <QtGlobal>
#include <QObject>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
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
#include "core/networkaccessmanager.h"
#include "core/jsonbaserequest.h"
#include "core/networktimeouts.h"
#include "utilities/coverutils.h"
#include "utilities/imageutils.h"
#include "utilities/strutils.h"
#include "constants/timeconstants.h"
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

QString UrlForLog(const QUrl &url) {
  return url.adjusted(QUrl::RemoveQuery).toString();
}
}  // namespace

JellyfinRequest::JellyfinRequest(JellyfinService *service, const SharedPtr<NetworkAccessManager> network, const Type query_type, QObject *parent)
    : JellyfinBaseRequest(service, network, parent),
      service_(service),
      network_(network),
      timeouts_(new NetworkTimeouts(kRequestTimeoutMs, this)),
      query_type_(query_type),
      query_id_(0),
      finished_(false),
      requests_active_(0),
      requests_received_(0),
      items_total_(-1),
      items_received_(0),
      paging_complete_(false),
      page_cap_hit_(false),
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
        Q_EMIT UpdateStatus(query_id_, tr("Retrieving artists..."));
        break;
      case Type::FavouriteAlbums:
        Q_EMIT UpdateStatus(query_id_, tr("Retrieving albums..."));
        break;
      case Type::FavouriteSongs:
        Q_EMIT UpdateStatus(query_id_, tr("Retrieving songs..."));
        break;
      default:
        break;
    }
  }

  AddRequest(0);

}

void JellyfinRequest::AddRequest(const int offset) {

  if (finished_) return;
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
           << Param(u"StartIndex"_s, QString::number(offset))
           << Param(u"Limit"_s, QString::number(kLimit));

    if (IsSearch()) {
      params << Param(u"SearchTerm"_s, search_text_);
      params << Param(u"IncludeItemTypes"_s, IncludeItemTypes());
    }
    else {
      params << Param(u"IncludeItemTypes"_s, u"Audio"_s);
    }

    QNetworkReply *reply = CreateGetRequest(RessourcePath(), params);
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, offset]() { ReplyReceived(reply, offset); });
    timeouts_->AddReply(reply);

  }

}

QString JellyfinRequest::RessourcePath() const {
  return u"Users/%1/Items"_s.arg(service_->user_id());
}

QString JellyfinRequest::IncludeItemTypes() const {

  switch (query_type_) {
    case Type::SearchArtists:
      return u"MusicArtist"_s;
    case Type::SearchAlbums:
      return u"MusicAlbum"_s;
    case Type::SearchSongs:
      return u"Audio"_s;
    default:
      return QString();
  }

}

void JellyfinRequest::ReplyReceived(QNetworkReply *reply, const int offset_requested) {

  if (!replies_.contains(reply)) return;
  replies_.removeAll(reply);
  QObject::disconnect(reply, nullptr, this, nullptr);
  reply->deleteLater();

  --requests_active_;
  ++requests_received_;

  const QScopeGuard finish_check = qScopeGuard([this]() { FinishCheck(); });

  if (finished_) return;

  const JsonObjectResult json_object_result = ParseJsonObject(reply);
  if (!json_object_result.success()) {

    pages_queued_.remove(offset_requested);

    if (json_object_result.http_status_code == 401) {
      qLog(Debug) << "Jellyfin:" << "Received HTTP code 401 for offset" << offset_requested << "- re-authenticating.";
      finished_ = true;
      service_->Catalog401();
      return;
    }

    if (RetryPage(offset_requested)) return;
    Error(json_object_result.error_message);
    return;
  }

  const QJsonObject json_object = json_object_result.json_object;
  if (json_object.isEmpty()) {
    return;
  }

  const JsonArrayResult array_items_result = GetJsonArray(json_object, u"Items"_s);
  if (!array_items_result.success()) {
    Error(array_items_result.error_message);
    return;
  }

  const QJsonArray array_items = array_items_result.json_array;
  const int page_items = array_items.size();

  int items_received = 0;
  for (const QJsonValue &value_item : array_items) {

    if (!value_item.isObject()) {
      Error(u"Invalid Json reply, item is not an object."_s);
      continue;
    }

    Song song(Song::Source::Jellyfin);
    if (!ParseItem(song, value_item.toObject())) {
      continue;
    }

    if (!songs_.contains(song.song_id())) ++items_received;
    songs_.insert(song.song_id(), song);

  }

  items_received_ += page_items;
  pages_queued_.remove(offset_requested);
  pages_scheduled_.insert(offset_requested);
  page_retries_.remove(offset_requested);

  // The TotalRecordCount is used for progress and sanity checks only, since some
  // Jellyfin versions misreport or omit it. Paging is terminated by the server
  // returning a page with fewer items than requested
  if (json_object.contains(u"TotalRecordCount"_s) && json_object.value(u"TotalRecordCount"_s).isDouble()) {
    const int server_total = json_object.value(u"TotalRecordCount"_s).toInt();
    if (server_total > items_total_) items_total_ = server_total;
  }
  if (items_total_ < items_received_) items_total_ = items_received_;

  Q_EMIT UpdateProgress(query_id_, GetProgress(items_received_, items_total_));

  qLog(Debug) << "Jellyfin:" << "Page for offset" << offset_requested << "->" << page_items << "items (" << items_received << "new), total" << items_total_ << ", unique songs" << songs_.size() << ", received" << items_received_;

  const int next_offset = offset_requested + kLimit;
  if (page_items >= kLimit && !paging_complete_) {
    if (pages_scheduled_.size() >= kMaxPages) {
      page_cap_hit_ = true;
      qLog(Error) << "Jellyfin:" << "Stopping pagination after reaching the maximum number of pages (" << kMaxPages << ").";
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

  int &retry_count = page_retries_[offset];
  if (retry_count >= kMaxPageRetries) {
    page_retries_.remove(offset);
    return false;
  }
  ++retry_count;
  AddRequest(offset);
  return true;

}

bool JellyfinRequest::ParseItem(Song &song, const QJsonObject &json_object) {

  if (!json_object.contains(u"Id"_s) || !json_object.contains(u"Name"_s) || !json_object.contains(u"Type"_s)) {
    Error(u"Invalid Json reply, item is missing Id, Name or Type."_s, json_object);
    return false;
  }

  const QString item_id = json_object.value(u"Id"_s).toString();
  const QString name = json_object.value(u"Name"_s).toString();
  const QString type = json_object.value(u"Type"_s).toString();

  if (item_id.isEmpty() || name.isEmpty()) {
    Error(u"Invalid Json reply, item has empty Id or Name."_s, json_object);
    return false;
  }

  song.set_source(Song::Source::Jellyfin);
  song.set_song_id(item_id);
  song.set_url(QUrl(u"jellyfin://"_s + item_id));

  if (type == u"Audio"_s) {
    return ParseAudio(song, json_object);
  }
  if (type == u"MusicAlbum"_s) {
    return ParseAlbum(song, json_object);
  }
  if (type == u"MusicArtist"_s) {
    return ParseArtist(song, json_object);
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

  const bool compilation = artists.count() > 1 && !album_artist.isEmpty();
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

  int bitrate = 0;
  if (json_object.contains(u"MediaStreams"_s) && json_object.value(u"MediaStreams"_s).isArray()) {
    const QJsonArray array_streams = json_object.value(u"MediaStreams"_s).toArray();
    if (!array_streams.isEmpty() && array_streams.at(0).isObject()) {
      const QJsonObject object_stream = array_streams.at(0).toObject();
      if (object_stream.contains(u"BitRate"_s)) {
        bitrate = static_cast<int>(object_stream.value(u"BitRate"_s).toDouble());
      }
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
  if (cover_item_id.isEmpty()) {
    cover_item_id = album_id;
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
  if (!cover_item_id.isEmpty()) song.set_art_automatic(QUrl(CreateImageUrl(cover_item_id, cover_image_tag)));
  song.set_valid(true);

  return true;

}

bool JellyfinRequest::ParseAlbum(Song &song, const QJsonObject &json_object) {

  const QString name = json_object.value(u"Name"_s).toString();
  const QString item_id = json_object.value(u"Id"_s).toString();
  const QString album_artist = json_object.value(u"AlbumArtist"_s).isNull() ? QString() : json_object.value(u"AlbumArtist"_s).toString();

  QStringList artists;
  if (json_object.contains(u"Artists"_s) && json_object.value(u"Artists"_s).isArray()) {
    const QJsonArray array_artists = json_object.value(u"Artists"_s).toArray();
    for (const QJsonValue &value_artist : array_artists) {
      if (value_artist.isString() && !value_artist.toString().isEmpty()) {
        artists << value_artist.toString();
      }
    }
  }

  const bool compilation = artists.count() > 1 || album_artist.isEmpty();
  const QString artist = artists.isEmpty() ? album_artist : artists[0];
  const int year = json_object.value(u"ProductionYear"_s).toInt();

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

  QString cover_image_tag;
  if (json_object.contains(u"ImageTags"_s) && json_object.value(u"ImageTags"_s).isObject()) {
    const QJsonObject object_image_tags = json_object.value(u"ImageTags"_s).toObject();
    if (object_image_tags.contains(u"Primary"_s)) {
      cover_image_tag = object_image_tags.value(u"Primary"_s).toString();
    }
  }

  song.set_title(name);
  song.set_artist(artist);
  song.set_albumartist(compilation ? u"Various Artists"_s : album_artist);
  song.set_album(name);
  song.set_album_id(item_id);
  song.set_year(year);
  song.set_genre(genre);
  song.set_directory_id(0);
  if (!cover_image_tag.isEmpty()) song.set_art_automatic(QUrl(CreateImageUrl(item_id, cover_image_tag)));
  song.set_valid(true);

  return true;

}

bool JellyfinRequest::ParseArtist(Song &song, const QJsonObject &json_object) {

  const QString name = json_object.value(u"Name"_s).toString();

  song.set_title(name);
  song.set_artist_id(json_object.value(u"Id"_s).toString());
  song.set_directory_id(0);
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
  if (!access_token().isEmpty()) url_query.addQueryItem(u"api_key"_s, access_token());
  url.setQuery(url_query);

  return url.toString();

}

void JellyfinRequest::GetAlbumCovers() {

  const SongList songs = songs_.values();
  for (const Song &song : songs) {
    if (!song.art_automatic().isEmpty()) AddAlbumCoverRequest(song);
  }
  FlushAlbumCoverRequests();

  if (album_covers_requested_ == 1) {
    Q_EMIT UpdateStatus(query_id_, tr("Retrieving album cover for %1 album...").arg(album_covers_requested_));
  }
  else if (album_covers_requested_ > 0) {
    Q_EMIT UpdateStatus(query_id_, tr("Retrieving album covers for %1 albums...").arg(album_covers_requested_));
  }
  Q_EMIT UpdateProgress(query_id_, GetProgress(0, album_covers_requested_));

}

void JellyfinRequest::AddAlbumCoverRequest(const Song &song) {

  const QUrl cover_url = song.art_automatic();
  if (!cover_url.isValid() || cover_url.scheme() == QLatin1String("file")) return;

  const QString image_id = song.album_id().isEmpty() ? song.song_id() : song.album_id();
  if (image_id.isEmpty()) return;

  const QString cover_path = Song::ImageCacheDir(Song::Source::Jellyfin);
  QDir dir(cover_path);
  if (!dir.exists()) dir.mkpath(cover_path);

  AlbumCoverRequest request;
  request.album_id = image_id;
  request.url = cover_url;
  request.filename = cover_path + QLatin1Char('/') + CoverUtils::CoverFilenameFromSource(Song::Source::Jellyfin, cover_url, song.effective_albumartist(), song.album(), image_id, u"jpg"_s);
  if (request.filename.isEmpty()) return;

  // The cover is already in the cache, so reuse it instead of re-downloading it on every catalog load.
  if (QFile::exists(request.filename)) {
    if (songs_.contains(song.song_id())) {
      songs_[song.song_id()].set_art_automatic(QUrl::fromLocalFile(request.filename));
    }
    return;
  }

  // Record the song ID under the shared image ID so that every song belonging
  // to the same album gets the downloaded cover applied, while only a single
  // network request is queued for each image.
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

  if (!album_covers_requests_sent_.contains(request.album_id)) return;

  if (reply->error() != QNetworkReply::NoError) {
    Error(QStringLiteral("%1 (%2) for %3").arg(reply->errorString()).arg(reply->error()).arg(UrlForLog(request.url)));
    if (album_covers_requests_sent_.contains(request.album_id)) album_covers_requests_sent_.remove(request.album_id);
    return;
  }

  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
    Error(QStringLiteral("Received HTTP code %1 for %2.").arg(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()).arg(UrlForLog(request.url)));
    if (album_covers_requests_sent_.contains(request.album_id)) album_covers_requests_sent_.remove(request.album_id);
    return;
  }

  QString mimetype = reply->header(QNetworkRequest::ContentTypeHeader).toString();
  if (mimetype.contains(u';')) {
    mimetype = mimetype.left(mimetype.indexOf(u';'));
  }
  if (!ImageUtils::SupportedImageMimeTypes().contains(mimetype, Qt::CaseInsensitive) && !ImageUtils::SupportedImageFormats().contains(mimetype, Qt::CaseInsensitive)) {
    Error(QStringLiteral("Unsupported mimetype for image reader %1 for %2").arg(mimetype, UrlForLog(request.url)));
    if (album_covers_requests_sent_.contains(request.album_id)) album_covers_requests_sent_.remove(request.album_id);
    return;
  }

  const QByteArray data = reply->readAll();
  if (data.isEmpty()) {
    Error(QStringLiteral("Received empty image data for %1").arg(UrlForLog(request.url)));
    if (album_covers_requests_sent_.contains(request.album_id)) album_covers_requests_sent_.remove(request.album_id);
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
      if (album_covers_requests_sent_.contains(request.album_id)) {
        const QStringList song_ids = album_covers_requests_sent_.take(request.album_id);
        for (const QString &song_id : song_ids) {
          if (songs_.contains(song_id)) {
            songs_[song_id].set_art_automatic(QUrl::fromLocalFile(request.filename));
          }
        }
      }
    }
    else {
      Error(QStringLiteral("Error saving image data to %1.").arg(request.filename));
      if (album_covers_requests_sent_.contains(request.album_id)) album_covers_requests_sent_.remove(request.album_id);
    }
  }
  else {
    Error(QStringLiteral("Error decoding image data from %1.").arg(UrlForLog(request.url)));
    if (album_covers_requests_sent_.contains(request.album_id)) album_covers_requests_sent_.remove(request.album_id);
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

  if (download_album_covers() &&
      paging_complete_ &&
      requests_queue_.isEmpty() &&
      requests_active_ <= 0 &&
      album_covers_requested_ == 0) {
    GetAlbumCovers();
  }

  if (requests_queue_.isEmpty() &&
      requests_active_ <= 0 &&
      album_cover_requests_queue_.isEmpty() &&
      album_covers_requests_active_ <= 0 &&
      album_covers_received_ >= album_covers_requested_) {

    if (!paging_complete_) {
      Error(tr("Catalog may be incomplete: stopped receiving results before the end of the list was reached."));
    }
    else if (items_total_ > 0 && items_received_ > items_total_) {
      qLog(Debug) << "Jellyfin:" << "Received" << items_received_ << "items but the server reported" << items_total_ << "total.";
    }

    finished_ = true;
    if (songs_.isEmpty() && errors_.isEmpty()) {
      Q_EMIT Results(query_id_, SongMap(), QString());
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

  qLog(Error) << "Jellyfin:" << error;
  if (debug.isValid()) qLog(Debug) << debug;

}

void JellyfinRequest::Error(const QString &error, const QVariant &debug) {

  if (!error.isEmpty()) {
    qLog(Error) << "Jellyfin:" << error;
    errors_ << error;
  }
  if (debug.isValid()) qLog(Debug) << debug;

}