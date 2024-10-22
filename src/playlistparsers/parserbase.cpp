/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019-2021, Jonas Kvinge <jonas@jkvinge.net>
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

#include <QtGlobal>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QRegularExpression>
#include <QUrl>

#include "includes/shared_ptr.h"
#include "core/logging.h"
#include "tagreader/tagreaderclient.h"
#include "collection/collectionbackend.h"
#include "constants/playlistsettings.h"
#include "parserbase.h"

using namespace Qt::Literals::StringLiterals;

ParserBase::ParserBase(const SharedPtr<TagReaderClient> tagreader_client, const SharedPtr<CollectionBackendInterface> collection_backend, QObject *parent)
    : QObject(parent), tagreader_client_(tagreader_client), collection_backend_(collection_backend) {}

void ParserBase::LoadSong(const QString &filename_or_url, const qint64 beginning, const int track, const QDir &dir, Song *song, const bool collection_lookup) const {

  if (filename_or_url.isEmpty()) {
    return;
  }

  QString filename = filename_or_url;

  static const QRegularExpression regex_url_schema(QStringLiteral("^[a-z]{2,}:"), QRegularExpression::CaseInsensitiveOption);
  if (filename_or_url.contains(regex_url_schema)) {
    QUrl url(filename_or_url);
    song->set_source(Song::SourceFromURL(url));
    if (song->source() == Song::Source::LocalFile) {
      filename = url.toLocalFile();
    }
    else if (song->is_stream()) {
      song->set_url(QUrl::fromUserInput(filename_or_url));
      song->set_filetype(Song::FileType::Stream);
      song->set_valid(true);
      return;
    }
    else {
      qLog(Error) << "Don't know how to handle" << url;
      Q_EMIT Error(tr("Don't know how to handle %1").arg(filename_or_url));
      return;
    }
  }

  filename = QDir::cleanPath(filename);

  // Make the path absolute
  if (!QDir::isAbsolutePath(filename)) {
    filename = dir.absoluteFilePath(filename);
  }

  const QUrl url = QUrl::fromLocalFile(filename);

  // Search the collection
  if (collection_backend_ && collection_lookup) {
    Song collection_song;
    if (track > 0) {
      collection_song = collection_backend_->GetSongByUrlAndTrack(url, track);
    }
    if (!collection_song.is_valid()) {
      collection_song = collection_backend_->GetSongByUrl(url, beginning);
    }
    // Try canonical path
    if (!collection_song.is_valid()) {
      const QString canonical_filepath = QFileInfo(filename).canonicalFilePath();
      if (canonical_filepath != filename) {
        const QUrl canonical_filepath_url = QUrl::fromLocalFile(canonical_filepath);
        if (track > 0) {
          collection_song = collection_backend_->GetSongByUrlAndTrack(canonical_filepath_url, track);
        }
        if (!collection_song.is_valid()) {
          collection_song = collection_backend_->GetSongByUrl(canonical_filepath_url, beginning);
        }
      }
    }
    // If it was found in the collection then use it, otherwise load metadata from disk.
    if (collection_song.is_valid()) {
      *song = collection_song;
      return;
    }
  }

  if (tagreader_client_) {
    const TagReaderResult result = tagreader_client_->ReadFileBlocking(filename, song);
    if (!result.success()) {
      qLog(Error) << "Could not read file" << filename << result.error_string();
    }
  }

}

Song ParserBase::LoadSong(const QString &filename_or_url, const qint64 beginning, const int track, const QDir &dir, const bool collection_lookup) const {

  Song song(Song::Source::LocalFile);
  LoadSong(filename_or_url, beginning, track, dir, &song, collection_lookup);

  return song;

}

QString ParserBase::URLOrFilename(const QUrl &url, const QDir &dir, const PlaylistSettings::PathType path_type) {

  if (!url.isLocalFile()) return url.toString();

  const QString filename = url.toLocalFile();

  if (path_type != PlaylistSettings::PathType::Absolute && QDir::isAbsolutePath(filename)) {
    const QString relative = dir.relativeFilePath(filename);

    if (!relative.startsWith("../"_L1) || path_type == PlaylistSettings::PathType::Relative) {
      return relative;
    }
  }

  return filename;

}
