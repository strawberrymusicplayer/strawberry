DROP VIEW IF EXISTS duplicated_songs;

DROP INDEX IF EXISTS idx_url;

DROP INDEX IF EXISTS idx_comp_artist;

DROP INDEX IF EXISTS idx_albumartist;

DROP INDEX IF EXISTS idx_artist;

DROP INDEX IF EXISTS idx_album;

DROP INDEX IF EXISTS idx_title;

ALTER TABLE songs RENAME TO songs_old;

ALTER TABLE subsonic_songs RENAME TO subsonic_songs_old;

ALTER TABLE playlist_items RENAME TO playlist_items_old;

CREATE TABLE songs (

  title TEXT,
  album TEXT,
  artist TEXT,
  albumartist TEXT,
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT 0,
  genre TEXT,
  compilation INTEGER NOT NULL DEFAULT 0,
  composer TEXT,
  performer TEXT,
  grouping TEXT,
  comment TEXT,
  lyrics TEXT,

  artist_id TEXT,
  album_id TEXT,
  song_id TEXT,

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT -1,
  samplerate INTEGER NOT NULL DEFAULT -1,
  bitdepth INTEGER NOT NULL DEFAULT -1,

  source INTEGER NOT NULL DEFAULT 0,
  directory_id INTEGER NOT NULL DEFAULT -1,
  url TEXT NOT NULL,
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER NOT NULL DEFAULT -1,
  mtime INTEGER NOT NULL DEFAULT -1,
  ctime INTEGER NOT NULL DEFAULT -1,
  unavailable INTEGER DEFAULT 0,

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT -1,

  compilation_detected INTEGER DEFAULT 0,
  compilation_on INTEGER NOT NULL DEFAULT 0,
  compilation_off INTEGER NOT NULL DEFAULT 0,
  compilation_effective INTEGER NOT NULL DEFAULT 0,

  art_automatic TEXT,
  art_manual TEXT,

  effective_albumartist TEXT,
  effective_originalyear INTEGER NOT NULL DEFAULT 0,

  cue_path TEXT

);

CREATE INDEX IF NOT EXISTS idx_url ON songs (url);

CREATE INDEX IF NOT EXISTS idx_comp_artist ON songs (compilation_effective, artist);

CREATE INDEX IF NOT EXISTS idx_albumartist ON songs (albumartist);

CREATE INDEX IF NOT EXISTS idx_artist ON songs (artist);

CREATE INDEX IF NOT EXISTS idx_album ON songs (album);

CREATE INDEX IF NOT EXISTS idx_title ON songs (title);

CREATE VIEW duplicated_songs as select artist dup_artist, album dup_album, title dup_title from songs as inner_songs where artist != '' and album != '' and title != '' and unavailable = 0 group by artist, album , title having count(*) > 1;

CREATE TABLE subsonic_songs (

  title TEXT,
  album TEXT,
  artist TEXT,
  albumartist TEXT,
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT 0,
  genre TEXT,
  compilation INTEGER NOT NULL DEFAULT 0,
  composer TEXT,
  performer TEXT,
  grouping TEXT,
  comment TEXT,
  lyrics TEXT,

  artist_id TEXT,
  album_id TEXT,
  song_id TEXT,

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT -1,
  samplerate INTEGER NOT NULL DEFAULT -1,
  bitdepth INTEGER NOT NULL DEFAULT -1,

  source INTEGER NOT NULL DEFAULT 0,
  directory_id INTEGER NOT NULL DEFAULT -1,
  url TEXT NOT NULL,
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER NOT NULL DEFAULT -1,
  mtime INTEGER NOT NULL DEFAULT -1,
  ctime INTEGER NOT NULL DEFAULT -1,
  unavailable INTEGER DEFAULT 0,

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT -1,

  compilation_detected INTEGER DEFAULT 0,
  compilation_on INTEGER NOT NULL DEFAULT 0,
  compilation_off INTEGER NOT NULL DEFAULT 0,
  compilation_effective INTEGER NOT NULL DEFAULT 0,

  art_automatic TEXT,
  art_manual TEXT,

  effective_albumartist TEXT,
  effective_originalyear INTEGER NOT NULL DEFAULT 0,

  cue_path TEXT

);

INSERT INTO songs (ROWID, title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, url, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path)
SELECT ROWID, title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, url, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path
FROM songs_old;

DROP TABLE songs_old;

DELETE FROM songs_fts;

INSERT INTO songs_fts (ROWID, ftstitle, ftsalbum, ftsartist, ftsalbumartist, ftscomposer, ftsperformer, ftsgrouping, ftsgenre, ftscomment)
SELECT ROWID, title, album, artist, albumartist, composer, performer, grouping, genre, comment
FROM songs;

INSERT INTO subsonic_songs (ROWID, title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, url, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path)
SELECT ROWID, title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, url, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path
FROM subsonic_songs_old;

DROP TABLE subsonic_songs_old;

DELETE FROM subsonic_songs_fts;

INSERT INTO subsonic_songs_fts (ROWID, ftstitle, ftsalbum, ftsartist, ftsalbumartist, ftscomposer, ftsperformer, ftsgrouping, ftsgenre, ftscomment)
SELECT ROWID, title, album, artist, albumartist, composer, performer, grouping, genre, comment
FROM subsonic_songs;

CREATE TABLE playlist_items (

  playlist INTEGER NOT NULL,
  type INTEGER NOT NULL DEFAULT 0,
  collection_id INTEGER,
  playlist_url TEXT,

  title TEXT,
  album TEXT,
  artist TEXT,
  albumartist TEXT,
  track INTEGER,
  disc INTEGER,
  year INTEGER,
  originalyear INTEGER,
  genre TEXT,
  compilation INTEGER DEFAULT 0,
  composer TEXT,
  performer TEXT,
  grouping TEXT,
  comment TEXT,
  lyrics TEXT,

  artist_id TEXT,
  album_id TEXT,
  song_id TEXT,

  beginning INTEGER,
  length INTEGER,

  bitrate INTEGER,
  samplerate INTEGER,
  bitdepth INTEGER,

  source INTEGER,
  directory_id INTEGER,
  url TEXT,
  filetype INTEGER,
  filesize INTEGER,
  mtime INTEGER,
  ctime INTEGER,
  unavailable INTEGER DEFAULT 0,

  playcount INTEGER DEFAULT 0,
  skipcount INTEGER DEFAULT 0,
  lastplayed INTEGER DEFAULT 0,

  compilation_detected INTEGER DEFAULT 0,
  compilation_on INTEGER DEFAULT 0,
  compilation_off INTEGER DEFAULT 0,
  compilation_effective INTEGER DEFAULT 0,

  art_automatic TEXT,
  art_manual TEXT,

  effective_albumartist TEXT,
  effective_originalyear INTEGER,

  cue_path TEXT

);

INSERT INTO playlist_items (ROWID, playlist, type, collection_id, playlist_url, title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, url, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path)
SELECT ROWID, playlist, type, collection_id, playlist_url, title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, url, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path
FROM playlist_items_old;

DROP TABLE playlist_items_old;

DELETE FROM playlist_items_fts_;

INSERT INTO playlist_items_fts_ (ROWID, ftstitle, ftsalbum, ftsartist, ftsalbumartist, ftscomposer, ftsperformer, ftsgrouping, ftsgenre, ftscomment)
SELECT ROWID, title, album, artist, albumartist, composer, performer, grouping, genre, comment
FROM playlist_items;

UPDATE schema_version SET version=11;
