/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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
#include <QRegExp>
#include <QUrl>

#include "collection/collectionbackend.h"
#include "core/tagreaderclient.h"
#include "parserbase.h"
#include "playlist/playlist.h"

ParserBase::ParserBase(CollectionBackendInterface *collection, QObject *parent)
    : QObject(parent), collection_(collection) {}

void ParserBase::LoadSong(const QString &filename_or_url, qint64 beginning, const QDir &dir, Song *song) const {

  if (filename_or_url.isEmpty()) {
    return;
  }

  QString filename = filename_or_url;

  if (filename_or_url.contains(QRegExp("^[a-z]{2,}:"))) {
    QUrl url(filename_or_url);
    song->set_source(Song::SourceFromURL(url));
    if (song->source() == Song::Source_LocalFile) {
      filename = url.toLocalFile();
    }
    else if (song->source() == Song::Source_Stream || song->source() == Song::Source_Tidal) {
      song->set_url(QUrl::fromUserInput(filename_or_url));
      song->set_filetype(Song::FileType_Stream);
      song->set_valid(true);
      return;
    }
    else {
      qLog(Error) << "Don't know how to handle" << url;
    }
  }

  // Strawberry always wants / separators internally.
  // Using QDir::fromNativeSeparators() only works on the same platform the playlist was created on/for, using replace() lets playlists work on any platform.
  filename = filename.replace('\\', '/');

  // Make the path absolute
  if (!QDir::isAbsolutePath(filename)) {
    filename = dir.absoluteFilePath(filename);
  }

  // Use the canonical path
  if (QFile::exists(filename)) {
    filename = QFileInfo(filename).canonicalFilePath();
  }

  const QUrl url = QUrl::fromLocalFile(filename);

  // Search in the collection
  Song collection_song(Song::Source_Collection);
  if (collection_) {
    collection_song = collection_->GetSongByUrl(url, beginning);
  }

  // If it was found in the collection then use it, otherwise load metadata from disk.
  if (collection_song.is_valid()) {
    *song = collection_song;
  }
  else {
    TagReaderClient::Instance()->ReadFileBlocking(filename, song);
  }

}

Song ParserBase::LoadSong(const QString &filename_or_url, qint64 beginning, const QDir &dir) const {

  Song song(Song::Source_LocalFile);
  LoadSong(filename_or_url, beginning, dir, &song);
  return song;

}

QString ParserBase::URLOrFilename(const QUrl &url, const QDir &dir, Playlist::Path path_type) const {

  if (!url.isLocalFile()) return url.toString();

  const QString filename = url.toLocalFile();

  if (path_type != Playlist::Path_Absolute && QDir::isAbsolutePath(filename)) {
    const QString relative = dir.relativeFilePath(filename);

    if (!relative.startsWith("../") || path_type == Playlist::Path_Relative)
      return relative;
  }
  return filename;

}
