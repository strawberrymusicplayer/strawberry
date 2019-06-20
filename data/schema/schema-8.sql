ALTER TABLE songs RENAME TO songs_old;

ALTER TABLE playlist_items RENAME TO playlist_items_old;

ALTER TABLE tidal_artists_songs RENAME TO tidal_artists_songs_old;

ALTER TABLE tidal_albums_songs RENAME TO tidal_albums_songs_old;

ALTER TABLE tidal_songs RENAME TO tidal_songs_old;

ALTER TABLE qobuz_artists_songs RENAME TO qobuz_artists_songs_old;

ALTER TABLE qobuz_albums_songs RENAME TO qobuz_albums_songs_old;

ALTER TABLE qobuz_songs RENAME TO qobuz_songs_old;

ALTER TABLE subsonic_songs RENAME TO subsonic_songs_old;

DROP INDEX idx_filename;

CREATE TABLE songs (

  title TEXT NOT NULL,
  album TEXT NOT NULL,
  artist TEXT NOT NULL,
  albumartist TEXT NOT NULL,
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT 0,
  genre TEXT NOT NULL,
  compilation INTEGER NOT NULL DEFAULT -1,
  composer TEXT NOT NULL,
  performer TEXT NOT NULL,
  grouping TEXT NOT NULL,
  comment TEXT NOT NULL,
  lyrics TEXT NOT NULL,

  artist_id INTEGER NOT NULL DEFAULT -1,
  album_id TEXT NOT NULL,
  song_id INTEGER NOT NULL DEFAULT -1,

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT 0,
  samplerate INTEGER NOT NULL DEFAULT 0,
  bitdepth INTEGER NOT NULL DEFAULT 0,

  source INTEGER NOT NULL DEFAULT 0,
  directory_id INTEGER NOT NULL,
  url TEXT NOT NULL,
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER NOT NULL DEFAULT 0,
  mtime INTEGER NOT NULL DEFAULT 0,
  ctime INTEGER NOT NULL DEFAULT 0,
  unavailable INTEGER DEFAULT 0,

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT 0,

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

CREATE TABLE tidal_artists_songs (

  title TEXT NOT NULL,
  album TEXT NOT NULL,
  artist TEXT NOT NULL,
  albumartist TEXT NOT NULL,
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT 0,
  genre TEXT NOT NULL,
  compilation INTEGER NOT NULL DEFAULT -1,
  composer TEXT NOT NULL,
  performer TEXT NOT NULL,
  grouping TEXT NOT NULL,
  comment TEXT NOT NULL,
  lyrics TEXT NOT NULL,

  artist_id INTEGER NOT NULL DEFAULT -1,
  album_id TEXT NOT NULL,
  song_id INTEGER NOT NULL DEFAULT -1,

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT 0,
  samplerate INTEGER NOT NULL DEFAULT 0,
  bitdepth INTEGER NOT NULL DEFAULT 0,

  source INTEGER NOT NULL DEFAULT 0,
  directory_id INTEGER NOT NULL,
  url TEXT NOT NULL,
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER NOT NULL DEFAULT 0,
  mtime INTEGER NOT NULL DEFAULT 0,
  ctime INTEGER NOT NULL DEFAULT 0,
  unavailable INTEGER DEFAULT 0,

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT 0,

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

CREATE TABLE tidal_albums_songs (

  title TEXT NOT NULL,
  album TEXT NOT NULL,
  artist TEXT NOT NULL,
  albumartist TEXT NOT NULL,
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT 0,
  genre TEXT NOT NULL,
  compilation INTEGER NOT NULL DEFAULT -1,
  composer TEXT NOT NULL,
  performer TEXT NOT NULL,
  grouping TEXT NOT NULL,
  comment TEXT NOT NULL,
  lyrics TEXT NOT NULL,

  artist_id INTEGER NOT NULL DEFAULT -1,
  album_id TEXT NOT NULL,
  song_id INTEGER NOT NULL DEFAULT -1,

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT 0,
  samplerate INTEGER NOT NULL DEFAULT 0,
  bitdepth INTEGER NOT NULL DEFAULT 0,

  source INTEGER NOT NULL DEFAULT 0,
  directory_id INTEGER NOT NULL,
  url TEXT NOT NULL,
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER NOT NULL DEFAULT 0,
  mtime INTEGER NOT NULL DEFAULT 0,
  ctime INTEGER NOT NULL DEFAULT 0,
  unavailable INTEGER DEFAULT 0,

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT 0,

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

CREATE TABLE tidal_songs (

  title TEXT NOT NULL,
  album TEXT NOT NULL,
  artist TEXT NOT NULL,
  albumartist TEXT NOT NULL,
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT 0,
  genre TEXT NOT NULL,
  compilation INTEGER NOT NULL DEFAULT -1,
  composer TEXT NOT NULL,
  performer TEXT NOT NULL,
  grouping TEXT NOT NULL,
  comment TEXT NOT NULL,
  lyrics TEXT NOT NULL,

  artist_id INTEGER NOT NULL DEFAULT -1,
  album_id TEXT NOT NULL,
  song_id INTEGER NOT NULL DEFAULT -1,

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT 0,
  samplerate INTEGER NOT NULL DEFAULT 0,
  bitdepth INTEGER NOT NULL DEFAULT 0,

  source INTEGER NOT NULL DEFAULT 0,
  directory_id INTEGER NOT NULL,
  url TEXT NOT NULL,
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER NOT NULL DEFAULT 0,
  mtime INTEGER NOT NULL DEFAULT 0,
  ctime INTEGER NOT NULL DEFAULT 0,
  unavailable INTEGER DEFAULT 0,

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT 0,

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

CREATE TABLE subsonic_songs (

  title TEXT NOT NULL,
  album TEXT NOT NULL,
  artist TEXT NOT NULL,
  albumartist TEXT NOT NULL,
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT 0,
  genre TEXT NOT NULL,
  compilation INTEGER NOT NULL DEFAULT -1,
  composer TEXT NOT NULL,
  performer TEXT NOT NULL,
  grouping TEXT NOT NULL,
  comment TEXT NOT NULL,
  lyrics TEXT NOT NULL,

  artist_id INTEGER NOT NULL DEFAULT -1,
  album_id TEXT NOT NULL,
  song_id INTEGER NOT NULL DEFAULT -1,

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT 0,
  samplerate INTEGER NOT NULL DEFAULT 0,
  bitdepth INTEGER NOT NULL DEFAULT 0,

  source INTEGER NOT NULL DEFAULT 0,
  directory_id INTEGER NOT NULL,
  url TEXT NOT NULL,
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER NOT NULL DEFAULT 0,
  mtime INTEGER NOT NULL DEFAULT 0,
  ctime INTEGER NOT NULL DEFAULT 0,
  unavailable INTEGER DEFAULT 0,

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT 0,

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

CREATE TABLE qobuz_artists_songs (

  title TEXT NOT NULL,
  album TEXT NOT NULL,
  artist TEXT NOT NULL,
  albumartist TEXT NOT NULL,
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT 0,
  genre TEXT NOT NULL,
  compilation INTEGER NOT NULL DEFAULT -1,
  composer TEXT NOT NULL,
  performer TEXT NOT NULL,
  grouping TEXT NOT NULL,
  comment TEXT NOT NULL,
  lyrics TEXT NOT NULL,

  artist_id INTEGER NOT NULL DEFAULT -1,
  album_id TEXT NOT NULL,
  song_id INTEGER NOT NULL DEFAULT -1,

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT 0,
  samplerate INTEGER NOT NULL DEFAULT 0,
  bitdepth INTEGER NOT NULL DEFAULT 0,

  source INTEGER NOT NULL DEFAULT 0,
  directory_id INTEGER NOT NULL,
  url TEXT NOT NULL,
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER NOT NULL DEFAULT 0,
  mtime INTEGER NOT NULL DEFAULT 0,
  ctime INTEGER NOT NULL DEFAULT 0,
  unavailable INTEGER DEFAULT 0,

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT 0,

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

CREATE TABLE qobuz_albums_songs (

  title TEXT NOT NULL,
  album TEXT NOT NULL,
  artist TEXT NOT NULL,
  albumartist TEXT NOT NULL,
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT 0,
  genre TEXT NOT NULL,
  compilation INTEGER NOT NULL DEFAULT -1,
  composer TEXT NOT NULL,
  performer TEXT NOT NULL,
  grouping TEXT NOT NULL,
  comment TEXT NOT NULL,
  lyrics TEXT NOT NULL,

  artist_id INTEGER NOT NULL DEFAULT -1,
  album_id TEXT NOT NULL,
  song_id INTEGER NOT NULL DEFAULT -1,

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT 0,
  samplerate INTEGER NOT NULL DEFAULT 0,
  bitdepth INTEGER NOT NULL DEFAULT 0,

  source INTEGER NOT NULL DEFAULT 0,
  directory_id INTEGER NOT NULL,
  url TEXT NOT NULL,
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER NOT NULL DEFAULT 0,
  mtime INTEGER NOT NULL DEFAULT 0,
  ctime INTEGER NOT NULL DEFAULT 0,
  unavailable INTEGER DEFAULT 0,

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT 0,

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

CREATE TABLE qobuz_songs (

  title TEXT NOT NULL,
  album TEXT NOT NULL,
  artist TEXT NOT NULL,
  albumartist TEXT NOT NULL,
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT 0,
  genre TEXT NOT NULL,
  compilation INTEGER NOT NULL DEFAULT -1,
  composer TEXT NOT NULL,
  performer TEXT NOT NULL,
  grouping TEXT NOT NULL,
  comment TEXT NOT NULL,
  lyrics TEXT NOT NULL,

  artist_id INTEGER NOT NULL DEFAULT -1,
  album_id TEXT NOT NULL,
  song_id INTEGER NOT NULL DEFAULT -1,

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT 0,
  samplerate INTEGER NOT NULL DEFAULT 0,
  bitdepth INTEGER NOT NULL DEFAULT 0,

  source INTEGER NOT NULL DEFAULT 0,
  directory_id INTEGER NOT NULL,
  url TEXT NOT NULL,
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER NOT NULL DEFAULT 0,
  mtime INTEGER NOT NULL DEFAULT 0,
  ctime INTEGER NOT NULL DEFAULT 0,
  unavailable INTEGER DEFAULT 0,

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT 0,

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

CREATE TABLE playlist_items (

  playlist INTEGER NOT NULL,
  type INTEGER NOT NULL DEFAULT 0,
  collection_id INTEGER,
  playlist_url TEXT,

  title TEXT NOT NULL,
  album TEXT NOT NULL,
  artist TEXT NOT NULL,
  albumartist TEXT NOT NULL,
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT 0,
  genre TEXT NOT NULL,
  compilation INTEGER NOT NULL DEFAULT -1,
  composer TEXT NOT NULL,
  performer TEXT NOT NULL,
  grouping TEXT NOT NULL,
  comment TEXT NOT NULL,
  lyrics TEXT NOT NULL,

  artist_id INTEGER NOT NULL DEFAULT -1,
  album_id TEXT NOT NULL,
  song_id INTEGER NOT NULL DEFAULT -1,

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT 0,
  samplerate INTEGER NOT NULL DEFAULT 0,
  bitdepth INTEGER NOT NULL DEFAULT 0,

  source INTEGER NOT NULL DEFAULT 0,
  directory_id INTEGER,
  url TEXT,
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER,
  mtime INTEGER,
  ctime INTEGER,
  unavailable INTEGER DEFAULT 0,

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT 0,

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

INSERT INTO songs (title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, url, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path)
SELECT title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, filename, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path
FROM songs_old;

DROP TABLE songs_old;

INSERT INTO tidal_artists_songs (title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, url, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path)
SELECT title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, filename, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path
FROM tidal_artists_songs_old;

DROP TABLE tidal_artists_songs_old;

INSERT INTO tidal_albums_songs (title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, url, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path)
SELECT title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, filename, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path
FROM tidal_albums_songs_old;

DROP TABLE tidal_albums_songs_old;

INSERT INTO tidal_songs (title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, url, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path)
SELECT title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, filename, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path
FROM tidal_songs_old;

DROP TABLE tidal_songs_old;

INSERT INTO qobuz_artists_songs (title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, url, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path)
SELECT title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, filename, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path
FROM qobuz_artists_songs_old;

DROP TABLE qobuz_artists_songs_old;

INSERT INTO qobuz_albums_songs (title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, url, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path)
SELECT title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, filename, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path
FROM qobuz_albums_songs_old;

DROP TABLE qobuz_albums_songs_old;

INSERT INTO qobuz_songs (title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, url, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path)
SELECT title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, filename, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path
FROM qobuz_songs_old;

DROP TABLE qobuz_songs_old;

INSERT INTO subsonic_songs (title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, url, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path)
SELECT title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, filename, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path
FROM subsonic_songs_old;

DROP TABLE subsonic_songs_old;

INSERT INTO playlist_items (playlist, type, collection_id, playlist_url, title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, url, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path)
SELECT playlist, type, collection_id, url, title, album, artist, albumartist, track, disc, year, originalyear, genre, compilation, composer, performer, grouping, comment, lyrics, artist_id, album_id, song_id, beginning, length, bitrate, samplerate, bitdepth, source, directory_id, filename, filetype, filesize, mtime, ctime, unavailable, playcount, skipcount, lastplayed, compilation_detected, compilation_on, compilation_off, compilation_effective, art_automatic, art_manual, effective_albumartist, effective_originalyear, cue_path
FROM playlist_items_old;

DROP TABLE playlist_items_old;

CREATE INDEX idx_url ON songs (url);

UPDATE schema_version SET version=8;
