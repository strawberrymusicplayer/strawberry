CREATE TABLE IF NOT EXISTS schema_version (
  version INTEGER NOT NULL
);

DELETE FROM schema_version;

INSERT INTO schema_version (version) VALUES (14);

CREATE TABLE IF NOT EXISTS directories (
  path TEXT NOT NULL DEFAULT '',
  subdirs INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS subdirectories (
  directory_id INTEGER NOT NULL,
  path TEXT NOT NULL DEFAULT '',
  mtime INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS songs (

  title TEXT DEFAULT '',
  album TEXT DEFAULT '',
  artist TEXT DEFAULT '',
  albumartist TEXT DEFAULT '',
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT -1,
  genre TEXT DEFAULT '',
  compilation INTEGER NOT NULL DEFAULT 0,
  composer TEXT DEFAULT '',
  performer TEXT DEFAULT '',
  grouping TEXT DEFAULT '',
  comment TEXT DEFAULT '',
  lyrics TEXT DEFAULT '',

  artist_id TEXT DEFAULT '',
  album_id TEXT DEFAULT '',
  song_id TEXT DEFAULT '',

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT -1,
  samplerate INTEGER NOT NULL DEFAULT -1,
  bitdepth INTEGER NOT NULL DEFAULT -1,

  source INTEGER NOT NULL DEFAULT 0,
  directory_id INTEGER NOT NULL DEFAULT -1,
  url TEXT NOT NULL DEFAULT '',
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER NOT NULL DEFAULT -1,
  mtime INTEGER NOT NULL DEFAULT -1,
  ctime INTEGER NOT NULL DEFAULT -1,
  unavailable INTEGER DEFAULT 0,

  fingerprint TEXT DEFAULT '',

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT -1,
  lastseen INTEGER NOT NULL DEFAULT -1,

  compilation_detected INTEGER DEFAULT 0,
  compilation_on INTEGER NOT NULL DEFAULT 0,
  compilation_off INTEGER NOT NULL DEFAULT 0,
  compilation_effective INTEGER NOT NULL DEFAULT 0,

  art_automatic TEXT DEFAULT '',
  art_manual TEXT DEFAULT '',

  effective_albumartist TEXT DEFAULT '',
  effective_originalyear INTEGER NOT NULL DEFAULT 0,

  cue_path TEXT DEFAULT '',

  rating INTEGER DEFAULT -1

);

CREATE TABLE IF NOT EXISTS subsonic_songs (

  title TEXT DEFAULT '',
  album TEXT DEFAULT '',
  artist TEXT DEFAULT '',
  albumartist TEXT DEFAULT '',
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT -1,
  genre TEXT DEFAULT '',
  compilation INTEGER NOT NULL DEFAULT 0,
  composer TEXT DEFAULT '',
  performer TEXT DEFAULT '',
  grouping TEXT DEFAULT '',
  comment TEXT DEFAULT '',
  lyrics TEXT DEFAULT '',

  artist_id TEXT DEFAULT '',
  album_id TEXT DEFAULT '',
  song_id TEXT DEFAULT '',

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT -1,
  samplerate INTEGER NOT NULL DEFAULT -1,
  bitdepth INTEGER NOT NULL DEFAULT -1,

  source INTEGER NOT NULL DEFAULT 0,
  directory_id INTEGER NOT NULL DEFAULT -1,
  url TEXT NOT NULL DEFAULT '',
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER NOT NULL DEFAULT -1,
  mtime INTEGER NOT NULL DEFAULT -1,
  ctime INTEGER NOT NULL DEFAULT -1,
  unavailable INTEGER DEFAULT 0,

  fingerprint TEXT DEFAULT '',

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT -1,
  lastseen INTEGER NOT NULL DEFAULT -1,

  compilation_detected INTEGER DEFAULT 0,
  compilation_on INTEGER NOT NULL DEFAULT 0,
  compilation_off INTEGER NOT NULL DEFAULT 0,
  compilation_effective INTEGER NOT NULL DEFAULT 0,

  art_automatic TEXT DEFAULT '',
  art_manual TEXT DEFAULT '',

  effective_albumartist TEXT DEFAULT '',
  effective_originalyear INTEGER NOT NULL DEFAULT 0,

  cue_path TEXT DEFAULT '',

  rating INTEGER DEFAULT -1

);

CREATE TABLE IF NOT EXISTS tidal_artists_songs (

  title TEXT DEFAULT '',
  album TEXT DEFAULT '',
  artist TEXT DEFAULT '',
  albumartist TEXT DEFAULT '',
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT -1,
  genre TEXT DEFAULT '',
  compilation INTEGER NOT NULL DEFAULT 0,
  composer TEXT DEFAULT '',
  performer TEXT DEFAULT '',
  grouping TEXT DEFAULT '',
  comment TEXT DEFAULT '',
  lyrics TEXT DEFAULT '',

  artist_id TEXT DEFAULT '',
  album_id TEXT DEFAULT '',
  song_id TEXT DEFAULT '',

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT -1,
  samplerate INTEGER NOT NULL DEFAULT -1,
  bitdepth INTEGER NOT NULL DEFAULT -1,

  source INTEGER NOT NULL DEFAULT 0,
  directory_id INTEGER NOT NULL DEFAULT -1,
  url TEXT NOT NULL DEFAULT '',
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER NOT NULL DEFAULT -1,
  mtime INTEGER NOT NULL DEFAULT -1,
  ctime INTEGER NOT NULL DEFAULT -1,
  unavailable INTEGER DEFAULT 0,

  fingerprint TEXT DEFAULT '',

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT -1,
  lastseen INTEGER NOT NULL DEFAULT -1,

  compilation_detected INTEGER DEFAULT 0,
  compilation_on INTEGER NOT NULL DEFAULT 0,
  compilation_off INTEGER NOT NULL DEFAULT 0,
  compilation_effective INTEGER NOT NULL DEFAULT 0,

  art_automatic TEXT DEFAULT '',
  art_manual TEXT DEFAULT '',

  effective_albumartist TEXT DEFAULT '',
  effective_originalyear INTEGER NOT NULL DEFAULT 0,

  cue_path TEXT DEFAULT '',

  rating INTEGER DEFAULT -1

);

CREATE TABLE IF NOT EXISTS tidal_albums_songs (

  title TEXT DEFAULT '',
  album TEXT DEFAULT '',
  artist TEXT DEFAULT '',
  albumartist TEXT DEFAULT '',
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT -1,
  genre TEXT DEFAULT '',
  compilation INTEGER NOT NULL DEFAULT 0,
  composer TEXT DEFAULT '',
  performer TEXT DEFAULT '',
  grouping TEXT DEFAULT '',
  comment TEXT DEFAULT '',
  lyrics TEXT DEFAULT '',

  artist_id TEXT DEFAULT '',
  album_id TEXT DEFAULT '',
  song_id TEXT DEFAULT '',

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT -1,
  samplerate INTEGER NOT NULL DEFAULT -1,
  bitdepth INTEGER NOT NULL DEFAULT -1,

  source INTEGER NOT NULL DEFAULT 0,
  directory_id INTEGER NOT NULL DEFAULT -1,
  url TEXT NOT NULL DEFAULT '',
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER NOT NULL DEFAULT -1,
  mtime INTEGER NOT NULL DEFAULT -1,
  ctime INTEGER NOT NULL DEFAULT -1,
  unavailable INTEGER DEFAULT 0,

  fingerprint TEXT DEFAULT '',

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT -1,
  lastseen INTEGER NOT NULL DEFAULT -1,

  compilation_detected INTEGER DEFAULT 0,
  compilation_on INTEGER NOT NULL DEFAULT 0,
  compilation_off INTEGER NOT NULL DEFAULT 0,
  compilation_effective INTEGER NOT NULL DEFAULT 0,

  art_automatic TEXT DEFAULT '',
  art_manual TEXT DEFAULT '',

  effective_albumartist TEXT DEFAULT '',
  effective_originalyear INTEGER NOT NULL DEFAULT 0,

  cue_path TEXT DEFAULT '',

  rating INTEGER DEFAULT -1

);

CREATE TABLE IF NOT EXISTS tidal_songs (

  title TEXT DEFAULT '',
  album TEXT DEFAULT '',
  artist TEXT DEFAULT '',
  albumartist TEXT DEFAULT '',
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT -1,
  genre TEXT DEFAULT '',
  compilation INTEGER NOT NULL DEFAULT 0,
  composer TEXT DEFAULT '',
  performer TEXT DEFAULT '',
  grouping TEXT DEFAULT '',
  comment TEXT DEFAULT '',
  lyrics TEXT DEFAULT '',

  artist_id TEXT DEFAULT '',
  album_id TEXT DEFAULT '',
  song_id TEXT DEFAULT '',

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT -1,
  samplerate INTEGER NOT NULL DEFAULT -1,
  bitdepth INTEGER NOT NULL DEFAULT -1,

  source INTEGER NOT NULL DEFAULT 0,
  directory_id INTEGER NOT NULL DEFAULT -1,
  url TEXT NOT NULL DEFAULT '',
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER NOT NULL DEFAULT -1,
  mtime INTEGER NOT NULL DEFAULT -1,
  ctime INTEGER NOT NULL DEFAULT -1,
  unavailable INTEGER DEFAULT 0,

  fingerprint TEXT DEFAULT '',

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT -1,
  lastseen INTEGER NOT NULL DEFAULT -1,

  compilation_detected INTEGER DEFAULT 0,
  compilation_on INTEGER NOT NULL DEFAULT 0,
  compilation_off INTEGER NOT NULL DEFAULT 0,
  compilation_effective INTEGER NOT NULL DEFAULT 0,

  art_automatic TEXT DEFAULT '',
  art_manual TEXT DEFAULT '',

  effective_albumartist TEXT DEFAULT '',
  effective_originalyear INTEGER NOT NULL DEFAULT 0,

  cue_path TEXT DEFAULT '',

  rating INTEGER DEFAULT -1

);

CREATE TABLE IF NOT EXISTS qobuz_artists_songs (

  title TEXT DEFAULT '',
  album TEXT DEFAULT '',
  artist TEXT DEFAULT '',
  albumartist TEXT DEFAULT '',
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT -1,
  genre TEXT DEFAULT '',
  compilation INTEGER NOT NULL DEFAULT 0,
  composer TEXT DEFAULT '',
  performer TEXT DEFAULT '',
  grouping TEXT DEFAULT '',
  comment TEXT DEFAULT '',
  lyrics TEXT DEFAULT '',

  artist_id TEXT DEFAULT '',
  album_id TEXT DEFAULT '',
  song_id TEXT DEFAULT '',

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT -1,
  samplerate INTEGER NOT NULL DEFAULT -1,
  bitdepth INTEGER NOT NULL DEFAULT -1,

  source INTEGER NOT NULL DEFAULT 0,
  directory_id INTEGER NOT NULL DEFAULT -1,
  url TEXT NOT NULL DEFAULT '',
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER NOT NULL DEFAULT -1,
  mtime INTEGER NOT NULL DEFAULT -1,
  ctime INTEGER NOT NULL DEFAULT -1,
  unavailable INTEGER DEFAULT 0,

  fingerprint TEXT DEFAULT '',

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT -1,
  lastseen INTEGER NOT NULL DEFAULT -1,

  compilation_detected INTEGER DEFAULT 0,
  compilation_on INTEGER NOT NULL DEFAULT 0,
  compilation_off INTEGER NOT NULL DEFAULT 0,
  compilation_effective INTEGER NOT NULL DEFAULT 0,

  art_automatic TEXT DEFAULT '',
  art_manual TEXT DEFAULT '',

  effective_albumartist TEXT DEFAULT '',
  effective_originalyear INTEGER NOT NULL DEFAULT 0,

  cue_path TEXT DEFAULT '',

  rating INTEGER DEFAULT -1

);

CREATE TABLE IF NOT EXISTS qobuz_albums_songs (

  title TEXT DEFAULT '',
  album TEXT DEFAULT '',
  artist TEXT DEFAULT '',
  albumartist TEXT DEFAULT '',
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT -1,
  genre TEXT DEFAULT '',
  compilation INTEGER NOT NULL DEFAULT 0,
  composer TEXT DEFAULT '',
  performer TEXT DEFAULT '',
  grouping TEXT DEFAULT '',
  comment TEXT DEFAULT '',
  lyrics TEXT DEFAULT '',

  artist_id TEXT DEFAULT '',
  album_id TEXT DEFAULT '',
  song_id TEXT DEFAULT '',

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT -1,
  samplerate INTEGER NOT NULL DEFAULT -1,
  bitdepth INTEGER NOT NULL DEFAULT -1,

  source INTEGER NOT NULL DEFAULT 0,
  directory_id INTEGER NOT NULL DEFAULT -1,
  url TEXT NOT NULL DEFAULT '',
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER NOT NULL DEFAULT -1,
  mtime INTEGER NOT NULL DEFAULT -1,
  ctime INTEGER NOT NULL DEFAULT -1,
  unavailable INTEGER DEFAULT 0,

  fingerprint TEXT DEFAULT '',

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT -1,
  lastseen INTEGER NOT NULL DEFAULT -1,

  compilation_detected INTEGER DEFAULT 0,
  compilation_on INTEGER NOT NULL DEFAULT 0,
  compilation_off INTEGER NOT NULL DEFAULT 0,
  compilation_effective INTEGER NOT NULL DEFAULT 0,

  art_automatic TEXT DEFAULT '',
  art_manual TEXT DEFAULT '',

  effective_albumartist TEXT DEFAULT '',
  effective_originalyear INTEGER NOT NULL DEFAULT 0,

  cue_path TEXT DEFAULT '',

  rating INTEGER DEFAULT -1

);

CREATE TABLE IF NOT EXISTS qobuz_songs (

  title TEXT DEFAULT '',
  album TEXT DEFAULT '',
  artist TEXT DEFAULT '',
  albumartist TEXT DEFAULT '',
  track INTEGER NOT NULL DEFAULT -1,
  disc INTEGER NOT NULL DEFAULT -1,
  year INTEGER NOT NULL DEFAULT -1,
  originalyear INTEGER NOT NULL DEFAULT -1,
  genre TEXT DEFAULT '',
  compilation INTEGER NOT NULL DEFAULT 0,
  composer TEXT DEFAULT '',
  performer TEXT DEFAULT '',
  grouping TEXT DEFAULT '',
  comment TEXT DEFAULT '',
  lyrics TEXT DEFAULT '',

  artist_id TEXT DEFAULT '',
  album_id TEXT DEFAULT '',
  song_id TEXT DEFAULT '',

  beginning INTEGER NOT NULL DEFAULT 0,
  length INTEGER NOT NULL DEFAULT 0,

  bitrate INTEGER NOT NULL DEFAULT -1,
  samplerate INTEGER NOT NULL DEFAULT -1,
  bitdepth INTEGER NOT NULL DEFAULT -1,

  source INTEGER NOT NULL DEFAULT 0,
  directory_id INTEGER NOT NULL DEFAULT -1,
  url TEXT NOT NULL DEFAULT '',
  filetype INTEGER NOT NULL DEFAULT 0,
  filesize INTEGER NOT NULL DEFAULT -1,
  mtime INTEGER NOT NULL DEFAULT -1,
  ctime INTEGER NOT NULL DEFAULT -1,
  unavailable INTEGER DEFAULT 0,

  fingerprint TEXT DEFAULT '',

  playcount INTEGER NOT NULL DEFAULT 0,
  skipcount INTEGER NOT NULL DEFAULT 0,
  lastplayed INTEGER NOT NULL DEFAULT -1,
  lastseen INTEGER NOT NULL DEFAULT -1,

  compilation_detected INTEGER DEFAULT 0,
  compilation_on INTEGER NOT NULL DEFAULT 0,
  compilation_off INTEGER NOT NULL DEFAULT 0,
  compilation_effective INTEGER NOT NULL DEFAULT 0,

  art_automatic TEXT DEFAULT '',
  art_manual TEXT DEFAULT '',

  effective_albumartist TEXT DEFAULT '',
  effective_originalyear INTEGER NOT NULL DEFAULT 0,

  cue_path TEXT DEFAULT '',

  rating INTEGER DEFAULT -1

);

CREATE TABLE IF NOT EXISTS playlists (

  name TEXT NOT NULL DEFAULT '',
  last_played INTEGER NOT NULL DEFAULT -1,
  ui_order INTEGER NOT NULL DEFAULT 0,
  special_type TEXT DEFAULT '',
  ui_path TEXT DEFAULT '',
  is_favorite INTEGER NOT NULL DEFAULT 0,

  dynamic_playlist_type INTEGER,
  dynamic_playlist_backend TEXT DEFAULT '',
  dynamic_playlist_data BLOB

);

CREATE TABLE IF NOT EXISTS playlist_items (

  playlist INTEGER NOT NULL,
  type INTEGER NOT NULL DEFAULT 0,
  collection_id INTEGER,
  playlist_url TEXT DEFAULT '',

  title TEXT DEFAULT '',
  album TEXT DEFAULT '',
  artist TEXT DEFAULT '',
  albumartist TEXT DEFAULT '',
  track INTEGER,
  disc INTEGER,
  year INTEGER,
  originalyear INTEGER,
  genre TEXT DEFAULT '',
  compilation INTEGER DEFAULT 0,
  composer TEXT DEFAULT '',
  performer TEXT DEFAULT '',
  grouping TEXT DEFAULT '',
  comment TEXT DEFAULT '',
  lyrics TEXT DEFAULT '',

  artist_id TEXT DEFAULT '',
  album_id TEXT DEFAULT '',
  song_id TEXT DEFAULT '',

  beginning INTEGER,
  length INTEGER,

  bitrate INTEGER,
  samplerate INTEGER,
  bitdepth INTEGER,

  source INTEGER,
  directory_id INTEGER,
  url TEXT DEFAULT '',
  filetype INTEGER,
  filesize INTEGER,
  mtime INTEGER,
  ctime INTEGER,
  unavailable INTEGER DEFAULT 0,

  fingerprint TEXT DEFAULT '',

  playcount INTEGER DEFAULT 0,
  skipcount INTEGER DEFAULT 0,
  lastplayed INTEGER DEFAULT -1,
  lastseen INTEGER DEFAULT -1,

  compilation_detected INTEGER DEFAULT 0,
  compilation_on INTEGER DEFAULT 0,
  compilation_off INTEGER DEFAULT 0,
  compilation_effective INTEGER DEFAULT 0,

  art_automatic TEXT DEFAULT '',
  art_manual TEXT DEFAULT '',

  effective_albumartist TEXT DEFAULT '',
  effective_originalyear INTEGER,

  cue_path TEXT DEFAULT '',

  rating INTEGER DEFAULT -1

);

CREATE TABLE IF NOT EXISTS devices (
  unique_id TEXT NOT NULL DEFAULT '',
  friendly_name TEXT DEFAULT '',
  size INTEGER,
  icon TEXT DEFAULT '',
  schema_version INTEGER NOT NULL DEFAULT 0,
  transcode_mode NOT NULL DEFAULT 3,
  transcode_format NOT NULL DEFAULT 5
);

CREATE INDEX IF NOT EXISTS idx_url ON songs (url);

CREATE INDEX IF NOT EXISTS idx_comp_artist ON songs (compilation_effective, artist);

CREATE INDEX IF NOT EXISTS idx_albumartist ON songs (albumartist);

CREATE INDEX IF NOT EXISTS idx_artist ON songs (artist);

CREATE INDEX IF NOT EXISTS idx_album ON songs (album);

CREATE INDEX IF NOT EXISTS idx_title ON songs (title);

CREATE VIEW IF NOT EXISTS duplicated_songs as select artist dup_artist, album dup_album, title dup_title from songs as inner_songs where artist != '' and album != '' and title != '' and unavailable = 0 group by artist, album , title having count(*) > 1;

CREATE VIRTUAL TABLE IF NOT EXISTS songs_fts USING fts5(

  ftstitle,
  ftsalbum,
  ftsartist,
  ftsalbumartist,
  ftscomposer,
  ftsperformer,
  ftsgrouping,
  ftsgenre,
  ftscomment,
  tokenize = "unicode61 remove_diacritics 1"

);

CREATE VIRTUAL TABLE IF NOT EXISTS subsonic_songs_fts USING fts5(

  ftstitle,
  ftsalbum,
  ftsartist,
  ftsalbumartist,
  ftscomposer,
  ftsperformer,
  ftsgrouping,
  ftsgenre,
  ftscomment,
  tokenize = "unicode61 remove_diacritics 1"

);

CREATE VIRTUAL TABLE IF NOT EXISTS tidal_artists_songs_fts USING fts5(

  ftstitle,
  ftsalbum,
  ftsartist,
  ftsalbumartist,
  ftscomposer,
  ftsperformer,
  ftsgrouping,
  ftsgenre,
  ftscomment,
  tokenize = "unicode61 remove_diacritics 1"

);

CREATE VIRTUAL TABLE IF NOT EXISTS tidal_albums_songs_fts USING fts5(

  ftstitle,
  ftsalbum,
  ftsartist,
  ftsalbumartist,
  ftscomposer,
  ftsperformer,
  ftsgrouping,
  ftsgenre,
  ftscomment,
  tokenize = "unicode61 remove_diacritics 1"

);

CREATE VIRTUAL TABLE IF NOT EXISTS tidal_songs_fts USING fts5(

  ftstitle,
  ftsalbum,
  ftsartist,
  ftsalbumartist,
  ftscomposer,
  ftsperformer,
  ftsgrouping,
  ftsgenre,
  ftscomment,
  tokenize = "unicode61 remove_diacritics 1"

);

CREATE VIRTUAL TABLE IF NOT EXISTS qobuz_artists_songs_fts USING fts5(

  ftstitle,
  ftsalbum,
  ftsartist,
  ftsalbumartist,
  ftscomposer,
  ftsperformer,
  ftsgrouping,
  ftsgenre,
  ftscomment,
  tokenize = "unicode61 remove_diacritics 1"

);

CREATE VIRTUAL TABLE IF NOT EXISTS qobuz_albums_songs_fts USING fts5(

  ftstitle,
  ftsalbum,
  ftsartist,
  ftsalbumartist,
  ftscomposer,
  ftsperformer,
  ftsgrouping,
  ftsgenre,
  ftscomment,
  tokenize = "unicode61 remove_diacritics 1"

);

CREATE VIRTUAL TABLE IF NOT EXISTS qobuz_songs_fts USING fts5(

  ftstitle,
  ftsalbum,
  ftsartist,
  ftsalbumartist,
  ftscomposer,
  ftsperformer,
  ftsgrouping,
  ftsgenre,
  ftscomment,
  tokenize = "unicode61 remove_diacritics 1"

);

CREATE VIRTUAL TABLE IF NOT EXISTS %allsongstables_fts USING fts5(

  ftstitle,
  ftsalbum,
  ftsartist,
  ftsalbumartist,
  ftscomposer,
  ftsperformer,
  ftsgrouping,
  ftsgenre,
  ftscomment,
  tokenize = "unicode61 remove_diacritics 1"

);
