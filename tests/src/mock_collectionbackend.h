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

#ifndef MOCKCOLLECTIONBACKEND_H
#define MOCKCOLLECTIONBACKEND_H

#include "gmock_include.h"

#include "collection/collectionbackend.h"

class MockCollectionBackend : public CollectionBackendInterface {
 public:
  MOCK_CONST_METHOD0(songs_table, QString());

  // Get a list of directories in the collection.  Emits DirectoriesDiscovered.
  MOCK_METHOD0(LoadDirectoriesAsync, void());

  // Counts the songs in the collection.
  MOCK_METHOD0(UpdateTotalSongCountAsync, void());
  MOCK_METHOD0(UpdateTotalAlbumCountAsync, void());
  MOCK_METHOD0(UpdateTotalArtistCountAsync, void());

  MOCK_METHOD1(FindSongsInDirectory, SongList(int));
  MOCK_METHOD1(SubdirsInDirectory, SubdirectoryList(int));
  MOCK_METHOD0(GetAllDirectories, DirectoryList());
  MOCK_METHOD3(ChangeDirPath, void(int, const QString&, const QString&));

  MOCK_METHOD1(GetAllArtists, QStringList(const QueryOptions&));
  MOCK_METHOD1(GetAllArtistsWithAlbums, QStringList(const QueryOptions&));
  MOCK_METHOD3(GetSongs, SongList(const QString&, const QString&, const QueryOptions&));

  MOCK_METHOD2(GetCompilationSongs, SongList(const QString&, const QueryOptions&));

  MOCK_METHOD1(GetAllAlbums, AlbumList(const QueryOptions&));
  MOCK_METHOD2(GetAlbumsByArtist, AlbumList(const QString&, const QueryOptions&));
  MOCK_METHOD1(GetCompilationAlbums, AlbumList(const QueryOptions&));

  MOCK_METHOD4(UpdateManualAlbumArtAsync, void(const QString&, const QString&, const QString&, const QString&));
  MOCK_METHOD3(GetAlbumArt, Album(const QString&, const QString&, const QString&));

  MOCK_METHOD1(GetSongById, Song(int));

  MOCK_METHOD1(GetSongsByUrl, SongList(const QUrl&));
  MOCK_METHOD2(GetSongByUrl, Song(const QUrl&, qint64));

  MOCK_METHOD1(AddDirectory, void(const QString&));
  MOCK_METHOD1(RemoveDirectory, void(const Directory&));

  MOCK_METHOD1(ExecQuery, bool(CollectionQuery*));

  MOCK_METHOD2(GetSongsByAlbum, SongList(const QString&, const QueryOptions&));

};

#endif  // MOCKCOLLECTIONBACKEND_H
